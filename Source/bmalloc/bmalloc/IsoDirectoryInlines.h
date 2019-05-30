/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "IsoDirectory.h"

namespace bmalloc {

template<typename Config>
IsoDirectoryBase<Config>::IsoDirectoryBase(IsoHeapImpl<Config>& heap)
    : m_heap(heap)
{
}

template<typename Config, unsigned passedNumPages>
IsoDirectory<Config, passedNumPages>::IsoDirectory(IsoHeapImpl<Config>& heap)
    : IsoDirectoryBase<Config>(heap)
{
    for (unsigned i = numPages; i--;)
        m_pages[i] = nullptr;
}

template<typename Config, unsigned passedNumPages>
EligibilityResult<Config> IsoDirectory<Config, passedNumPages>::takeFirstEligible()
{
    unsigned pageIndex = (m_eligible | ~m_committed).findBit(m_firstEligibleOrDecommitted, true);
    m_firstEligibleOrDecommitted = pageIndex;
    BASSERT((m_eligible | ~m_committed).findBit(0, true) == pageIndex);
    if (pageIndex >= numPages)
        return EligibilityKind::Full;

    Scavenger& scavenger = *Scavenger::get();
    scavenger.didStartGrowing();
    
    IsoPage<Config>* page = m_pages[pageIndex];
    
    if (!m_committed[pageIndex]) {
        scavenger.scheduleIfUnderMemoryPressure(IsoPageBase::pageSize);
        
        // It could be that we haven't even allocated a page yet. Do that now!
        if (!page) {
            page = IsoPage<Config>::tryCreate(*this, pageIndex);
            if (!page)
                return EligibilityKind::OutOfMemory;
            m_pages[pageIndex] = page;
        } else {
            // This means that we have a page that we previously allocated and that page just needs to be
            // committed.
            vmAllocatePhysicalPages(page, IsoPageBase::pageSize);
            new (page) IsoPage<Config>(*this, pageIndex);
        }

        m_committed[pageIndex] = true;
        this->m_heap.didCommit(page, IsoPageBase::pageSize);
    } else {
        if (m_empty[pageIndex])
            this->m_heap.isNoLongerFreeable(page, IsoPageBase::pageSize);
    }
    
    RELEASE_BASSERT(page);
    
    // Make the page non-empty and non-eligible.
    m_eligible[pageIndex] = false;
    m_empty[pageIndex] = false;
    return page;
}

template<typename Config, unsigned passedNumPages>
void IsoDirectory<Config, passedNumPages>::didBecome(IsoPage<Config>* page, IsoPageTrigger trigger)
{
    static constexpr bool verbose = false;
    unsigned pageIndex = page->index();
    switch (trigger) {
    case IsoPageTrigger::Eligible:
        if (verbose)
            fprintf(stderr, "%p: %p did become eligible.\n", this, page);
        m_eligible[pageIndex] = true;
        m_firstEligibleOrDecommitted = std::min(m_firstEligibleOrDecommitted, pageIndex);
        this->m_heap.didBecomeEligibleOrDecommited(this);
        return;
    case IsoPageTrigger::Empty:
        if (verbose)
            fprintf(stderr, "%p: %p did become empty.\n", this, page);
        BASSERT(!!m_committed[pageIndex]);
        this->m_heap.isNowFreeable(page, IsoPageBase::pageSize);
        m_empty[pageIndex] = true;
        Scavenger::get()->schedule(IsoPageBase::pageSize);
        return;
    }
    BCRASH();
}

template<typename Config, unsigned passedNumPages>
void IsoDirectory<Config, passedNumPages>::didDecommit(unsigned index)
{
    // FIXME: We could do this without grabbing the lock. I just doubt that it matters. This is not going
    // to be a frequently executed path, in the sense that decommitting perf will be dominated by the
    // syscall itself (which has to do many hard things).
    std::lock_guard<Mutex> locker(this->m_heap.lock);
    BASSERT(!!m_committed[index]);
    this->m_heap.isNoLongerFreeable(m_pages[index], IsoPageBase::pageSize);
    m_committed[index] = false;
    m_firstEligibleOrDecommitted = std::min(m_firstEligibleOrDecommitted, index);
    this->m_heap.didBecomeEligibleOrDecommited(this);
    this->m_heap.didDecommit(m_pages[index], IsoPageBase::pageSize);
}

template<typename Config, unsigned passedNumPages>
void IsoDirectory<Config, passedNumPages>::scavengePage(size_t index, Vector<DeferredDecommit>& decommits)
{
    // Make sure that this page is now off limits.
    m_empty[index] = false;
    m_eligible[index] = false;
    decommits.push(DeferredDecommit(this, m_pages[index], index));
}

template<typename Config, unsigned passedNumPages>
void IsoDirectory<Config, passedNumPages>::scavenge(Vector<DeferredDecommit>& decommits)
{
    (m_empty & m_committed).forEachSetBit(
        [&] (size_t index) {
            scavengePage(index, decommits);
        });
}

template<typename Config, unsigned passedNumPages>
template<typename Func>
void IsoDirectory<Config, passedNumPages>::forEachCommittedPage(const Func& func)
{
    m_committed.forEachSetBit(
        [&] (size_t index) {
            func(*m_pages[index]);
        });
}
    
} // namespace bmalloc

