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

#include "Bits.h"
#include "EligibilityResult.h"
#include "IsoPage.h"
#include "Vector.h"

namespace bmalloc {

template<typename Config> class IsoHeapImpl;

class IsoDirectoryBaseBase {
public:
    IsoDirectoryBaseBase() { }
    virtual ~IsoDirectoryBaseBase() { }

    virtual void didDecommit(unsigned index) = 0;
};

template<typename Config>
class IsoDirectoryBase : public IsoDirectoryBaseBase {
public:
    IsoDirectoryBase(IsoHeapImpl<Config>&);
    
    IsoHeapImpl<Config>& heap() { return m_heap; }
    
    virtual void didBecome(IsoPage<Config>*, IsoPageTrigger) = 0;
    
protected:
    IsoHeapImpl<Config>& m_heap;
};

template<typename Config, unsigned passedNumPages>
class IsoDirectory : public IsoDirectoryBase<Config> {
public:
    static constexpr unsigned numPages = passedNumPages;
    
    IsoDirectory(IsoHeapImpl<Config>&);
    
    // Find the first page that is eligible for allocation and return it. May return null if there is no
    // such thing. May allocate a new page if we have an uncommitted page.
    EligibilityResult<Config> takeFirstEligible();
    
    void didBecome(IsoPage<Config>*, IsoPageTrigger) override;
    
    // This gets called from a bulk decommit function in the Scavenger, so no locks are held. This function
    // needs to get the heap lock.
    void didDecommit(unsigned index) override;
    
    // Iterate over all empty and committed pages, and put them into the vector. This also records the
    // pages as being decommitted. It's the caller's job to do the actual decommitting.
    void scavenge(Vector<DeferredDecommit>&);

    template<typename Func>
    void forEachCommittedPage(const Func&);
    
private:
    void scavengePage(size_t, Vector<DeferredDecommit>&);

    // NOTE: I suppose that this could be two bitvectors. But from working on the GC, I found that the
    // number of bitvectors does not matter as much as whether or not they make intuitive sense.
    Bits<numPages> m_eligible;
    Bits<numPages> m_empty;
    Bits<numPages> m_committed;
    std::array<IsoPage<Config>*, numPages> m_pages;
    unsigned m_firstEligibleOrDecommitted { 0 };
};

} // namespace bmalloc

