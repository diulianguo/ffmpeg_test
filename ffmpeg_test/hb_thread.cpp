/*
* hb_thread.c
*
*  Created on: Dec 14, 2016
*      Author: root
*/
#include "stdafx.h"
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "hb_thread.h"
//#include "log.h"
#include "lock.h"
#include "hb_mem.h"
#include <stdarg.h>

/* Get a unique identifier to thread and represent as 64-bit unsigned.
* If unsupported, the value 0 is be returned.
* Caller should use result only for display/log purposes.
*/
//static uint64_t hb_thread_to_integer(const hb_thread_t* t)
//{
//    return (uint64_t)t->thread;
//}

typedef enum hb_debug_level_s
{
    HB_SUPPORT_LOG = 1, // helpful in tech support
    HB_HOUSEKEEPING_LOG = 2, // stuff we hate scrolling through
    HB_GRANULAR_LOG = 3 // sample-by-sample
} hb_debug_level_t;

//void hb_deep_log(hb_debug_level_t level, char * log, ...)
//{
//    va_list args;
//    va_start(args, log);
//    hb_valog(level, NULL, log, args);
//    va_end(args);
//}

/************************************************************************
* hb_thread_func()
************************************************************************
* We use it as the root routine for any thread, for two reasons:
*  + To set the thread priority on OS X (pthread_setschedparam() could
*    be called from hb_thread_init(), but it's nicer to do it as we
*    are sure it is done before the real routine starts)
*  + Get informed when the thread exits, so we know whether
*    hb_thread_close() will block or not.
***********************************************************************/
static void hb_thread_func(void * _t)
{
    hb_thread_t * t = (hb_thread_t *)_t;
    /* Start the actual routine */
    t->function(t->arg);

    /* Inform that the thread can be joined now */
    //hb_deep_log(HB_HOUSEKEEPING_LOG, "thread %" PRIx64" exited (\"%s\")", hb_thread_to_integer(t), t->name);
    hb_lock(t->lock);
    t->exited = 1;
    hb_unlock(t->lock);
}

/************************************************************************
* hb_thread_init()
************************************************************************
* name:     user-friendly name
* function: the thread routine
* arg:      argument of the routine
* priority: HB_LOW_PRIORITY or HB_NORMAL_PRIORITY
***********************************************************************/
hb_thread_t * hb_thread_init(char * name, void(*function)(void *), void * arg, int priority)
{
    hb_thread_t * t = (hb_thread_t *)calloc(sizeof(hb_thread_t), 1);
    t->name = _strdup(name);
    t->function = function;
    t->arg = arg;
    t->priority = priority;
    t->lock = hb_lock_init();
    /* Create and start the thread */
    pthread_create(&t->thread, NULL, (void * (*)(void *)) hb_thread_func, t);
    //hb_log( "-->%s started\n", t->name );
    //hb_deep_log(HB_HOUSEKEEPING_LOG, "thread %" PRIx64" started (\"%s\")", hb_thread_to_integer(t), t->name);
    return t;
}

/************************************************************************
* hb_thread_close()
************************************************************************
* Joins the thread and frees memory.
***********************************************************************/
void hb_thread_close(hb_thread_t ** _t)
{
    hb_thread_t * t = *_t;
    /* Join the thread */
    pthread_join(t->thread, NULL);
    //hb_deep_log(HB_HOUSEKEEPING_LOG, "thread %" PRIx64" joined (\"%s\")", hb_thread_to_integer(t), t->name);
    hb_lock_close(&t->lock);
    hb_free((void**)&t->name);
    free(t);
    *_t = NULL;
}

/************************************************************************
* hb_thread_has_exited()
************************************************************************
* Returns 1 if the thread can be joined right away, 0 otherwise.
***********************************************************************/
int hb_thread_has_exited(hb_thread_t * t)
{
    int exited;
    hb_lock(t->lock);
    exited = t->exited;
    hb_unlock(t->lock);
    return exited;
}


