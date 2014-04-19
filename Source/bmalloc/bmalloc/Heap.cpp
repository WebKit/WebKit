/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "BoundaryTagInlines.h"
#include "Heap.h"
#include "LargeChunk.h"
#include "Line.h"
#include "MediumChunk.h"
#include "Page.h"
#include "PerProcess.h"
#include "SmallChunk.h"
#include "XLargeChunk.h"
#include <thread>

namespace bmalloc {

static inline void sleep(std::unique_lock<StaticMutex>& lock, std::chrono::milliseconds duration)
{
    if (duration == std::chrono::milliseconds(0))
        return;
    
    lock.unlock();
    std::this_thread::sleep_for(duration);
    lock.lock();
}

Heap::Heap(std::lock_guard<StaticMutex>&)
    : m_isAllocatingPages(false)
    , m_scavenger(*this, &Heap::concurrentScavenge)
{
}

void Heap::concurrentScavenge()
{
    std::unique_lock<StaticMutex> lock(PerProcess<Heap>::mutex());
    scavenge(lock, scavengeSleepDuration);
}
    
void Heap::scavenge(std::unique_lock<StaticMutex>& lock, std::chrono::milliseconds sleepDuration)
{
    scavengeSmallPages(lock, sleepDuration);
    scavengeMediumPages(lock, sleepDuration);
    scavengeLargeRanges(lock, sleepDuration);

    sleep(lock, sleepDuration);
}

void Heap::scavengeSmallPages(std::unique_lock<StaticMutex>& lock, std::chrono::milliseconds sleepDuration)
{
    while (1) {
        if (m_isAllocatingPages) {
            m_isAllocatingPages = false;

            sleep(lock, sleepDuration);
            continue;
        }

        if (!m_smallPages.size())
            return;
        m_vmHeap.deallocateSmallPage(lock, m_smallPages.pop());
    }
}

void Heap::scavengeMediumPages(std::unique_lock<StaticMutex>& lock, std::chrono::milliseconds sleepDuration)
{
    while (1) {
        if (m_isAllocatingPages) {
            m_isAllocatingPages = false;

            sleep(lock, sleepDuration);
            continue;
        }

        if (!m_mediumPages.size())
            return;
        m_vmHeap.deallocateMediumPage(lock, m_mediumPages.pop());
    }
}

void Heap::scavengeLargeRanges(std::unique_lock<StaticMutex>& lock, std::chrono::milliseconds sleepDuration)
{
    while (1) {
        if (m_isAllocatingPages) {
            m_isAllocatingPages = false;

            sleep(lock, sleepDuration);
            continue;
        }

        Range range = m_largeRanges.takeGreedy(vmPageSize);
        if (!range)
            return;
        m_vmHeap.deallocateLargeRange(lock, range);
    }
}

SmallLine* Heap::allocateSmallLineSlowCase(std::lock_guard<StaticMutex>& lock)
{
    m_isAllocatingPages = true;

    SmallPage* page = [this]() {
        if (m_smallPages.size())
            return m_smallPages.pop();
        
        SmallPage* page = m_vmHeap.allocateSmallPage();
        vmAllocatePhysicalPages(page->begin()->begin(), vmPageSize);
        return page;
    }();

    SmallLine* line = page->begin();
    for (auto it = line + 1; it != page->end(); ++it)
        m_smallLines.push(it);

    page->ref(lock);
    return line;
}

MediumLine* Heap::allocateMediumLineSlowCase(std::lock_guard<StaticMutex>& lock)
{
    m_isAllocatingPages = true;

    MediumPage* page = [this]() {
        if (m_mediumPages.size())
            return m_mediumPages.pop();
        
        MediumPage* page = m_vmHeap.allocateMediumPage();
        vmAllocatePhysicalPages(page->begin()->begin(), vmPageSize);
        return page;
    }();

    MediumLine* line = page->begin();
    for (auto it = line + 1; it != page->end(); ++it)
        m_mediumLines.push(it);

    page->ref(lock);
    return line;
}

void* Heap::allocateXLarge(std::lock_guard<StaticMutex>&, size_t size)
{
    XLargeChunk* chunk = XLargeChunk::create(size);

    BeginTag* beginTag = LargeChunk::beginTag(chunk->begin());
    beginTag->setXLarge();
    beginTag->setFree(false);
    beginTag->setHasPhysicalPages(true);
    
    return chunk->begin();
}

void Heap::deallocateXLarge(std::lock_guard<StaticMutex>&, void* object)
{
    XLargeChunk* chunk = XLargeChunk::get(object);
    XLargeChunk::destroy(chunk);
}

void* Heap::allocateLarge(std::lock_guard<StaticMutex>&, size_t size)
{
    BASSERT(size <= largeMax);
    BASSERT(size >= largeMin);
    
    m_isAllocatingPages = true;

    Range range = m_largeRanges.take(size);
    if (!range)
        range = m_vmHeap.allocateLargeRange(size);
    
    Range leftover;
    bool hasPhysicalPages;
    BoundaryTag::allocate(size, range, leftover, hasPhysicalPages);

    if (!!leftover)
        m_largeRanges.insert(leftover);
    
    if (!hasPhysicalPages)
        vmAllocatePhysicalPagesSloppy(range.begin(), range.size());

    return range.begin();
}

void Heap::deallocateLarge(std::lock_guard<StaticMutex>&, void* object)
{
    Range range = BoundaryTag::deallocate(object);
    m_largeRanges.insert(range);
    m_scavenger.run();
}

} // namespace bmalloc
