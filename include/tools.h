#ifndef TOOLS_H
#define TOOLS_H
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

// 包含标准库，如果函数声明中使用了其类型或常量
// 例如，如果函数签名中需要size_t，可能需要<stddef.h>
// 这里直接使用unsigned char和int，无需特殊包含

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief 将 RGB888 数据转换为 NV12 数据。
     *
     * @param rgb_in 输入的 RGB888 数据指针。
     * @param nv12_out 输出的 NV12 数据指针。请确保已分配足够大的内存：width * height * 3 / 2 字节。
     * @param width 图像宽度 (必须是偶数，否则UV采样可能不正确)。
     * @param height 图像高度 (必须是偶数，否则UV采样可能不正确)。
     */
    void rgb888_to_nv12(unsigned char *rgb_in, unsigned char *nv12_out, int width, int height);

    /**
     * @brief 将 NV12 数据转换为 RGB888 数据。
     *
     * @param nv12_in 输入的 NV12 数据指针。
     * @param rgb_out 输出的 RGB888 数据指针。请确保已分配足够大的内存：width * height * 3 字节。
     * @param width 图像宽度 (必须是偶数)。
     * @param height 图像高度 (必须是偶数)。
     */
    void nv12_to_rgb888(unsigned char *nv12_in, unsigned char *rgb_out, int width, int height);

    /**
     * @brief 获取当前时间戳。  
     *
     * @return 当前时间戳。
     */
    long long get_time_ms();

    /**
     * @brief 获取北京时间。
     *
     * @return 
     */
    void get_time_str(long long ms_timestamp, char *time_str, int buf_size);

#ifdef __cplusplus
}
#endif

#endif