#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include "threadpool.h"

typedef struct task
{
    void(*runTask)(void*);
    void *arg;
    struct task *next;
} task;

typedef struct
{
    task *front;
    task *back;
    int max_size;
    int size;
    int stop;
    pthread_mutex_t queue_mutex;
    pthread_cond_t not_empty,not_full;

} taskQueue;

struct threadPool
{
    pthread_t *threads;
    taskQueue *queue;
    int thread_nums;
    int working_thread_nums;
    int shutdown;
    pthread_mutex_t pool_mutex;
};

static void *runTaskThread(void *thread_pool);

static taskQueue *taskQueueInit(int que_max_size);
static void taskQueuePush(taskQueue *que,task *new_task);
static task *taskQueuePop(taskQueue *que);
static void taskQueueStop(taskQueue *que);
static void taskQueueDestroy(taskQueue **que);

threadPool *threadPoolInit(int thread_nums,int que_max_size)
{
    threadPool *pool;

    if(thread_nums<=0 ||que_max_size<=0)
        return NULL;

    if((pool=(threadPool *)malloc(sizeof(threadPool)))==NULL)
    {
        return NULL;
    }

    pool->thread_nums=pool->shutdown=0;
    pool->working_thread_nums=0;

    if((pool->queue=taskQueueInit(que_max_size))==NULL)
    {
        free(pool);
        return NULL;
    }

    if((pool->threads=(pthread_t*)malloc(sizeof(pthread_t)*thread_nums))==NULL)
    {
        free(pool->queue);
        free(pool);
        return NULL;
    }

    pthread_mutex_init(&pool->pool_mutex,NULL);

    for(int i=0; i<thread_nums; i++)
    {
        if(pthread_create(&pool->threads[i],NULL,runTaskThread,(void *)pool)==0)
        {
            pool->thread_nums++;
        }
    }

    while(pool->working_thread_nums!=pool->thread_nums) {}

    return pool;
}


int threadPoolAddTask(threadPool *pool,void(*runTask)(void *),void *arg)
{

    if(pool==NULL ||runTask==NULL)
    {
        return -1;
    }

    task *new_task;
    if((new_task=(task *)malloc(sizeof(task)))==NULL)
    {
        return -1;
    }

    new_task->runTask=runTask;
    new_task->arg=arg;
    new_task->next=NULL;

    taskQueuePush(pool->queue,new_task);

    return 0;
}


int threadPoolDestroy(threadPool **pool)
{
    if(*pool==NULL ||(*pool)->shutdown) return -1;

    taskQueueStop((*pool)->queue);
    (*pool)->shutdown=1;

    for(int i=0; i<(*pool)->thread_nums; i++)
        pthread_join((*pool)->threads[i],NULL);

    taskQueueDestroy(&(*pool)->queue);

    free((*pool)->threads);
    pthread_mutex_destroy(&(*pool)->pool_mutex);
    free(*pool);
    return 0;
}


static void *runTaskThread(void *thread_pool)
{
    threadPool *pool=(threadPool *)thread_pool;
    task *new_task;

    pthread_mutex_lock(&pool->pool_mutex);
    pool->working_thread_nums++;
    pthread_mutex_unlock(&pool->pool_mutex);

    for(;;)
    {
        pthread_mutex_lock(&pool->pool_mutex);

        if(pool->shutdown)
        {
            pthread_mutex_unlock(&pool->pool_mutex);
            break;
        }
        else
        {
            pthread_mutex_unlock(&pool->pool_mutex);
        }

        new_task=taskQueuePop(pool->queue);

        if(new_task)
        {
            new_task->runTask(new_task->arg);
            free(new_task);
        }
    }

    pthread_mutex_lock(&pool->pool_mutex);
    pool->working_thread_nums--;
    pthread_mutex_unlock(&pool->pool_mutex);

    pthread_exit(NULL);
}


static taskQueue *taskQueueInit(int que_max_size)
{
    taskQueue *que;
    que=(taskQueue *)malloc(sizeof(taskQueue));
    if(que==NULL)
    {
        return NULL;
    }

    que->front=que->back=NULL;
    que->max_size=que_max_size;
    que->size=que->stop=0;

    pthread_mutex_init(&que->queue_mutex,NULL);
    pthread_cond_init(&que->not_empty,NULL);
    pthread_cond_init(&que->not_full,NULL);
    return que;
}


static void  taskQueuePush(taskQueue *que,task *new_task)
{
    pthread_mutex_lock(&que->queue_mutex);

    while(que->size==que->max_size && !que->stop)
    {
        printf("queue is full,need to wait,Asyn thread ID is: %ld\n",(long)pthread_self());
        pthread_cond_wait(&que->not_full,&que->queue_mutex);
    }

    if(que->stop)
    {
        pthread_mutex_unlock(&que->queue_mutex);
        free(new_task);
        return;
    }

    if(que->size==0)
    {
        que->front=que->back=new_task;
    }
    else
    {
        que->back->next=new_task;
        que->back=new_task;
    }
    que->size++;
    pthread_cond_signal(&que->not_empty);

    pthread_mutex_unlock(&que->queue_mutex);
}


static task *taskQueuePop(taskQueue *que)
{
    task *que_front;
    pthread_mutex_lock(&que->queue_mutex);
    while(que->size==0 && !que->stop)
    {
        printf("queue is empty,need to wait,Asyn thread ID is: %ld\n",(long)pthread_self());
        pthread_cond_wait(&que->not_empty,&que->queue_mutex);
    }

    if(que->stop)
    {
        pthread_mutex_unlock(&que->queue_mutex);
        return NULL;
    }

    que_front=que->front;
    if(que->size==1)
    {
        que->front=que->back=NULL;
    }
    else
    {
        que->front=que->front->next;
    }
    que->size--;

    pthread_cond_signal(&que->not_full);

    pthread_mutex_unlock(&que->queue_mutex);

    return que_front;
}


static void taskQueueStop(taskQueue *que)
{
    pthread_mutex_lock(&que->queue_mutex);

    que->stop=1;
    pthread_cond_broadcast(&que->not_empty);
    pthread_cond_broadcast(&que->not_full);

    pthread_mutex_unlock(&que->queue_mutex);
}


static void taskQueueDestroy(taskQueue **que)
{
    int size;
    task *current,*next;

    size=(*que)->size;
    current=(*que)->front;
    while(size--)
    {
        next=current->next;
        free(current);
        current=next;
    }
    (*que)->front=(*que)->back=NULL;
    pthread_mutex_destroy(&(*que)->queue_mutex);
    pthread_cond_destroy(&(*que)->not_empty);
    pthread_cond_destroy(&(*que)->not_full);

    free(*que);
}



















