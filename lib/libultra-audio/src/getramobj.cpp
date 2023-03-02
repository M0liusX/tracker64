/*====================================================================
 * getramobj.c
 *
 * No Copyright
 * No Rights Reserved.
 *====================================================================*/

#include <libaudio.h>
#include <cassert>

void* getRamObject(s32 address) {
   return (void*) ((u8*) alGlobals->rdram + address);
}

u32 virtualToPhysical(void *virtualLoc) {
   s64 physicalLoc = (s64) virtualLoc - (s64) alGlobals->rdram;
   assert((physicalLoc > 0) && (physicalLoc < 0x800000)); // from 0 to 8Mb-1 address
   return physicalLoc;
}