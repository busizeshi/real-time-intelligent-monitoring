// rknn_yolo.h
#ifndef RKNN_YOLO_H
#define RKNN_YOLO_H

#include <string>
#include <vector>

// 包含所有必要的Rockchip头文件
#include "rknn_api.h"
#include "postprocess.h"
#include "drm_func.h"
#include "rga_func.h"
#include "logger.h"

#include "opencv2/opencv.hpp"

class RknnDetector {
public:
    /**
     * @brief 构造函数
     * @param model_path rknn模型文件的路径
     * @param conf_threshold 置信度阈值
     * @param nms_threshold NMS（非极大值抑制）阈值
     */
    RknnDetector(const std::string& model_path, float conf_threshold = 0.3f, float nms_threshold = 0.5f);

    /**
     * @brief 析构函数，自动释放所有资源
     */
    ~RknnDetector();

    /**
     * @brief 对输入的图像数据进行推理，并将检测框直接绘制在图像上
     * @param img_data 指向RGB888格式图像数据的指针。函数将直接修改此缓冲区。
     * @param img_width 图像宽度
     * @param img_height 图像高度
     * @return 0 表示成功, 其他值表示失败
     */
    int inferAndDraw(unsigned char* img_data, int img_width, int img_height);

    /**
     * @brief 检查初始化是否成功
     * @return true 如果初始化成功, false 如果失败
     */
    bool isInitialized() const { return m_is_initialized; }

private:
    /**
     * @brief 内部初始化函数，由构造函数调用
     * @return 0 表示成功, 其他值表示失败
     */
    int init(const std::string& model_path);

    /**
     * @brief 在给定的RGB888缓冲区上绘制一个矩形
     */
    void draw_rectangle_on_buffer(unsigned char* buffer, int width, int height, const detect_result_t& result, const unsigned char color[3]);

    // RKNN 和硬件上下文
    rknn_context m_ctx;
    rga_context  m_rga_ctx;
    drm_context  m_drm_ctx;
    int          m_drm_fd;

    // 模型信息
    int m_model_width;
    int m_model_height;
    int m_model_channel;
    rknn_input_output_num m_io_num;
    std::vector<rknn_tensor_attr> m_input_attrs;
    std::vector<rknn_tensor_attr> m_output_attrs;
    std::vector<float> m_out_scales;
    std::vector<uint8_t> m_out_zps;

    // 运行参数
    float m_conf_threshold;
    float m_nms_threshold;
    const float m_vis_threshold = 0.1; // 可视化阈值, 可根据需要调整

    // 状态和缓冲区
    bool m_is_initialized;
    void* m_resize_buf;
};
#endif // RKNN_YOLO_H