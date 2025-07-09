#ifndef RTSP_PUSH_H
#define RTSP_PUSH_H

#ifdef __cplusplus
extern "C" {
#endif

// 定义一个不透明的结构体指针，隐藏内部实现
typedef struct RTSPPusher RTSPPusher;

/**
 * @brief 初始化RTSP推流器
 * 
 * @param url ZLMediaKit的推流地址，例如 "rtsp://127.0.0.1:554/live/stream_id"
 * @param width 视频宽度
 * @param height 视频高度
 * @param fps 视频帧率
 * @return 成功返回RTSPPusher句柄，失败返回NULL
 */
RTSPPusher* rtsp_pusher_init(const char* url, int width, int height, int fps);

/**
 * @brief 推送一帧H.264数据
 * 
 * @param pusher rtsp_pusher_init返回的句柄
 * @param data H.264帧数据的缓冲区指针 (通常是一个完整的NALU或一个访问单元)
 * @param size H.264帧数据的大小
 * @return 成功返回0，失败返回负数
 */
int rtsp_pusher_push_frame(RTSPPusher* pusher, const unsigned char* data, int size);

/**
 * @brief 关闭RTSP推流器并释放资源
 * 
 * @param pusher rtsp_pusher_init返回的句柄
 */
void rtsp_pusher_close(RTSPPusher* pusher);

#ifdef __cplusplus
}
#endif

#endif // RTSP_PUSH_H