/*
 * mem_lib.h
 *
 * Common definitions for C-based F/W
 */

#ifndef _MEM_LIB_H_
#define _MEM_LIB_H_

#include "c_fw_defs.h"

// Release packet in PSRAM / DDR 
INLINE void c_packet_buffer_release(uint32_t pd3, uint32_t bb_source_id);


#endif /* _MEM_LIB_H_ */