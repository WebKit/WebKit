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

#ifndef Heap_h
#define Heap_h

#include "FixedVector.h"
#include "VMHeap.h"
#include "MediumLine.h"
#include "Mutex.h"
#include "SmallPage.h"
#include "MediumChunk.h"
#include "MediumPage.h"
#include "SegregatedFreeList.h"
#include "SmallChunk.h"
#include "SmallLine.h"
#include "Vector.h"
#include <array>
#include <mutex>

namespace bmalloc {

class BeginTag;
class EndTag;

class Heap {
public:
    Heap(std::lock_guard<Mutex>&);
    
    SmallLine* allocateSmallLine(std::lock_guard<Mutex>&);
    void deallocateSmallLine(SmallLine*);

    MediumLine* allocateMediumLine(std::lock_guard<Mutex>&);
    void deallocateMediumLine(MediumLine*);
    
    void* allocateLarge(std::lock_guard<Mutex>&, size_t);
    void deallocateLarge(std::lock_guard<Mutex>&, void*);

    void* allocateXLarge(std::lock_guard<Mutex>&, size_t);
    void deallocateXLarge(std::lock_guard<Mutex>&, void*);

private:
    ~Heap() = delete;

    SmallLine* allocateSmallLineSlowCase(std::lock_guard<Mutex>&);
    MediumLine* allocateMediumLineSlowCase(std::lock_guard<Mutex>&);

    void* allocateLarge(Range, size_t);
    Range allocateLargeChunk();

    void splitLarge(BeginTag*, size_t, EndTag*&, Range&);
    void mergeLarge(BeginTag*&, EndTag*&, Range&);
    void mergeLargeLeft(EndTag*&, BeginTag*&, Range&, bool& hasPhysicalPages);
    void mergeLargeRight(EndTag*&, BeginTag*&, Range&, bool& hasPhysicalPages);
    
    void concurrentScavenge();
    void scavengeSmallPages(std::unique_lock<Mutex>&);
    void scavengeMediumPages(std::unique_lock<Mutex>&);
    void scavengeLargeRanges(std::unique_lock<Mutex>&);

    Vector<SmallLine*> m_smallLines;
    Vector<MediumLine*> m_mediumLines;

    Vector<SmallPage*> m_smallPages;
    Vector<MediumPage*> m_mediumPages;

    SegregatedFreeList m_largeRanges;

    bool m_isAllocatingPages;

    VMHeap m_vmHeap;
    AsyncTask<Heap, decltype(&Heap::concurrentScavenge)> m_scavenger;
};

inline void Heap::deallocateSmallLine(SmallLine* line)
{
    SmallPage* page = SmallPage::get(line);
    if (page->deref()) {
        m_smallPages.push(page);
        m_scavenger.run();
        return;
    }
    m_smallLines.push(line);
}

inline SmallLine* Heap::allocateSmallLine(std::lock_guard<Mutex>& lock)
{
    while (m_smallLines.size()) {
        SmallLine* line = m_smallLines.pop();
        SmallPage* page = SmallPage::get(line);
        if (!page->refCount(lock)) // The line was promoted to the small pages list.
            continue;
        page->ref(lock);
        return line;
    }

    return allocateSmallLineSlowCase(lock);
}

inline void Heap::deallocateMediumLine(MediumLine* line)
{
    MediumPage* page = MediumPage::get(line);
    if (page->deref()) {
        m_mediumPages.push(page);
        m_scavenger.run();
        return;
    }
    m_mediumLines.push(line);
}

inline MediumLine* Heap::allocateMediumLine(std::lock_guard<Mutex>& lock)
{
    while (m_mediumLines.size()) {
        MediumLine* line = m_mediumLines.pop();
        MediumPage* page = MediumPage::get(line);
        if (!page->refCount(lock)) // The line was promoted to the medium pages list.
            continue;
        page->ref(lock);
        return line;
    }

    return allocateMediumLineSlowCase(lock);
}

} // namespace bmalloc

#endif // Heap_h
