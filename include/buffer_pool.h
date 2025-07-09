// thread_safe_buffer.h
#ifndef BUFFER_POLL_H
#define BUFFER_POLL_H

#include <stddef.h>
#include <pthread.h>

typedef struct {
    unsigned char *buffer;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t size;

    pthread_mutex_t mutex;
} TSBuffer;

// 初始化缓冲区
int tsbuffer_init(TSBuffer *buf, size_t capacity);

// 释放缓冲区资源
void tsbuffer_destroy(TSBuffer *buf);

// 写入数据（返回成功写入的字节数）
size_t tsbuffer_write(TSBuffer *buf, const unsigned char *data, size_t len);

// 读取数据（返回成功读取的字节数）
size_t tsbuffer_read(TSBuffer *buf, unsigned char *data, size_t len);

// 获取当前已用大小
size_t tsbuffer_size(TSBuffer *buf);

#endif
