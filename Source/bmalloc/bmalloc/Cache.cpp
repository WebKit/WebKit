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

#include "BInline.h"
#include "Cache.h"
#include "DebugHeap.h"
#include "Environment.h"
#include "Heap.h"
#include "PerProcess.h"

namespace bmalloc {

static DebugHeap* debugHeapCache { nullptr };

void Cache::scavenge(HeapKind heapKind)
{
    PerHeapKind<Cache>* caches = PerThread<PerHeapKind<Cache>>::getFastCase();
    if (!caches)
        return;
    if (!isActiveHeapKind(heapKind))
        return;

    caches->at(heapKind).allocator().scavenge();
    caches->at(heapKind).deallocator().scavenge();
}

static BINLINE DebugHeap* debugHeap()
{
    if (debugHeapCache)
        return debugHeapCache;
    if (PerProcess<Environment>::get()->isDebugHeapEnabled()) {
        debugHeapCache = PerProcess<DebugHeap>::get();
        return debugHeapCache;
    }
    return nullptr;
}

Cache::Cache(HeapKind heapKind)
    : m_deallocator(PerProcess<PerHeapKind<Heap>>::get()->at(heapKind))
    , m_allocator(PerProcess<PerHeapKind<Heap>>::get()->at(heapKind), m_deallocator)
{
    BASSERT(!PerProcess<Environment>::get()->isDebugHeapEnabled());
}

BNO_INLINE void* Cache::tryAllocateSlowCaseNullCache(HeapKind heapKind, size_t size)
{
    if (auto* heap = debugHeap()) {
        constexpr bool crashOnFailure = false;
        return heap->malloc(size, crashOnFailure);
    }
    return PerThread<PerHeapKind<Cache>>::getSlowCase()->at(mapToActiveHeapKind(heapKind)).allocator().tryAllocate(size);
}

BNO_INLINE void* Cache::allocateSlowCaseNullCache(HeapKind heapKind, size_t size)
{
    if (auto* heap = debugHeap()) {
        constexpr bool crashOnFailure = true;
        return heap->malloc(size, crashOnFailure);
    }
    return PerThread<PerHeapKind<Cache>>::getSlowCase()->at(mapToActiveHeapKind(heapKind)).allocator().allocate(size);
}

BNO_INLINE void* Cache::tryAllocateSlowCaseNullCache(HeapKind heapKind, size_t alignment, size_t size)
{
    if (auto* heap = debugHeap()) {
        constexpr bool crashOnFailure = false;
        return heap->memalign(alignment, size, crashOnFailure);
    }
    return PerThread<PerHeapKind<Cache>>::getSlowCase()->at(mapToActiveHeapKind(heapKind)).allocator().tryAllocate(alignment, size);
}

BNO_INLINE void* Cache::allocateSlowCaseNullCache(HeapKind heapKind, size_t alignment, size_t size)
{
    if (auto* heap = debugHeap()) {
        constexpr bool crashOnFailure = true;
        return heap->memalign(alignment, size, crashOnFailure);
    }
    return PerThread<PerHeapKind<Cache>>::getSlowCase()->at(mapToActiveHeapKind(heapKind)).allocator().allocate(alignment, size);
}

BNO_INLINE void Cache::deallocateSlowCaseNullCache(HeapKind heapKind, void* object)
{
    if (auto* heap = debugHeap()) {
        heap->free(object);
        return;
    }
    PerThread<PerHeapKind<Cache>>::getSlowCase()->at(mapToActiveHeapKind(heapKind)).deallocator().deallocate(object);
}

BNO_INLINE void* Cache::tryReallocateSlowCaseNullCache(HeapKind heapKind, void* object, size_t newSize)
{
    if (auto* heap = debugHeap()) {
        constexpr bool crashOnFailure = false;
        return heap->realloc(object, newSize, crashOnFailure);
    }
    return PerThread<PerHeapKind<Cache>>::getSlowCase()->at(mapToActiveHeapKind(heapKind)).allocator().tryReallocate(object, newSize);
}

BNO_INLINE void* Cache::reallocateSlowCaseNullCache(HeapKind heapKind, void* object, size_t newSize)
{
    if (auto* heap = debugHeap()) {
        constexpr bool crashOnFailure = true;
        return heap->realloc(object, newSize, crashOnFailure);
    }
    return PerThread<PerHeapKind<Cache>>::getSlowCase()->at(mapToActiveHeapKind(heapKind)).allocator().reallocate(object, newSize);
}

} // namespace bmalloc
