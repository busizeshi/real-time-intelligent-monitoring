#include "safe_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct ByteBuffer {
    unsigned char *buffer;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t size;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
};

ByteBuffer *byte_buffer_create(size_t capacity) {
    ByteBuffer *buf = malloc(sizeof(ByteBuffer));
    if (!buf) return NULL;

    buf->buffer = malloc(capacity);
    if (!buf->buffer) {
        free(buf);
        return NULL;
    }

    buf->capacity = capacity;
    buf->head = buf->tail = buf->size = 0;
    pthread_mutex_init(&buf->mutex, NULL);
    pthread_cond_init(&buf->not_full, NULL);
    pthread_cond_init(&buf->not_empty, NULL);
    return buf;
}

void byte_buffer_destroy(ByteBuffer *buf) {
    if (!buf) return;
    pthread_mutex_destroy(&buf->mutex);
    pthread_cond_destroy(&buf->not_full);
    pthread_cond_destroy(&buf->not_empty);
    free(buf->buffer);
    free(buf);
}

size_t byte_buffer_write(ByteBuffer *buf, const unsigned char *data, size_t len) {
    size_t written = 0;
    pthread_mutex_lock(&buf->mutex);

    while (written < len) {
        while (buf->size == buf->capacity)
            pthread_cond_wait(&buf->not_full, &buf->mutex);

        buf->buffer[buf->tail] = data[written];
        buf->tail = (buf->tail + 1) % buf->capacity;
        buf->size++;
        written++;

        pthread_cond_signal(&buf->not_empty);
    }

    pthread_mutex_unlock(&buf->mutex);
    return written;
}

size_t byte_buffer_read(ByteBuffer *buf, unsigned char *data, size_t len) {
    size_t read = 0;
    pthread_mutex_lock(&buf->mutex);

    while (read < len) {
        while (buf->size == 0)
            pthread_cond_wait(&buf->not_empty, &buf->mutex);

        data[read] = buf->buffer[buf->head];
        buf->head = (buf->head + 1) % buf->capacity;
        buf->size--;
        read++;

        pthread_cond_signal(&buf->not_full);
    }

    pthread_mutex_unlock(&buf->mutex);
    return read;
}
