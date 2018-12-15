/* $Id: ports.c,v 1.15 2005/10/15 18:05:03 titer Exp $

This file is part of the HandBrake source code.
Homepage: <http://handbrake.fr/>.
It may be used under the terms of the GNU General Public License. */

#define _GNU_SOURCE
#include "stdafx.h"
#include <sched.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>


#include <time.h>



#include <fcntl.h>


#include <stddef.h>

#include <ctype.h>

#include "ports.h"
#include <stdlib.h>

#pragma comment(lib,"x86/pthreadVC2.lib")



// Convert utf8 string to current code page.
// The internal string representation in hb is utf8. But some
// libraries (libmkv, and mp4v2) expect filenames in the current
// code page.  So we must convert.

/************************************************************************
* hb_get_date()
************************************************************************
* Returns the current date in milliseconds.
* On Win32, we implement a gettimeofday emulation here because
* libdvdread and libmp4v2 use it without checking.
************************************************************************/

int hb_dvd_region(char *device, int *region_mask)
{
#if defined( DVD_LU_SEND_RPC_STATE ) && defined( DVD_AUTH )
    struct stat st;
    dvd_authinfo ai;
    int fd, ret;

    fd = open(device, O_RDONLY);
    if (fd < 0)
        return -1;
    if (fstat(fd, &st) < 0)
    {
        close(fd);
        return -1;
    }
    if (!(S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)))
    {
        close(fd);
        return -1;
    }

    ai.type = DVD_LU_SEND_RPC_STATE;
    ret = ioctl(fd, DVD_AUTH, &ai);
    close(fd);
    if (ret < 0)
        return ret;

    *region_mask = ai.lrpcs.region_mask;
    return 0;
#else
    return -1;
#endif
}




/************************************************************************
* hb_stat
************************************************************************
* Wrapper to the real stat, needed to handle utf8 filenames on
* windows.
***********************************************************************/
int hb_stat(const char *path, hb_stat_t *sb)
{
    return stat(path, sb);
}

/************************************************************************
* Portable condition variable implementation
***********************************************************************/
struct hb_cond_s {
    pthread_cond_t cond;
};

/************************************************************************
* hb_cond_init()
* hb_cond_close()
* hb_cond_wait()
* hb_cond_signal()
************************************************************************
* Win9x is not supported by this implementation (SignalObjectAndWait()
* only available on Windows 2000/XP).
***********************************************************************/
hb_cond_t * hb_cond_init()
{
    hb_cond_t * c = (hb_cond_t *)calloc(sizeof(hb_cond_t), 1);
    pthread_cond_init(&c->cond, NULL);
    return c;
}

void hb_cond_close(hb_cond_t ** _c)
{
    hb_cond_t * c = *_c;

    pthread_cond_destroy(&c->cond);
    free(c);
    *_c = NULL;
}

void hb_cond_wait(hb_cond_t * c, hb_lock_t * lock)
{
    pthread_cond_wait(&c->cond, &lock->mutex);
}



//void hb_cond_timedwait(hb_cond_t * c, hb_lock_t * lock, int msec)
//{
//    struct timespec ts;
//    hb_clock_gettime(&ts);
//    ts.tv_nsec += (msec % 1000) * 1000000;
//    ts.tv_sec += msec / 1000 + (ts.tv_nsec / 1000000000);
//    ts.tv_nsec %= 1000000000;
//    pthread_cond_timedwait(&c->cond, &lock->mutex, &ts);
//}

void hb_cond_signal(hb_cond_t * c)
{
    pthread_cond_signal(&c->cond);
}

void hb_cond_broadcast(hb_cond_t * c)
{
    pthread_cond_broadcast(&c->cond);
}

/************************************************************************
* Network
***********************************************************************/

struct hb_net_s {
    int socket;
};

//hb_net_t * hb_net_open(char * address, int port)
//{
//    hb_net_t * n = calloc(sizeof(hb_net_t), 1);
//
//    struct sockaddr_in sock;
//    struct hostent * host;
//    /* TODO: find out why this doesn't work on Win32 */
//    if (!(host = gethostbyname(address)))
//    {
//        hb_log("gethostbyname failed (%s)", address);
//        free(n);
//        return NULL;
//    }
//
//    memset(&sock, 0, sizeof(struct sockaddr_in));
//    sock.sin_family = host->h_addrtype;
//    sock.sin_port = htons(port);
//    memcpy(&sock.sin_addr, host->h_addr, host->h_length);
//
//    if ((n->socket = socket(host->h_addrtype, SOCK_STREAM, 0)) < 0)
//    {
//        hb_log("socket failed");
//        free(n);
//        return NULL;
//    }
//
//    if (connect(n->socket, (struct sockaddr *) &sock,
//        sizeof(struct sockaddr_in)) < 0)
//    {
//        hb_log("connect failed");
//        free(n);
//        return NULL;
//    }
//
//    return n;
//}

//int hb_net_send(hb_net_t * n, char * buffer)
//{
//    return send(n->socket, buffer, strlen(buffer), 0);
//}
//
//int hb_net_recv(hb_net_t * n, char * buffer, int size)
//{
//    return recv(n->socket, buffer, size - 1, 0);
//}
//
//void hb_net_close(hb_net_t ** _n)
//{
//    hb_net_t * n = (hb_net_t *)* _n;
//    close(n->socket);
//    free(n);
//    *_n = NULL;
//}
