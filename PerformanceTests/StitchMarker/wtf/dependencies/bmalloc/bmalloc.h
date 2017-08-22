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

#include "AvailableMemory.h"
#include "Cache.h"
#include "Gigacage.h"
#include "Heap.h"
#include "PerHeapKind.h"
#include "PerProcess.h"
#include "Scavenger.h"
#include "StaticMutex.h"

namespace bmalloc {
namespace api {

// Returns null on failure.
inline void* tryMalloc(size_t size, HeapKind kind = HeapKind::Primary)
{
    return Cache::tryAllocate(kind, size);
}

// Crashes on failure.
inline void* malloc(size_t size, HeapKind kind = HeapKind::Primary)
{
    return Cache::allocate(kind, size);
}

// Returns null on failure.
inline void* tryMemalign(size_t alignment, size_t size, HeapKind kind = HeapKind::Primary)
{
    return Cache::tryAllocate(kind, alignment, size);
}

// Crashes on failure.
inline void* memalign(size_t alignment, size_t size, HeapKind kind = HeapKind::Primary)
{
    return Cache::allocate(kind, alignment, size);
}

// Crashes on failure.
inline void* realloc(void* object, size_t newSize, HeapKind kind = HeapKind::Primary)
{
    return Cache::reallocate(kind, object, newSize);
}

// Returns null for failure
inline void* tryLargeMemalignVirtual(size_t alignment, size_t size, HeapKind kind = HeapKind::Primary)
{
    Heap& heap = PerProcess<PerHeapKind<Heap>>::get()->at(kind);
    std::lock_guard<StaticMutex> lock(Heap::mutex());
    return heap.allocateLarge(lock, alignment, size, AllocationKind::Virtual);
}

inline void free(void* object, HeapKind kind = HeapKind::Primary)
{
    Cache::deallocate(kind, object);
}

inline void freeLargeVirtual(void* object, HeapKind kind = HeapKind::Primary)
{
    Heap& heap = PerProcess<PerHeapKind<Heap>>::get()->at(kind);
    std::lock_guard<StaticMutex> lock(Heap::mutex());
    heap.deallocateLarge(lock, object, AllocationKind::Virtual);
}

inline void scavengeThisThread()
{
    for (unsigned i = numHeaps; i--;)
        Cache::scavenge(static_cast<HeapKind>(i));
}

inline void scavenge()
{
    scavengeThisThread();

    PerProcess<Scavenger>::get()->scavenge();
}

inline bool isEnabled(HeapKind kind = HeapKind::Primary)
{
    std::unique_lock<StaticMutex> lock(Heap::mutex());
    return !PerProcess<PerHeapKind<Heap>>::getFastCase()->at(kind).debugHeap();
}
    
inline size_t availableMemory()
{
    return bmalloc::availableMemory();
}
    
#if BPLATFORM(IOS)
inline size_t memoryFootprint()
{
    return bmalloc::memoryFootprint();
}

inline double percentAvailableMemoryInUse()
{
    return bmalloc::percentAvailableMemoryInUse();
}
#endif

#if BOS(DARWIN)
inline void setScavengerThreadQOSClass(qos_class_t overrideClass)
{
    std::unique_lock<StaticMutex> lock(Heap::mutex());
    PerProcess<Scavenger>::get()->setScavengerThreadQOSClass(overrideClass);
}
#endif

} // namespace api
} // namespace bmalloc
