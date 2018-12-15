/*
* lock.c
*
*  Created on: Dec 13, 2016
*      Author: root
*/
#include "stdafx.h"
#include <stdlib.h>
#include <pthread.h>

#include "lock.h"

#pragma comment(lib,"x86/pthreadVC2.lib")
/************************************************************************
* hb_lock_init()
* hb_lock_close()
* hb_lock()
* hb_unlock()
************************************************************************
* Basic wrappers to OS-specific semaphore or mutex functions.
***********************************************************************/
hb_lock_t * hb_lock_init()
{
    hb_lock_t * l = (hb_lock_t *)calloc(sizeof(hb_lock_t), 1);
    pthread_mutexattr_t mta;

    pthread_mutexattr_init(&mta);
    pthread_mutex_init(&l->mutex, &mta);
    return l;
}

void hb_lock_close(hb_lock_t ** _l)
{
    hb_lock_t * l = *_l;

    pthread_mutex_destroy(&l->mutex);
    free(l);
    *_l = NULL;
}

void hb_lock(hb_lock_t * l)
{
    pthread_mutex_lock(&l->mutex);
}

void hb_unlock(hb_lock_t * l)
{
    pthread_mutex_unlock(&l->mutex);
}

