#ifndef TOOLS_H
#define TOOLS_H
#include <time.h>

/**
 * 获取当前时间戳
 */
time_t get_time()
{
    return time(NULL);
}

#endif