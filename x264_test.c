/*
 * =====================================================================================
 *
 *       Filename:  x265_test.c
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  12/17/2018 04:33:22 PM CST
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhihang wang (zhwang),
 *        Company:  www.qiyi.com
 *
 * =====================================================================================
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>

#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include "common.h"
#include "hb.h"
#include "log.h"
#include "../libhb/taskset.h"
#include "../libhb/hbtypes.h"
#include "../libhb/h264_common.h"
#include "x264.h"

#define FIRSTPASS 1

#if FIRSTPASS
//1 pass 
#define NUM_THREADS 4
#else
//2 pass
#define NUM_THREADS 4
#endif

#define HB_CONFIG_MAX_SIZE (2*8192)

typedef struct
{
    uint8_t sps[HB_CONFIG_MAX_SIZE];
    int sps_length;
    uint8_t pps[HB_CONFIG_MAX_SIZE];
    int pps_length;
    int init_delay;
} h264;

typedef struct
{
    struct hb_work_private_s *pv;
    int thread_idx;
    AVPacket *out;
}encwrapper_thread_arg_t;

struct hb_work_private_s {
    int                   thread_count;
    //x265_picture          *frame[NUM_THREADS];
    x264_picture_t        *frame;
    taskset_t             taskset;
    encwrapper_thread_arg_t *thread_data[NUM_THREADS];
    /*const x265_api *api[NUM_THREADS];
    x265_param *param[NUM_THREADS];
    x265_encoder *x265[NUM_THREADS];
    h265 x265header[NUM_THREADS];*/
    x264_param_t *param[NUM_THREADS];
    x264_t *x264[NUM_THREADS];
    h264 x264header[NUM_THREADS];
    FILE *fp_out[NUM_THREADS];
};

void write_packet(x264_picture_t *pic_out, x264_nal_t *nal, uint32_t nnal, h264 *header, FILE *outfile)
{
    /*if(pic_out->sliceType == X265_TYPE_IDR || pic_out->sliceType == X265_TYPE_I){
        fwrite(header->headers, 1, header->headers_length, outfile);
    }*/
    for(int i =0;i < nnal; i++){
        //printf("Pid is %d, Write packet %3"PRId64" (size=%5d)\n", getpid(), pic_out->pts, nal[i].sizeBytes);
        fwrite(nal[i].p_payload, 1, nal[i].i_payload, outfile);
    }
}

static void encode(x264_t *x264, x264_picture_t *pic_in, h264 *header, FILE *outfile)
{
    int ret;
    x264_picture_t pic_out;
    x264_nal_t *nal;
    uint32_t nnal;

    /* send the frame to the encoder */
    if (pic_in){
        printf("Pid is %d, Send frame %3"PRId64"\n", getpid(), pic_in->i_pts);

        ret = x264_encoder_encode(x264, &nal, &nnal, pic_in, &pic_out);
        if(nnal > 0) {
            write_packet(&pic_out, nal, nnal, header, outfile);
        }
    }else{
        while(x264_encoder_delayed_frames(x264)){
            x264_encoder_encode(x264, &nal, &nnal, pic_in, &pic_out);
            if(nnal == 0)
                continue;
            if(nnal < 0)
                return;

            write_packet(&pic_out, nal, nnal, header, outfile);
        }
    }
}

static void encoder_work(x264_t *x264, x264_picture_t *frame, h264 *header, FILE *fp_out)
{
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };

    /* encode the image or flush the encoder */
    encode(x264, frame, header, fp_out);

    /* add sequence end code to have a real MPEG file */
    fwrite(endcode, 1, sizeof(endcode), fp_out);
}

void encWork_thread(void *thread_args_v)
{
    encwrapper_thread_arg_t *thread_data = (encwrapper_thread_arg_t *)thread_args_v;
    hb_work_private_t *pv = thread_data->pv;
    int thread_idx = thread_data->thread_idx;

    while (1)
    {
        // Wait until there is work to do.
        taskset_thread_wait4start(&pv->taskset, thread_idx);

        if (taskset_thread_stop(&pv->taskset, thread_idx))
        {
            break;
        }

        encoder_work(pv->x264[thread_idx], pv->frame, &pv->x264header[thread_idx], pv->fp_out[thread_idx]);

        // Finished this segment, notify.
        taskset_thread_complete(&pv->taskset, thread_idx);
    }
    taskset_thread_complete(&pv->taskset, thread_idx);
}

int tasksetInit(hb_work_private_t *pv)
{
    if (taskset_init(&pv->taskset, pv->thread_count, sizeof(encwrapper_thread_arg_t)) == 0)
    {
        printf("Could not initialize taskset!\n");
        goto fail;
    }

    for (int i = 0; i < pv->thread_count; i++)
    {
        pv->thread_data[i] = (encwrapper_thread_arg_t *)taskset_thread_args(&pv->taskset, i);
        if (pv->thread_data[i] == NULL)
        {
            printf("Could not create thread args!\n");
            goto fail;
        }
        pv->thread_data[i]->pv = pv;
        pv->thread_data[i]->thread_idx = i;
        if (taskset_thread_spawn(&pv->taskset, i, "encwrapper", encWork_thread, 0) == 0)
        {
            printf("Could not spawn thread!\n");
            goto fail;
        }
    }

    return 0;

    fail:
        taskset_fini(&pv->taskset);
        free(pv);
        return -1;

    return 0;
}

int main(int argc, char **argv)
{
    //const char *filename_out, *codec_name;
    const AVCodec *codec[4];
    const int nal_prefix_length = 4;
    //AVCodecContext *c[4] = { NULL, NULL, NULL, NULL };
    int i, j, ret, y_size;
    x264_nal_t           *nal;
    uint32_t            nnal;
    FILE *fp_in;
    //FILE *fp_out;
    x264_picture_t pic_in;
    x264_picture_t pic_out;
    AVPacket *pkt[NUM_THREADS];
    //AVPixelFormat pixel_fmt = AV_PIX_FMT_YUV420P;
    int width = 1920;
    int height = 1080;
    int depth = 8;
    
    char filename_in[] = "/data/seqs/test3.yuv";
    char codec_name[] = "libx265";

    hb_work_private_t *pv = (hb_work_private_t *)calloc(1, sizeof(hb_work_private_t));
    pv->thread_count = NUM_THREADS;

    /*x264_param_t param;
    x264_param_default(&param);

    param.rc.i_rc_method = X264_RC_ABR;
    param.rc.i_bitrate = 1000;
    param.i_width = width;
    param.i_height = height;
    param.i_fps_num = 25;
    param.i_fps_den = 1;*/

    /*param.rc.i_rc_method = X264_RC_ABR;
    param.rc.i_bitrate = 1000;
    param.rc.i_qp_min = 3;
    param.i_threads = 12;
    param.i_width = width;
    param.i_height = height;
    param.i_timebase_num = 1;
    param.i_timebase_den = 90000;
    param.i_bframe = 3;
    param.i_bframe_adaptive = 2;
    param.i_bframe_pyramid = 1;
    param.b_annexb = 0;
    param.i_keyint_min = 25;
    param.i_keyint_max = 250;
    param.vui.i_colorprim = 1;
    param.vui.i_transfer = 1;
    param.vui.i_colmatrix = 1;
    param.analyse.i_weighted_pred = 0;
    param.analyse.b_weighted_bipred = 1;
    param.analyse.b_psnr = 1;
    param.analyse.b_ssim = 1;
    char *advanced_opts;*/

    if(FIRSTPASS){
        //1 pass
        //param.i_fps_num = 27000000;
        //param.i_fps_den = 1080000;
        //param.i_log_level = X264_LOG_INFO;
        //param.rc.b_stat_write = 1;
        //param.rc.psz_stat_out = "./1passdir/x264.log";
        //advanced_opts = "version=vtc-wh-8.0.0:handbrake:ssim:psnr:max-psnr=45:max-bitrate=2000:enhance-plain:no-psy:aq-mode=1:b-adapt=2:log=3:ref=3:mixed-refs:bframes=3:b-pyramid=1:weightb=1:weightp=0:analyse=all:8x8dct=1:subme=10:me=umh:merange=64:filter=-2,-2:trellis=2:fast-pskip=1:no-dct-decimate=1:direct=auto:qpmax=51:ref=1:subme=2:me=dia:analyse=none:trellis=0:no-fast-pskip=0:8x8dct=0:weightb=0:weightp=0";
    }else{
        //2 pass
        //param.i_fps_num = 26715039;
        //param.i_fps_den = 1080000;
        //param.i_log_level = X264_LOG_DEBUG;
        //param.rc.b_stat_read = 1;
        //param.rc.psz_stat_in = "./1passdir/x264.log";
        //advanced_opts = "version=vtc-wh-8.0.0:handbrake:ssim:psnr:max-psnr=45:max-bitrate=2000:enhance-plain:no-psy:aq-mode=1:b-adapt=2:log=3:ref=3:mixed-refs:bframes=3:b-pyramid=1:weightb=1:weightp=0:analyse=all:8x8dct=1:subme=10:me=umh:merange=64:filter=-2,-2:trellis=2:fast-pskip=1:no-dct-decimate=1:direct=auto:qpmax=51";
    }

    /*if (advanced_opts != NULL && *advanced_opts != '\0')
    {
        char *x264opts_start, *x264opts;

        hb_log("x264opts: %s", x264opts);

        x264opts_start = x264opts = strdup(advanced_opts);

        while (x264opts_start && *x264opts)
        {
            char *name = x264opts;
            char *value;
            int ret;

            x264opts += strcspn(x264opts, ":");
            if (*x264opts)
            {
                *x264opts = 0;
                x264opts++;
            }

            value = strchr(name, '=');
            if (value)
            {
                *value = 0;
                value++;
            }

            // Here's where the strings are passed to libx264 for parsing.
            if (FIRSTPASS && !strcmp(name, "dump-yuv"))
                continue;
            ret = x264_param_parse(&param, name, value);

            // Let x264 sanity check the options for us
            if (ret == X264_PARAM_BAD_NAME)
                hb_log("x264 options: Unknown suboption %s", name);
            if (ret == X264_PARAM_BAD_VALUE)
                hb_log("x264 options: Bad argument %s=%s", name, value ? value : "(null)");
        }
        free(x264opts_start);
    }*/

    for (i = 0; i < NUM_THREADS; i++) {
        x264_param_t *param = pv->param[i] = malloc(sizeof(x264_param_t));
        x264_param_default(param);

        param->rc.i_rc_method = X264_RC_ABR;
        param->rc.i_bitrate = 1000;
        param->i_width = width;
        param->i_height = height;
        param->i_fps_num = 25;
        param->i_fps_den = 1;

        //pv->api[i]->cleanup();//这句加上结果就不对了

        pv->x264[i] = x264_encoder_open(param);
        if (pv->x264[i] == NULL)
        {
            hb_error("encx264: x264_encoder_open failed.");
            goto fail;
        }
        ret = x264_encoder_headers(pv->x264[i], &nal, &nnal);
        if (ret < 0)
        {
            hb_error("encx264: x264_encoder_headers failed (%d)", ret);
            goto fail;
        }
        /* Sequence Parameter Set */
        memcpy(pv->x264header[i].sps, nal[0].p_payload + nal_prefix_length, nal[0].i_payload - nal_prefix_length);
        pv->x264header[i].sps_length = nal[0].i_payload - nal_prefix_length;

        /* Picture Parameter Set */
        memcpy(pv->x264header[i].pps, nal[1].p_payload + nal_prefix_length, nal[1].i_payload - nal_prefix_length);
        pv->x264header[i].pps_length = nal[1].i_payload - nal_prefix_length;

        char filename_out[20];
        sprintf(filename_out, "output_%d.264", i);
        //Output bitstream
        pv->fp_out[i] = fopen(filename_out, "wb");
        if (!pv->fp_out[i]) {
            fprintf(stderr, "Could not open %s\n", filename_out);
            exit(1);
        }
    }

    x264_picture_init(&pic_in);
    pic_in.img.i_csp = X264_CSP_I420;
    pic_in.img.i_plane = 3;
    pic_in.img.plane[0] = malloc(sizeof(uint8_t)*width*height);
    pic_in.img.plane[1] = malloc(sizeof(uint8_t)*width*height/4);
    pic_in.img.plane[2] = malloc(sizeof(uint8_t)*width*height/4);
    pic_in.img.i_stride[0] = width;
    pic_in.img.i_stride[1] = width / 2;
    pic_in.img.i_stride[2] = width / 2;
    pv->frame = &pic_in;

    tasksetInit(pv);

    //Input raw data
    fp_in = fopen(filename_in, "rb");
    if (!fp_in) {
        fprintf(stderr, "Could not open %s\n", filename_in);
        return -1;
    }

    y_size = width * height;
    /* encode 1 second of video */
    for (i = 0; i < 40; i++) {
        fflush(stdout);

        //Read raw YUV data
        if (fread(pic_in.img.plane[0], 1, y_size, fp_in) <= 0 ||     // Y
            fread(pic_in.img.plane[1], 1, y_size / 4, fp_in) <= 0 || // U
            fread(pic_in.img.plane[2], 1, y_size / 4, fp_in) <= 0) { // V
                return -1;
        }
        else if (feof(fp_in)) {
                break;
        }
        pic_in.i_pts = i;
        //pic_in.bitDepth = depth;

        //pv->frame = &pic_in;
        /* encode the image */
        taskset_cycle(&pv->taskset);
    }

    /* flush the encoder */
    pv->frame = NULL;
    taskset_cycle(&pv->taskset);

    fclose(fp_in);
    free(pic_in.img.plane[0]);
    free(pic_in.img.plane[1]);
    free(pic_in.img.plane[2]);
    //x264_picture_clean(&pic_in);

    for (i = 0; i < NUM_THREADS; i++)
    {
        //pv->api[i]->param_free(pv->param[i]);
        x264_encoder_close(pv->x264[i]);
        fclose(pv->fp_out[i]);
        free(pv->param[i]);
    }
    taskset_fini(&pv->taskset);
    //free(pv);

fail:
    free(pv);

    return 0;
}



