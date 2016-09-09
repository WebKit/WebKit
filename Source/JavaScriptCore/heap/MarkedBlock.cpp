/*
 * Copyright (C) 2011, 2016 Apple Inc. All rights reserved.
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
#include "SuperSampler.h"

namespace JSC {

static const bool computeBalance = false;
static size_t balance;

MarkedBlock::Handle* MarkedBlock::tryCreate(Heap& heap, MarkedAllocator* allocator, size_t cellSize, const AllocatorAttributes& attributes)
{
    if (computeBalance) {
        balance++;
        if (!(balance % 10))
            dataLog("MarkedBlock Balance: ", balance, "\n");
    }
    void* blockSpace = tryFastAlignedMalloc(blockSize, blockSize);
    if (!blockSpace)
        return nullptr;
    if (scribbleFreeCells())
        scribble(blockSpace, blockSize);
    return new Handle(heap, allocator, cellSize, attributes, blockSpace);
}

MarkedBlock::Handle::Handle(Heap& heap, MarkedAllocator* allocator, size_t cellSize, const AllocatorAttributes& attributes, void* blockSpace)
    : m_atomsPerCell((cellSize + atomSize - 1) / atomSize)
    , m_endAtom(atomsPerBlock - m_atomsPerCell + 1)
    , m_attributes(attributes)
    , m_state(New) // All cells start out unmarked.
    , m_allocator(allocator)
    , m_weakSet(allocator->heap()->vm(), CellContainer())
{
    m_block = new (NotNull, blockSpace) MarkedBlock(*heap.vm(), *this);
    
    m_weakSet.setContainer(*m_block);
    
    heap.didAllocateBlock(blockSize);
    HEAP_LOG_BLOCK_STATE_TRANSITION(this);
    ASSERT(allocator);
    if (m_attributes.cellKind != HeapCell::JSCell)
        RELEASE_ASSERT(m_attributes.destruction == DoesNotNeedDestruction);
}

MarkedBlock::Handle::~Handle()
{
    Heap& heap = *this->heap();
    if (computeBalance) {
        balance--;
        if (!(balance % 10))
            dataLog("MarkedBlock Balance: ", balance, "\n");
    }
    m_block->~MarkedBlock();
    fastAlignedFree(m_block);
    heap.didFreeBlock(blockSize);
}

MarkedBlock::MarkedBlock(VM& vm, Handle& handle)
    : m_needsDestruction(handle.needsDestruction())
    , m_handle(handle)
    , m_vm(&vm)
    , m_version(vm.heap.objectSpace().version())
{
    unsigned cellsPerBlock = MarkedSpace::blockPayload / handle.cellSize();
    double markCountBias = -(Options::minMarkedBlockUtilization() * cellsPerBlock);
    
    // The mark count bias should be comfortably within this range.
    RELEASE_ASSERT(markCountBias > static_cast<double>(std::numeric_limits<int16_t>::min()));
    RELEASE_ASSERT(markCountBias < 0);
    
    m_markCountBias = static_cast<int16_t>(markCountBias);
    
    m_biasedMarkCount = m_markCountBias; // This means we haven't marked anything yet.
}

template<MarkedBlock::BlockState blockState, MarkedBlock::Handle::SweepMode sweepMode, DestructionMode destructionMode, MarkedBlock::Handle::ScribbleMode scribbleMode, MarkedBlock::Handle::NewlyAllocatedMode newlyAllocatedMode>
FreeList MarkedBlock::Handle::specializedSweep()
{
    SuperSamplerScope superSamplerScope(false);
    ASSERT(blockState == New || blockState == Marked);
    ASSERT(!(destructionMode == DoesNotNeedDestruction && sweepMode == SweepOnly));
    
    assertFlipped();
    MarkedBlock& block = this->block();
    
    bool isNewBlock = blockState == New;
    bool isEmptyBlock = !block.hasAnyMarked()
        && newlyAllocatedMode == DoesNotHaveNewlyAllocated
        && destructionMode == DoesNotNeedDestruction;
    if (Options::useBumpAllocator() && (isNewBlock || isEmptyBlock)) {
        ASSERT(block.m_marks.isEmpty());
        
        char* startOfLastCell = static_cast<char*>(cellAlign(block.atoms() + m_endAtom - 1));
        char* payloadEnd = startOfLastCell + cellSize();
        RELEASE_ASSERT(payloadEnd - MarkedBlock::blockSize <= bitwise_cast<char*>(&block));
        char* payloadBegin = bitwise_cast<char*>(block.atoms() + firstAtom());
        if (scribbleMode == Scribble)
            scribble(payloadBegin, payloadEnd - payloadBegin);
        m_state = ((sweepMode == SweepToFreeList) ? FreeListed : Marked);
        FreeList result = FreeList::bump(payloadEnd, payloadEnd - payloadBegin);
        if (false)
            dataLog("Quickly swept block ", RawPointer(this), " with cell size ", cellSize(), " and attributes ", m_attributes, ": ", result, "\n");
        return result;
    }

    // This produces a free list that is ordered in reverse through the block.
    // This is fine, since the allocation code makes no assumptions about the
    // order of the free list.
    FreeCell* head = 0;
    size_t count = 0;
    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
        if (blockState == Marked
            && (block.m_marks.get(i)
                || (newlyAllocatedMode == HasNewlyAllocated && m_newlyAllocated->get(i))))
            continue;

        HeapCell* cell = reinterpret_cast_ptr<HeapCell*>(&block.atoms()[i]);

        if (destructionMode == NeedsDestruction && blockState != New)
            static_cast<JSCell*>(cell)->callDestructor(*vm());

        if (sweepMode == SweepToFreeList) {
            FreeCell* freeCell = reinterpret_cast<FreeCell*>(cell);
            if (scribbleMode == Scribble)
                scribble(freeCell, cellSize());
            freeCell->next = head;
            head = freeCell;
            ++count;
        }
    }

    // We only want to discard the newlyAllocated bits if we're creating a FreeList,
    // otherwise we would lose information on what's currently alive.
    if (sweepMode == SweepToFreeList && newlyAllocatedMode == HasNewlyAllocated)
        m_newlyAllocated = nullptr;

    FreeList result = FreeList::list(head, count * cellSize());
    m_state = (sweepMode == SweepToFreeList ? FreeListed : Marked);
    if (false)
        dataLog("Slowly swept block ", RawPointer(&block), " with cell size ", cellSize(), " and attributes ", m_attributes, ": ", result, "\n");
    return result;
}

FreeList MarkedBlock::Handle::sweep(SweepMode sweepMode)
{
    flipIfNecessary();
    
    HEAP_LOG_BLOCK_STATE_TRANSITION(this);

    m_weakSet.sweep();

    if (sweepMode == SweepOnly && m_attributes.destruction == DoesNotNeedDestruction)
        return FreeList();

    if (m_attributes.destruction == NeedsDestruction)
        return sweepHelperSelectScribbleMode<NeedsDestruction>(sweepMode);
    return sweepHelperSelectScribbleMode<DoesNotNeedDestruction>(sweepMode);
}

template<DestructionMode destructionMode>
FreeList MarkedBlock::Handle::sweepHelperSelectScribbleMode(SweepMode sweepMode)
{
    if (scribbleFreeCells())
        return sweepHelperSelectStateAndSweepMode<destructionMode, Scribble>(sweepMode);
    return sweepHelperSelectStateAndSweepMode<destructionMode, DontScribble>(sweepMode);
}

template<DestructionMode destructionMode, MarkedBlock::Handle::ScribbleMode scribbleMode>
FreeList MarkedBlock::Handle::sweepHelperSelectStateAndSweepMode(SweepMode sweepMode)
{
    switch (m_state) {
    case New:
        ASSERT(sweepMode == SweepToFreeList);
        return specializedSweep<New, SweepToFreeList, destructionMode, scribbleMode, DoesNotHaveNewlyAllocated>();
    case FreeListed:
        // Happens when a block transitions to fully allocated.
        ASSERT(sweepMode == SweepToFreeList);
        return FreeList();
    case Allocated:
        RELEASE_ASSERT_NOT_REACHED();
        return FreeList();
    case Marked:
        if (m_newlyAllocated) {
            return sweepMode == SweepToFreeList
                ? specializedSweep<Marked, SweepToFreeList, destructionMode, scribbleMode, HasNewlyAllocated>()
                : specializedSweep<Marked, SweepOnly, destructionMode, scribbleMode, HasNewlyAllocated>();
        } else {
            return sweepMode == SweepToFreeList
                ? specializedSweep<Marked, SweepToFreeList, destructionMode, scribbleMode, DoesNotHaveNewlyAllocated>()
                : specializedSweep<Marked, SweepOnly, destructionMode, scribbleMode, DoesNotHaveNewlyAllocated>();
        }
    }
    RELEASE_ASSERT_NOT_REACHED();
    return FreeList();
}

void MarkedBlock::Handle::unsweepWithNoNewlyAllocated()
{
    flipIfNecessary();
    
    HEAP_LOG_BLOCK_STATE_TRANSITION(this);
    
    RELEASE_ASSERT(m_state == FreeListed);
    m_state = Marked;
}

class SetNewlyAllocatedFunctor : public MarkedBlock::VoidFunctor {
public:
    SetNewlyAllocatedFunctor(MarkedBlock::Handle* block)
        : m_block(block)
    {
    }

    IterationStatus operator()(HeapCell* cell, HeapCell::Kind) const
    {
        ASSERT(MarkedBlock::blockFor(cell) == &m_block->block());
        m_block->setNewlyAllocated(cell);
        return IterationStatus::Continue;
    }

private:
    MarkedBlock::Handle* m_block;
};

void MarkedBlock::Handle::stopAllocating(const FreeList& freeList)
{
    flipIfNecessary();
    HEAP_LOG_BLOCK_STATE_TRANSITION(this);

    if (m_state == Marked) {
        // If the block is in the Marked state then we know that one of these
        // conditions holds:
        //
        // - It was not used for allocation during the previous allocation cycle.
        //   It may have dead objects, and we only know them to be dead by the
        //   fact that their mark bits are unset.
        //
        // - Someone had already done stopAllocating(), for example because of
        //   heap iteration, and they had already 
        // Hence if the block is Marked we need to leave it Marked.
        ASSERT(freeList.allocationWillFail());
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

    forEachFreeCell(
        freeList,
        [&] (HeapCell* cell) {
            if (m_attributes.destruction == NeedsDestruction)
                cell->zap();
            clearNewlyAllocated(cell);
        });
    
    m_state = Marked;
}

void MarkedBlock::Handle::lastChanceToFinalize()
{
    m_block->clearMarks();
    m_weakSet.lastChanceToFinalize();

    clearNewlyAllocated();
    sweep();
}

FreeList MarkedBlock::Handle::resumeAllocating()
{
    flipIfNecessary();
    HEAP_LOG_BLOCK_STATE_TRANSITION(this);

    ASSERT(m_state == Marked);

    if (!m_newlyAllocated) {
        // We didn't have to create a "newly allocated" bitmap. That means we were already Marked
        // when we last stopped allocation, so return an empty free list and stay in the Marked state.
        return FreeList();
    }

    // Re-create our free list from before stopping allocation. Note that this may return an empty
    // freelist, in which case the block will still be Marked!
    return sweep(SweepToFreeList);
}

void MarkedBlock::Handle::zap(const FreeList& freeList)
{
    forEachFreeCell(
        freeList,
        [&] (HeapCell* cell) {
            if (m_attributes.destruction == NeedsDestruction)
                cell->zap();
        });
}

template<typename Func>
void MarkedBlock::Handle::forEachFreeCell(const FreeList& freeList, const Func& func)
{
    if (freeList.remaining) {
        for (unsigned remaining = freeList.remaining; remaining; remaining -= cellSize())
            func(bitwise_cast<HeapCell*>(freeList.payloadEnd - remaining));
    } else {
        for (FreeCell* current = freeList.head; current;) {
            FreeCell* next = current->next;
            func(bitwise_cast<HeapCell*>(current));
            current = next;
        }
    }
}

void MarkedBlock::flipIfNecessary()
{
    flipIfNecessary(vm()->heap.objectSpace().version());
}

void MarkedBlock::Handle::flipIfNecessary()
{
    block().flipIfNecessary();
}

void MarkedBlock::flipIfNecessarySlow()
{
    ASSERT(m_version != vm()->heap.objectSpace().version());
    clearMarks();
}

void MarkedBlock::flipIfNecessaryConcurrentlySlow()
{
    LockHolder locker(m_lock);
    if (m_version != vm()->heap.objectSpace().version())
        clearMarks();
}

void MarkedBlock::clearMarks()
{
    m_marks.clearAll();
    clearHasAnyMarked();
    // This will become true at the end of the mark phase. We set it now to
    // avoid an extra pass to do so later.
    handle().m_state = Marked;
    WTF::storeStoreFence();
    m_version = vm()->heap.objectSpace().version();
}

#if !ASSERT_DISABLED
void MarkedBlock::assertFlipped()
{
    ASSERT(m_version == vm()->heap.objectSpace().version());
}
#endif // !ASSERT_DISABLED

bool MarkedBlock::needsFlip()
{
    return vm()->heap.objectSpace().version() != m_version;
}

bool MarkedBlock::Handle::needsFlip()
{
    return m_block->needsFlip();
}

void MarkedBlock::Handle::willRemoveBlock()
{
    flipIfNecessary();
}

void MarkedBlock::Handle::didConsumeFreeList()
{
    flipIfNecessary();
    HEAP_LOG_BLOCK_STATE_TRANSITION(this);
    
    ASSERT(m_state == FreeListed);

    m_state = Allocated;
}

size_t MarkedBlock::markCount()
{
    flipIfNecessary();
    return m_marks.count();
}

bool MarkedBlock::Handle::isEmpty()
{
    flipIfNecessary();
    return m_state == Marked && !block().hasAnyMarked() && m_weakSet.isEmpty() && (!m_newlyAllocated || m_newlyAllocated->isEmpty());
}

void MarkedBlock::clearHasAnyMarked()
{
    m_biasedMarkCount = m_markCountBias;
}

void MarkedBlock::noteMarkedSlow()
{
    handle().m_allocator->retire(&handle());
}

} // namespace JSC

namespace WTF {

using namespace JSC;

void printInternal(PrintStream& out, MarkedBlock::BlockState blockState)
{
    switch (blockState) {
    case MarkedBlock::New:
        out.print("New");
        return;
    case MarkedBlock::FreeListed:
        out.print("FreeListed");
        return;
    case MarkedBlock::Allocated:
        out.print("Allocated");
        return;
    case MarkedBlock::Marked:
        out.print("Marked");
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WTF

