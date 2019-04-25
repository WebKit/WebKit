/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "IsoPage.h"
#include "StdLibExtras.h"
#include "VMAllocate.h"

namespace bmalloc {

// IsoSharedPage never becomes empty state again after we allocate some cells from IsoSharedPage. This makes IsoSharedPage management super simple.
// This is because empty IsoSharedPage is still split into various different objects that should keep some part of virtual memory region dedicated.
// We cannot set up bump allocation for such a page. Not freeing IsoSharedPages are OK since IsoSharedPage is only used for the lower tier of IsoHeap.
template<typename Config, typename Type>
void IsoSharedPage::free(const std::lock_guard<Mutex>&, api::IsoHeap<Type>& handle, void* ptr)
{
    auto& heapImpl = handle.impl();
    uint8_t index = *indexSlotFor<Config>(ptr) & IsoHeapImplBase::maxAllocationFromSharedMask;
    // IsoDeallocator::deallocate is called from delete operator. This is dispatched by vtable if virtual destructor exists.
    // If vptr is replaced to the other vptr, we may accidentally chain this pointer to the incorrect HeapImplBase, which totally breaks the IsoHeap's goal.
    // To harden that, we validate that this pointer is actually allocated for a specific HeapImplBase here by checking whether this pointer is listed in HeapImplBase's shared cells.
    RELEASE_BASSERT(heapImpl.m_sharedCells[index] == ptr);
    heapImpl.m_availableShared |= (1U << index);
}

inline VariadicBumpAllocator IsoSharedPage::startAllocating()
{
    static constexpr bool verbose = false;

    if (verbose) {
        fprintf(stderr, "%p: starting shared allocation.\n", this);
        fprintf(stderr, "%p: preparing to shared bump.\n", this);
    }

    char* payloadEnd = reinterpret_cast<char*>(this) + IsoSharedPage::pageSize;
    unsigned remaining = static_cast<unsigned>(roundDownToMultipleOf<alignmentForIsoSharedAllocation>(static_cast<uintptr_t>(IsoSharedPage::pageSize - sizeof(IsoSharedPage))));

    return VariadicBumpAllocator(payloadEnd, remaining);
}

inline void IsoSharedPage::stopAllocating()
{
    static constexpr bool verbose = false;

    if (verbose)
        fprintf(stderr, "%p: stopping shared allocation.\n", this);
}

} // namespace bmalloc

