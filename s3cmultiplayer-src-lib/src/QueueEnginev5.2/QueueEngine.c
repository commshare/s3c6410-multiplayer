#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>

#include "QueueEngine.h"
#include "CodecEngine.h"

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)>(b)?(b):(a))
#endif

#ifndef CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#endif

#define POINTER_TO_UINT(p)  ((unsigned int)(p))

#if defined (__GNUC__)
#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#else
#define likely(x)	(x)
#define unlikely(x)	(x)
#endif

static QueueData *get_node_link_nth(QueueBuffer *qu, uint32_t n);

static void lock_queue(QueueBuffer *qu)
{
	if ( unlikely(NULL == qu) )
		return ;
	pthread_mutex_lock(&qu->mutex);
}

static void unlock_queue(QueueBuffer *qu)
{
	if ( unlikely(NULL == qu) )
		return ;
	pthread_mutex_unlock(&qu->mutex);
}

static int dlp_wakeup_on_queue(QueueBuffer *qu)
{
	if ( unlikely(NULL == qu) )
		return -1;
	return pthread_cond_broadcast(&qu->cond);
}

int QueueBufferWait(QueueBuffer *qu)
{
	if ( unlikely(NULL == qu) )
		return -1;

	pthread_cond_wait(&qu->cond, &qu->mutex);
	pthread_mutex_unlock(&qu->mutex);
	return 0;
}


int QueueBufferWaitTimeOut(QueueBuffer *qu, int timeout)
{
	if ( unlikely(NULL == qu) )
		return -1;

	if ( 0 == timeout )
	{
		pthread_cond_wait(&qu->cond, &qu->mutex);
		pthread_mutex_unlock(&qu->mutex);
	}
	else
	{
		struct timespec abstime;
		struct timeval tv;
		long s;

		gettimeofday( &tv, NULL );
	    	s = timeout * 1000 + tv.tv_usec;
		tv.tv_sec += s / 1000000;
		tv.tv_usec = s % 1000000;

		abstime.tv_sec = tv.tv_sec;
		abstime.tv_nsec = tv.tv_usec * 1000;

		pthread_cond_timedwait(&qu->cond, &qu->mutex, &abstime);
		pthread_mutex_unlock(&qu->mutex);
	}
	return 0;
}


QueueBuffer *InitQueueBuffer(void)
{
	QueueBuffer *q = (QueueBuffer*)UseAlloc(sizeof(QueueBuffer));
	q->head = q->tail = NULL;
	q->length = 0;
	pthread_mutex_init(&q->mutex,NULL);
	pthread_cond_init(&q->cond,NULL);
	return q;
}

void UninitQueueBuffer(QueueBuffer *qu)
{
	MediaPacket data;

	if ( unlikely(NULL == qu) )
		return;

	while ( (!FetchDataFromTail(qu,&data)) )
	{
		if(data.data && data.size > 0)
			FreeAlloc(data.data);
	}

	pthread_mutex_destroy(&qu->mutex);
	pthread_cond_destroy(&qu->cond);

	FreeAlloc( qu );
}

void QueueBufferFlush(QueueBuffer *qu)
{
	MediaPacket data;

	if ( unlikely(NULL == qu) )
		return;

	while ( (!FetchDataFromTail(qu,&data)) )
	{
		
		if(data.data && data.size > 0)
			FreeAlloc(data.data);
	}
}

uint32_t ParseQueueBufferLength(QueueBuffer *qu)
{
	if ( unlikely(NULL == qu) )
		return 0;
	return qu->length;
}

void PutDataToHead(QueueBuffer *qu, MediaPacket *data)
{
	QueueData *node = NULL;

	if ( unlikely(NULL == qu) )
		return;

	lock_queue(qu);
	node = (QueueData*)UseAlloc(sizeof(QueueData));
	node->data = *data;
	node->next = qu->head;
	node->prev = NULL;

	if ( qu->head )
		qu->head->prev = node;
	qu->head = node;
	if ( NULL == qu->tail )
		qu->tail = qu->head;
	qu->length++;
	unlock_queue(qu);
	dlp_wakeup_on_queue(qu);
}


void PutDataToTail(QueueBuffer *qu,MediaPacket *data)
{
	QueueData *node = NULL;

	if ( unlikely(NULL == qu) )
		return;

	lock_queue(qu);
	node = (QueueData*)UseAlloc(sizeof(QueueData));
	node->data = *data;
	node->next = NULL;
	node->prev = qu->tail;

	if ( qu->tail )
		qu->tail->next = node;
	qu->tail = node;
	if ( NULL == qu->head )
		qu->head = qu->tail;
	qu->length++;
	unlock_queue(qu);
	dlp_wakeup_on_queue(qu);
}

int FetchDataFromHead(QueueBuffer *qu,MediaPacket *pkt)
{
	QueueData *p;
	int ret = -1;

	lock_queue(qu);
	if ( unlikely(NULL == qu) || unlikely(NULL == qu->head) )
	{
		ret = -1;
		goto end;
	}

	p = qu->head;

	qu->head = p->next;
	if ( qu->head )
		qu->head->prev = NULL;
	else
		qu->tail = NULL;

	qu->length--;
	*pkt = p->data;
	FreeAlloc( p );
	ret = 0;

end:
	unlock_queue(qu);
	return ret;
}


int FetchDataFromTail(QueueBuffer *qu,MediaPacket *pkt)
{
	QueueData *p;

	int ret = -1;

	lock_queue(qu);
	if ( unlikely(NULL == qu) || unlikely(NULL == qu->tail) )
	{
		ret = -1;
		goto end;
	}

	p = qu->tail;
	qu->tail = p->prev;
	if ( qu->tail )
		qu->tail->next = NULL;
	else
		qu->head = NULL;

	qu->length--;
	*pkt = p->data;
	FreeAlloc( p );

	ret = 0;

end:
	unlock_queue(qu);

	return ret;
}

static QueueData *get_node_link_nth(QueueBuffer *qu, uint32_t n)
{
	QueueData *p = NULL;

	if ( unlikely(NULL == qu) )
		return NULL;

	if ( n < qu->length / 2 )
	{
		p = qu->head;
		n = n - 1;
		while ( n-- )
			p = p->next;
	}
	else
	{
		p = qu->tail;
		n = qu->length - n;
		while ( n-- )
			p = p->prev;
	}
	return p;
}

