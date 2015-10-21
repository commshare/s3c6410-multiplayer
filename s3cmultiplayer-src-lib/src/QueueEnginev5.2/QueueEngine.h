#ifndef __QUEUE_ENGINE_H
#define __QUEUE_ENGINE_H

#include "basetype.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct QueueData
{
	MediaPacket data;
	int size;
	struct QueueData *next;
	struct QueueData *prev;
} QueueData;

typedef struct QueueCache
{
	QueueData *head;
	QueueData *tail;
	uint32_t length;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} QueueBuffer;

QueueBuffer *InitQueueBuffer(void);

typedef void (*free_func) (void*) ;

void UninitQueueBuffer(QueueBuffer *qu);
void QueueBufferFlush(QueueBuffer *qu);

int QueueBufferWait(QueueBuffer *qu);
int QueueBufferWaitTimeOut(QueueBuffer *qu, int timeout);

uint32_t ParseQueueBufferLength(QueueBuffer *qu);

void PutDataToHead(QueueBuffer *qu,MediaPacket *data);
void PutDataToTail(QueueBuffer *qu,MediaPacket *data);

int FetchDataFromHead(QueueBuffer *qu,MediaPacket *pkt);
int FetchDataFromTail(QueueBuffer *qu,MediaPacket *pkt);

#ifdef __cplusplus
}
#endif

#endif
