/**
 * =====================================================================================
 *
 *       Filename:  complete_pq.c
 *
 *    Description:  一个完整的、自包含的线程安全优先级队列实现。
 *                  队列中的元素按时间戳（值越小，优先级越高）排序。
 *                  入队操作后，一个内部工作线程会自动取出元素并调用用户提供的回调函数。
 *
 *        Version:  1.0
 *        Created:  [Current Date]
 *       Compiler:  gcc
 *
 *         Author:  AI Assistant
 *
 *   To Compile & Run:
 *   gcc -o program complete_pq.c -pthread
 *   ./program
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <time.h>

// --- Section 1: Data Structures and Public API Declarations (equivalent to .h file) ---

// 队列中存储的元素载荷 (Payload)
typedef struct
{
    unsigned char *data; // 数据指针
    int size;            // 数据大小
} Payload;

// 定义回调函数指针类型
// 当一个元素被处理时，此函数会被调用
// user_context 是一个可选的指针，可以在创建队列时传入，用于向回调函数传递状态
typedef void (*pq_callback_t)(Payload payload, void *user_context);

// 队列元素的完整结构 (内部使用)
typedef struct
{
    long long timestamp; // 优先级
    Payload payload;
} QueueElement;

// 线程安全的优先级队列结构体
typedef struct
{
    QueueElement *elements; // 存储元素的数组（堆）
    size_t size;            // 当前元素数量
    size_t capacity;        // 数组的容量

    pthread_mutex_t mutex; // 互斥锁，保护队列
    pthread_cond_t cond;   // 条件变量，用于等待/通知

    pthread_t worker_thread; // 内部工作线程
    pq_callback_t callback;  // 用户提供的回调函数
    void *user_context;      // 用户上下文，会传递给回调函数
    bool is_active;          // 标志位，用于优雅地关闭队列
} PriorityQueue;

/**
 * @brief 创建并初始化一个带回调的线程安全优先级队列。
 *        此函数会启动一个内部工作线程来处理队列中的元素。
 */
PriorityQueue *pq_create(size_t initial_capacity, pq_callback_t callback, void *user_context);

/**
 * @brief 向队列中添加一个元素 (线程安全)。
 *        元素入队后，内部工作线程会在适当的时候（根据优先级）处理它。
 */
int pq_enqueue(PriorityQueue *pq, long long timestamp, const unsigned char *data, int size);

/**
 * @brief 销毁优先级队列并释放所有资源。
 *        此函数会首先通知工作线程停止，并等待其完成后再清理资源。
 */
void pq_destroy(PriorityQueue *pq);

// --- Section 2: Priority Queue Implementation (equivalent to .c file) ---

// --- 内部辅助函数 (堆操作) ---
static void swap_elements(QueueElement *a, QueueElement *b)
{
    QueueElement temp = *a;
    *a = *b;
    *b = temp;
}

static void heapify_up(PriorityQueue *pq, size_t index)
{
    if (index == 0)
        return;
    size_t parent_index = (index - 1) / 2;
    if (pq->elements[index].timestamp < pq->elements[parent_index].timestamp)
    {
        swap_elements(&pq->elements[index], &pq->elements[parent_index]);
        heapify_up(pq, parent_index);
    }
}

static void heapify_down(PriorityQueue *pq, size_t index)
{
    size_t left_child = 2 * index + 1;
    size_t right_child = 2 * index + 2;
    size_t smallest = index;

    if (left_child < pq->size && pq->elements[left_child].timestamp < pq->elements[smallest].timestamp)
    {
        smallest = left_child;
    }
    if (right_child < pq->size && pq->elements[right_child].timestamp < pq->elements[smallest].timestamp)
    {
        smallest = right_child;
    }

    if (smallest != index)
    {
        swap_elements(&pq->elements[index], &pq->elements[smallest]);
        heapify_down(pq, smallest);
    }
}

// --- 内部工作线程和私有Dequeue函数 ---

static int _internal_dequeue(PriorityQueue *pq, QueueElement *element_out)
{
    pthread_mutex_lock(&pq->mutex);
    while (pq->size == 0 && pq->is_active)
    {
        pthread_cond_wait(&pq->cond, &pq->mutex);
    }
    if (!pq->is_active && pq->size == 0)
    {
        pthread_mutex_unlock(&pq->mutex);
        return -1;
    }
    *element_out = pq->elements[0];
    pq->size--;
    if (pq->size > 0)
    {
        pq->elements[0] = pq->elements[pq->size];
        heapify_down(pq, 0);
    }
    pthread_mutex_unlock(&pq->mutex);
    return 0;
}

static void *worker_thread_func(void *arg)
{
    PriorityQueue *pq = (PriorityQueue *)arg;
    QueueElement item;

    while (_internal_dequeue(pq, &item) == 0)
    {
        if (pq->callback)
        {
            pq->callback(item.payload, pq->user_context);
        }
        free(item.payload.data);
    }
    return NULL;
}

// --- 公共 API 实现 ---

PriorityQueue *pq_create(size_t initial_capacity, pq_callback_t callback, void *user_context)
{
    if (initial_capacity == 0)
        initial_capacity = 16;
    if (callback == NULL)
    {
        fprintf(stderr, "Error: Callback function cannot be NULL.\n");
        return NULL;
    }

    PriorityQueue *pq = malloc(sizeof(PriorityQueue));
    if (!pq)
        return NULL;

    pq->elements = malloc(sizeof(QueueElement) * initial_capacity);
    if (!pq->elements)
    {
        free(pq);
        return NULL;
    }

    pq->size = 0;
    pq->capacity = initial_capacity;
    pq->is_active = true;
    pq->callback = callback;
    pq->user_context = user_context;

    pthread_mutex_init(&pq->mutex, NULL);
    pthread_cond_init(&pq->cond, NULL);

    if (pthread_create(&pq->worker_thread, NULL, worker_thread_func, pq) != 0)
    {
        fprintf(stderr, "Error: Failed to create worker thread.\n");
        free(pq->elements);
        free(pq);
        return NULL;
    }
    return pq;
}

int pq_enqueue(PriorityQueue *pq, long long timestamp, const unsigned char *data, int size)
{
    pthread_mutex_lock(&pq->mutex);
    if (!pq->is_active)
    {
        pthread_mutex_unlock(&pq->mutex);
        return -1;
    }

    if (pq->size >= pq->capacity)
    {
        size_t new_capacity = pq->capacity * 2;
        QueueElement *new_elements = realloc(pq->elements, sizeof(QueueElement) * new_capacity);
        if (!new_elements)
        {
            pthread_mutex_unlock(&pq->mutex);
            fprintf(stderr, "Error: Failed to reallocate memory for queue.\n");
            return -1;
        }
        pq->elements = new_elements;
        pq->capacity = new_capacity;
    }

    unsigned char *data_copy = malloc(size);
    if (!data_copy)
    {
        pthread_mutex_unlock(&pq->mutex);
        fprintf(stderr, "Error: Failed to allocate memory for data copy.\n");
        return -1;
    }
    memcpy(data_copy, data, size);

    QueueElement *new_element = &pq->elements[pq->size];
    new_element->timestamp = timestamp;
    new_element->payload.data = data_copy;
    new_element->payload.size = size;
    pq->size++;

    heapify_up(pq, pq->size - 1);

    pthread_cond_signal(&pq->cond);
    pthread_mutex_unlock(&pq->mutex);
    return 0;
}

void pq_destroy(PriorityQueue *pq)
{
    if (!pq)
        return;

    pthread_mutex_lock(&pq->mutex);
    pq->is_active = false;
    pthread_cond_broadcast(&pq->cond);
    pthread_mutex_unlock(&pq->mutex);

    pthread_join(pq->worker_thread, NULL);

    for (size_t i = 0; i < pq->size; ++i)
    {
        free(pq->elements[i].payload.data);
    }

    free(pq->elements);
    pthread_mutex_destroy(&pq->mutex);
    pthread_cond_destroy(&pq->cond);
    free(pq);
}

// --- Section 3: Example Usage (equivalent to main.c) ---

#define NUM_PRODUCERS 3
#define ITEMS_PER_PRODUCER 5
