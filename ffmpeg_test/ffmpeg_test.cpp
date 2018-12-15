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

int main(int argc, char **argv)
{
    //const char *filename_out, *codec_name;
    const AVCodec *codec;
    AVCodecContext *c = NULL;
    int i, ret, x, y, y_size;
    FILE *fp_in;
    FILE *fp_out;
    AVFrame *frame;
    AVPacket *pkt;
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    char filename_in[] = "G:/test.yuv";
    char filename_out[] = "output.265";
    char codec_name[] = "libx265";

    /*if (argc <= 2) {
        fprintf(stderr, "Usage: %s <output file> <codec name>\n", argv[0]);
        exit(0);
    }*/
    //filename_out = argv[1];
    //codec_name = argv[2];

    /* find the mpeg1video encoder */
    codec = avcodec_find_encoder_by_name(codec_name);
    if (!codec) {
        fprintf(stderr, "Codec '%s' not found\n", codec_name);
        exit(1);
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    pkt = av_packet_alloc();
    if (!pkt)
        exit(1);

    /* put sample parameters */
    c->bit_rate = 400000;
    /* resolution must be a multiple of two */
    c->width = 1920;
    c->height = 1080;
    /* frames per second */
    AVRational tmp1{ 1,25 };
    AVRational tmp2{ 25,1 };
    c->time_base = tmp1;
    c->framerate = tmp2;
    //c->time_base = (AVRational) { 1, 25 };
    //c->framerate = (AVRational) { 25, 1 };

    /* emit one intra frame every ten frames
    * check frame pict_type before passing frame
    * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
    * then gop_size is ignored and the output of encoder
    * will always be I frame irrespective to gop_size
    */
    c->gop_size = 10;
    c->max_b_frames = 1;
    c->pix_fmt = AV_PIX_FMT_YUV420P;

    if (codec->id == AV_CODEC_ID_H264)
        av_opt_set(c->priv_data, "preset", "slow", 0);

    /* open it */
    ret = avcodec_open2(c, codec, NULL);
    if (ret < 0) {
        //fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
        exit(1);
    }

    //Input raw data
    fp_in = fopen(filename_in, "rb");
    if (!fp_in) {
        fprintf(stderr, "Could not open %s\n", filename_in);
        return -1;
    }
    //Output bitstream
    fp_out = fopen(filename_out, "wb");
    if (!fp_out) {
        fprintf(stderr, "Could not open %s\n", filename_out);
        exit(1);
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    frame->format = c->pix_fmt;
    frame->width = c->width;
    frame->height = c->height;

    ret = av_frame_get_buffer(frame, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        exit(1);
    }

    y_size = frame->width * frame->height;
    /* encode 1 second of video */
    for (i = 0; i < 25; i++) {
        fflush(stdout);

        /* make sure the frame data is writable */
        ret = av_frame_make_writable(frame);
        if (ret < 0)
            exit(1);

        /* prepare a dummy image */
        /* Y */
        /*for (y = 0; y < c->height; y++) {
            for (x = 0; x < c->width; x++) {
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
            }
        }*/

        /* Cb and Cr */
        /*for (y = 0; y < c->height / 2; y++) {
            for (x = 0; x < c->width / 2; x++) {
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
            }
        }*/
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

        /* encode the image */
        encode(c, frame, pkt, fp_out);
    }

    /* flush the encoder */
    encode(c, NULL, pkt, fp_out);

    /* add sequence end code to have a real MPEG file */
    fwrite(endcode, 1, sizeof(endcode), fp_out);
    fclose(fp_out);
    fclose(fp_in);

    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);

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


