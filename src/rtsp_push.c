#include "rtsp_push.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 引入FFmpeg头文件
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>

// 内部使用的结构体，对调用者隐藏
struct RTSPPusher
{
    AVFormatContext *format_ctx;
    AVStream *video_stream;
    int fps;
    int64_t frame_index;
};

RTSPPusher *rtsp_pusher_init(const char *url, int width, int height, int fps)
{
    int ret;

    RTSPPusher *pusher = (RTSPPusher *)calloc(1, sizeof(RTSPPusher));
    if (!pusher)
    {
        fprintf(stderr, "Failed to allocate RTSPPusher\n");
        return NULL;
    }
    pusher->fps = fps;
    pusher->frame_index = 0;

    // 1. 分配AVFormatContext
    // avformat_alloc_output_context2 会根据url的后缀或指定的format_name来选择合适的复用器
    ret = avformat_alloc_output_context2(&pusher->format_ctx, NULL, "rtsp", url);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to allocate output context: %s\n", av_err2str(ret));
        goto fail;
    }

    // 2. 创建视频流
    pusher->video_stream = avformat_new_stream(pusher->format_ctx, NULL);
    if (!pusher->video_stream)
    {
        fprintf(stderr, "Failed to create new stream\n");
        goto fail;
    }
    pusher->video_stream->id = pusher->format_ctx->nb_streams - 1;

    // 3. 设置视频流参数
    // 这些参数会写入SDP中，客户端需要这些信息来正确解码
    AVCodecParameters *codecpar = pusher->video_stream->codecpar;
    codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    codecpar->codec_id = AV_CODEC_ID_H264;
    codecpar->width = width;
    codecpar->height = height;
    codecpar->format = AV_PIX_FMT_YUV420P; // H.264通常使用YUV420P

    // 4. 打开网络输出
    // AVFMT_NOFILE标志表示该格式不需要本地文件IO（例如RTSP、RTMP）
    if (!(pusher->format_ctx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&pusher->format_ctx->pb, url, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            fprintf(stderr, "Could not open output URL '%s': %s\n", url, av_err2str(ret));
            goto fail;
        }
    }

    // 设置RTSP特定选项，例如传输协议为TCP，可以避免UDP丢包问题
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);
    av_dict_set(&opts, "stimeout", "5000000", 0); // 设置5秒超时

    // 5. 写入媒体头信息
    // 这会与服务器进行RTSP握手（ANNOUNCE/SETUP/RECORD）
    ret = avformat_write_header(pusher->format_ctx, &opts);
    av_dict_free(&opts);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to write header: %s\n", av_err2str(ret));
        goto fail;
    }

    printf("RTSP pusher initialized successfully. Streaming to: %s\n", url);
    av_dump_format(pusher->format_ctx, 0, url, 1);

    return pusher;

fail:
    rtsp_pusher_close(pusher);
    return NULL;
}

int rtsp_pusher_push_frame(RTSPPusher *pusher, const unsigned char *data, int size)
{
    if (!pusher || !pusher->format_ctx || !data || size <= 0)
    {
        return -1;
    }

    printf("推流帧为%d字节\n", size);

    AVPacket pkt;
    av_init_packet(&pkt);

    // 设置时间戳 (PTS 和 DTS)
    // H.264流，没有B帧的情况下，PTS和DTS通常是相同的
    // 时间基 (time_base) 是计算时间戳的基础
    AVRational time_base = pusher->video_stream->time_base;

    // 我们需要将帧号转换为时间戳。1/fps 是帧的持续时间。
    // av_rescale_q 用于在不同的时间基之间安全地转换时间戳。
    pkt.pts = av_rescale_q(pusher->frame_index, (AVRational){1, pusher->fps}, time_base);
    pkt.dts = pkt.pts;
    pkt.duration = av_rescale_q(1, (AVRational){1, pusher->fps}, time_base);

    pkt.data = (uint8_t *)data;
    pkt.size = size;
    pkt.stream_index = pusher->video_stream->index;

    // 关键帧的标志，对于I帧需要设置
    // H.264 NALU type 5 is an IDR frame (a type of I-frame)
    // NALU type 7 is SPS, 8 is PPS.
    // 我们简单地检查NALU type来判断是否是关键帧
    if (data[4] != 0 && (data[4] & 0x1F) == 5)
    {
        pkt.flags |= AV_PKT_FLAG_KEY;
    }

    pusher->frame_index++;

    // 发送数据包
    // av_interleaved_write_frame 会处理RTP打包和发送
    int ret = av_interleaved_write_frame(pusher->format_ctx, &pkt);
    if (ret < 0)
    {
        fprintf(stderr, "Error while writing frame: %s\n", av_err2str(ret));
        return ret;
    }

    return 0;
}

void rtsp_pusher_close(RTSPPusher *pusher)
{
    if (!pusher)
    {
        return;
    }

    if (pusher->format_ctx)
    {
        // 发送RTSP TEARDOWN命令
        av_write_trailer(pusher->format_ctx);

        // 如果打开了avio context，则关闭它
        if (!(pusher->format_ctx->oformat->flags & AVFMT_NOFILE) && pusher->format_ctx->pb)
        {
            avio_closep(&pusher->format_ctx->pb);
        }

        // 释放AVFormatContext
        avformat_free_context(pusher->format_ctx);
    }

    free(pusher);
    printf("RTSP pusher closed.\n");
}