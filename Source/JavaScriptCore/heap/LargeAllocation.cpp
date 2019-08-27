/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "LargeAllocation.h"

#include "AlignedMemoryAllocator.h"
#include "Heap.h"
#include "JSCInlines.h"
#include "Operations.h"
#include "SubspaceInlines.h"

namespace JSC {

static inline bool isAlignedForLargeAllocation(void* memory)
{
    uintptr_t allocatedPointer = bitwise_cast<uintptr_t>(memory);
    return !(allocatedPointer & (LargeAllocation::alignment - 1));
}

LargeAllocation* LargeAllocation::tryCreate(Heap& heap, size_t size, Subspace* subspace, unsigned indexInSpace)
{
    if (validateDFGDoesGC)
        RELEASE_ASSERT(heap.expectDoesGC());

    size_t adjustedAlignmentAllocationSize = headerSize() + size + halfAlignment;
    static_assert(halfAlignment == 8, "We assume that memory returned by malloc has alignment >= 8.");
    
    // We must use tryAllocateMemory instead of tryAllocateAlignedMemory since we want to use "realloc" feature.
    void* space = subspace->alignedMemoryAllocator()->tryAllocateMemory(adjustedAlignmentAllocationSize);
    if (!space)
        return nullptr;

    bool adjustedAlignment = false;
    if (!isAlignedForLargeAllocation(space)) {
        space = bitwise_cast<void*>(bitwise_cast<uintptr_t>(space) + halfAlignment);
        adjustedAlignment = true;
        ASSERT(isAlignedForLargeAllocation(space));
    }
    
    if (scribbleFreeCells())
        scribble(space, size);
    return new (NotNull, space) LargeAllocation(heap, size, subspace, indexInSpace, adjustedAlignment);
}

LargeAllocation* LargeAllocation::tryReallocate(size_t size, Subspace* subspace)
{
    size_t adjustedAlignmentAllocationSize = headerSize() + size + halfAlignment;
    static_assert(halfAlignment == 8, "We assume that memory returned by malloc has alignment >= 8.");

    ASSERT(subspace == m_subspace);

    unsigned oldCellSize = m_cellSize;
    bool oldAdjustedAlignment = m_adjustedAlignment;
    void* oldBasePointer = basePointer();

    void* newBasePointer = subspace->alignedMemoryAllocator()->tryReallocateMemory(oldBasePointer, adjustedAlignmentAllocationSize);
    if (!newBasePointer)
        return nullptr;

    LargeAllocation* newAllocation = bitwise_cast<LargeAllocation*>(newBasePointer);
    bool newAdjustedAlignment = false;
    if (!isAlignedForLargeAllocation(newBasePointer)) {
        newAdjustedAlignment = true;
        newAllocation = bitwise_cast<LargeAllocation*>(bitwise_cast<uintptr_t>(newBasePointer) + halfAlignment);
        ASSERT(isAlignedForLargeAllocation(static_cast<void*>(newAllocation)));
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
            memmove(newBasePointer, bitwise_cast<char*>(newBasePointer) + halfAlignment, oldCellSize + LargeAllocation::headerSize());
        } else {
            ASSERT(newAdjustedAlignment);
            ASSERT(newAllocation != newBasePointer);
            ASSERT(newAllocation == bitwise_cast<void*>(bitwise_cast<char*>(newBasePointer) + halfAlignment));
            // Old   [  content  ]
            // Now   [  content  ][   ]
            // New   [ 8 ][  content  ]
            memmove(bitwise_cast<char*>(newBasePointer) + halfAlignment, newBasePointer, oldCellSize + LargeAllocation::headerSize());
        }
    }

    newAllocation->m_cellSize = size;
    newAllocation->m_adjustedAlignment = newAdjustedAlignment;
    return newAllocation;
}

LargeAllocation::LargeAllocation(Heap& heap, size_t size, Subspace* subspace, unsigned indexInSpace, bool adjustedAlignment)
    : m_cellSize(size)
    , m_indexInSpace(indexInSpace)
    , m_isNewlyAllocated(true)
    , m_hasValidCell(true)
    , m_adjustedAlignment(adjustedAlignment)
    , m_attributes(subspace->attributes())
    , m_subspace(subspace)
    , m_weakSet(heap.vm(), *this)
{
    m_isMarked.store(0);
}

LargeAllocation::~LargeAllocation()
{
    if (isOnList())
        remove();
}

void LargeAllocation::lastChanceToFinalize()
{
    m_weakSet.lastChanceToFinalize();
    clearMarked();
    clearNewlyAllocated();
    sweep();
}

void LargeAllocation::shrink()
{
    m_weakSet.shrink();
}

void LargeAllocation::visitWeakSet(SlotVisitor& visitor)
{
    m_weakSet.visit(visitor);
}

void LargeAllocation::reapWeakSet()
{
    return m_weakSet.reap();
}

void LargeAllocation::flip()
{
    ASSERT(heap()->collectionScope() == CollectionScope::Full);
    clearMarked();
}

bool LargeAllocation::isEmpty()
{
    return !isMarked() && m_weakSet.isEmpty() && !isNewlyAllocated();
}

void LargeAllocation::sweep()
{
    m_weakSet.sweep();
    
    if (m_hasValidCell && !isLive()) {
        if (m_attributes.destruction == NeedsDestruction)
            m_subspace->destroy(vm(), static_cast<JSCell*>(cell()));
        m_hasValidCell = false;
    }
}

void LargeAllocation::destroy()
{
    AlignedMemoryAllocator* allocator = m_subspace->alignedMemoryAllocator();
    void* basePointer = this->basePointer();
    this->~LargeAllocation();
    allocator->freeMemory(basePointer);
}

void LargeAllocation::dump(PrintStream& out) const
{
    out.print(RawPointer(this), ":(cell at ", RawPointer(cell()), " with size ", m_cellSize, " and attributes ", m_attributes, ")");
}

#if !ASSERT_DISABLED
void LargeAllocation::assertValidCell(VM& vm, HeapCell* cell) const
{
    ASSERT(&vm == &this->vm());
    ASSERT(cell == this->cell());
    ASSERT(m_hasValidCell);
}
#endif

} // namespace JSC

