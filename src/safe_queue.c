// safe_queue.c

#include "safe_queue.h"
#include <stdlib.h>
#include <stdio.h>

SafeQueue *safe_queue_create(int capacity)
{
    SafeQueue *queue = (SafeQueue *)malloc(sizeof(SafeQueue));
    if (!queue)
    {
        perror("Failed to allocate memory for queue");
        return NULL;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->capacity = capacity;
    queue->is_valid = 1; // 队列有效

    // 初始化互斥锁和条件变量
    if (pthread_mutex_init(&queue->mutex, NULL) != 0)
    {
        perror("Mutex initialization failed");
        free(queue);
        return NULL;
    }
    if (pthread_cond_init(&queue->cond_non_empty, NULL) != 0)
    {
        perror("Condition variable (non-empty) initialization failed");
        pthread_mutex_destroy(&queue->mutex);
        free(queue);
        return NULL;
    }
    if (pthread_cond_init(&queue->cond_non_full, NULL) != 0)
    {
        perror("Condition variable (non-full) initialization failed");
        pthread_mutex_destroy(&queue->mutex);
        pthread_cond_destroy(&queue->cond_non_empty);
        free(queue);
        return NULL;
    }

    return queue;
}

void safe_queue_destroy(SafeQueue *queue)
{
    if (!queue)
        return;

    // 锁定队列以安全地修改状态
    pthread_mutex_lock(&queue->mutex);

    // 标记队列为无效，防止其他线程继续操作
    queue->is_valid = 0;

    // 唤醒所有可能在等待的线程，让它们退出
    pthread_cond_broadcast(&queue->cond_non_empty);
    pthread_cond_broadcast(&queue->cond_non_full);

    pthread_mutex_unlock(&queue->mutex);

    // 此时，用户需要确保所有线程都已经退出对该队列的访问
    // 这里我们假设调用者已经处理了线程的join

    // 清理队列中剩余的节点
    QueueNode *current = queue->head;
    while (current != NULL)
    {
        QueueNode *temp = current;
        current = current->next;
        // 注意：这里只释放节点本身，不释放节点中的数据。
        // 数据的内存管理由用户负责。
        free(temp);
    }

    // 销毁同步原语
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond_non_empty);
    pthread_cond_destroy(&queue->cond_non_full);

    // 释放队列结构本身
    free(queue);
}

void safe_queue_enqueue(SafeQueue *queue, void *data)
{
    if (!queue)
        return;

    pthread_mutex_lock(&queue->mutex);

    // 使用 while 循环检查队列是否已满
    // 这是标准做法，可以防止“虚假唤醒”(spurious wakeup)
    while (queue->size >= queue->capacity && queue->is_valid)
    {
        // 队列已满，等待消费者取出元素
        pthread_cond_wait(&queue->cond_non_full, &queue->mutex);
    }

    if (!queue->is_valid)
    {
        // 如果在等待期间队列被销毁，则直接返回
        pthread_mutex_unlock(&queue->mutex);
        return;
    }

    // 创建新节点
    QueueNode *new_node = (QueueNode *)malloc(sizeof(QueueNode));
    new_node->data = data;
    new_node->next = NULL;

    // 将节点添加到队列尾部
    if (queue->tail == NULL)
    {
        // 队列为空
        queue->head = new_node;
        queue->tail = new_node;
    }
    else
    {
        queue->tail->next = new_node;
        queue->tail = new_node;
    }
    queue->size++;

    // 通知可能正在等待的消费者，队列现在非空
    pthread_cond_signal(&queue->cond_non_empty);

    pthread_mutex_unlock(&queue->mutex);
}

void *safe_queue_dequeue(SafeQueue *queue)
{
    if (!queue)
        return NULL;

    pthread_mutex_lock(&queue->mutex);

    // 使用 while 循环检查队列是否为空
    while (queue->size == 0 && queue->is_valid)
    {
        // 队列为空，等待生产者放入元素
        pthread_cond_wait(&queue->cond_non_empty, &queue->mutex);
    }

    if (!queue->is_valid && queue->size == 0)
    {
        // 队列被销毁且已空，直接返回
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }

    // 取出头节点
    QueueNode *old_head = queue->head;
    void *data = old_head->data;
    queue->head = old_head->next;

    // 如果取出后队列为空，需要更新尾指针
    if (queue->head == NULL)
    {
        queue->tail = NULL;
    }
    queue->size--;

    // 释放旧的头节点
    free(old_head);

    // 通知可能正在等待的生产者，队列现在有空间了
    pthread_cond_signal(&queue->cond_non_full);

    pthread_mutex_unlock(&queue->mutex);

    return data;
}