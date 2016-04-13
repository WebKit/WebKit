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

#ifndef VMHeap_h
#define VMHeap_h

#include "AsyncTask.h"
#include "Chunk.h"
#include "FixedVector.h"
#include "LargeObject.h"
#include "Range.h"
#include "SegregatedFreeList.h"
#include "VMState.h"
#include "Vector.h"
#if BOS(DARWIN)
#include "Zone.h"
#endif

namespace bmalloc {

class BeginTag;
class EndTag;
class Heap;

class VMHeap {
public:
    VMHeap();
    
    SmallPage* allocateSmallPage(std::lock_guard<StaticMutex>&, size_t);
    void deallocateSmallPage(std::unique_lock<StaticMutex>&, size_t, SmallPage*);

    LargeObject allocateLargeObject(std::lock_guard<StaticMutex>&, size_t);
    LargeObject allocateLargeObject(std::lock_guard<StaticMutex>&, size_t, size_t, size_t);

    void deallocateLargeObject(std::unique_lock<StaticMutex>&, LargeObject);
    
private:
    LargeObject allocateChunk(std::lock_guard<StaticMutex>&);
    void allocateSmallChunk(std::lock_guard<StaticMutex>&, size_t);

    std::array<List<SmallPage>, pageClassCount> m_smallPages;
    SegregatedFreeList m_largeObjects;

#if BOS(DARWIN)
    Zone m_zone;
#endif
};

inline SmallPage* VMHeap::allocateSmallPage(std::lock_guard<StaticMutex>& lock, size_t pageClass)
{
    if (m_smallPages[pageClass].isEmpty())
        allocateSmallChunk(lock, pageClass);

    SmallPage* page = m_smallPages[pageClass].pop();
    vmAllocatePhysicalPagesSloppy(page->begin()->begin(), pageSize(pageClass));
    return page;
}

inline void VMHeap::deallocateSmallPage(std::unique_lock<StaticMutex>& lock, size_t pageClass, SmallPage* page)
{
    lock.unlock();
    vmDeallocatePhysicalPagesSloppy(page->begin()->begin(), pageSize(pageClass));
    lock.lock();
    
    m_smallPages[pageClass].push(page);
}

inline LargeObject VMHeap::allocateLargeObject(std::lock_guard<StaticMutex>& lock, size_t size)
{
    if (LargeObject largeObject = m_largeObjects.take(size))
        return largeObject;

    BASSERT(size <= largeMax);
    return allocateChunk(lock);
}

inline LargeObject VMHeap::allocateLargeObject(std::lock_guard<StaticMutex>& lock, size_t alignment, size_t size, size_t unalignedSize)
{
    if (LargeObject largeObject = m_largeObjects.take(alignment, size, unalignedSize))
        return largeObject;

    BASSERT(unalignedSize <= largeMax);
    return allocateChunk(lock);
}

inline void VMHeap::deallocateLargeObject(std::unique_lock<StaticMutex>& lock, LargeObject largeObject)
{
    // Multiple threads might scavenge concurrently, meaning that new merging opportunities
    // become visible after we reacquire the lock. Therefore we loop.
    do {
        largeObject = largeObject.merge();

        // Temporarily mark this object as allocated to prevent clients from merging
        // with it or allocating it while we're messing with its physical pages.
        largeObject.setFree(false);

        lock.unlock();
        vmDeallocatePhysicalPagesSloppy(largeObject.begin(), largeObject.size());
        lock.lock();

        largeObject.setFree(true);
    } while (largeObject.prevCanMerge() || largeObject.nextCanMerge());

    largeObject.setVMState(VMState::Virtual);
    m_largeObjects.insert(largeObject);
}

} // namespace bmalloc

#endif // VMHeap_h
