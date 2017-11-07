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

#pragma once

#include "BMalloced.h"
#include "IsoDirectoryPage.h"
#include "IsoTLSAllocatorEntry.h"

namespace bmalloc {

class AllIsoHeaps;

class BEXPORT IsoHeapImplBase {
    MAKE_BMALLOCED;
public:
    virtual ~IsoHeapImplBase();
    
    virtual void scavenge(Vector<DeferredDecommit>&) = 0;
    
    void scavengeNow();
    static void finishScavenging(Vector<DeferredDecommit>&);
    
protected:
    IsoHeapImplBase();

private:
    friend class AllIsoHeaps;
    
    IsoHeapImplBase* m_next { nullptr };
};

template<typename Config>
class IsoHeapImpl : public IsoHeapImplBase {
    // Pick a size that makes us most efficiently use the bitvectors.
    static constexpr unsigned numPagesInInlineDirectory = 32;
    
public:
    IsoHeapImpl();
    
    EligibilityResult<Config> takeFirstEligible();
    
    // Callbacks from directory.
    void didBecomeEligible(IsoDirectory<Config, numPagesInInlineDirectory>*);
    void didBecomeEligible(IsoDirectory<Config, IsoDirectoryPage<Config>::numPages>*);
    
    void scavenge(Vector<DeferredDecommit>&) override;
    
    unsigned allocatorOffset();
    unsigned deallocatorOffset();

    // White-box testing functions.
    unsigned numLiveObjects();
    unsigned numCommittedPages();
    
    template<typename Func>
    void forEachDirectory(const Func&);
    
    template<typename Func>
    void forEachCommittedPage(const Func&);
    
    // This is only accurate when all threads are scavenged. Otherwise it will overestimate.
    template<typename Func>
    void forEachLiveObject(const Func&);
    
    // It's almost always the caller's responsibility to grab the lock. This lock comes from the
    // PerProcess<IsoTLSDeallocatorEntry<Config>>::get()->lock. That's pretty weird, and we don't
    // try to disguise the fact that it's weird. We only do that because heaps in the same size class
    // share the same deallocator log, so it makes sense for them to also share the same lock to
    // amortize lock acquisition costs.
    Mutex& lock;

private:
    IsoDirectory<Config, numPagesInInlineDirectory> m_inlineDirectory;
    IsoDirectoryPage<Config>* m_headDirectory { nullptr };
    IsoDirectoryPage<Config>* m_tailDirectory { nullptr };
    unsigned m_numDirectoryPages { 0 };
    
    bool m_isInlineDirectoryEligible { true };
    IsoDirectoryPage<Config>* m_firstEligibleDirectory { nullptr };
    
    IsoTLSAllocatorEntry<Config> m_allocator;
};

} // namespace bmalloc


