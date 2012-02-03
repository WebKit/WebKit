/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MarkedBlock.h"

#include "JSCell.h"
#include "JSObject.h"
#include "ScopeChain.h"

namespace JSC {

MarkedBlock* MarkedBlock::create(Heap* heap, size_t cellSize)
{
    PageAllocationAligned allocation = PageAllocationAligned::allocate(blockSize, blockSize, OSAllocator::JSGCHeapPages);
    if (!static_cast<bool>(allocation))
        CRASH();
    return new (NotNull, allocation.base()) MarkedBlock(allocation, heap, cellSize);
}

MarkedBlock* MarkedBlock::recycle(MarkedBlock* block, Heap* heap, size_t cellSize)
{
    return new (NotNull, block) MarkedBlock(block->m_allocation, heap, cellSize);
}

void MarkedBlock::destroy(MarkedBlock* block)
{
    block->m_allocation.deallocate();
}

MarkedBlock::MarkedBlock(PageAllocationAligned& allocation, Heap* heap, size_t cellSize)
    : HeapBlock(allocation)
    , m_atomsPerCell((cellSize + atomSize - 1) / atomSize)
    , m_endAtom(atomsPerBlock - m_atomsPerCell + 1)
    , m_state(New) // All cells start out unmarked.
    , m_heap(heap)
{
    ASSERT(heap);
    HEAP_LOG_BLOCK_STATE_TRANSITION(this);
}

inline void MarkedBlock::callDestructor(JSCell* cell)
{
    // A previous eager sweep may already have run cell's destructor.
    if (cell->isZapped())
        return;

#if ENABLE(SIMPLE_HEAP_PROFILING)
    m_heap->m_destroyedTypeCounts.countVPtr(vptr);
#endif
    if (cell->classInfo() != &JSFinalObject::s_info)
        cell->methodTable()->destroy(cell);

    cell->zap();
}

template<MarkedBlock::BlockState blockState, MarkedBlock::SweepMode sweepMode>
MarkedBlock::FreeCell* MarkedBlock::specializedSweep()
{
    ASSERT(blockState != Allocated && blockState != FreeListed);

    // This produces a free list that is ordered in reverse through the block.
    // This is fine, since the allocation code makes no assumptions about the
    // order of the free list.
    FreeCell* head = 0;
    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
        if (blockState == Marked && m_marks.get(i))
            continue;

        JSCell* cell = reinterpret_cast_ptr<JSCell*>(&atoms()[i]);
        if (blockState == Zapped && !cell->isZapped())
            continue;

        if (blockState != New)
            callDestructor(cell);

        if (sweepMode == SweepToFreeList) {
            FreeCell* freeCell = reinterpret_cast<FreeCell*>(cell);
            freeCell->next = head;
            head = freeCell;
        }
    }

    m_state = ((sweepMode == SweepToFreeList) ? FreeListed : Zapped);
    return head;
}

MarkedBlock::FreeCell* MarkedBlock::sweep(SweepMode sweepMode)
{
    HEAP_LOG_BLOCK_STATE_TRANSITION(this);

    switch (m_state) {
    case New:
        ASSERT(sweepMode == SweepToFreeList);
        return specializedSweep<New, SweepToFreeList>();
    case FreeListed:
        // Happens when a block transitions to fully allocated.
        ASSERT(sweepMode == SweepToFreeList);
        return 0;
    case Allocated:
        ASSERT_NOT_REACHED();
        return 0;
    case Marked:
        return sweepMode == SweepToFreeList
            ? specializedSweep<Marked, SweepToFreeList>()
            : specializedSweep<Marked, SweepOnly>();
    case Zapped:
        return sweepMode == SweepToFreeList
            ? specializedSweep<Zapped, SweepToFreeList>()
            : specializedSweep<Zapped, SweepOnly>();
    }

    ASSERT_NOT_REACHED();
    return 0;
}

void MarkedBlock::zapFreeList(FreeCell* firstFreeCell)
{
    HEAP_LOG_BLOCK_STATE_TRANSITION(this);

    if (m_state == Marked) {
        // If the block is in the Marked state then we know that:
        // 1) It was not used for allocation during the previous allocation cycle.
        // 2) It may have dead objects, and we only know them to be dead by the
        //    fact that their mark bits are unset.
        // Hence if the block is Marked we need to leave it Marked.
        
        ASSERT(!firstFreeCell);
        
        return;
    }
    
    if (m_state == Zapped) {
        // If the block is in the Zapped state then we know that someone already
        // zapped it for us. This could not have happened during a GC, but might
        // be the result of someone having done a GC scan to perform some operation
        // over all live objects (or all live blocks). It also means that somebody
        // had allocated in this block since the last GC, swept all dead objects
        // onto the free list, left the block in the FreeListed state, then the heap
        // scan happened, and canonicalized the block, leading to all dead objects
        // being zapped. Therefore, it is safe for us to simply do nothing, since
        // dead objects will have 0 in their vtables and live objects will have
        // non-zero vtables, which is consistent with the block being zapped.
        
        ASSERT(!firstFreeCell);
        
        return;
    }
    
    ASSERT(m_state == FreeListed);
    
    // Roll back to a coherent state for Heap introspection. Cells newly
    // allocated from our free list are not currently marked, so we need another
    // way to tell what's live vs dead. We use zapping for that.
    
    FreeCell* next;
    for (FreeCell* current = firstFreeCell; current; current = next) {
        next = current->next;
        reinterpret_cast<JSCell*>(current)->zap();
    }
    
    m_state = Zapped;
}

} // namespace JSC
