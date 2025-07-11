#include "tools.h" // 引入我们自己定义的头文件
#include <stdio.h>              // 用于 printf, fopen, fclose, fprintf
#include <stdlib.h>             // 用于 malloc, free, abs

// 简单的测试用例
int main()
{
    int width = 640;
    int height = 480;

    // 1. 分配内存
    unsigned char *rgb_in = (unsigned char *)malloc(width * height * 3);
    unsigned char *nv12_buffer = (unsigned char *)malloc(width * height * 3 / 2); // NV12是YUV420，占1.5字节/像素
    unsigned char *rgb_out = (unsigned char *)malloc(width * height * 3);

    if (!rgb_in || !nv12_buffer || !rgb_out)
    {
        fprintf(stderr, "Failed to allocate memory!\n");
        free(rgb_in);
        free(nv12_buffer);
        free(rgb_out);
        return 1;
    }

    // 2. 构造一个简单的测试图像 (渐变色)
    // 从左到右 R 增加，从上到下 G 增加，B 混合
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int idx = (y * width + x) * 3;
            rgb_in[idx] = (unsigned char)(x * 255 / width);                      // R 红色 (左黑右红)
            rgb_in[idx + 1] = (unsigned char)(y * 255 / height);                 // G 绿色 (上黑下绿)
            rgb_in[idx + 2] = (unsigned char)((x + y) * 255 / (width + height)); // B 蓝色 (混合渐变)
        }
    }
    printf("Generated test RGB888 image.\n");

    // 3. RGB888 转 NV12
    rgb888_to_nv12(rgb_in, nv12_buffer, width, height);
    printf("RGB888 to NV12 conversion complete.\n");

    // 4. NV12 转 RGB888
    nv12_to_rgb888(nv12_buffer, rgb_out, width, height);
    printf("NV12 to RGB888 conversion complete.\n");

    // 5. 验证 (简单验证，例如计算差异)
    long long diff_sum = 0;
    for (int i = 0; i < width * height * 3; i++)
    {
        diff_sum += abs(rgb_in[i] - rgb_out[i]);
    }
    double avg_diff = (double)diff_sum / (width * height * 3);
    printf("Average pixel difference between original RGB and converted back RGB: %.2f\n", avg_diff);

    // 保存图像文件以便视觉检查
    // 保存原始RGB (PPM格式)
    FILE *fp_orig = fopen("original.ppm", "wb");
    if (fp_orig)
    {
        fprintf(fp_orig, "P6\n%d %d\n255\n", width, height);
        fwrite(rgb_in, 1, width * height * 3, fp_orig);
        fclose(fp_orig);
        printf("Original RGB image saved to original.ppm\n");
    }

    // 保存转换回来的RGB (PPM格式)
    FILE *fp_conv = fopen("converted.ppm", "wb");
    if (fp_conv)
    {
        fprintf(fp_conv, "P6\n%d %d\n255\n", width, height);
        fwrite(rgb_out, 1, width * height * 3, fp_conv);
        fclose(fp_conv);
        printf("Converted back RGB image saved to converted.ppm\n");
    }

    // 保存NV12的Y平面 (PGM灰度图)
    FILE *fp_y = fopen("nv12_y_plane.pgm", "wb");
    if (fp_y)
    {
        fprintf(fp_y, "P5\n%d %d\n255\n", width, height);
        fwrite(nv12_buffer, 1, width * height, fp_y);
        fclose(fp_y);
        printf("NV12 Y-plane saved to nv12_y_plane.pgm\n");
    }

    // 6. 释放内存
    free(rgb_in);
    free(nv12_buffer);
    free(rgb_out);
    printf("Memory freed.\n");

    return 0;
}