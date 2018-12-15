#pragma once
/* $Id: ports.h,v 1.7 2005/10/15 18:05:03 titer Exp $

This file is part of the HandBrake source code.
Homepage: <http://handbrake.fr/>.
It may be used under the terms of the GNU General Public License. */

#ifndef HB_PORTS_H
#define HB_PORTS_H
#include <sys/stat.h>

#include "lock.h"

typedef struct stat hb_stat_t;
typedef void (thread_func_t)(void *);
/************************************************************************
* Utils
***********************************************************************/
void     hb_snooze(int delay);
int      hb_platform_init();
// Convert utf8 string to current code page.
char * hb_utf8_to_cp(const char *src);
/* Everything from now is only used internally and hidden to the UI */

/************************************************************************
* DVD utils
***********************************************************************/
int hb_dvd_region(char *device, int *region_mask);

/************************************************************************
* File utils
***********************************************************************/
int hb_stat(const char *path, hb_stat_t *sb);
void hb_mkdir(char * name);

/************************************************************************
* Threads
***********************************************************************/
//typedef struct hb_thread_s hb_thread_t;

hb_thread_t * hb_thread_init(char * name, void(*function)(void *), void * arg, int priority);
void          hb_thread_close(hb_thread_t **);
int           hb_thread_has_exited(hb_thread_t *);

/************************************************************************
* Mutexes
***********************************************************************/

hb_lock_t * hb_lock_init();
void        hb_lock_close(hb_lock_t **);
void        hb_lock(hb_lock_t *);
void        hb_unlock(hb_lock_t *);

/************************************************************************
* Condition variables
***********************************************************************/

hb_cond_t * hb_cond_init();
void        hb_cond_wait(hb_cond_t *, hb_lock_t *);
void        hb_cond_timedwait(hb_cond_t * c, hb_lock_t * lock, int msec);
void        hb_cond_signal(hb_cond_t *);
void        hb_cond_broadcast(hb_cond_t * c);
void        hb_cond_close(hb_cond_t **);

/************************************************************************
* Network
***********************************************************************/
typedef struct hb_net_s hb_net_t;

hb_net_t * hb_net_open(char * address, int port);
int        hb_net_send(hb_net_t *, char *);
int        hb_net_recv(hb_net_t *, char *, int);
void       hb_net_close(hb_net_t **);

#endif


