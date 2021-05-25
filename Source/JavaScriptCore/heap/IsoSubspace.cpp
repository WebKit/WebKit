/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "IsoSubspace.h"

#include "IsoAlignedMemoryAllocator.h"
#include "IsoCellSetInlines.h"
#include "JSCellInlines.h"
#include "MarkedSpaceInlines.h"

namespace JSC {

IsoSubspace::IsoSubspace(CString name, Heap& heap, HeapCellType* heapCellType, size_t size, uint8_t numberOfLowerTierCells)
    : Subspace(name, heap)
    , m_directory(WTF::roundUpToMultipleOf<MarkedBlock::atomSize>(size))
    , m_localAllocator(&m_directory)
    , m_isoAlignedMemoryAllocator(makeUnique<IsoAlignedMemoryAllocator>(name))
{
    m_remainingLowerTierCellCount = numberOfLowerTierCells;
    ASSERT(WTF::roundUpToMultipleOf<MarkedBlock::atomSize>(size) == cellSize());
    ASSERT(numberOfLowerTierCells <= MarkedBlock::maxNumberOfLowerTierCells);
    m_isIsoSubspace = true;
    initialize(heapCellType, m_isoAlignedMemoryAllocator.get());

    Locker locker { m_space.directoryLock() };
    m_directory.setSubspace(this);
    m_space.addBlockDirectory(locker, &m_directory);
    m_alignedMemoryAllocator->registerDirectory(heap, &m_directory);
    m_firstDirectory = &m_directory;
}

IsoSubspace::~IsoSubspace()
{
}

Allocator IsoSubspace::allocatorFor(size_t size, AllocatorForMode mode)
{
    return allocatorForNonVirtual(size, mode);
}

void* IsoSubspace::allocate(VM& vm, size_t size, GCDeferralContext* deferralContext, AllocationFailureMode failureMode)
{
    return allocateNonVirtual(vm, size, deferralContext, failureMode);
}

void IsoSubspace::didResizeBits(unsigned blockIndex)
{
    m_cellSets.forEach(
        [&] (IsoCellSet* set) {
            set->didResizeBits(blockIndex);
        });
}

void IsoSubspace::didRemoveBlock(unsigned blockIndex)
{
    m_cellSets.forEach(
        [&] (IsoCellSet* set) {
            set->didRemoveBlock(blockIndex);
        });
}

void IsoSubspace::didBeginSweepingToFreeList(MarkedBlock::Handle* block)
{
    m_cellSets.forEach(
        [&] (IsoCellSet* set) {
            set->sweepToFreeList(block);
        });
}

void* IsoSubspace::tryAllocateFromLowerTier()
{
    auto revive = [&] (PreciseAllocation* allocation) {
        allocation->setIndexInSpace(m_space.m_preciseAllocations.size());
        allocation->m_hasValidCell = true;
        m_space.m_preciseAllocations.append(allocation);
        if (auto* set = m_space.preciseAllocationSet())
            set->add(allocation->cell());
        ASSERT(allocation->indexInSpace() == m_space.m_preciseAllocations.size() - 1);
        m_preciseAllocations.append(allocation);
        return allocation->cell();
    };

    if (!m_lowerTierFreeList.isEmpty()) {
        PreciseAllocation* allocation = m_lowerTierFreeList.begin();
        allocation->remove();
        return revive(allocation);
    }
    if (m_remainingLowerTierCellCount) {
        PreciseAllocation* allocation = PreciseAllocation::createForLowerTier(m_space.heap(), cellSize(), this, --m_remainingLowerTierCellCount);
        return revive(allocation);
    }
    return nullptr;
}

void IsoSubspace::sweepLowerTierCell(PreciseAllocation* preciseAllocation)
{
    preciseAllocation = preciseAllocation->reuseForLowerTier();
    m_lowerTierFreeList.append(preciseAllocation);
}

void IsoSubspace::destroyLowerTierFreeList()
{
    m_lowerTierFreeList.forEach([&](PreciseAllocation* allocation) {
        allocation->destroy();
    });
}

} // namespace JSC

