#include "threadpool.h"
#include <stdlib.h>

#define alloc(type, size) (type *) malloc(sizeof(type)*size)
/*
 * creates thread pool struct and initialises it
 * allocates space for the threads array with size of parameter entered
 * and also inits the mutex locks and conditions, while also telling each thread to start its required routine
 * */
threadpool *create_threadpool(int num_threads_in_pool) {
    if (num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL)
        return NULL;
    threadpool *pool = alloc(threadpool, 1);
    pool->num_threads = num_threads_in_pool;
    pool->qsize = 0;
    pool->threads = alloc(pthread_t, num_threads_in_pool);
    pool->qhead = NULL;
    pool->qtail = NULL;
    pool->shutdown = pool->dont_accept = 0;
    pthread_mutex_init(&pool->qlock, NULL);
    pthread_cond_init(&pool->q_empty, NULL);
    pthread_cond_init(&pool->q_not_empty, NULL);
    for (int i = 0; i < num_threads_in_pool; i++)
        pthread_create(&pool->threads[i], NULL, do_work, pool);
    return pool;
}
/*
 * the dispatch function is to tell a thread to start its work which is calling the
 * dispatch_to_here function with parameter arg as its argument
 * before doing anything, this will tell the mutex to lock and the first available thread in the queue
 * will take the work and dispatch it, after finishing the job it will unlock the mutex, so other threads can access it*/
void dispatch(threadpool *from_me, dispatch_fn dispatch_to_here, void *arg) {
    pthread_mutex_lock(&from_me->qlock);
    while (from_me->dont_accept)
        pthread_cond_wait(&from_me->q_empty, &from_me->qlock);
    work_t *work = alloc(work_t, 1);
    work->routine = dispatch_to_here;
    work->arg = arg;
    work->next = NULL;
    if (from_me->qsize == 0)
        from_me->qhead = from_me->qtail = work;
    else {
        from_me->qtail->next = work;
        from_me->qtail = work;
    }
    from_me->qsize++;
    pthread_cond_signal(&from_me->q_not_empty);
    pthread_mutex_unlock(&from_me->qlock);
}

/*the work do function runs in an endless loop, that will be handling requests from the server
 * the available thread will do the work requested after locking the mutex, if the pool is about to be destroyed, it will wait until
 * the queue is empty
 * and if the pool is already shutdown, all threads will exit*/
void *do_work(void *p) {
    threadpool *pool = (threadpool *) p;
    while (1) {
        pthread_mutex_lock(&pool->qlock);

        while (pool->qsize == 0 && !pool->shutdown)
            pthread_cond_wait(&pool->q_not_empty, &pool->qlock);

        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->qlock);
            pthread_exit(NULL);
        }
        work_t *work = pool->qhead;
        pool->qhead = work->next;
        pool->qsize--;
        if (pool->qsize == 0)
            pool->qtail = NULL;

        if (pool->qsize == 0 && pool->dont_accept)
            pthread_cond_signal(&pool->q_empty);

        pthread_mutex_unlock(&pool->qlock);
        work->routine(work->arg);
        free(work);
    }
}
/*After finishing all requests, the threadpool struct will start freeing all its variables, such as
 * its threads, the mutex lock and the conditions, and finally the struct itself*/
void destroy_threadpool(threadpool *destroyme) {
    pthread_mutex_lock(&destroyme->qlock);
    destroyme->dont_accept = 1;
    while (destroyme->qsize != 0)
        pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);
    destroyme->shutdown = 1;
    pthread_cond_broadcast(&destroyme->q_not_empty);
    pthread_mutex_unlock(&destroyme->qlock);
    for (int i = 0; i < destroyme->num_threads; i++)
        pthread_join(destroyme->threads[i], NULL);
    pthread_mutex_destroy(&destroyme->qlock);
    pthread_cond_destroy(&destroyme->q_not_empty);
    pthread_cond_destroy(&destroyme->q_empty);
    free(destroyme->threads);
    free(destroyme);
}
