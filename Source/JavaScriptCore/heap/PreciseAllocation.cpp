/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
#include "PreciseAllocation.h"

#include "AlignedMemoryAllocator.h"
#include "IsoCellSetInlines.h"
#include "JSCInlines.h"
#include "Scribble.h"
#include "SubspaceInlines.h"

namespace JSC {

static inline bool isAlignedForPreciseAllocation(void* memory)
{
    uintptr_t allocatedPointer = bitwise_cast<uintptr_t>(memory);
    return !(allocatedPointer & (PreciseAllocation::alignment - 1));
}

PreciseAllocation* PreciseAllocation::tryCreate(Heap& heap, size_t size, Subspace* subspace, unsigned indexInSpace)
{
    if constexpr (validateDFGDoesGC)
        heap.verifyCanGC();

    size_t adjustedAlignmentAllocationSize = headerSize() + size + halfAlignment;
    static_assert(halfAlignment == 8, "We assume that memory returned by malloc has alignment >= 8.");
    
    // We must use tryAllocateMemory instead of tryAllocateAlignedMemory since we want to use "realloc" feature.
    void* space = subspace->alignedMemoryAllocator()->tryAllocateMemory(adjustedAlignmentAllocationSize);
    if (!space)
        return nullptr;

    bool adjustedAlignment = false;
    if (!isAlignedForPreciseAllocation(space)) {
        space = bitwise_cast<void*>(bitwise_cast<uintptr_t>(space) + halfAlignment);
        adjustedAlignment = true;
        ASSERT(isAlignedForPreciseAllocation(space));
    }
    
    if (scribbleFreeCells())
        scribble(space, size);
    return new (NotNull, space) PreciseAllocation(heap, size, subspace, indexInSpace, adjustedAlignment);
}

PreciseAllocation* PreciseAllocation::tryReallocate(size_t size, Subspace* subspace)
{
    ASSERT(!isLowerTier());
    size_t adjustedAlignmentAllocationSize = headerSize() + size + halfAlignment;
    static_assert(halfAlignment == 8, "We assume that memory returned by malloc has alignment >= 8.");

    ASSERT(subspace == m_subspace);

    unsigned oldCellSize = m_cellSize;
    bool oldAdjustedAlignment = m_adjustedAlignment;
    void* oldBasePointer = basePointer();

    void* newBasePointer = subspace->alignedMemoryAllocator()->tryReallocateMemory(oldBasePointer, adjustedAlignmentAllocationSize);
    if (!newBasePointer)
        return nullptr;

    PreciseAllocation* newAllocation = bitwise_cast<PreciseAllocation*>(newBasePointer);
    bool newAdjustedAlignment = false;
    if (!isAlignedForPreciseAllocation(newBasePointer)) {
        newAdjustedAlignment = true;
        newAllocation = bitwise_cast<PreciseAllocation*>(bitwise_cast<uintptr_t>(newBasePointer) + halfAlignment);
        ASSERT(isAlignedForPreciseAllocation(static_cast<void*>(newAllocation)));
    }

    // We have 4 patterns.
    // oldAdjustedAlignment = true  newAdjustedAlignment = true  => Do nothing.
    // oldAdjustedAlignment = true  newAdjustedAlignment = false => Shift forward by halfAlignment
    // oldAdjustedAlignment = false newAdjustedAlignment = true  => Shift backward by halfAlignment
    // oldAdjustedAlignment = false newAdjustedAlignment = false => Do nothing.

    if (oldAdjustedAlignment != newAdjustedAlignment) {
        if (oldAdjustedAlignment) {
            ASSERT(!newAdjustedAlignment);
            ASSERT(newAllocation == newBasePointer);
            // Old   [ 8 ][  content  ]
            // Now   [   ][  content  ]
            // New   [  content  ]...
            memmove(newBasePointer, bitwise_cast<char*>(newBasePointer) + halfAlignment, oldCellSize + PreciseAllocation::headerSize());
        } else {
            ASSERT(newAdjustedAlignment);
            ASSERT(newAllocation != newBasePointer);
            ASSERT(newAllocation == bitwise_cast<void*>(bitwise_cast<char*>(newBasePointer) + halfAlignment));
            // Old   [  content  ]
            // Now   [  content  ][   ]
            // New   [ 8 ][  content  ]
            memmove(bitwise_cast<char*>(newBasePointer) + halfAlignment, newBasePointer, oldCellSize + PreciseAllocation::headerSize());
        }
    }

    newAllocation->m_cellSize = size;
    newAllocation->m_adjustedAlignment = newAdjustedAlignment;
    return newAllocation;
}


PreciseAllocation* PreciseAllocation::createForLowerTier(Heap& heap, size_t size, Subspace* subspace, uint8_t lowerTierIndex)
{
    if constexpr (validateDFGDoesGC)
        heap.verifyCanGC();

    size_t adjustedAlignmentAllocationSize = headerSize() + size + halfAlignment;
    static_assert(halfAlignment == 8, "We assume that memory returned by malloc has alignment >= 8.");

    void* space = subspace->alignedMemoryAllocator()->tryAllocateMemory(adjustedAlignmentAllocationSize);
    RELEASE_ASSERT(space);

    bool adjustedAlignment = false;
    if (!isAlignedForPreciseAllocation(space)) {
        space = bitwise_cast<void*>(bitwise_cast<uintptr_t>(space) + halfAlignment);
        adjustedAlignment = true;
        ASSERT(isAlignedForPreciseAllocation(space));
    }

    if (scribbleFreeCells())
        scribble(space, size);
    PreciseAllocation* preciseAllocation = new (NotNull, space) PreciseAllocation(heap, size, subspace, 0, adjustedAlignment);
    preciseAllocation->m_lowerTierIndex = lowerTierIndex;
    return preciseAllocation;
}

PreciseAllocation* PreciseAllocation::reuseForLowerTier()
{
    Heap& heap = *this->heap();
    size_t size = m_cellSize;
    Subspace* subspace = m_subspace;
    bool adjustedAlignment = m_adjustedAlignment;
    uint8_t lowerTierIndex = m_lowerTierIndex;
    void* basePointer = this->basePointer();

    this->~PreciseAllocation();

    void* space = basePointer;
    ASSERT((!isAlignedForPreciseAllocation(basePointer)) == adjustedAlignment);
    if (adjustedAlignment)
        space = bitwise_cast<void*>(bitwise_cast<uintptr_t>(basePointer) + halfAlignment);

    PreciseAllocation* preciseAllocation = new (NotNull, space) PreciseAllocation(heap, size, subspace, 0, adjustedAlignment);
    preciseAllocation->m_lowerTierIndex = lowerTierIndex;
    preciseAllocation->m_hasValidCell = false;
    return preciseAllocation;
}

PreciseAllocation::PreciseAllocation(Heap& heap, size_t size, Subspace* subspace, unsigned indexInSpace, bool adjustedAlignment)
    : m_indexInSpace(indexInSpace)
    , m_cellSize(size)
    , m_isNewlyAllocated(true)
    , m_hasValidCell(true)
    , m_adjustedAlignment(adjustedAlignment)
    , m_attributes(subspace->attributes())
    , m_subspace(subspace)
    , m_weakSet(heap.vm())
{
    m_isMarked.store(0);
    ASSERT(cell()->isPreciseAllocation());
}

PreciseAllocation::~PreciseAllocation()
{
    if (isOnList())
        remove();
}

void PreciseAllocation::lastChanceToFinalize()
{
    m_weakSet.lastChanceToFinalize();
    clearMarked();
    clearNewlyAllocated();
    sweep();
}

void PreciseAllocation::shrink()
{
    m_weakSet.shrink();
}

void PreciseAllocation::visitWeakSet(SlotVisitor& visitor)
{
    m_weakSet.visit(visitor);
}

void PreciseAllocation::reapWeakSet()
{
    return m_weakSet.reap();
}

void PreciseAllocation::flip()
{
    ASSERT(heap()->collectionScope() == CollectionScope::Full);
    clearMarked();
}

bool PreciseAllocation::isEmpty()
{
    return !isMarked() && m_weakSet.isEmpty() && !isNewlyAllocated();
}

void PreciseAllocation::sweep()
{
    m_weakSet.sweep();
    
    if (m_hasValidCell && !isLive()) {
        if (m_attributes.destruction == NeedsDestruction)
            m_subspace->destroy(vm(), static_cast<JSCell*>(cell()));
        // We should clear IsoCellSet's bit before actually destroying PreciseAllocation
        // since PreciseAllocation's destruction can be delayed until its WeakSet is cleared.
        if (isLowerTier())
            static_cast<IsoSubspace*>(m_subspace)->clearIsoCellSetBit(this);
        m_hasValidCell = false;
    }
}

void PreciseAllocation::destroy()
{
    AlignedMemoryAllocator* allocator = m_subspace->alignedMemoryAllocator();
    void* basePointer = this->basePointer();
    this->~PreciseAllocation();
    allocator->freeMemory(basePointer);
}

void PreciseAllocation::dump(PrintStream& out) const
{
    out.print(RawPointer(this), ":(cell at ", RawPointer(cell()), " with size ", m_cellSize, " and attributes ", m_attributes, ")");
}

#if ASSERT_ENABLED
void PreciseAllocation::assertValidCell(VM& vm, HeapCell* cell) const
{
    ASSERT(&vm == &this->vm());
    ASSERT(cell == this->cell());
    ASSERT(m_hasValidCell);
}
#endif

} // namespace JSC

