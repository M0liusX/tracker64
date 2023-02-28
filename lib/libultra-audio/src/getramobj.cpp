/*====================================================================
 * getramobj.c
 *
 * No Copyright
 * No Rights Reserved.
 *====================================================================*/

#include <libaudio.h>

void* getRamObject(s32 address) {
   return (void*) ((u8*) alGlobals->rdram + address);
}