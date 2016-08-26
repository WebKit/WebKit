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

#include "AsyncTask.h"
#include "BumpRange.h"
#include "Environment.h"
#include "LargeMap.h"
#include "LineMetadata.h"
#include "List.h"
#include "Map.h"
#include "Mutex.h"
#include "Object.h"
#include "SmallLine.h"
#include "SmallPage.h"
#include "VMHeap.h"
#include "Vector.h"
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

    void* allocateLarge(std::lock_guard<StaticMutex>&, size_t alignment, size_t);
    void* tryAllocateLarge(std::lock_guard<StaticMutex>&, size_t alignment, size_t);
    void deallocateLarge(std::lock_guard<StaticMutex>&, void*);

    bool isLarge(std::lock_guard<StaticMutex>&, void*);
    size_t largeSize(std::lock_guard<StaticMutex>&, void*);
    void shrinkLarge(std::lock_guard<StaticMutex>&, const Range&, size_t);

    void scavenge(std::unique_lock<StaticMutex>&, std::chrono::milliseconds sleepDuration);

private:
    struct LargeObjectHash {
        static unsigned hash(void* key)
        {
            return static_cast<unsigned>(
                reinterpret_cast<uintptr_t>(key) / smallMax);
        }
    };

    ~Heap() = delete;
    
    void initializeLineMetadata();
    void initializePageMetadata();

    void allocateSmallBumpRangesByMetadata(std::lock_guard<StaticMutex>&,
        size_t sizeClass, BumpAllocator&, BumpRangeCache&);
    void allocateSmallBumpRangesByObject(std::lock_guard<StaticMutex>&,
        size_t sizeClass, BumpAllocator&, BumpRangeCache&);

    SmallPage* allocateSmallPage(std::lock_guard<StaticMutex>&, size_t sizeClass);

    void deallocateSmallLine(std::lock_guard<StaticMutex>&, Object);

    void mergeLarge(BeginTag*&, EndTag*&, Range&);
    void mergeLargeLeft(EndTag*&, BeginTag*&, Range&, bool& inVMHeap);
    void mergeLargeRight(EndTag*&, BeginTag*&, Range&, bool& inVMHeap);

    LargeRange splitAndAllocate(LargeRange&, size_t alignment, size_t);

    void concurrentScavenge();
    void scavengeSmallPages(std::unique_lock<StaticMutex>&, std::chrono::milliseconds);
    void scavengeLargeObjects(std::unique_lock<StaticMutex>&, std::chrono::milliseconds);

    size_t m_vmPageSizePhysical;
    Vector<LineMetadata> m_smallLineMetadata;
    std::array<size_t, sizeClassCount> m_pageClasses;

    std::array<List<SmallPage>, sizeClassCount> m_smallPagesWithFreeLines;
    std::array<List<SmallPage>, pageClassCount> m_smallPages;

    Map<void*, size_t, LargeObjectHash> m_largeAllocated;
    LargeMap m_largeFree;

    Map<Chunk*, ObjectType, ChunkHash> m_objectTypes;

    bool m_isAllocatingPages;
    AsyncTask<Heap, decltype(&Heap::concurrentScavenge)> m_scavenger;

    Environment m_environment;

    VMHeap m_vmHeap;
};

inline void Heap::allocateSmallBumpRanges(
    std::lock_guard<StaticMutex>& lock, size_t sizeClass,
    BumpAllocator& allocator, BumpRangeCache& rangeCache)
{
    if (sizeClass < bmalloc::sizeClass(smallLineSize))
        return allocateSmallBumpRangesByMetadata(lock, sizeClass, allocator, rangeCache);
    return allocateSmallBumpRangesByObject(lock, sizeClass, allocator, rangeCache);
}

inline void Heap::derefSmallLine(std::lock_guard<StaticMutex>& lock, Object object)
{
    if (!object.line()->deref(lock))
        return;
    deallocateSmallLine(lock, object);
}

} // namespace bmalloc

#endif // Heap_h
