// safe_queue.h

#ifndef SAFE_QUEUE_H
#define SAFE_QUEUE_H

#include <pthread.h>

// 队列节点
typedef struct QueueNode
{
    void *data; // 指向数据的通用指针
    struct QueueNode *next;
} QueueNode;

// 线程安全队列结构
typedef struct
{
    QueueNode *head; // 队列头指针
    QueueNode *tail; // 队列尾指针

    int size;     // 当前队列中的元素数量
    int capacity; // 队列的最大容量

    pthread_mutex_t mutex;         // 互斥锁，保护队列结构
    pthread_cond_t cond_non_empty; // 条件变量，当队列非空时触发
    pthread_cond_t cond_non_full;  // 条件变量，当队列未满时触发

    int is_valid; // 标志队列是否仍然有效（用于安全销毁）
} SafeQueue;

/**
 * @brief 创建一个线程安全的队列
 *
 * @param capacity 队列的最大容量
 * @return 成功则返回队列指针，失败则返回 NULL
 */
SafeQueue *safe_queue_create(int capacity);

/**
 * @brief 销毁线程安全的队列
 *
 * @param queue 要销毁的队列
 */
void safe_queue_destroy(SafeQueue *queue);

/**
 * @brief 向队列中添加一个元素（入队）
 *        如果队列已满，此操作会阻塞直到有空间为止。
 *
 * @param queue 队列指针
 * @param data 要添加的数据指针
 */
void safe_queue_enqueue(SafeQueue *queue, void *data);

/**
 * @brief 从队列中取出一个元素（出队）
 *        如果队列为空，此操作会阻塞直到有元素为止。
 *
 * @param queue 队列指针
 * @return void* 取出的数据指针
 */
void *safe_queue_dequeue(SafeQueue *queue);

#endif // SAFE_QUEUE_H