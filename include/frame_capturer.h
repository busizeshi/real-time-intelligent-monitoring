/**
 * @file frame_capturer.h
 * @brief 封装瑞芯微isp摄像头类
 */
#ifndef FRAME_CAPTURER_H
#define FRAME_CAPTURER_H

#include <string>
#include <functional>
#include <atomic>
#include <pthread.h>

#include "rkmedia_api.h"
#include "config.h"
#include "thread_safe_pq.h"

struct CaptureConfig
{
    int camera_id = 0;
    std::string device_name = "rkispp_scale0";
    uint32_t width = frame_width;
    uint32_t height = frame_height;
    IMAGE_TYPE_E format = IMAGE_TYPE_NV12;

    // AIQ (ISP) 相关配置
    bool enable_aiq = false;
    std::string iq_files_path = iq_files;
    bool multi_ctx = false;

    // 输出控制
    int frame_count_to_save = -1; // -1表示不限制
    std::string output_path;      // 如果为空，则不保存文件
};

class FrameCapturer
{
public:
    // 定义一个回调函数类型，用于处理获取到的视频帧
    // 参数是获取到的 MEDIA_BUFFER
    using FrameCallback = std::function<void(MEDIA_BUFFER)>;

    /**
     * @brief 构造函数
     * @param config 捕获配置
     * @param callback 处理视频帧的回调函数
     */
    FrameCapturer(const CaptureConfig &config, FrameCallback callback);

    /**
     * @brief 析构函数，会自动调用Stop()和DeInit()来释放资源
     */
    ~FrameCapturer();

    /**
     * @brief 初始化系统和硬件。必须在Start()之前调用。
     * @return bool true表示成功, false表示失败
     */
    bool Init();

    /**
     * @brief 启动捕获线程和视频流
     * @return bool true表示成功, false表示失败
     */
    bool Start();

    /**
     * @brief 停止捕获线程和视频流
     */
    void Stop();

    /**
     * @brief 检查捕获器是否正在运行
     */
    bool IsRunning() const;

    // 禁止拷贝和赋值，因为这个类管理着唯一的硬件资源和线程
    FrameCapturer(const FrameCapturer &) = delete;
    FrameCapturer &operator=(const FrameCapturer &) = delete;

private:
    /**
     * @brief 线程入口函数 (静态)
     */
    static void *WorkerThreadEntry(void *arg);

    /**
     * @brief 实际的工作线程循环
     */
    void WorkerThreadLoop();

    /**
     * @brief 反初始化，释放Init()中申请的资源
     */
    void DeInit();

private:
    CaptureConfig m_config;
    FrameCallback m_frameCallback;
    pthread_t m_threadId;
    std::atomic<bool> m_isRunning;
};

#endif