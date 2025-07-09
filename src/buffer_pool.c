// thread_safe_buffer.c
#include "buffer_pool.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

int tsbuffer_init(TSBuffer *buf, size_t capacity) {
    buf->buffer = (unsigned char *)malloc(capacity);
    if (!buf->buffer) return -1;

    buf->capacity = capacity;
    buf->head = 0;
    buf->tail = 0;
    buf->size = 0;
    pthread_mutex_init(&buf->mutex, NULL);
    return 0;
}

void tsbuffer_destroy(TSBuffer *buf) {
    pthread_mutex_destroy(&buf->mutex);
    free(buf->buffer);
}

size_t tsbuffer_write(TSBuffer *buf, const unsigned char *data, size_t len) {
    pthread_mutex_lock(&buf->mutex);

    size_t free_space = buf->capacity - buf->size;
    if (len > free_space) len = free_space;

    size_t first_part = buf->capacity - buf->tail;
    if (first_part > len) first_part = len;

    memcpy(buf->buffer + buf->tail, data, first_part);
    memcpy(buf->buffer, data + first_part, len - first_part);

    buf->tail = (buf->tail + len) % buf->capacity;
    buf->size += len;

    pthread_mutex_unlock(&buf->mutex);
    return len;
}

size_t tsbuffer_read(TSBuffer *buf, unsigned char *data, size_t len) {
    pthread_mutex_lock(&buf->mutex);

    if (len > buf->size) len = buf->size;

    size_t first_part = buf->capacity - buf->head;
    if (first_part > len) first_part = len;

    memcpy(data, buf->buffer + buf->head, first_part);
    memcpy(data + first_part, buf->buffer, len - first_part);

    buf->head = (buf->head + len) % buf->capacity;
    buf->size -= len;

    pthread_mutex_unlock(&buf->mutex);
    return len;
}

size_t tsbuffer_size(TSBuffer *buf) {
    pthread_mutex_lock(&buf->mutex);
    size_t sz = buf->size;
    pthread_mutex_unlock(&buf->mutex);
    return sz;
}
