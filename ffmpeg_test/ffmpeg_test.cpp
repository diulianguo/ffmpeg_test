// ffmpeg_test.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>

#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

#include "taskset.h"
#include "hbtypes.h"

#define NUM_THREADS 4

typedef struct
{
    struct hb_work_private_s *pv;
    int thread_idx;
    AVPacket *out;
}encwrapper_thread_arg_t;

struct hb_work_private_s {
    int                   thread_count;
    AVFrame               *frame;
    AVPacket              *packet[NUM_THREADS];
    taskset_t             taskset;
    encwrapper_thread_arg_t *thread_data[NUM_THREADS];
    AVCodecContext        *c[NUM_THREADS];
};

static void encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt,
    FILE *outfile)
{
    int ret;

    /* send the frame to the encoder */
    if (frame)
        printf("Send frame %3" PRId64"\n", frame->pts);

    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        exit(1);
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            exit(1);
        }

        printf("Write packet %3" PRId64" (size=%5d)\n", pkt->pts, pkt->size);
        fwrite(pkt->data, 1, pkt->size, outfile);
        av_packet_unref(pkt);
    }
}

static void encoder_work(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt, int i)
{
    FILE *fp_out;
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    char filename_out[20];
    sprintf(filename_out, "output_%d.265", i);
    char codec_name[] = "libx265";
    //Output bitstream
    fp_out = fopen(filename_out, "wb");
    if (!fp_out) {
        fprintf(stderr, "Could not open %s\n", filename_out);
        exit(1);
    }

    /* encode the image or flush the encoder */
    encode(enc_ctx, frame, pkt, fp_out);

    /* add sequence end code to have a real MPEG file */
    fwrite(endcode, 1, sizeof(endcode), fp_out);
    fclose(fp_out);
}

void encWork_thread(void *thread_args_v)
{
    encwrapper_thread_arg_t *thread_data = (encwrapper_thread_arg_t *)thread_args_v;
    hb_work_private_s *pv = thread_data->pv;
    int thread_idx = thread_data->thread_idx;

    while (1)
    {
        // Wait until there is work to do.
        taskset_thread_wait4start(&pv->taskset, thread_idx);

        if (taskset_thread_stop(&pv->taskset, thread_idx))
        {
            break;
        }

        encoder_work(pv->c[thread_idx], pv->frame, pv->packet[thread_idx], thread_idx);

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
    FILE *fp_in;
    //FILE *fp_out;
    AVFrame *frame;
    AVPacket *pkt[NUM_THREADS];
    AVPixelFormat pixel_fmt = AV_PIX_FMT_YUV420P;
    int width = 1920;
    int height = 1080;
    
    char filename_in[] = "G:/test3.yuv";
    char filename_out[] = "output.265";
    char codec_name[] = "libx265";

    hb_work_private_t *pv = (hb_work_private_t *)calloc(1, sizeof(hb_work_private_t));
    pv->thread_count = NUM_THREADS;

    /*if (argc <= 2) {
        fprintf(stderr, "Usage: %s <output file> <codec name>\n", argv[0]);
        exit(0);
    }*/
    //filename_out = argv[1];
    //codec_name = argv[2];

    for (i = 0; i < NUM_THREADS; i++)
    {
        pkt[i] = av_packet_alloc();
        if (!pkt[i])
            exit(1);
    }

    for (i = 0; i < NUM_THREADS; i++) {
        /* find the mpeg1video encoder */
        codec[i] = avcodec_find_encoder_by_name(codec_name);
        if (!codec) {
            fprintf(stderr, "Codec '%s' not found\n", codec_name);
            exit(1);
        }

        pv->c[i] = avcodec_alloc_context3(codec[i]);
        if (!pv->c[i]) {
            fprintf(stderr, "Could not allocate video codec context\n");
            exit(1);
        }
        /* put sample parameters */
        pv->c[i]->bit_rate = 400000;
        /* resolution must be a multiple of two */
        pv->c[i]->width = width;
        pv->c[i]->height = height;
        /* frames per second */
        AVRational tmp1{ 1,25 };
        AVRational tmp2{ 25,1 };
        pv->c[i]->time_base = tmp1;
        pv->c[i]->framerate = tmp2;
        //c->time_base = (AVRational) { 1, 25 };
        //c->framerate = (AVRational) { 25, 1 };

        /* emit one intra frame every ten frames
        * check frame pict_type before passing frame
        * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
        * then gop_size is ignored and the output of encoder
        * will always be I frame irrespective to gop_size
        */
        pv->c[i]->gop_size = 10;
        pv->c[i]->max_b_frames = 1;
        pv->c[i]->pix_fmt = pixel_fmt;

        if (codec[i]->id == AV_CODEC_ID_H264)
            av_opt_set(pv->c[i]->priv_data, "preset", "slow", 0);

        /* open it */
        ret = avcodec_open2(pv->c[i], codec[i], NULL);
        if (ret < 0) {
            //fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
            exit(1);
        }
    }

    tasksetInit(pv);

    //Input raw data
    fp_in = fopen(filename_in, "rb");
    if (!fp_in) {
        fprintf(stderr, "Could not open %s\n", filename_in);
        return -1;
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    frame->format = pixel_fmt;
    frame->width = width;
    frame->height = height;

    ret = av_frame_get_buffer(frame, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        exit(1);
    }

    y_size = frame->width * frame->height;
    /* encode 1 second of video */
    for (i = 0; i < 252; i++) {
        fflush(stdout);

        /* make sure the frame data is writable */
        ret = av_frame_make_writable(frame);
        if (ret < 0)
            exit(1);

        //Read raw YUV data
        if (fread(frame->data[0], 1, y_size, fp_in) <= 0 ||		// Y
            fread(frame->data[1], 1, y_size / 4, fp_in) <= 0 ||	// U
            fread(frame->data[2], 1, y_size / 4, fp_in) <= 0) {	// V
            return -1;
        }
        else if (feof(fp_in)) {
            break;
        }
        frame->pts = i;

        pv->frame = frame;
        for (j = 0; j < NUM_THREADS; j++)
        {
            pv->packet[j] = pkt[j];
        }
        /* encode the image */
        taskset_cycle(&pv->taskset);
    }

    /* flush the encoder */
    pv->frame = NULL;
    for (j = 0; j < NUM_THREADS; j++)
    {
        pv->packet[j] = pkt[j];
    }
    taskset_cycle(&pv->taskset);

    fclose(fp_in);

    for (i = 0; i < NUM_THREADS; i++)
    {
        avcodec_free_context(&pv->c[i]);
    }
    av_frame_free(&frame);
    for (i = 0; i < NUM_THREADS; i++)
    {
        av_packet_free(&pkt[i]);
    }
    taskset_fini(&pv->taskset);
    free(pv);

    system("pause");
    return 0;
}

//int main(int argc, char* argv[])
//{
//    AVCodec *pCodec;
//    AVCodecContext *pCodecCtx = NULL;
//    int i, ret, got_output;
//    FILE *fp_in;
//    FILE *fp_out;
//    AVFrame *pFrame;
//    AVPacket pkt;
//    int y_size;
//    int framecnt = 0;
//
//    char filename_in[] = "../ds_480x272.yuv";
//
//#if TEST_HEVC
//    AVCodecID codec_id = AV_CODEC_ID_HEVC;
//    char filename_out[] = "ds.hevc";
//#else
//    AVCodecID codec_id = AV_CODEC_ID_H264;
//    char filename_out[] = "ds.h264";
//#endif
//
//
//    int in_w = 480, in_h = 272;
//    int framenum = 100;
//
//    avcodec_register_all();
//
//    pCodec = avcodec_find_encoder(codec_id);
//    if (!pCodec) {
//        printf("Codec not found\n");
//        return -1;
//    }
//    pCodecCtx = avcodec_alloc_context3(pCodec);
//    if (!pCodecCtx) {
//        printf("Could not allocate video codec context\n");
//        return -1;
//    }
//    pCodecCtx->bit_rate = 400000;
//    pCodecCtx->width = in_w;
//    pCodecCtx->height = in_h;
//    pCodecCtx->time_base.num = 1;
//    pCodecCtx->time_base.den = 25;
//    pCodecCtx->gop_size = 10;
//    pCodecCtx->max_b_frames = 1;
//    pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
//
//    if (codec_id == AV_CODEC_ID_H264)
//        av_opt_set(pCodecCtx->priv_data, "preset", "slow", 0);
//
//    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
//        printf("Could not open codec\n");
//        return -1;
//    }
//
//    pFrame = av_frame_alloc();
//    if (!pFrame) {
//        printf("Could not allocate video frame\n");
//        return -1;
//    }
//    pFrame->format = pCodecCtx->pix_fmt;
//    pFrame->width = pCodecCtx->width;
//    pFrame->height = pCodecCtx->height;
//
//    ret = av_image_alloc(pFrame->data, pFrame->linesize, pCodecCtx->width, pCodecCtx->height,
//        pCodecCtx->pix_fmt, 16);
//    if (ret < 0) {
//        printf("Could not allocate raw picture buffer\n");
//        return -1;
//    }
//    //Input raw data
//    fp_in = fopen(filename_in, "rb");
//    if (!fp_in) {
//        printf("Could not open %s\n", filename_in);
//        return -1;
//    }
//    //Output bitstream
//    fp_out = fopen(filename_out, "wb");
//    if (!fp_out) {
//        printf("Could not open %s\n", filename_out);
//        return -1;
//    }
//
//    y_size = pCodecCtx->width * pCodecCtx->height;
//    //Encode
//    for (i = 0; i < framenum; i++) {
//        av_init_packet(&pkt);
//        pkt.data = NULL;    // packet data will be allocated by the encoder
//        pkt.size = 0;
//        //Read raw YUV data
//        if (fread(pFrame->data[0], 1, y_size, fp_in) <= 0 ||		// Y
//            fread(pFrame->data[1], 1, y_size / 4, fp_in) <= 0 ||	// U
//            fread(pFrame->data[2], 1, y_size / 4, fp_in) <= 0) {	// V
//            return -1;
//        }
//        else if (feof(fp_in)) {
//            break;
//        }
//
//        pFrame->pts = i;
//        /* encode the image */
//        ret = avcodec_encode_video2(pCodecCtx, &pkt, pFrame, &got_output);
//        if (ret < 0) {
//            printf("Error encoding frame\n");
//            return -1;
//        }
//        if (got_output) {
//            printf("Succeed to encode frame: %5d\tsize:%5d\n", framecnt, pkt.size);
//            framecnt++;
//            fwrite(pkt.data, 1, pkt.size, fp_out);
//            av_free_packet(&pkt);
//        }
//    }
//    //Flush Encoder
//    for (got_output = 1; got_output; i++) {
//        ret = avcodec_encode_video2(pCodecCtx, &pkt, NULL, &got_output);
//        if (ret < 0) {
//            printf("Error encoding frame\n");
//            return -1;
//        }
//        if (got_output) {
//            printf("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n", pkt.size);
//            fwrite(pkt.data, 1, pkt.size, fp_out);
//            av_free_packet(&pkt);
//        }
//    }
//
//    fclose(fp_out);
//    avcodec_close(pCodecCtx);
//    av_free(pCodecCtx);
//    av_freep(&pFrame->data[0]);
//    av_frame_free(&pFrame);
//
//    return 0;
//}


