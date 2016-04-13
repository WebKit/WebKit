/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#ifndef Heap_h
#define Heap_h

#include "BumpRange.h"
#include "Environment.h"
#include "LineMetadata.h"
#include "List.h"
#include "Mutex.h"
#include "Object.h"
#include "SegregatedFreeList.h"
#include "SmallChunk.h"
#include "SmallLine.h"
#include "SmallPage.h"
#include "VMHeap.h"
#include "Vector.h"
#include "XLargeMap.h"
#include <array>
#include <mutex>

namespace bmalloc {

class BeginTag;
class BumpAllocator;
class EndTag;

class Heap {
public:
    Heap(std::lock_guard<StaticMutex>&);
    
    Environment& environment() { return m_environment; }

    void allocateSmallBumpRanges(std::lock_guard<StaticMutex>&, size_t sizeClass, BumpAllocator&, BumpRangeCache&);
    void derefSmallLine(std::lock_guard<StaticMutex>&, Object);

    void* allocateLarge(std::lock_guard<StaticMutex>&, size_t);
    void* allocateLarge(std::lock_guard<StaticMutex>&, size_t alignment, size_t, size_t unalignedSize);
    void deallocateLarge(std::lock_guard<StaticMutex>&, void*);

    void* allocateXLarge(std::lock_guard<StaticMutex>&, size_t);
    void* allocateXLarge(std::lock_guard<StaticMutex>&, size_t alignment, size_t);
    void* tryAllocateXLarge(std::lock_guard<StaticMutex>&, size_t alignment, size_t);
    size_t xLargeSize(std::unique_lock<StaticMutex>&, void*);
    void shrinkXLarge(std::unique_lock<StaticMutex>&, const Range&, size_t newSize);
    void deallocateXLarge(std::unique_lock<StaticMutex>&, void*);

    void scavenge(std::unique_lock<StaticMutex>&, std::chrono::milliseconds sleepDuration);

private:
    ~Heap() = delete;
    
    void initializeLineMetadata();

    SmallPage* allocateSmallPage(std::lock_guard<StaticMutex>&, size_t sizeClass);

    void deallocateSmallLine(std::lock_guard<StaticMutex>&, Object);
    void deallocateLarge(std::lock_guard<StaticMutex>&, const LargeObject&);

    LargeObject& splitAndAllocate(LargeObject&, size_t);
    LargeObject& splitAndAllocate(LargeObject&, size_t, size_t);
    void mergeLarge(BeginTag*&, EndTag*&, Range&);
    void mergeLargeLeft(EndTag*&, BeginTag*&, Range&, bool& inVMHeap);
    void mergeLargeRight(EndTag*&, BeginTag*&, Range&, bool& inVMHeap);

    XLargeRange splitAndAllocate(XLargeRange&, size_t alignment, size_t);

    void concurrentScavenge();
    void scavengeSmallPages(std::unique_lock<StaticMutex>&, std::chrono::milliseconds);
    void scavengeLargeObjects(std::unique_lock<StaticMutex>&, std::chrono::milliseconds);
    void scavengeXLargeObjects(std::unique_lock<StaticMutex>&, std::chrono::milliseconds);

    std::array<std::array<LineMetadata, smallLineCount>, sizeClassCount> m_smallLineMetadata;

    std::array<List<SmallPage>, sizeClassCount> m_smallPagesWithFreeLines;

    List<SmallPage> m_smallPages;

    SegregatedFreeList m_largeObjects;
    
    XLargeMap m_xLargeMap;

    bool m_isAllocatingPages;
    AsyncTask<Heap, decltype(&Heap::concurrentScavenge)> m_scavenger;

    Environment m_environment;

    VMHeap m_vmHeap;
};

inline void Heap::derefSmallLine(std::lock_guard<StaticMutex>& lock, Object object)
{
    if (!object.line()->deref(lock))
        return;
    deallocateSmallLine(lock, object);
}

} // namespace bmalloc

#endif // Heap_h
