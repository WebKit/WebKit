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

#include "Bits.h"
#include "DeferredTrigger.h"
#include "FreeList.h"
#include "Mutex.h"
#include <climits>
#include <mutex>

namespace bmalloc {

class IsoHeapImplBase;
template<typename Config> class IsoDirectoryBase;
template<typename Config> class IsoHeapImpl;

class IsoPageBase {
public:    
    static constexpr size_t pageSize = 16384;

    explicit IsoPageBase(bool isShared)
        : m_isShared(isShared)
        , m_eligibilityHasBeenNoted(true)
        , m_isInUseForAllocation(false)
    {
    }

    static IsoPageBase* pageFor(void*);

    bool isShared() const { return m_isShared; }
    
protected:
    BEXPORT static void* allocatePageMemory();

    bool m_isShared : 1;
    bool m_eligibilityHasBeenNoted : 1;
    bool m_isInUseForAllocation : 1;
};

template<typename Config>
class IsoPage : public IsoPageBase {
public:
    static constexpr unsigned numObjects = pageSize / Config::objectSize;
    
    static_assert(numObjects, "IsoHeap size should allow at least one allocation per page");
    
    static IsoPage* tryCreate(IsoDirectoryBase<Config>& directory, unsigned index);
    
    // It's expected that you will only use this with placement new and direct destruction.
    IsoPage(IsoDirectoryBase<Config>& directory, unsigned index);
    
    static IsoPage* pageFor(void*);

    unsigned index() const { return m_index; }
    
    void free(const LockHolder&, void*);

    // Called after this page is already selected for allocation.
    FreeList startAllocating(const LockHolder&);
    
    // Called after the allocator picks another page to replace this one.
    void stopAllocating(const LockHolder&, FreeList);

    IsoDirectoryBase<Config>& directory() { return m_directory; }
    bool isInUseForAllocation() const { return m_isInUseForAllocation; }
    
    template<typename Func>
    void forEachLiveObject(const LockHolder&, const Func&);
    
    IsoHeapImpl<Config>& heap();
    
private:
    static constexpr unsigned indexOfFirstObject()
    {
        return (sizeof(IsoPage) + Config::objectSize - 1) / Config::objectSize;
    }
    
    // The possible states of a page are as follows. We mark these states by their corresponding
    // eligible, empty, and committed bits (respectively).
    //
    // 000 - Deallocated. It has no objects and its memory is not paged in.
    // 111 - Empty.
    // 101 - Eligible for allocation, meaning that there is at least one free object in the page.
    // 001 - Full.
    // 001 - Currently being used for allocation.
    //
    // Note that the last two states have identical representation in the directory, which is fine - in
    // both cases we are basically telling the directory that this page is off limits. But we keep track
    // of the distinction internally.
    
    // We manage the bitvector ourselves. This bitvector works in a special way to enable very fast
    // freeing.

    // This must have a trivial destructor.

    DeferredTrigger<IsoPageTrigger::Eligible> m_eligibilityTrigger;
    DeferredTrigger<IsoPageTrigger::Empty> m_emptyTrigger;
    uint8_t m_numNonEmptyWords { 0 };
    static_assert(bitsArrayLength(numObjects) <= UINT8_MAX);
    unsigned m_index { UINT_MAX };
    IsoDirectoryBase<Config>& m_directory;
    unsigned m_allocBits[bitsArrayLength(numObjects)];
};

} // namespace bmalloc

