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

#include "IsoSharedHeap.h"

#include "IsoSharedPage.h"
#include "StdLibExtras.h"

namespace bmalloc {

template<unsigned objectSize, typename Func>
void* VariadicBumpAllocator::allocate(const Func& slowPath)
{
    unsigned remaining = m_remaining;
    if (!__builtin_usub_overflow(remaining, objectSize, &remaining)) {
        m_remaining = remaining;
        return m_payloadEnd - remaining - objectSize;
    }
    return slowPath();
}

inline constexpr unsigned computeObjectSizeForSharedCell(unsigned objectSize)
{
    return roundUpToMultipleOf<alignmentForIsoSharedAllocation>(static_cast<uintptr_t>(objectSize));
}

template<unsigned passedObjectSize>
void* IsoSharedHeap::allocateNew(bool abortOnFailure)
{
    std::lock_guard<Mutex> locker(mutex());
    constexpr unsigned objectSize = computeObjectSizeForSharedCell(passedObjectSize);
    return m_allocator.template allocate<objectSize>(
        [&] () -> void* {
            return allocateSlow<passedObjectSize>(abortOnFailure);
        });
}

template<unsigned passedObjectSize>
BNO_INLINE void* IsoSharedHeap::allocateSlow(bool abortOnFailure)
{
    Scavenger& scavenger = *Scavenger::get();
    scavenger.didStartGrowing();
    scavenger.scheduleIfUnderMemoryPressure(IsoSharedPage::pageSize);

    IsoSharedPage* page = IsoSharedPage::tryCreate();
    if (!page) {
        RELEASE_BASSERT(!abortOnFailure);
        return nullptr;
    }

    if (m_currentPage)
        m_currentPage->stopAllocating();

    m_currentPage = page;
    m_allocator = m_currentPage->startAllocating();

    constexpr unsigned objectSize = computeObjectSizeForSharedCell(passedObjectSize);
    return m_allocator.allocate<objectSize>([] () { BCRASH(); return nullptr; });
}

} // namespace bmalloc
