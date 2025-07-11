#include "rknn_yolo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <ctime>
#include <iomanip>

static std::string getCurrentTimeString()
{
    std::time_t now = std::time(nullptr);
    std::tm *tm_now = std::localtime(&now);

    std::ostringstream oss;
    oss << std::put_time(tm_now, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

static void writeRGBToFile(const unsigned char *data, int width, int height)
{
    std::string filename = "../jpegs/" + getCurrentTimeString() + ".rgb";

    std::ofstream outfile(filename, std::ios::binary);
    if (!outfile)
    {
        std::cerr << "无法创建文件: " << filename << std::endl;
        return;
    }

    outfile.write(reinterpret_cast<const char *>(data), width * height * 3);
    outfile.close();

    std::cout << "已写入文件: " << filename << std::endl;
}

static void draw_on_rgb888(unsigned char *rgb_data, int width, int height,
                           int x1, int y1, int x2, int y2, const std::string &label)
{
    // Step 1: RGB → BGR
    cv::Mat img_rgb(height, width, CV_8UC3, rgb_data);
    cv::Mat img_bgr;
    cv::cvtColor(img_rgb, img_bgr, cv::COLOR_RGB2BGR);

    // Step 2: draw
    cv::rectangle(img_bgr, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(0, 255, 0), 1);
    cv::putText(img_bgr, label, cv::Point(x1, y1 - 12),
                cv::FONT_HERSHEY_SIMPLEX, 1.5, cv::Scalar(255, 255, 255), 1);

    // Step 3: BGR → RGB
    cv::cvtColor(img_bgr, img_rgb, cv::COLOR_BGR2RGB);

    // Step 4: 覆盖原始数据
    memcpy(rgb_data, img_rgb.data, width * height * 3);
}

static void printRKNNTensor(rknn_tensor_attr *attr)
{
    printf("index=%d name=%s n_dims=%d dims=[%d %d %d %d] n_elems=%d size=%d "
           "fmt=%d type=%d qnt_type=%d fl=%d zp=%d scale=%f\n",
           attr->index, attr->name, attr->n_dims, attr->dims[3], attr->dims[2],
           attr->dims[1], attr->dims[0], attr->n_elems, attr->size, 0, attr->type,
           attr->qnt_type, attr->fl, attr->zp, attr->scale);
}

static unsigned char *load_data(FILE *fp, size_t ofst, size_t sz)
{
    unsigned char *data;
    int ret;

    data = NULL;

    if (NULL == fp)
    {
        return NULL;
    }

    ret = fseek(fp, ofst, SEEK_SET);
    if (ret != 0)
    {
        log_error("blob seek failure.\n");
        return NULL;
    }

    data = (unsigned char *)malloc(sz);
    if (data == NULL)
    {
        log_error("buffer malloc failure.\n");
        return NULL;
    }
    ret = fread(data, 1, sz, fp);
    return data;
}

// 辅助函数：加载模型文件
static unsigned char *load_model_from_file(const char *filename, int *model_size)
{
    FILE *fp;
    unsigned char *data;

    fp = fopen(filename, "rb");
    if (NULL == fp)
    {
        log_error("Open file failed");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);

    data = load_data(fp, 0, size);

    fclose(fp);

    *model_size = size;
    return data;
}

RknnDetector::RknnDetector(const std::string &model_path, float conf_threshold, float nms_threshold)
    : m_ctx(0), m_drm_fd(-1), m_model_width(0), m_model_height(0), m_model_channel(0),
      m_conf_threshold(conf_threshold), m_nms_threshold(nms_threshold),
      m_is_initialized(false), m_resize_buf(nullptr)
{

    memset(&m_rga_ctx, 0, sizeof(rga_context));
    memset(&m_drm_ctx, 0, sizeof(drm_context));
    memset(&m_io_num, 0, sizeof(rknn_input_output_num));

    if (init(model_path) != 0)
    {
        log_error("Failed to initialize RKNN.");
        // 在析构函数中进行清理
    }
}

RknnDetector::~RknnDetector()
{
    if (m_resize_buf)
    {
        free(m_resize_buf);
    }
    if (m_ctx > 0)
    {
        rknn_destroy(m_ctx);
    }
    if (m_drm_fd >= 0)
    {
        drm_deinit(&m_drm_ctx, m_drm_fd);
    }
    RGA_deinit(&m_rga_ctx);
}

int RknnDetector::init(const std::string &model_path)
{
    int ret;
    int model_data_size = 0;
    unsigned char *model_data = load_model_from_file(model_path.c_str(), &model_data_size);
    if (!model_data)
    {
        return -1;
    }

    printf("Model data pointer: %p\n", model_data);
    if (model_data == NULL)
    {
        log_error("Failed to load model.");
        return -1;
    }

    ret = rknn_init(&m_ctx, model_data, model_data_size, 0);
    // free(model_data); // 模型数据已加载到NPU，可以释放
    if (ret < 0)
    {
        log_error("rknn_init error");
        return -1;
    }

    // 获取模型输入输出数量
    ret = rknn_query(m_ctx, RKNN_QUERY_IN_OUT_NUM, &m_io_num, sizeof(m_io_num));
    if (ret < 0)
    {
        log_error("rknn_query(RKNN_QUERY_IN_OUT_NUM) error");
        return -1;
    }

    // 获取输入张量属性
    m_input_attrs.resize(m_io_num.n_input);
    memset(m_input_attrs.data(), 0, m_io_num.n_input * sizeof(rknn_tensor_attr));
    for (uint32_t i = 0; i < m_io_num.n_input; i++)
    {
        m_input_attrs[i].index = i;
        ret = rknn_query(m_ctx, RKNN_QUERY_INPUT_ATTR, &(m_input_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret < 0)
        {
            log_error("rknn_query(RKNN_QUERY_INPUT_ATTR) error");
            return -1;
        }
        printRKNNTensor(&(m_input_attrs[i]));
    }

    // 获取输出张量属性
    m_output_attrs.resize(m_io_num.n_output);
    memset(m_output_attrs.data(), 0, m_io_num.n_output * sizeof(rknn_tensor_attr));
    for (uint32_t i = 0; i < m_io_num.n_output; i++)
    {
        m_output_attrs[i].index = i;
        ret = rknn_query(m_ctx, RKNN_QUERY_OUTPUT_ATTR, &(m_output_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret < 0)
        {
            log_error("rknn_query(RKNN_QUERY_OUTPUT_ATTR) error");
            return -1;
        }
        printRKNNTensor(&(m_output_attrs[i]));
    }

    m_model_channel = 3;
    // 设置模型输入尺寸
    if (m_input_attrs[0].fmt == RKNN_TENSOR_NCHW)
    {
        m_model_width = m_input_attrs[0].dims[0];
        m_model_height = m_input_attrs[0].dims[1];
    }
    else
    { // NHWC
        m_model_width = m_input_attrs[0].dims[1];
        m_model_height = m_input_attrs[0].dims[2];
    }
    printf("Model input size: %dx%d, channel: %d\n", m_model_width, m_model_height, m_model_channel);

    // 初始化RGA和DRM
    m_drm_fd = drm_init(&m_drm_ctx);
    if (m_drm_fd < 0)
    {
        log_error("drm_init failed.");
        return -1;
    }

    RGA_init(&m_rga_ctx);

    // 为缩放后的图像分配内存
    m_resize_buf = malloc(m_model_width * m_model_height * m_model_channel);
    if (!m_resize_buf)
    {
        log_error("Failed to malloc resize buffer.");
        return -1;
    }

    // 缓存输出的量化参数
    for (uint32_t i = 0; i < m_io_num.n_output; ++i)
    {
        m_out_scales.push_back(m_output_attrs[i].scale);
        m_out_zps.push_back(m_output_attrs[i].zp);
    }

    m_is_initialized = true;
    return 0;
}

int RknnDetector::inferAndDraw(unsigned char *img_data, int img_width, int img_height)
{
    if (!m_is_initialized)
    {
        log_error("RknnDetector is not initialized.");
        return -1;
    }

    // 假设输入通道为3 (RGB)
    int img_channel = 3;

    // 1. 使用DRM/RGA进行图像缩放
    void *drm_buf = NULL;
    int buf_fd = -1;
    unsigned int handle;
    size_t actual_size = 0;

    // 分配DRM缓冲区并拷贝原始图像数据
    drm_buf = drm_buf_alloc_fix(&m_drm_ctx, m_drm_fd, img_width, img_height, img_channel * 8, &buf_fd, &handle, &actual_size);
    if (!drm_buf)
    {
        log_error("drm_buf_alloc_fix failed.");
        return -1;
    }
    memcpy(drm_buf, img_data, img_width * img_height * img_channel);

    // 使用RGA将DRM中的图像缩放到模型输入尺寸
    img_resize_slow(&m_rga_ctx, drm_buf, img_width, img_height, m_resize_buf, m_model_width, m_model_height);

    // 2. 设置模型输入
    rknn_input inputs[1];
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;
    inputs[0].size = m_model_width * m_model_height * m_model_channel;
    inputs[0].fmt = RKNN_TENSOR_NHWC; // RGA输出为NHWC
    inputs[0].buf = m_resize_buf;

    int ret = rknn_inputs_set(m_ctx, m_io_num.n_input, inputs);
    if (ret < 0)
    {
        log_error("rknn_inputs_set error");
        drm_buf_destroy_fix(&m_drm_ctx, m_drm_fd, buf_fd, handle, drm_buf, actual_size);
        return -1;
    }

    // 3. 执行推理
    ret = rknn_run(m_ctx, NULL);
    if (ret < 0)
    {
        log_error("rknn_run error");
        drm_buf_destroy_fix(&m_drm_ctx, m_drm_fd, buf_fd, handle, drm_buf, actual_size);
        return -1;
    }

    // 4. 获取推理输出
    rknn_output outputs[m_io_num.n_output];
    memset(outputs, 0, sizeof(outputs));
    for (uint32_t i = 0; i < m_io_num.n_output; i++)
    {
        outputs[i].want_float = 0; // 获取量化后的INT8/UINT8数据
    }
    ret = rknn_outputs_get(m_ctx, m_io_num.n_output, outputs, NULL);
    if (ret < 0)
    {
        log_error("rknn_outputs_get error");
        drm_buf_destroy_fix(&m_drm_ctx, m_drm_fd, buf_fd, handle, drm_buf, actual_size);
        return -1;
    }

    // 5. 后处理
    float scale_w = (float)m_model_width / img_width;
    float scale_h = (float)m_model_height / img_height;

    detect_result_group_t detect_result_group;
    post_process(
        (uint8_t *)outputs[0].buf, (uint8_t *)outputs[1].buf, (uint8_t *)outputs[2].buf,
        m_model_height, m_model_width,
        m_conf_threshold, m_nms_threshold, m_vis_threshold,
        scale_w, scale_h,
        m_out_zps, m_out_scales,
        &detect_result_group);

    // 6. 绘制结果到原始图像缓冲区
    for (int i = 0; i < detect_result_group.count; i++)
    {
        detect_result_t *det_result = &(detect_result_group.results[i]);

        printf("\n-----------------------------------检测到%s @ (%d %d %d %d) 置信度为:%f-----------------------------------------\n",
               det_result->name,
               det_result->box.left, det_result->box.top, det_result->box.right, det_result->box.bottom,
               det_result->prop);
        int x1 = det_result->box.left;
        int y1 = det_result->box.top;
        int x2 = det_result->box.right;
        int y2 = det_result->box.bottom;

        draw_on_rgb888(img_data, img_width, img_height, x1, y1, x2, y2, det_result->name);

        // todo 针对检测结果做其他处理  此处仅作写入文件处理
        writeRGBToFile(img_data, 1920, 1080);

    }

    // 释放本次推理的资源
    rknn_outputs_release(m_ctx, m_io_num.n_output, outputs);
    drm_buf_destroy_fix(&m_drm_ctx, m_drm_fd, buf_fd, handle, drm_buf, actual_size);

    return 0;
}