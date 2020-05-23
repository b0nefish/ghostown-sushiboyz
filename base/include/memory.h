#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <exec/types.h>

#ifndef MEMF_PUBLIC
#define MEMF_PUBLIC (1L << 0)
#endif

#ifndef MEMF_CHIP
#define MEMF_CHIP (1L << 1)
#endif

#ifndef MEMF_FAST
#define MEMF_FAST (1L << 2)
#endif

#ifndef MEMF_CLEAR
#define MEMF_CLEAR (1L << 16)
#endif

#ifndef MEMF_LARGEST
#define MEMF_LARGEST (1L << 17)
#endif

#ifndef MEMF_REVERSE
#define MEMF_REVERSE (1L << 18)
#endif

__regargs void MemDebug(ULONG attributes);
__regargs LONG MemAvail(ULONG attributes);
__regargs LONG MemUsed(ULONG attributes);

__regargs APTR MemAlloc(ULONG byteSize, ULONG attributes);
__regargs void MemResize(APTR memoryBlock, ULONG byteSize);
__regargs void MemFree(APTR memoryBlock);
__regargs LONG MemTypeOf(APTR address);

#endif
