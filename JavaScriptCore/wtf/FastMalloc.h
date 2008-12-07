/*
 *  Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_FastMalloc_h
#define WTF_FastMalloc_h

#include "Platform.h"
#include <stdlib.h>
#include <new>

namespace WTF {

    // These functions call CRASH() if an allocation fails.
    void* fastMalloc(size_t n);
    void* fastZeroedMalloc(size_t n);
    void* fastCalloc(size_t n_elements, size_t element_size);
    void* fastRealloc(void* p, size_t n);

    // These functions return NULL if an allocation fails.
    void* tryFastMalloc(size_t n);
    void* tryFastZeroedMalloc(size_t n);
    void* tryFastCalloc(size_t n_elements, size_t element_size);
    void* tryFastRealloc(void* p, size_t n);

    void fastFree(void* p);

#ifndef NDEBUG    
    void fastMallocForbid();
    void fastMallocAllow();
#endif

    void releaseFastMallocFreeMemory();
    
    struct FastMallocStatistics {
        size_t heapSize;
        size_t freeSizeInHeap;
        size_t freeSizeInCaches;
        size_t returnedSize;
    };
    FastMallocStatistics fastMallocStatistics();

} // namespace WTF

using WTF::fastMalloc;
using WTF::fastZeroedMalloc;
using WTF::fastCalloc;
using WTF::fastRealloc;
using WTF::tryFastMalloc;
using WTF::tryFastZeroedMalloc;
using WTF::tryFastCalloc;
using WTF::tryFastRealloc;
using WTF::fastFree;

#ifndef NDEBUG    
using WTF::fastMallocForbid;
using WTF::fastMallocAllow;
#endif

#if COMPILER(GCC) && PLATFORM(DARWIN)
#define WTF_PRIVATE_INLINE __private_extern__ inline __attribute__((always_inline))
#elif COMPILER(GCC)
#define WTF_PRIVATE_INLINE inline __attribute__((always_inline))
#elif COMPILER(MSVC)
#define WTF_PRIVATE_INLINE __forceinline
#else
#define WTF_PRIVATE_INLINE inline
#endif

#ifndef _CRTDBG_MAP_ALLOC

#if !defined(USE_SYSTEM_MALLOC) || !(USE_SYSTEM_MALLOC)
WTF_PRIVATE_INLINE void* operator new(size_t s) { return fastMalloc(s); }
WTF_PRIVATE_INLINE void operator delete(void* p) { fastFree(p); }
WTF_PRIVATE_INLINE void* operator new[](size_t s) { return fastMalloc(s); }
WTF_PRIVATE_INLINE void operator delete[](void* p) { fastFree(p); }
#endif

#endif // _CRTDBG_MAP_ALLOC

#endif /* WTF_FastMalloc_h */
