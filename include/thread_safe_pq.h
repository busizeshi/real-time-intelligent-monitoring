#ifndef THREAD_SAFE_PQ_H
#define THREAD_SAFE_PQ_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

// 队列中存储的元素载荷 (Payload)
typedef struct {
    unsigned char* data; // 数据指针
    int size;            // 数据大小
} Payload;

// 定义回调函数指针类型
// 当一个元素被处理时，此函数会被调用
// user_context 是一个可选的指针，可以在创建队列时传入，用于向回调函数传递状态
typedef void (*pq_callback_t)(Payload payload, void* user_context);

// 队列元素的完整结构 (内部使用)
typedef struct {
    long long timestamp; // 优先级
    Payload payload;
} QueueElement;

// 线程安全的优先级队列结构体
typedef struct {
    QueueElement* elements;     // 存储元素的数组（堆）
    size_t size;                // 当前元素数量
    size_t capacity;            // 数组的容量

    pthread_mutex_t mutex;      // 互斥锁，保护队列
    pthread_cond_t cond;        // 条件变量，用于等待/通知

    pthread_t worker_thread;    // 内部工作线程
    pq_callback_t callback;     // 用户提供的回调函数
    void* user_context;         // 用户上下文，会传递给回调函数
    bool is_active;             // 标志位，用于优雅地关闭队列
} PriorityQueue;

/**
 * @brief 创建并初始化一个带回调的线程安全优先级队列。
 *        此函数会启动一个内部工作线程来处理队列中的元素。
 *
 * @param initial_capacity 队列的初始容量
 * @param callback         处理元素的函数。此函数会在工作线程中被调用。
 * @param user_context     一个用户定义的指针，会作为参数传递给回调函数。可以为 NULL。
 * @return PriorityQueue*   成功则返回队列指针，失败则返回 NULL。
 */
PriorityQueue* pq_create(size_t initial_capacity, pq_callback_t callback, void* user_context);

/**
 * @brief 向队列中添加一个元素 (线程安全)。
 *        元素入队后，内部工作线程会在适当的时候（根据优先级）处理它。
 *
 * @param pq 优先级队列指针
 * @param timestamp 元素的时间戳（优先级）
 * @param data 指向数据的指针。队列会创建此数据的副本。
 * @param size 数据的大小
 * @return int 0 表示成功, -1 表示队列已关闭或内存分配失败。
 */
int pq_enqueue(PriorityQueue* pq, long long timestamp, const unsigned char* data, int size);

/**
 * @brief 销毁优先级队列并释放所有资源。
 *        此函数会首先通知工作线程停止，并等待其完成后再清理资源。
 *
 * @param pq 优先级队列指针
 */
void pq_destroy(PriorityQueue* pq);

#endif // THREAD_SAFE_PQ_H