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

#ifndef VMHeap_h
#define VMHeap_h

#include "AsyncTask.h"
#include "FixedVector.h"
#include "LargeChunk.h"
#include "MediumChunk.h"
#include "Range.h"
#include "SegregatedFreeList.h"
#include "SmallChunk.h"
#include "Vector.h"
#include "XSmallChunk.h"

namespace bmalloc {

class BeginTag;
class EndTag;
class Heap;

class VMHeap {
public:
    VMHeap();

    XSmallPage* allocateXSmallPage();
    SmallPage* allocateSmallPage();
    MediumPage* allocateMediumPage();
    Range allocateLargeRange(size_t);

    void deallocateXSmallPage(std::unique_lock<StaticMutex>&, XSmallPage*);
    void deallocateSmallPage(std::unique_lock<StaticMutex>&, SmallPage*);
    void deallocateMediumPage(std::unique_lock<StaticMutex>&, MediumPage*);
    void deallocateLargeRange(std::unique_lock<StaticMutex>&, Range);

private:
    void allocateXSmallChunk();
    void allocateSmallChunk();
    void allocateMediumChunk();
    Range allocateLargeChunk();
    
    Vector<XSmallPage*> m_xSmallPages;
    Vector<SmallPage*> m_smallPages;
    Vector<MediumPage*> m_mediumPages;
    SegregatedFreeList m_largeRanges;
};

inline XSmallPage* VMHeap::allocateXSmallPage()
{
    if (!m_xSmallPages.size())
        allocateXSmallChunk();

    return m_xSmallPages.pop();
}

inline SmallPage* VMHeap::allocateSmallPage()
{
    if (!m_smallPages.size())
        allocateSmallChunk();

    return m_smallPages.pop();
}

inline MediumPage* VMHeap::allocateMediumPage()
{
    if (!m_mediumPages.size())
        allocateMediumChunk();

    return m_mediumPages.pop();
}

inline Range VMHeap::allocateLargeRange(size_t size)
{
    Range range = m_largeRanges.take(size);
    if (!range)
        range = allocateLargeChunk();
    return range;
}

inline void VMHeap::deallocateXSmallPage(std::unique_lock<StaticMutex>& lock, XSmallPage* page)
{
    lock.unlock();
    vmDeallocatePhysicalPages(page->begin()->begin(), vmPageSize);
    lock.lock();
    
    m_xSmallPages.push(page);
}

inline void VMHeap::deallocateSmallPage(std::unique_lock<StaticMutex>& lock, SmallPage* page)
{
    lock.unlock();
    vmDeallocatePhysicalPages(page->begin()->begin(), vmPageSize);
    lock.lock();
    
    m_smallPages.push(page);
}

inline void VMHeap::deallocateMediumPage(std::unique_lock<StaticMutex>& lock, MediumPage* page)
{
    lock.unlock();
    vmDeallocatePhysicalPages(page->begin()->begin(), vmPageSize);
    lock.lock();
    
    m_mediumPages.push(page);
}

inline void VMHeap::deallocateLargeRange(std::unique_lock<StaticMutex>& lock, Range range)
{
    BeginTag* beginTag = LargeChunk::beginTag(range.begin());
    EndTag* endTag = LargeChunk::endTag(range.begin(), range.size());
    
    // Temporarily mark this range as allocated to prevent clients from merging
    // with it and then reallocating it while we're messing with its physical pages.
    beginTag->setFree(false);
    endTag->setFree(false);

    lock.unlock();
    vmDeallocatePhysicalPagesSloppy(range.begin(), range.size());
    lock.lock();

    beginTag->setFree(true);
    endTag->setFree(true);

    beginTag->setHasPhysicalPages(false);
    endTag->setHasPhysicalPages(false);

    m_largeRanges.insert(range);
}

} // namespace bmalloc

#endif // VMHeap_h
