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

#include "IsoHeapImpl.h"
#include "IsoTLSDeallocatorEntry.h"
#include "IsoSharedHeapInlines.h"
#include "IsoSharedPageInlines.h"

namespace bmalloc {

template<typename Config>
IsoHeapImpl<Config>::IsoHeapImpl()
    : lock(PerProcess<IsoTLSDeallocatorEntry<Config>>::get()->lock)
    , m_inlineDirectory(*this)
    , m_allocator(*this)
{
    addToAllIsoHeaps();
}

template<typename Config>
EligibilityResult<Config> IsoHeapImpl<Config>::takeFirstEligible()
{
    if (m_isInlineDirectoryEligibleOrDecommitted) {
        EligibilityResult<Config> result = m_inlineDirectory.takeFirstEligible();
        if (result.kind == EligibilityKind::Full)
            m_isInlineDirectoryEligibleOrDecommitted = false;
        else
            return result;
    }
    
    if (!m_firstEligibleOrDecommitedDirectory) {
        // If nothing is eligible, it can only be because we have no directories. It wouldn't be the end
        // of the world if we broke this invariant. It would only mean that didBecomeEligibleOrDecommited() would need
        // a null check.
        RELEASE_BASSERT(!m_headDirectory);
        RELEASE_BASSERT(!m_tailDirectory);
    }
    
    for (; m_firstEligibleOrDecommitedDirectory; m_firstEligibleOrDecommitedDirectory = m_firstEligibleOrDecommitedDirectory->next) {
        EligibilityResult<Config> result = m_firstEligibleOrDecommitedDirectory->payload.takeFirstEligible();
        if (result.kind != EligibilityKind::Full) {
            m_directoryHighWatermark = std::max(m_directoryHighWatermark, m_firstEligibleOrDecommitedDirectory->index());
            return result;
        }
    }
    
    auto* newDirectory = new IsoDirectoryPage<Config>(*this, m_nextDirectoryPageIndex++);
    if (m_headDirectory) {
        m_tailDirectory->next = newDirectory;
        m_tailDirectory = newDirectory;
    } else {
        RELEASE_BASSERT(!m_tailDirectory);
        m_headDirectory = newDirectory;
        m_tailDirectory = newDirectory;
    }
    m_directoryHighWatermark = newDirectory->index();
    m_firstEligibleOrDecommitedDirectory = newDirectory;
    EligibilityResult<Config> result = newDirectory->payload.takeFirstEligible();
    RELEASE_BASSERT(result.kind != EligibilityKind::Full);
    return result;
}

template<typename Config>
void IsoHeapImpl<Config>::didBecomeEligibleOrDecommited(IsoDirectory<Config, numPagesInInlineDirectory>* directory)
{
    RELEASE_BASSERT(directory == &m_inlineDirectory);
    m_isInlineDirectoryEligibleOrDecommitted = true;
}

template<typename Config>
void IsoHeapImpl<Config>::didBecomeEligibleOrDecommited(IsoDirectory<Config, IsoDirectoryPage<Config>::numPages>* directory)
{
    RELEASE_BASSERT(m_firstEligibleOrDecommitedDirectory);
    auto* directoryPage = IsoDirectoryPage<Config>::pageFor(directory);
    if (directoryPage->index() < m_firstEligibleOrDecommitedDirectory->index())
        m_firstEligibleOrDecommitedDirectory = directoryPage;
}

template<typename Config>
void IsoHeapImpl<Config>::scavenge(Vector<DeferredDecommit>& decommits)
{
    std::lock_guard<Mutex> locker(this->lock);
    forEachDirectory(
        [&] (auto& directory) {
            directory.scavenge(decommits);
        });
    m_directoryHighWatermark = 0;
}

template<typename Config>
size_t IsoHeapImpl<Config>::freeableMemory()
{
    return m_freeableMemory;
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
    for (unsigned index = 0; index < maxAllocationFromShared; ++index) {
        void* pointer = m_sharedCells[index];
        if (pointer && !(m_availableShared & (1U << index)))
            func(pointer);
    }
}

template<typename Config>
size_t IsoHeapImpl<Config>::footprint()
{
#if ENABLE_PHYSICAL_PAGE_MAP
    RELEASE_BASSERT(m_footprint == m_physicalPageMap.footprint());
#endif
    return m_footprint;
}

template<typename Config>
void IsoHeapImpl<Config>::didCommit(void* ptr, size_t bytes)
{
    BUNUSED_PARAM(ptr);
    m_footprint += bytes;
#if ENABLE_PHYSICAL_PAGE_MAP
    m_physicalPageMap.commit(ptr, bytes);
#endif
}

template<typename Config>
void IsoHeapImpl<Config>::didDecommit(void* ptr, size_t bytes)
{
    BUNUSED_PARAM(ptr);
    m_footprint -= bytes;
#if ENABLE_PHYSICAL_PAGE_MAP
    m_physicalPageMap.decommit(ptr, bytes);
#endif
}

template<typename Config>
void IsoHeapImpl<Config>::isNowFreeable(void* ptr, size_t bytes)
{
    BUNUSED_PARAM(ptr);
    m_freeableMemory += bytes;
}

template<typename Config>
void IsoHeapImpl<Config>::isNoLongerFreeable(void* ptr, size_t bytes)
{
    BUNUSED_PARAM(ptr);
    m_freeableMemory -= bytes;
}

template<typename Config>
AllocationMode IsoHeapImpl<Config>::updateAllocationMode()
{
    auto getNewAllocationMode = [&] {
        // Exhaust shared free cells, which means we should start activating the fast allocation mode for this type.
        if (!m_availableShared) {
            m_lastSlowPathTime = std::chrono::steady_clock::now();
            return AllocationMode::Fast;
        }

        switch (m_allocationMode) {
        case AllocationMode::Shared:
            // Currently in the shared allocation mode. Until we exhaust shared free cells, continue using the shared allocation mode.
            // But if we allocate so many shared cells within very short period, we should use the fast allocation mode instead.
            // This avoids the following pathological case.
            //
            //     for (int i = 0; i < 1e6; ++i) {
            //         auto* ptr = allocate();
            //         ...
            //         free(ptr);
            //     }
            if (m_numberOfAllocationsFromSharedInOneCycle <= IsoPage<Config>::numObjects)
                return AllocationMode::Shared;
            BFALLTHROUGH;

        case AllocationMode::Fast: {
            // The allocation pattern may change. We should check the allocation rate and decide which mode is more appropriate.
            // If we don't go to the allocation slow path during ~1 seconds, we think the allocation becomes quiescent state.
            auto now = std::chrono::steady_clock::now();
            if ((now - m_lastSlowPathTime) < std::chrono::seconds(1)) {
                m_lastSlowPathTime = now;
                return AllocationMode::Fast;
            }

            m_numberOfAllocationsFromSharedInOneCycle = 0;
            m_lastSlowPathTime = now;
            return AllocationMode::Shared;
        }

        case AllocationMode::Init:
            m_lastSlowPathTime = std::chrono::steady_clock::now();
            return AllocationMode::Shared;
        }

        return AllocationMode::Shared;
    };
    AllocationMode allocationMode = getNewAllocationMode();
    m_allocationMode = allocationMode;
    return allocationMode;
}

template<typename Config>
void* IsoHeapImpl<Config>::allocateFromShared(const std::lock_guard<Mutex>&, bool abortOnFailure)
{
    static constexpr bool verbose = false;

    unsigned indexPlusOne = __builtin_ffs(m_availableShared);
    BASSERT(indexPlusOne);
    unsigned index = indexPlusOne - 1;
    void* result = m_sharedCells[index];
    if (result) {
        if (verbose)
            fprintf(stderr, "%p: allocated %p from shared again of size %u\n", this, result, Config::objectSize);
    } else {
        constexpr unsigned objectSizeWithHeapImplPointer = Config::objectSize + sizeof(uint8_t);
        result = IsoSharedHeap::get()->allocateNew<objectSizeWithHeapImplPointer>(abortOnFailure);
        if (!result)
            return nullptr;
        if (verbose)
            fprintf(stderr, "%p: allocated %p from shared of size %u\n", this, result, Config::objectSize);
        BASSERT(index < IsoHeapImplBase::maxAllocationFromShared);
        *indexSlotFor<Config>(result) = index;
        m_sharedCells[index] = result;
    }
    BASSERT(result);
    m_availableShared &= ~(1U << index);
    ++m_numberOfAllocationsFromSharedInOneCycle;
    return result;
}

} // namespace bmalloc

