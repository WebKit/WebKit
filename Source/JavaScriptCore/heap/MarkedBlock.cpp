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
#include "JSDestructibleObject.h"
#include "JSCInlines.h"

namespace JSC {

static const bool computeBalance = false;
static size_t balance;

MarkedBlock* MarkedBlock::create(Heap& heap, MarkedAllocator* allocator, size_t capacity, size_t cellSize, bool needsDestruction)
{
    if (computeBalance) {
        balance++;
        if (!(balance % 10))
            dataLog("MarkedBlock Balance: ", balance, "\n");
    }
    MarkedBlock* block = new (NotNull, fastAlignedMalloc(blockSize, capacity)) MarkedBlock(allocator, capacity, cellSize, needsDestruction);
    heap.didAllocateBlock(capacity);
    return block;
}

void MarkedBlock::destroy(Heap& heap, MarkedBlock* block)
{
    if (computeBalance) {
        balance--;
        if (!(balance % 10))
            dataLog("MarkedBlock Balance: ", balance, "\n");
    }
    size_t capacity = block->capacity();
    block->~MarkedBlock();
    fastAlignedFree(block);
    heap.didFreeBlock(capacity);
}

MarkedBlock::MarkedBlock(MarkedAllocator* allocator, size_t capacity, size_t cellSize, bool needsDestruction)
    : DoublyLinkedListNode<MarkedBlock>()
    , m_atomsPerCell((cellSize + atomSize - 1) / atomSize)
    , m_endAtom((allocator->cellSize() ? atomsPerBlock - m_atomsPerCell : firstAtom()) + 1)
    , m_capacity(capacity)
    , m_needsDestruction(needsDestruction)
    , m_allocator(allocator)
    , m_state(New) // All cells start out unmarked.
    , m_weakSet(allocator->heap()->vm(), *this)
{
    ASSERT(allocator);
    HEAP_LOG_BLOCK_STATE_TRANSITION(this);
}

inline void MarkedBlock::callDestructor(JSCell* cell)
{
    // A previous eager sweep may already have run cell's destructor.
    if (cell->isZapped())
        return;

    ASSERT(cell->structureID());
    if (cell->inlineTypeFlags() & StructureIsImmortal)
        cell->structure(*vm())->classInfo()->methodTable.destroy(cell);
    else
        jsCast<JSDestructibleObject*>(cell)->classInfo()->methodTable.destroy(cell);
    cell->zap();
}

template<MarkedBlock::BlockState blockState, MarkedBlock::SweepMode sweepMode, bool callDestructors>
MarkedBlock::FreeList MarkedBlock::specializedSweep()
{
    ASSERT(blockState != Allocated && blockState != FreeListed);
    ASSERT(!(!callDestructors && sweepMode == SweepOnly));

    // This produces a free list that is ordered in reverse through the block.
    // This is fine, since the allocation code makes no assumptions about the
    // order of the free list.
    FreeCell* head = 0;
    size_t count = 0;
    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
        if (blockState == Marked && (m_marks.get(i) || (m_newlyAllocated && m_newlyAllocated->get(i))))
            continue;

        JSCell* cell = reinterpret_cast_ptr<JSCell*>(&atoms()[i]);

        if (callDestructors && blockState != New)
            callDestructor(cell);

        if (sweepMode == SweepToFreeList) {
            FreeCell* freeCell = reinterpret_cast<FreeCell*>(cell);
            freeCell->next = head;
            head = freeCell;
            ++count;
        }
    }

    // We only want to discard the newlyAllocated bits if we're creating a FreeList,
    // otherwise we would lose information on what's currently alive.
    if (sweepMode == SweepToFreeList && m_newlyAllocated)
        m_newlyAllocated = nullptr;

    m_state = ((sweepMode == SweepToFreeList) ? FreeListed : Marked);
    return FreeList(head, count * cellSize());
}

MarkedBlock::FreeList MarkedBlock::sweep(SweepMode sweepMode)
{
    HEAP_LOG_BLOCK_STATE_TRANSITION(this);

    m_weakSet.sweep();

    if (sweepMode == SweepOnly && !m_needsDestruction)
        return FreeList();

    if (m_needsDestruction)
        return sweepHelper<true>(sweepMode);
    return sweepHelper<false>(sweepMode);
}

template<bool callDestructors>
MarkedBlock::FreeList MarkedBlock::sweepHelper(SweepMode sweepMode)
{
    switch (m_state) {
    case New:
        ASSERT(sweepMode == SweepToFreeList);
        return specializedSweep<New, SweepToFreeList, callDestructors>();
    case FreeListed:
        // Happens when a block transitions to fully allocated.
        ASSERT(sweepMode == SweepToFreeList);
        return FreeList();
    case Retired:
    case Allocated:
        RELEASE_ASSERT_NOT_REACHED();
        return FreeList();
    case Marked:
        return sweepMode == SweepToFreeList
            ? specializedSweep<Marked, SweepToFreeList, callDestructors>()
            : specializedSweep<Marked, SweepOnly, callDestructors>();
    }

    RELEASE_ASSERT_NOT_REACHED();
    return FreeList();
}

class SetNewlyAllocatedFunctor : public MarkedBlock::VoidFunctor {
public:
    SetNewlyAllocatedFunctor(MarkedBlock* block)
        : m_block(block)
    {
    }

    IterationStatus operator()(JSCell* cell)
    {
        ASSERT(MarkedBlock::blockFor(cell) == m_block);
        m_block->setNewlyAllocated(cell);
        return IterationStatus::Continue;
    }

private:
    MarkedBlock* m_block;
};

void MarkedBlock::stopAllocating(const FreeList& freeList)
{
    HEAP_LOG_BLOCK_STATE_TRANSITION(this);
    FreeCell* head = freeList.head;

    if (m_state == Marked) {
        // If the block is in the Marked state then we know that:
        // 1) It was not used for allocation during the previous allocation cycle.
        // 2) It may have dead objects, and we only know them to be dead by the
        //    fact that their mark bits are unset.
        // Hence if the block is Marked we need to leave it Marked.
        
        ASSERT(!head);
        return;
    }
   
    ASSERT(m_state == FreeListed);
    
    // Roll back to a coherent state for Heap introspection. Cells newly
    // allocated from our free list are not currently marked, so we need another
    // way to tell what's live vs dead. 
    
    ASSERT(!m_newlyAllocated);
    m_newlyAllocated = std::make_unique<WTF::Bitmap<atomsPerBlock>>();

    SetNewlyAllocatedFunctor functor(this);
    forEachCell(functor);

    FreeCell* next;
    for (FreeCell* current = head; current; current = next) {
        next = current->next;
        reinterpret_cast<JSCell*>(current)->zap();
        clearNewlyAllocated(current);
    }
    
    m_state = Marked;
}

void MarkedBlock::clearMarks()
{
    if (heap()->operationInProgress() == JSC::EdenCollection)
        this->clearMarksWithCollectionType<EdenCollection>();
    else
        this->clearMarksWithCollectionType<FullCollection>();
}

template <HeapOperation collectionType>
void MarkedBlock::clearMarksWithCollectionType()
{
    ASSERT(collectionType == FullCollection || collectionType == EdenCollection);
    HEAP_LOG_BLOCK_STATE_TRANSITION(this);

    ASSERT(m_state != New && m_state != FreeListed);
    if (collectionType == FullCollection) {
        m_marks.clearAll();
        // This will become true at the end of the mark phase. We set it now to
        // avoid an extra pass to do so later.
        m_state = Marked;
        return;
    }

    ASSERT(collectionType == EdenCollection);
    // If a block was retired then there's no way an EdenCollection can un-retire it.
    if (m_state != Retired)
        m_state = Marked;
}

void MarkedBlock::lastChanceToFinalize()
{
    m_weakSet.lastChanceToFinalize();

    clearNewlyAllocated();
    clearMarksWithCollectionType<FullCollection>();
    sweep();
}

MarkedBlock::FreeList MarkedBlock::resumeAllocating()
{
    HEAP_LOG_BLOCK_STATE_TRANSITION(this);

    ASSERT(m_state == Marked);

    if (!m_newlyAllocated) {
        // We didn't have to create a "newly allocated" bitmap. That means we were already Marked
        // when we last stopped allocation, so return an empty free list and stay in the Marked state.
        return FreeList();
    }

    // Re-create our free list from before stopping allocation. 
    return sweep(SweepToFreeList);
}

void MarkedBlock::didRetireBlock(const FreeList& freeList)
{
    HEAP_LOG_BLOCK_STATE_TRANSITION(this);
    FreeCell* head = freeList.head;

    // Currently we don't notify the Heap that we're giving up on this block. 
    // The Heap might be able to make a better decision about how many bytes should 
    // be allocated before the next collection if it knew about this retired block.
    // On the other hand we'll waste at most 10% of our Heap space between FullCollections 
    // and only under heavy fragmentation.

    // We need to zap the free list when retiring a block so that we don't try to destroy 
    // previously destroyed objects when we re-sweep the block in the future.
    FreeCell* next;
    for (FreeCell* current = head; current; current = next) {
        next = current->next;
        reinterpret_cast<JSCell*>(current)->zap();
    }

    ASSERT(m_state == FreeListed);
    m_state = Retired;
}

} // namespace JSC
