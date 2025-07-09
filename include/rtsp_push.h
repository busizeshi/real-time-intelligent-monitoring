#ifndef RTSP_PUSHER_C_H
#define RTSP_PUSHER_C_H

#include <stdint.h>

// 在C++中包含此头文件时，确保FFmpeg头文件以C方式链接
#ifdef __cplusplus
extern "C" {
#endif

#include <libavformat/avformat.h>
#include <libavutil/time.h>

// 前向声明我们的上下文结构体
struct RTSPPusherContext;
typedef struct RTSPPusherContext RTSPPusherContext;

/**
 * @brief 创建并初始化推流器上下文
 * @param rtsp_url ZLMediaKit的RTSP推流地址，例如 "rtsp://192.168.1.10/live/stream"
 * @param width 视频宽度
 * @param height 视频高度
 * @param framerate 视频帧率
 * @return 成功返回一个指向RTSPPusherContext的指针，失败返回NULL
 */
RTSPPusherContext* rtsp_pusher_init(const char* rtsp_url, int width, int height, int framerate);

/**
 * @brief 推送一帧H.264数据
 * @param ctx rtsp_pusher_init返回的上下文指针
 * @param data H.264帧数据的指针 (NALU)
 * @param size 帧数据的大小
 * @return 成功返回0，失败返回一个负数错误码
 */
int rtsp_pusher_push_frame(RTSPPusherContext* ctx, const uint8_t* data, int size);

/**
 * @brief 清理并释放所有推流器相关的资源
 * @param ctx rtsp_pusher_init返回的上下文指针
 */
void rtsp_pusher_cleanup(RTSPPusherContext* ctx);


#ifdef __cplusplus
}
#endif

#endif // RTSP_PUSHER_C_H