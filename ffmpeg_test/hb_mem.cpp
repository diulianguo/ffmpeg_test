/*
* hb_mem.c
*
*  Created on: Dec 14, 2016
*      Author: root
*/
#include "stdafx.h"
#include <malloc.h>
#include <stdint.h>

#include "hb_mem.h"
//#include "log.h"

/****************************************************************************
*  * hb_malloc:
* ****************************************************************************/
//void *hb_malloc(int i_size)
//{
//    uint8_t *align_buf = NULL;
//    align_buf = memalign(16, i_size);
//    if (!align_buf)
//        hb_log("malloc of size %d failed\n", i_size);
//    return align_buf;
//}

void hb_free(void** ptr)
{
    if (*ptr)
    {
        free(*ptr);
        *ptr = NULL;
    }
}
