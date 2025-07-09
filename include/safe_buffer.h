#ifndef SAFE_BUFFER_H
#define SAFE_BUFFER_H

#include <stddef.h>

typedef struct ByteBuffer ByteBuffer;

// 创建字节缓冲区（返回NULL表示失败）
ByteBuffer *byte_buffer_create(size_t capacity);

// 释放缓冲区资源
void byte_buffer_destroy(ByteBuffer *buf);

// 写入数据（阻塞直到写入成功）
size_t byte_buffer_write(ByteBuffer *buf, const unsigned char *data, size_t len);

// 读取数据（阻塞直到读取成功）
size_t byte_buffer_read(ByteBuffer *buf, unsigned char *data, size_t len);

#endif
