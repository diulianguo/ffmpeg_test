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
#include "../libhb/h265_common.h"
#include "x265.h"

/*#define NUM_THREADS 4
#define HB_CONFIG_MAX_SIZE (2*8192)

typedef struct
{
    uint8_t headers[HB_CONFIG_MAX_SIZE];
    int headers_length;
} h265;

typedef struct
{
    struct hb_work_private_s *pv;
    int thread_idx;
    AVPacket *out;
}encwrapper_thread_arg_t;

struct hb_work_private_s {
    int                   thread_count;
    x265_picture          *frame[NUM_THREADS];
    taskset_t             taskset;
    encwrapper_thread_arg_t *thread_data[NUM_THREADS];
    const x265_api *api[NUM_THREADS];
    x265_param *param[NUM_THREADS];
    x265_encoder *x265[NUM_THREADS];
    h265 x265header[NUM_THREADS];
    FILE *fp_out[NUM_THREADS];
};

void write_packet(x265_picture *pic_out, x265_nal *nal, uint32_t nnal, h265 *header, FILE *outfile)
{
    if(pic_out->sliceType == X265_TYPE_IDR || pic_out->sliceType == X265_TYPE_I){
        fwrite(header->headers, 1, header->headers_length, outfile);
    }
    for(int i =0;i < nnal; i++){
        printf("Pid is %d, Write packet %3"PRId64" (size=%5d)\n", getpid(), pic_out->pts, nal[i].sizeBytes);
        fwrite(nal[i].payload, 1, nal[i].sizeBytes, outfile);
    }
}

static void encode(x265_api *api, x265_encoder *x265, x265_picture *pic_in, h265 *header, FILE *outfile)
{
    int ret;
    x265_picture pic_out;
    x265_nal *nal;
    uint32_t nnal;

    // send the frame to the encoder
    if (pic_in){
        printf("Pid is %d, Send frame %3"PRId64"\n", getpid(), pic_in->pts);

        ret = api->encoder_encode(x265, &nal, &nnal, pic_in, &pic_out);
        if (ret < 0) {
            fprintf(stderr, "Error sending a frame for encoding\n");
            exit(1);
        }

        if(ret > 0) {
            write_packet(&pic_out, nal, nnal, header, outfile);
        }
    }else{
        while(api->encoder_encode(x265, &nal, &nnal, pic_in, &pic_out) > 0){
            write_packet(&pic_out, nal, nnal, header, outfile);
        }
    }
}

static void encoder_work(x265_api *api, x265_encoder *x265, x265_picture *frame, h265 *header, FILE *fp_out)
{
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };

    // encode the image or flush the encoder
    encode(api, x265, frame, header, fp_out);

    // add sequence end code to have a real MPEG file
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

        encoder_work(pv->api[thread_idx], pv->x265[thread_idx], pv->frame[thread_idx], &pv->x265header[thread_idx], pv->fp_out[thread_idx]);

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
    //AVCodecContext *c[4] = { NULL, NULL, NULL, NULL };
    int i, j, ret, y_size;
    x265_nal           *nal;
    uint32_t            nnal;
    FILE *fp_in;
    //FILE *fp_out;
    x265_picture pic_in[4];
    x265_picture pic_out;
    AVPacket *pkt[NUM_THREADS];
    //AVPixelFormat pixel_fmt = AV_PIX_FMT_YUV420P;
    int width = 1920;
    int height = 1080;
    int depth = 8;
    
    char filename_in[] = "/data/seqs/test3.yuv";
    char codec_name[] = "libx265";

    hb_work_private_t *pv = (hb_work_private_t *)calloc(1, sizeof(hb_work_private_t));
    pv->thread_count = NUM_THREADS;

    for (i = 0; i < NUM_THREADS; i++) {
        pv->api[i] =  x265_api_query(depth, X265_BUILD, NULL);
        if (pv->api[i] == NULL)
        {
            hb_error("encx265: x265_api_query failed, bit depth %d.", depth);
            goto fail;
        }
        x265_param * param = pv->param[i] = pv->api[i]->param_alloc();
        // find the mpeg1video encoder
        if (pv->api[i]->param_default_preset(param, 0, 0) < 0)
        {
            hb_error("encx265: x265_param_default_preset failed. Preset 0 Tune 0");
            goto fail;
        }
        param->sourceWidth = width;
        param->sourceHeight = height;
        param->fpsNum = 25;
        param->fpsDenom = 1;

        //pv->api[i]->cleanup();//这句加上结果就不对了

        pv->x265[i] = pv->api[i]->encoder_open(param);
        if (pv->x265[i] == NULL)
        {
            hb_error("encx265: x265_encoder_open failed.");
            goto fail;
        }
        ret = pv->api[i]->encoder_headers(pv->x265[i], &nal, &nnal);
        if (ret < 0)
        {
            hb_error("encx265: x265_encoder_headers failed (%d)", ret);
            goto fail;
        }
        if (ret > sizeof(pv->x265header[i].headers))
        {
            hb_error("encx265: bitstream headers too large (%d)", ret);
            goto fail;
        }
        memcpy(pv->x265header[i].headers, nal->payload, ret);
        pv->x265header[i].headers_length = ret;

        char filename_out[20];
        sprintf(filename_out, "output_%d.265", i);
        //Output bitstream
        pv->fp_out[i] = fopen(filename_out, "wb");
        if (!pv->fp_out[i]) {
            fprintf(stderr, "Could not open %s\n", filename_out);
            exit(1);
        }
        pv->api[i]->picture_init(pv->param[0], &pic_in[i]);
        pic_in[i].planes[0] = malloc(sizeof(uint8_t)*width*height);
        pic_in[i].planes[1] = malloc(sizeof(uint8_t)*width*height/4);
        pic_in[i].planes[2] = malloc(sizeof(uint8_t)*width*height/4);
        pic_in[i].stride[0] = width;
        pic_in[i].stride[1] = width / 2;
        pic_in[i].stride[2] = width / 2;
        pv->frame[i] = &pic_in[i];
    }

    tasksetInit(pv);

    //Input raw data
    fp_in = fopen(filename_in, "rb");
    if (!fp_in) {
        fprintf(stderr, "Could not open %s\n", filename_in);
        return -1;
    }

    y_size = width * height;
    // encode 1 second of video
    for (i = 0; i < 50; i++) {
        fflush(stdout);

        //Read raw YUV data
        if (fread(pic_in[0].planes[0], 1, y_size, fp_in) <= 0 ||     // Y
            fread(pic_in[0].planes[1], 1, y_size / 4, fp_in) <= 0 || // U
            fread(pic_in[0].planes[2], 1, y_size / 4, fp_in) <= 0) { // V
                return -1;
        }
        else if (feof(fp_in)) {
                break;
        }
        pic_in[0].pts = i;
        pic_in[0].bitDepth = depth;
        for(int j = 1; j<NUM_THREADS;j++){
            //Read raw YUV data
            memcpy(pic_in[j].planes[0], pic_in[0].planes[0], y_size);
            memcpy(pic_in[j].planes[1], pic_in[0].planes[1], y_size / 4);
            memcpy(pic_in[j].planes[2], pic_in[0].planes[2], y_size / 4);
            pic_in[j].pts = i;
            pic_in[j].bitDepth = depth;
        }

        //pv->frame = &pic_in;
        // encode the image
        taskset_cycle(&pv->taskset);
    }

    // flush the encoder
    for(int j = 0; j<NUM_THREADS;j++){
        pv->frame[j] = NULL;
    }
    taskset_cycle(&pv->taskset);

    fclose(fp_in);
    for(int j = 0; j<NUM_THREADS;j++){
        free(pic_in[j].planes[0]);
        free(pic_in[j].planes[1]);
        free(pic_in[j].planes[2]);
    }

    for (i = 0; i < NUM_THREADS; i++)
    {
        pv->api[i]->param_free(pv->param[i]);
        pv->api[i]->encoder_close(pv->x265[i]);
        fclose(pv->fp_out[i]);
    }
    taskset_fini(&pv->taskset);
    //free(pv);

fail:
    free(pv);

    return 0;
}
*/


