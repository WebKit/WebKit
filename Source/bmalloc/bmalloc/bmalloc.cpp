/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "bmalloc.h"

#include "PerProcess.h"

namespace bmalloc { namespace api {

void* mallocOutOfLine(size_t size, HeapKind kind)
{
    return malloc(size, kind);
}

void freeOutOfLine(void* object, HeapKind kind)
{
    free(object, kind);
}

void* tryLargeZeroedMemalignVirtual(size_t alignment, size_t size, HeapKind kind)
{
    BASSERT(isPowerOfTwo(alignment));

    size_t pageSize = vmPageSize();
    alignment = roundUpToMultipleOf(pageSize, alignment);
    size = roundUpToMultipleOf(pageSize, size);

    kind = mapToActiveHeapKind(kind);
    Heap& heap = PerProcess<PerHeapKind<Heap>>::get()->at(kind);

    void* result;
    {
        std::lock_guard<StaticMutex> lock(Heap::mutex());
        result = heap.tryAllocateLarge(lock, alignment, size);
    }

    if (result)
        vmZeroAndPurge(result, size);
    return result;
}

void freeLargeVirtual(void* object, HeapKind kind)
{
    kind = mapToActiveHeapKind(kind);
    Heap& heap = PerProcess<PerHeapKind<Heap>>::get()->at(kind);
    std::lock_guard<StaticMutex> lock(Heap::mutex());
    heap.deallocateLarge(lock, object);
}

void scavenge()
{
    scavengeThisThread();

    SafePerProcess<Scavenger>::get()->scavenge();
}

bool isEnabled(HeapKind kind)
{
    kind = mapToActiveHeapKind(kind);
    std::unique_lock<StaticMutex> lock(Heap::mutex());
    return !PerProcess<PerHeapKind<Heap>>::getFastCase()->at(kind).debugHeap();
}

#if BOS(DARWIN)
void setScavengerThreadQOSClass(qos_class_t overrideClass)
{
    std::unique_lock<StaticMutex> lock(Heap::mutex());
    SafePerProcess<Scavenger>::get()->setScavengerThreadQOSClass(overrideClass);
}
#endif

} } // namespace bmalloc::api

