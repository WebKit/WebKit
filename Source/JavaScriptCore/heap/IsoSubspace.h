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

#pragma once

#include "BlockDirectory.h"
#include "Subspace.h"
#include "SubspaceAccess.h"
#include <wtf/SinglyLinkedListWithTail.h>

namespace JSC {

class IsoAlignedMemoryAllocator;
class IsoCellSet;

class IsoSubspace : public Subspace {
public:
    JS_EXPORT_PRIVATE IsoSubspace(CString name, Heap&, HeapCellType*, size_t size, uint8_t numberOfLowerTierCells);
    JS_EXPORT_PRIVATE ~IsoSubspace();

    size_t cellSize() { return m_directory.cellSize(); }

    Allocator allocatorFor(size_t, AllocatorForMode) override;
    Allocator allocatorForNonVirtual(size_t, AllocatorForMode);

    void* allocate(VM&, size_t, GCDeferralContext*, AllocationFailureMode) override;
    void* allocateNonVirtual(VM&, size_t, GCDeferralContext*, AllocationFailureMode);

    void sweepLowerTierCell(PreciseAllocation*);
    void clearIsoCellSetBit(PreciseAllocation*);

    void* tryAllocateFromLowerTier();
    void destroyLowerTierFreeList();

    void sweep();

private:
    friend class IsoCellSet;
    
    void didResizeBits(unsigned newSize) override;
    void didRemoveBlock(unsigned blockIndex) override;
    void didBeginSweepingToFreeList(MarkedBlock::Handle*) override;
    
    BlockDirectory m_directory;
    LocalAllocator m_localAllocator;
    std::unique_ptr<IsoAlignedMemoryAllocator> m_isoAlignedMemoryAllocator;
    SentinelLinkedList<PreciseAllocation, PackedRawSentinelNode<PreciseAllocation>> m_lowerTierFreeList;
    SentinelLinkedList<IsoCellSet, PackedRawSentinelNode<IsoCellSet>> m_cellSets;
};

ALWAYS_INLINE Allocator IsoSubspace::allocatorForNonVirtual(size_t size, AllocatorForMode)
{
    RELEASE_ASSERT(WTF::roundUpToMultipleOf<MarkedBlock::atomSize>(size) == cellSize());
    return Allocator(&m_localAllocator);
}

#define ISO_SUBSPACE_INIT(heap, heapCellType, type) ("Isolated " #type " Space", (heap), (heapCellType), sizeof(type), type::numberOfLowerTierCells)

template<typename T>
struct isAllocatedFromIsoSubspace {
    static constexpr bool value = std::is_same<std::result_of_t<decltype(T::template subspaceFor<T, SubspaceAccess::OnMainThread>)&(VM&)>, IsoSubspace*>::value;
};

} // namespace JSC

