/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

namespace JSC {

namespace GCClient {

ALWAYS_INLINE void* IsoSubspace::allocate(VM& vm, size_t size, GCDeferralContext* deferralContext, AllocationFailureMode failureMode)
{
    RELEASE_ASSERT(size <= cellSize());
    Allocator allocator = allocatorFor(size, AllocatorForMode::MustAlreadyHaveAllocator);
    void* result = allocator.allocate(vm.heap, deferralContext, failureMode);
    return result;
}

} // namespace GCClient

inline void IsoSubspace::clearIsoCellSetBit(PreciseAllocation* preciseAllocation)
{
    unsigned lowerTierIndex = preciseAllocation->lowerTierIndex();
    m_cellSets.forEach(
        [&](IsoCellSet* set) {
            set->clearLowerTierCell(lowerTierIndex);
        });
}

inline void IsoSubspace::sweep()
{
    Subspace::sweepBlocks();
    // We sweep precise-allocations eagerly, but we do not free it immediately.
    // This part should be done by MarkedSpace::sweepPreciseAllocations.
    m_preciseAllocations.forEach([&](PreciseAllocation* allocation) {
        allocation->sweep();
    });
}

template<typename Func>
void IsoSubspace::forEachLowerTierFreeListedPreciseAllocation(const Func& func)
{
    m_lowerTierFreeList.forEach(func);
}

} // namespace JSC

