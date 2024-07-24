/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

constexpr size_t dataCacheLineSize()
{
#if CPU(ARM64) || CPU(X86_64)
    return 64;
#else
    return 32; // This is a conservative assumption.
#endif
}

// We require cacheLineAdjustment to be 2 * halfAlignment because we always apply the cacheLineAdjustment
// after we've calibrated for the cell address to start at an odd halfAlignment. This way, adding
// cacheLineAdjustment still keeps the cell address starting at an odd halfAlignment.
static_assert(PreciseAllocation::cacheLineAdjustment == 2 * PreciseAllocation::halfAlignment);

// The purpose of cacheLineAdjustment is to ensure that the JSObject header word and its butterfly
// are both in the same cache line. Therefore, cacheLineAdjustment must be greater than sizeof(JSObject)
// in order for the adjustment to adequately push the start of the object into the next cache line.
static_assert(PreciseAllocation::cacheLineAdjustment >= sizeof(JSObject));

// Also, by definition, cacheLineAdjustment must be smaller than dataCacheLineSize. Otherwise, there's
// not way to fit the JSObject header word and its butterfly in a cache line.
static_assert(PreciseAllocation::cacheLineAdjustment <= dataCacheLineSize());

static inline bool isAlignedForPreciseAllocation(void* memory)
{
    // Checks if the allocated pointer is 16 byte aligned. If it's 16 byte aligned,
    // then the object will have halfAlignment because headerSize() ensures that it
    // has an odd halfAlignment at the end.
    uintptr_t allocatedPointer = bitwise_cast<uintptr_t>(memory);
    uintptr_t maskedPointer = allocatedPointer & (PreciseAllocation::alignment - 1);
    ASSERT(!maskedPointer || maskedPointer == PreciseAllocation::halfAlignment);
    return maskedPointer;
}

static inline bool isCacheAlignedForPreciseAllocation(void* memory)
{
    uintptr_t allocatedPointer = bitwise_cast<uintptr_t>(memory);
    uintptr_t cellStart = allocatedPointer + PreciseAllocation::headerSize();
    uintptr_t cacheLineOffsetForCellStart = cellStart % dataCacheLineSize();
    return dataCacheLineSize() - cacheLineOffsetForCellStart >= PreciseAllocation::cacheLineAdjustment;
}

PreciseAllocation* PreciseAllocation::tryCreate(JSC::Heap& heap, size_t size, Subspace* subspace, unsigned indexInSpace)
{
    if constexpr (validateDFGDoesGC)
        heap.vm().verifyCanGC();

    size_t adjustedAlignmentAllocationSize = headerSize() + size + halfAlignment + cacheLineAdjustment;
    static_assert(halfAlignment == 8, "We assume that memory returned by malloc has alignment >= 8.");
    
    // We must use tryAllocateMemory instead of tryAllocateAlignedMemory since we want to use "realloc" feature.
    void* space = subspace->alignedMemoryAllocator()->tryAllocateMemory(adjustedAlignmentAllocationSize);
    if (!space)
        return nullptr;

    unsigned adjustment = halfAlignment;
    space = bitwise_cast<void*>(bitwise_cast<uintptr_t>(space) + halfAlignment);
    if (UNLIKELY(!isAlignedForPreciseAllocation(space))) {
        space = bitwise_cast<void*>(bitwise_cast<uintptr_t>(space) - halfAlignment);
        adjustment -= halfAlignment;
        ASSERT(isAlignedForPreciseAllocation(space));
    }

    if (!isCacheAlignedForPreciseAllocation(space)) {
        space = bitwise_cast<void*>(bitwise_cast<uintptr_t>(space) + cacheLineAdjustment);
        adjustment += cacheLineAdjustment;
        ASSERT(isCacheAlignedForPreciseAllocation(space));
        ASSERT(isAlignedForPreciseAllocation(space));
    }

    if (UNLIKELY(scribbleFreeCells()))
        scribble(space, size);
    return new (NotNull, space) PreciseAllocation(heap, size, subspace, indexInSpace, adjustment);
}

PreciseAllocation* PreciseAllocation::tryReallocate(size_t size, Subspace* subspace)
{
    ASSERT(!isLowerTierPrecise());
    size_t adjustedAlignmentAllocationSize = headerSize() + size + halfAlignment + cacheLineAdjustment;
    static_assert(halfAlignment == 8, "We assume that memory returned by malloc has alignment >= 8.");

    ASSERT(subspace == m_subspace);

    unsigned oldCellSize = m_cellSize;
    unsigned oldAdjustment = m_adjustment;
    void* oldBasePointer = basePointer();

    void* newSpace = subspace->alignedMemoryAllocator()->tryReallocateMemory(oldBasePointer, adjustedAlignmentAllocationSize);
    if (!newSpace)
        return nullptr;

    void* newBasePointer = newSpace;
    unsigned newAdjustment = halfAlignment;
    newBasePointer = bitwise_cast<void*>(bitwise_cast<uintptr_t>(newBasePointer) + halfAlignment);
    if (UNLIKELY(!isAlignedForPreciseAllocation(newBasePointer))) {
        newBasePointer = bitwise_cast<void*>(bitwise_cast<uintptr_t>(newBasePointer) - halfAlignment);
        newAdjustment -= halfAlignment;
        ASSERT(isAlignedForPreciseAllocation(newBasePointer));
    }

    if (!isCacheAlignedForPreciseAllocation(newBasePointer)) {
        newBasePointer = bitwise_cast<void*>(bitwise_cast<uintptr_t>(newBasePointer) + cacheLineAdjustment);
        newAdjustment += cacheLineAdjustment;
        ASSERT(isCacheAlignedForPreciseAllocation(newBasePointer));
        ASSERT(isAlignedForPreciseAllocation(newBasePointer));
    }

    PreciseAllocation* newAllocation = bitwise_cast<PreciseAllocation*>(newBasePointer);
    if (oldAdjustment != newAdjustment) {
        void* basePointerAfterRealloc = bitwise_cast<void*>(bitwise_cast<uintptr_t>(newSpace) + oldAdjustment);
        memmove(newBasePointer, basePointerAfterRealloc, oldCellSize + PreciseAllocation::headerSize());
    }

    newAllocation->m_cellSize = size;
    newAllocation->m_adjustment = newAdjustment;
    return newAllocation;
}


PreciseAllocation* PreciseAllocation::tryCreateForLowerTierPrecise(JSC::Heap& heap, size_t size, Subspace* subspace, uint8_t lowerTierPreciseIndex)
{
    if constexpr (validateDFGDoesGC)
        heap.vm().verifyCanGC();


    size_t adjustedAlignmentAllocationSize = headerSize() + size + halfAlignment + cacheLineAdjustment;
    static_assert(halfAlignment == 8, "We assume that memory returned by malloc has alignment >= 8.");

    void* space = subspace->alignedMemoryAllocator()->tryAllocateMemory(adjustedAlignmentAllocationSize);
    RELEASE_ASSERT(space);

    unsigned adjustment = halfAlignment;
    space = bitwise_cast<void*>(bitwise_cast<uintptr_t>(space) + halfAlignment);
    if (UNLIKELY(!isAlignedForPreciseAllocation(space))) {
        space = bitwise_cast<void*>(bitwise_cast<uintptr_t>(space) - halfAlignment);
        adjustment -= halfAlignment;
        ASSERT(isAlignedForPreciseAllocation(space));
    }

    if (!isCacheAlignedForPreciseAllocation(space)) {
        space = bitwise_cast<void*>(bitwise_cast<uintptr_t>(space) + cacheLineAdjustment);
        adjustment += cacheLineAdjustment;
        ASSERT(isCacheAlignedForPreciseAllocation(space));
        ASSERT(isAlignedForPreciseAllocation(space));
    }

    if (UNLIKELY(scribbleFreeCells()))
        scribble(space, size);
    PreciseAllocation* preciseAllocation = new (NotNull, space) PreciseAllocation(heap, size, subspace, 0, adjustment);
    preciseAllocation->m_lowerTierPreciseIndex = lowerTierPreciseIndex;
    return preciseAllocation;
}

PreciseAllocation* PreciseAllocation::reuseForLowerTierPrecise()
{
    JSC::Heap& heap = *this->heap();
    size_t size = m_cellSize;
    Subspace* subspace = m_subspace;
    unsigned adjustment = m_adjustment;
    uint8_t lowerTierPreciseIndex = m_lowerTierPreciseIndex;
    void* basePointer = this->basePointer();

    this->~PreciseAllocation();

    void* space = bitwise_cast<void*>(bitwise_cast<uintptr_t>(basePointer) + adjustment);

    PreciseAllocation* preciseAllocation = new (NotNull, space) PreciseAllocation(heap, size, subspace, 0, adjustment);
    preciseAllocation->m_lowerTierPreciseIndex = lowerTierPreciseIndex;
    preciseAllocation->m_hasValidCell = false;
    return preciseAllocation;
}

PreciseAllocation::PreciseAllocation(JSC::Heap& heap, size_t size, Subspace* subspace, unsigned indexInSpace, unsigned adjustment)
    : m_indexInSpace(indexInSpace)
    , m_cellSize(size)
    , m_isNewlyAllocated(true)
    , m_hasValidCell(true)
    , m_adjustment(adjustment)
    , m_attributes(subspace->attributes())
    , m_subspace(subspace)
    , m_weakSet(heap.vm())
{
    m_isMarked.store(0);
    ASSERT(cell()->isPreciseAllocation());
    ASSERT(m_adjustment == adjustment);
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

void PreciseAllocation::flip()
{
    ASSERT(heap()->collectionScope() == CollectionScope::Full);
    // Propagate the last time's mark bit to m_isNewlyAllocated so that `isLive` will say "yes" until this GC cycle finishes.
    // After that, m_isNewlyAllocated is cleared again. So only previously marked or actually newly created objects survive.
    // We do not need to care about concurrency here since marking thread is stopped right now. This is equivalent to the logic
    // of MarkedBlock::aboutToMarkSlow.
    // We invoke this function only when this is full collection. This ensures that at the end of upcoming cycle, we will
    // clear NewlyAllocated bits of all objects. So this works correctly.
    //
    //                                      N: NewlyAllocated, M: Marked
    //                                                 after this         at the end        When cycle
    //                                            N M  function    N M     of cycle    N M  is finished   N M
    // The live object survives the last cycle    0 1      =>      1 0        =>       1 1       =>       0 1    => live
    // The dead object in the last cycle          0 0      =>      0 0        =>       0 0       =>       0 0    => dead
    // The live object newly created after this            =>      1 0        =>       1 1       =>       0 1    => live
    // The dead object newly created after this            =>      1 0        =>       1 0       =>       0 0    => dead
    // The live object newly created before this  1 0      =>      1 0        =>       1 1       =>       0 1    => live
    // The dead object newly created before this  1 0      =>      1 0        =>       1 0       =>       0 0    => dead
    //                                                                                                    ^
    //                                                              This is ensured since this function is used only for full GC.
    m_isNewlyAllocated |= isMarked();
    m_isMarked.store(false, std::memory_order_relaxed);
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
        if (isLowerTierPrecise())
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

