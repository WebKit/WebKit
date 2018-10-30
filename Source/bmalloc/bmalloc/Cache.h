/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef Cache_h
#define Cache_h

#include "Allocator.h"
#include "BExport.h"
#include "Deallocator.h"
#include "HeapKind.h"
#include "PerThread.h"

namespace bmalloc {

// Per-thread allocation / deallocation cache, backed by a per-process Heap.

class Cache {
public:
    static void* tryAllocate(HeapKind, size_t);
    static void* allocate(HeapKind, size_t);
    static void* tryAllocate(HeapKind, size_t alignment, size_t);
    static void* allocate(HeapKind, size_t alignment, size_t);
    static void deallocate(HeapKind, void*);
    static void* tryReallocate(HeapKind, void*, size_t);
    static void* reallocate(HeapKind, void*, size_t);

    static void scavenge(HeapKind);

    Cache(HeapKind);

    Allocator& allocator() { return m_allocator; }
    Deallocator& deallocator() { return m_deallocator; }

private:
    BEXPORT static void* tryAllocateSlowCaseNullCache(HeapKind, size_t);
    BEXPORT static void* allocateSlowCaseNullCache(HeapKind, size_t);
    BEXPORT static void* allocateSlowCaseNullCache(HeapKind, size_t alignment, size_t);
    BEXPORT static void deallocateSlowCaseNullCache(HeapKind, void*);
    BEXPORT static void* reallocateSlowCaseNullCache(HeapKind, void*, size_t);

    Deallocator m_deallocator;
    Allocator m_allocator;
};

inline void* Cache::tryAllocate(HeapKind heapKind, size_t size)
{
    PerHeapKind<Cache>* caches = PerThread<PerHeapKind<Cache>>::getFastCase();
    if (!caches)
        return tryAllocateSlowCaseNullCache(heapKind, size);
    return caches->at(mapToActiveHeapKindAfterEnsuringGigacage(heapKind)).allocator().tryAllocate(size);
}

inline void* Cache::allocate(HeapKind heapKind, size_t size)
{
    PerHeapKind<Cache>* caches = PerThread<PerHeapKind<Cache>>::getFastCase();
    if (!caches)
        return allocateSlowCaseNullCache(heapKind, size);
    return caches->at(mapToActiveHeapKindAfterEnsuringGigacage(heapKind)).allocator().allocate(size);
}

inline void* Cache::tryAllocate(HeapKind heapKind, size_t alignment, size_t size)
{
    PerHeapKind<Cache>* caches = PerThread<PerHeapKind<Cache>>::getFastCase();
    if (!caches)
        return allocateSlowCaseNullCache(heapKind, alignment, size);
    return caches->at(mapToActiveHeapKindAfterEnsuringGigacage(heapKind)).allocator().tryAllocate(alignment, size);
}

inline void* Cache::allocate(HeapKind heapKind, size_t alignment, size_t size)
{
    PerHeapKind<Cache>* caches = PerThread<PerHeapKind<Cache>>::getFastCase();
    if (!caches)
        return allocateSlowCaseNullCache(heapKind, alignment, size);
    return caches->at(mapToActiveHeapKindAfterEnsuringGigacage(heapKind)).allocator().allocate(alignment, size);
}

inline void Cache::deallocate(HeapKind heapKind, void* object)
{
    PerHeapKind<Cache>* caches = PerThread<PerHeapKind<Cache>>::getFastCase();
    if (!caches)
        return deallocateSlowCaseNullCache(heapKind, object);
    return caches->at(mapToActiveHeapKindAfterEnsuringGigacage(heapKind)).deallocator().deallocate(object);
}

inline void* Cache::tryReallocate(HeapKind heapKind, void* object, size_t newSize)
{
    PerHeapKind<Cache>* caches = PerThread<PerHeapKind<Cache>>::getFastCase();
    if (!caches)
        return reallocateSlowCaseNullCache(heapKind, object, newSize);
    return caches->at(mapToActiveHeapKindAfterEnsuringGigacage(heapKind)).allocator().tryReallocate(object, newSize);
}

inline void* Cache::reallocate(HeapKind heapKind, void* object, size_t newSize)
{
    PerHeapKind<Cache>* caches = PerThread<PerHeapKind<Cache>>::getFastCase();
    if (!caches)
        return reallocateSlowCaseNullCache(heapKind, object, newSize);
    return caches->at(mapToActiveHeapKindAfterEnsuringGigacage(heapKind)).allocator().reallocate(object, newSize);
}

} // namespace bmalloc

#endif // Cache_h
