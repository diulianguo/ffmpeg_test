#pragma once
/*
* hb_thread.h
*
*  Created on: Dec 14, 2016
*      Author: root
*/

#ifndef LIBHB_HB_THREAD_H_
#define LIBHB_HB_THREAD_H_
#include <pthread.h>
#include "lock.h"
#include "hbtypes.h"

#pragma comment(lib,"x86/pthreadVC2.lib")

struct hb_thread_s
{
    char * name;
    int priority;
    void(*function)(void *);
    void * arg;

    hb_lock_t * lock;
    int exited;
    pthread_t thread;
};

hb_thread_t * hb_thread_init(char * name, void(*function)(void *), void * arg, int priority);

#endif /* LIBHB_HB_THREAD_H_ */
