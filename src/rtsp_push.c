#include "rtsp_push.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 引入FFmpeg头文件
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
#include <libavutil/time.h> // [ADDED] 引入用于获取高精度时间的头文件

// 内部使用的结构体，对调用者隐藏
struct RTSPPusher
{
    AVFormatContext *format_ctx;
    AVStream *video_stream;
    int fps;
    int64_t start_time; // [MODIFIED] 使用起始时间来计算时间戳，替换 frame_index
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
    // [ADDED] 记录推流开始的精确时间
    pusher->start_time = av_gettime_relative();

    // 1. 分配AVFormatContext
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
    AVCodecParameters *codecpar = pusher->video_stream->codecpar;
    codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    codecpar->codec_id = AV_CODEC_ID_H264;
    codecpar->width = width;
    codecpar->height = height;
    codecpar->format = AV_PIX_FMT_YUV420P;

    // 4. 打开网络输出
    // [REMOVED] 对于RTSP, avformat_write_header会处理网络连接，无需手动调用avio_open
    // 因此，相关的代码块被移除。

    // 设置RTSP特定选项
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "rtsp_transport", "udp", 0);
    av_dict_set(&opts, "stimeout", "5000000", 0);

    // 5. 写入媒体头信息 (与服务器进行RTSP握手)
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

    // [MODIFIED] 不再打印每帧日志，以提升性能。如果需要，可以手动取消注释。
    // printf("推流帧为%d字节\n", size);

    AVPacket pkt;
    av_init_packet(&pkt);

    // [MODIFIED] 使用基于真实时间的精确时间戳计算
    // --------------------------------------------------------------------
    // 获取从推流开始到现在的微秒数
    int64_t elapsed_time_us = av_gettime_relative() - pusher->start_time;

    // FFmpeg内部的时间基，用于 av_gettime_relative()
    AVRational us_time_base = {1, 1000000};

    // 视频流的时间基
    AVRational stream_time_base = pusher->video_stream->time_base;

    // 将我们计算的流逝时间从微秒基转换为视频流的时间基
    pkt.pts = av_rescale_q(elapsed_time_us, us_time_base, stream_time_base);
    pkt.dts = pkt.pts; // 在没有B帧的情况下，DTS和PTS相同

    // 帧的持续时间仍然可以基于FPS估算，这对播放器是有益的
    pkt.duration = av_rescale_q(1, (AVRational){1, pusher->fps}, stream_time_base);
    // --------------------------------------------------------------------

    pkt.data = (uint8_t *)data;
    pkt.size = size;
    pkt.stream_index = pusher->video_stream->index;

    // 简单地检查NALU type来判断是否是关键帧
    // H.264 NALU type 5 is an IDR frame (a type of I-frame)
    // 注意: 这个检查假设了H.264流以 `00 00 00 01` 开头。
    if (size > 4 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1 && (data[4] & 0x1F) == 5)
    {
        pkt.flags |= AV_PKT_FLAG_KEY;
    }

    // 发送数据包
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
        avformat_free_context(pusher->format_ctx);
    }

    free(pusher);
    printf("RTSP pusher closed.\n");
}