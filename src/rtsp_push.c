#include "rtsp_push.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 定义我们的上下文结构体
struct RTSPPusherContext
{
    AVFormatContext *format_ctx;
    AVStream *video_stream;
    char *rtsp_url;
    int framerate;
    int64_t frame_index;
    int is_initialized;
};

// 内部错误日志函数
static void log_ffmpeg_error(int err_num, const char *message)
{
    char err_buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err_num, err_buf, sizeof(err_buf));
    fprintf(stderr, "%s: %s (code: %d)\n", message, err_buf, err_num);
}

RTSPPusherContext *rtsp_pusher_init(const char *rtsp_url, int width, int height, int framerate)
{
    RTSPPusherContext *ctx = (RTSPPusherContext *)malloc(sizeof(RTSPPusherContext));
    if (!ctx)
    {
        fprintf(stderr, "Failed to allocate RTSPPusherContext\n");
        return NULL;
    }
    // 使用memset初始化，防止野指针
    memset(ctx, 0, sizeof(RTSPPusherContext));

    ctx->rtsp_url = strdup(rtsp_url);
    if (!ctx->rtsp_url)
    {
        fprintf(stderr, "Failed to duplicate rtsp_url string.\n");
        free(ctx);
        return NULL;
    }
    ctx->framerate = framerate;
    ctx->frame_index = 0;

    int ret = 0;

    // 1. 分配AVFormatContext
    ret = avformat_alloc_output_context2(&ctx->format_ctx, NULL, "rtsp", ctx->rtsp_url);
    if (ret < 0)
    {
        log_ffmpeg_error(ret, "Failed to allocate output context");
        rtsp_pusher_cleanup(ctx);
        return NULL;
    }

    // 2. 创建视频流
    ctx->video_stream = avformat_new_stream(ctx->format_ctx, NULL);
    if (!ctx->video_stream)
    {
        fprintf(stderr, "Failed to create new stream\n");
        rtsp_pusher_cleanup(ctx);
        return NULL;
    }
    ctx->video_stream->id = ctx->format_ctx->nb_streams - 1;

    // 3. 设置视频流参数
    AVCodecParameters *codec_params = ctx->video_stream->codecpar;
    codec_params->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_params->codec_id = AV_CODEC_ID_H264;
    codec_params->width = width;
    codec_params->height = height;

    // 设置时间基，这是计算PTS/DTS的关键
    ctx->video_stream->time_base = (AVRational){1, ctx->framerate};

    // 4. 打开网络输出
    if (!(ctx->format_ctx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ctx->format_ctx->pb, ctx->rtsp_url, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            log_ffmpeg_error(ret, "Failed to open URL");
            rtsp_pusher_cleanup(ctx);
            return NULL;
        }
    }

    // 5. 写入RTSP头信息
    AVDictionary *options = NULL;
    av_dict_set(&options, "rtsp_transport", "udp", 0);
    av_dict_set(&options, "stimeout", "5000000", 0);
    ret = avformat_write_header(ctx->format_ctx, &options);
    av_dict_free(&options);
    if (ret < 0)
    {
        log_ffmpeg_error(ret, "Failed to write header");
        rtsp_pusher_cleanup(ctx);
        return NULL;
    }

    printf("RTSP Pusher initialized successfully. Streaming to: %s\n", ctx->rtsp_url);
    ctx->is_initialized = 1;
    return ctx;
}

int rtsp_pusher_push_frame(RTSPPusherContext *ctx, const uint8_t *data, int size)
{
    if (!ctx || !ctx->is_initialized)
    {
        fprintf(stderr, "Pusher not initialized.\n");
        return -1;
    }

    AVPacket pkt;
    av_init_packet(&pkt);

    // 计算PTS (Presentation Timestamp) 和 DTS (Decoding Timestamp)
    pkt.pts = ctx->frame_index;
    pkt.dts = ctx->frame_index;
    pkt.duration = 1;

    av_packet_rescale_ts(&pkt, (AVRational){1, ctx->framerate}, ctx->video_stream->time_base);

    // FFmpeg API需要非const指针，但我们保证不修改
    pkt.data = (uint8_t *)data;
    pkt.size = size;
    pkt.stream_index = ctx->video_stream->id;

    // H264的NALU类型，0x05 (IDR), 0x07 (SPS), 0x08 (PPS) 都很重要
    // NALU Header (1 byte): [F(1) NRI(2) Type(5)]
    // 类型码是低5位
    uint8_t nalu_type = data[4] & 0x1F;
    if (nalu_type == 5 || nalu_type == 7)
    { // IDR帧或SPS/PPS组合
        pkt.flags |= AV_PKT_FLAG_KEY;
    }

    // 发送数据包
    int ret = av_interleaved_write_frame(ctx->format_ctx, &pkt);
    if (ret < 0)
    {
        log_ffmpeg_error(ret, "Failed to write frame");
        // 不需要 av_packet_unref(&pkt); 因为pkt.data是我们外部的，没有分配内存
        return ret;
    }

    ctx->frame_index++;
    return 0;
}

void rtsp_pusher_cleanup(RTSPPusherContext *ctx)
{
    if (!ctx)
    {
        return;
    }

    printf("Cleaning up RTSP Pusher...\n");

    // 仅在成功初始化后才发送trailer
    if (ctx->is_initialized && ctx->format_ctx)
    {
        av_write_trailer(ctx->format_ctx);
    }

    // 关闭网络IO
    if (ctx->format_ctx && !(ctx->format_ctx->oformat->flags & AVFMT_NOFILE) && ctx->format_ctx->pb)
    {
        avio_closep(&ctx->format_ctx->pb);
    }

    // 释放Context
    if (ctx->format_ctx)
    {
        avformat_free_context(ctx->format_ctx);
    }

    // 释放我们自己分配的内存
    if (ctx->rtsp_url)
    {
        free(ctx->rtsp_url);
    }

    free(ctx);
    printf("Cleanup finished.\n");
}