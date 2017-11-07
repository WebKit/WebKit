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

#include "IsoHeapImpl.h"
#include "IsoTLSDeallocatorEntry.h"

namespace bmalloc {

template<typename Config>
IsoHeapImpl<Config>::IsoHeapImpl()
    : lock(PerProcess<IsoTLSDeallocatorEntry<Config>>::get()->lock)
    , m_inlineDirectory(*this)
    , m_allocator(*this)
{
}

template<typename Config>
EligibilityResult<Config> IsoHeapImpl<Config>::takeFirstEligible()
{
    if (m_isInlineDirectoryEligible) {
        EligibilityResult<Config> result = m_inlineDirectory.takeFirstEligible();
        if (result.kind == EligibilityKind::Full)
            m_isInlineDirectoryEligible = false;
        else
            return result;
    }
    
    if (!m_firstEligibleDirectory) {
        // If nothing is eligible, it can only be because we have no directories. It wouldn't be the end
        // of the world if we broke this invariant. It would only mean that didBecomeEligible() would need
        // a null check.
        RELEASE_BASSERT(!m_headDirectory);
        RELEASE_BASSERT(!m_tailDirectory);
    }
    
    for (; m_firstEligibleDirectory; m_firstEligibleDirectory = m_firstEligibleDirectory->next) {
        EligibilityResult<Config> result = m_firstEligibleDirectory->payload.takeFirstEligible();
        if (result.kind != EligibilityKind::Full)
            return result;
    }
    
    auto* newDirectory = new IsoDirectoryPage<Config>(*this, m_numDirectoryPages++);
    if (m_headDirectory) {
        m_tailDirectory->next = newDirectory;
        m_tailDirectory = newDirectory;
    } else {
        RELEASE_BASSERT(!m_tailDirectory);
        m_headDirectory = newDirectory;
        m_tailDirectory = newDirectory;
    }
    m_firstEligibleDirectory = newDirectory;
    EligibilityResult<Config> result = newDirectory->payload.takeFirstEligible();
    RELEASE_BASSERT(result.kind != EligibilityKind::Full);
    return result;
}

template<typename Config>
void IsoHeapImpl<Config>::didBecomeEligible(IsoDirectory<Config, numPagesInInlineDirectory>* directory)
{
    RELEASE_BASSERT(directory == &m_inlineDirectory);
    m_isInlineDirectoryEligible = true;
}

template<typename Config>
void IsoHeapImpl<Config>::didBecomeEligible(IsoDirectory<Config, IsoDirectoryPage<Config>::numPages>* directory)
{
    RELEASE_BASSERT(m_firstEligibleDirectory);
    auto* directoryPage = IsoDirectoryPage<Config>::pageFor(directory);
    if (directoryPage->index() < m_firstEligibleDirectory->index())
        m_firstEligibleDirectory = directoryPage;
}

template<typename Config>
void IsoHeapImpl<Config>::scavenge(Vector<DeferredDecommit>& decommits)
{
    forEachDirectory(
        [&] (auto& directory) {
            directory.scavenge(decommits);
        });
}

template<typename Config>
unsigned IsoHeapImpl<Config>::allocatorOffset()
{
    return m_allocator.offset();
}

template<typename Config>
unsigned IsoHeapImpl<Config>::deallocatorOffset()
{
    return PerProcess<IsoTLSDeallocatorEntry<Config>>::get()->offset();
}

template<typename Config>
unsigned IsoHeapImpl<Config>::numLiveObjects()
{
    unsigned result = 0;
    forEachLiveObject(
        [&] (void*) {
            result++;
        });
    return result;
}

template<typename Config>
unsigned IsoHeapImpl<Config>::numCommittedPages()
{
    unsigned result = 0;
    forEachCommittedPage(
        [&] (IsoPage<Config>&) {
            result++;
        });
    return result;
}

template<typename Config>
template<typename Func>
void IsoHeapImpl<Config>::forEachDirectory(const Func& func)
{
    func(m_inlineDirectory);
    for (IsoDirectoryPage<Config>* page = m_headDirectory; page; page = page->next)
        func(page->payload);
}

template<typename Config>
template<typename Func>
void IsoHeapImpl<Config>::forEachCommittedPage(const Func& func)
{
    forEachDirectory(
        [&] (auto& directory) {
            directory.forEachCommittedPage(func);
        });
}

template<typename Config>
template<typename Func>
void IsoHeapImpl<Config>::forEachLiveObject(const Func& func)
{
    forEachCommittedPage(
        [&] (IsoPage<Config>& page) {
            page.forEachLiveObject(func);
        });
}

} // namespace bmalloc

