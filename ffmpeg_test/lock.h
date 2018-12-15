#pragma once
/*
* lock.h
*
*  Created on: Dec 13, 2016
*      Author: root
*/

#ifndef LIBHB_LOCK_H_
#define LIBHB_LOCK_H_
#include <pthread.h>
#include "hbtypes.h"

#pragma comment(lib,"x86/pthreadVC2.lib")

/************************************************************************
* Portable mutex implementation
***********************************************************************/
struct hb_lock_s
{
    pthread_mutex_t mutex;
};

hb_lock_t * hb_lock_init();
void hb_lock_close(hb_lock_t ** _l);
void hb_lock(hb_lock_t * l);
void hb_unlock(hb_lock_t * l);
#endif /* LIBHB_LOCK_H_ */

