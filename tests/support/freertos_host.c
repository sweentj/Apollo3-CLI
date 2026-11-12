#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"

struct host_queue {
    size_t capacity;
    size_t item_size;
    size_t head;
    size_t count;
    unsigned char data[];
};

void cli_example_assert_failed(const char *file, unsigned int line)
{
    fprintf(stderr, "FreeRTOS assertion failed: %s:%u\n", file, line);
    abort();
}

void *pvPortMalloc(size_t size)
{
    return malloc(size);
}

QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t item_size)
{
    QueueHandle_t queue = calloc(1, sizeof(*queue) + (size_t)length * item_size);
    assert(queue != NULL && length != 0 && item_size != 0);
    queue->capacity = length;
    queue->item_size = item_size;
    return queue;
}

BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t wait)
{
    assert(queue != NULL && item != NULL && wait == 0);
    if (queue->count == queue->capacity) return pdFAIL;
    size_t tail = (queue->head + queue->count) % queue->capacity;
    memcpy(queue->data + tail * queue->item_size, item, queue->item_size);
    ++queue->count;
    return pdPASS;
}

BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t wait)
{
    assert(queue != NULL && item != NULL && wait == 0);
    if (queue->count == 0) return pdFAIL;
    memcpy(item, queue->data + queue->head * queue->item_size, queue->item_size);
    queue->head = (queue->head + 1) % queue->capacity;
    --queue->count;
    return pdPASS;
}

void vQueueDelete(QueueHandle_t queue)
{
    free(queue);
}
