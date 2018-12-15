#pragma once
/*
* hb_mem.h
*
*  Created on: Dec 14, 2016
*      Author: root
*/

#ifndef LIBHB_HB_MEM_H_
#define LIBHB_HB_MEM_H_

#define MALLOCZERO( var, size )\
    do {\
            var = hb_malloc( size );\
            memset( var, 0, size );\
    } while( 0 )

void *hb_malloc(int i_size);
void hb_free(void** ptr);
#endif /* LIBHB_HB_MEM_H_ */
