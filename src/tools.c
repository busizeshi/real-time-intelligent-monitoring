#include "tools.h"
#include <stdio.h>  // 用于 fprintf
#include <stdlib.h> // 用于 abs (虽然我们手动 clamp，但为了通用性保留)

// 辅助函数：将值限制在0-255之间
// static 关键字使其成为仅在本文件可见的私有函数
static unsigned char clamp(int value)
{
    if (value < 0)
    {
        return 0;
    }
    if (value > 255)
    {
        return 255;
    }
    return (unsigned char)value;
}

void rgb888_to_nv12(unsigned char *rgb_in, unsigned char *nv12_out, int width, int height)
{
    if (width % 2 != 0 || height % 2 != 0)
    {
        fprintf(stderr, "Error: Width and height must be even for NV12 conversion.\n");
        return;
    }

    unsigned char *y_plane = nv12_out;
    unsigned char *uv_plane = nv12_out + width * height;

    int uv_width = width / 2;
    // int uv_height = height / 2; // 不直接使用，但逻辑上存在

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int rgb_idx = (y * width + x) * 3;
            unsigned char R = rgb_in[rgb_idx];
            unsigned char G = rgb_in[rgb_idx + 1];
            unsigned char B = rgb_in[rgb_idx + 2];

            // 计算 Y 分量 (BT.601, 全范围 0-255)
            // Y = 0.299*R + 0.587*G + 0.114*B
            // 整数近似: Y = (77*R + 150*G + 29*B) >> 8
            int Y = ((R * 77 + G * 150 + B * 29) >> 8);
            y_plane[y * width + x] = clamp(Y);

            // 计算 UV 分量 (每 2x2 像素块计算一次 U 和 V)
            // NV12格式，UV是2x2下采样的，所以只需要在偶数行偶数列计算一次
            if (y % 2 == 0 && x % 2 == 0)
            {
                // 为了更准确的下采样，取2x2区域的平均RGB来计算UV
                int R_sum = 0, G_sum = 0, B_sum = 0;
                int count = 0;

                for (int dy = 0; dy < 2; dy++)
                {
                    for (int dx = 0; dx < 2; dx++)
                    {
                        int current_x = x + dx;
                        int current_y = y + dy;
                        if (current_x < width && current_y < height)
                        { // 确保不越界
                            int current_rgb_idx = (current_y * width + current_x) * 3;
                            R_sum += rgb_in[current_rgb_idx];
                            G_sum += rgb_in[current_rgb_idx + 1];
                            B_sum += rgb_in[current_rgb_idx + 2];
                            count++;
                        }
                    }
                }

                // 计算平均值
                int R_avg = R_sum / count;
                int G_avg = G_sum / count;
                int B_avg = B_sum / count;

                // 计算 U 和 V 分量 (BT.601, 全范围 0-255)
                // U = -0.14713*R - 0.28886*G + 0.436*B + 128
                // 整数近似: U = (-43*R_avg - 84*G_avg + 127*B_avg) >> 8 + 128
                int U = ((-R_avg * 43 - G_avg * 84 + B_avg * 127) >> 8) + 128;

                // V = 0.615*R - 0.51499*G - 0.10001*B + 128
                // 整数近似: V = (127*R_avg - 106*G_avg - 21*B_avg) >> 8 + 128
                int V = ((R_avg * 127 - G_avg * 106 - B_avg * 21) >> 8) + 128;

                // 存储 U 和 V
                int uv_idx = ((y / 2) * uv_width + (x / 2)) * 2;
                uv_plane[uv_idx] = clamp(U);     // U
                uv_plane[uv_idx + 1] = clamp(V); // V
            }
        }
    }
}

void nv12_to_rgb888(unsigned char *nv12_in, unsigned char *rgb_out, int width, int height)
{
    if (width % 2 != 0 || height % 2 != 0)
    {
        fprintf(stderr, "Error: Width and height must be even for NV12 conversion.\n");
        return;
    }

    const unsigned char *y_plane = nv12_in;
    const unsigned char *uv_plane = nv12_in + width * height;

    int uv_width = width / 2;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            unsigned char Y = y_plane[y * width + x];

            // U 和 V 的索引是基于 2x2 块的，所以对所有Y像素，UV值是共享的
            int uv_idx = ((y / 2) * uv_width + (x / 2)) * 2;
            unsigned char U = uv_plane[uv_idx];
            unsigned char V = uv_plane[uv_idx + 1];

            // 将 U 和 V 转换为相对值 (减去128)
            int U_sub = U - 128;
            int V_sub = V - 128;

            // 计算 R, G, B 分量 (BT.601, 全范围 0-255)
            // R = Y + 1.402 * V_sub
            // 整数近似: R = Y + (V_sub * 359) >> 8
            int R = Y + ((V_sub * 359) >> 8);

            // G = Y - 0.34414 * U_sub - 0.71414 * V_sub
            // 整数近似: G = Y - ((U_sub * 88) >> 8) - ((V_sub * 183) >> 8)
            int G = Y - ((U_sub * 88) >> 8) - ((V_sub * 183) >> 8);

            // B = Y + 1.772 * U_sub
            // 整数近似: B = Y + (U_sub * 454) >> 8
            int B = Y + ((U_sub * 454) >> 8);

            // 存储 RGB888 数据
            int rgb_idx = (y * width + x) * 3;
            rgb_out[rgb_idx] = clamp(R);
            rgb_out[rgb_idx + 1] = clamp(G);
            rgb_out[rgb_idx + 2] = clamp(B);
        }
    }
}

long long get_time_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000LL + tv.tv_usec / 1000;
}

void get_time_str(long long ms_timestamp, char *time_str, int buf_size)
{
    time_t seconds = ms_timestamp / 1000;
    suseconds_t msec = ms_timestamp % 1000;

    struct tm *tm_info = localtime(&seconds);
    strftime(time_str, buf_size, "%Y-%m-%d %H:%M:%S", tm_info);

    // 拼接毫秒部分
    char temp[8];
    snprintf(temp, sizeof(temp), ".%03ld", (long)msec);
    strncat(time_str, temp, buf_size - strlen(time_str) - 1);
}