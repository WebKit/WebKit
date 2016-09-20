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
#include "MarkedBlockInlines.h"
#include "SuperSampler.h"

namespace JSC {

static const bool computeBalance = false;
static size_t balance;

MarkedBlock::Handle* MarkedBlock::tryCreate(Heap& heap)
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
    return new Handle(heap, blockSpace);
}

MarkedBlock::Handle::Handle(Heap& heap, void* blockSpace)
    : m_weakSet(heap.vm(), CellContainer())
{
    m_block = new (NotNull, blockSpace) MarkedBlock(*heap.vm(), *this);
    
    m_weakSet.setContainer(*m_block);
    
    heap.didAllocateBlock(blockSize);
}

MarkedBlock::Handle::~Handle()
{
    Heap& heap = *this->heap();
    if (computeBalance) {
        balance--;
        if (!(balance % 10))
            dataLog("MarkedBlock Balance: ", balance, "\n");
    }
    removeFromAllocator();
    m_block->~MarkedBlock();
    fastAlignedFree(m_block);
    heap.didFreeBlock(blockSize);
}

MarkedBlock::MarkedBlock(VM& vm, Handle& handle)
    : m_version(MarkedSpace::nullVersion)
    , m_handle(handle)
    , m_vm(&vm)
{
}

template<MarkedBlock::Handle::EmptyMode emptyMode, MarkedBlock::Handle::SweepMode sweepMode, DestructionMode destructionMode, MarkedBlock::Handle::ScribbleMode scribbleMode, MarkedBlock::Handle::NewlyAllocatedMode newlyAllocatedMode, MarkedBlock::Handle::FlipMode flipMode>
FreeList MarkedBlock::Handle::specializedSweep()
{
    RELEASE_ASSERT(!(destructionMode == DoesNotNeedDestruction && sweepMode == SweepOnly));
    
    SuperSamplerScope superSamplerScope(false);

    MarkedBlock& block = this->block();
    
    if (false)
        dataLog(RawPointer(this), ": MarkedBlock::Handle::specializedSweep!\n");
    
    if (Options::useBumpAllocator()
        && emptyMode == IsEmpty
        && newlyAllocatedMode == DoesNotHaveNewlyAllocated) {
        ASSERT(flipMode == NeedsFlip);
        
        char* startOfLastCell = static_cast<char*>(cellAlign(block.atoms() + m_endAtom - 1));
        char* payloadEnd = startOfLastCell + cellSize();
        RELEASE_ASSERT(payloadEnd - MarkedBlock::blockSize <= bitwise_cast<char*>(&block));
        char* payloadBegin = bitwise_cast<char*>(block.atoms() + firstAtom());
        if (scribbleMode == Scribble)
            scribble(payloadBegin, payloadEnd - payloadBegin);
        if (sweepMode == SweepToFreeList)
            setIsFreeListed();
        else
            m_allocator->setIsEmpty(this, true);
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
    bool isEmpty = true;
    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
        if (emptyMode == NotEmpty
            && ((flipMode == DoesNotNeedFlip && block.m_marks.get(i))
                || (newlyAllocatedMode == HasNewlyAllocated && m_newlyAllocated->get(i)))) {
            isEmpty = false;
            continue;
        }
        
        HeapCell* cell = reinterpret_cast_ptr<HeapCell*>(&block.atoms()[i]);

        if (destructionMode == NeedsDestruction && emptyMode == NotEmpty)
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
    if (sweepMode == SweepToFreeList)
        setIsFreeListed();
    else if (isEmpty)
        m_allocator->setIsEmpty(this, true);
    if (false)
        dataLog("Slowly swept block ", RawPointer(&block), " with cell size ", cellSize(), " and attributes ", m_attributes, ": ", result, "\n");
    return result;
}

FreeList MarkedBlock::Handle::sweep(SweepMode sweepMode)
{
    m_allocator->setIsUnswept(this, false);
    
    m_weakSet.sweep();

    if (sweepMode == SweepOnly && m_attributes.destruction == DoesNotNeedDestruction)
        return FreeList();

    if (UNLIKELY(m_isFreeListed)) {
        RELEASE_ASSERT(sweepMode == SweepToFreeList);
        return FreeList();
    }
    
    ASSERT(!m_allocator->isAllocated(this));
    
    if (m_attributes.destruction == NeedsDestruction)
        return sweepHelperSelectScribbleMode<NeedsDestruction>(sweepMode);
    return sweepHelperSelectScribbleMode<DoesNotNeedDestruction>(sweepMode);
}

template<DestructionMode destructionMode>
FreeList MarkedBlock::Handle::sweepHelperSelectScribbleMode(SweepMode sweepMode)
{
    if (scribbleFreeCells())
        return sweepHelperSelectEmptyMode<destructionMode, Scribble>(sweepMode);
    return sweepHelperSelectEmptyMode<destructionMode, DontScribble>(sweepMode);
}

template<DestructionMode destructionMode, MarkedBlock::Handle::ScribbleMode scribbleMode>
FreeList MarkedBlock::Handle::sweepHelperSelectEmptyMode(SweepMode sweepMode)
{
    // It's not obvious, but this is the only way to know if the block is empty. It's the only
    // bit that captures these caveats:
    // - It's true when the block is freshly allocated.
    // - It's true if the block had been swept in the past, all destructors were called, and that
    //   sweep proved that the block is empty.
    // - It's false if there are any destructors that need to be called, even if the block has no
    //   live objects.
    if (m_allocator->isEmpty(this))
        return sweepHelperSelectHasNewlyAllocated<IsEmpty, destructionMode, scribbleMode>(sweepMode);
    return sweepHelperSelectHasNewlyAllocated<NotEmpty, destructionMode, scribbleMode>(sweepMode);
}

template<MarkedBlock::Handle::EmptyMode emptyMode, DestructionMode destructionMode, MarkedBlock::Handle::ScribbleMode scribbleMode>
FreeList MarkedBlock::Handle::sweepHelperSelectHasNewlyAllocated(SweepMode sweepMode)
{
    if (m_newlyAllocated)
        return sweepHelperSelectSweepMode<emptyMode, destructionMode, scribbleMode, HasNewlyAllocated>(sweepMode);
    return sweepHelperSelectSweepMode<emptyMode, destructionMode, scribbleMode, DoesNotHaveNewlyAllocated>(sweepMode);
}

template<MarkedBlock::Handle::EmptyMode emptyMode, DestructionMode destructionMode, MarkedBlock::Handle::ScribbleMode scribbleMode, MarkedBlock::Handle::NewlyAllocatedMode newlyAllocatedMode>
FreeList MarkedBlock::Handle::sweepHelperSelectSweepMode(SweepMode sweepMode)
{
    if (sweepMode == SweepToFreeList)
        return sweepHelperSelectFlipMode<emptyMode, SweepToFreeList, destructionMode, scribbleMode, newlyAllocatedMode>();
    return sweepHelperSelectFlipMode<emptyMode, SweepOnly, destructionMode, scribbleMode, newlyAllocatedMode>();
}

template<MarkedBlock::Handle::EmptyMode emptyMode, MarkedBlock::Handle::SweepMode sweepMode, DestructionMode destructionMode, MarkedBlock::Handle::ScribbleMode scribbleMode, MarkedBlock::Handle::NewlyAllocatedMode newlyAllocatedMode>
FreeList MarkedBlock::Handle::sweepHelperSelectFlipMode()
{
    if (needsFlip())
        return specializedSweep<emptyMode, sweepMode, destructionMode, scribbleMode, newlyAllocatedMode, NeedsFlip>();
    return specializedSweep<emptyMode, sweepMode, destructionMode, scribbleMode, newlyAllocatedMode, DoesNotNeedFlip>();
}

void MarkedBlock::Handle::unsweepWithNoNewlyAllocated()
{
    RELEASE_ASSERT(m_isFreeListed);
    m_isFreeListed = false;
}

void MarkedBlock::Handle::setIsFreeListed()
{
    m_allocator->setIsEmpty(this, false);
    m_isFreeListed = true;
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
    if (false)
        dataLog(RawPointer(this), ": MarkedBlock::Handle::stopAllocating!\n");
    ASSERT(!allocator()->isAllocated(this));

    if (!isFreeListed()) {
        // This means that we either didn't use this block at all for allocation since last GC,
        // or someone had already done stopAllocating() before.
        ASSERT(freeList.allocationWillFail());
        return;
    }
    
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
    
    m_isFreeListed = false;
}

void MarkedBlock::Handle::lastChanceToFinalize()
{
    allocator()->setIsAllocated(this, false);
    m_block->clearMarks();
    m_weakSet.lastChanceToFinalize();

    clearNewlyAllocated();
    sweep();
}

FreeList MarkedBlock::Handle::resumeAllocating()
{
    ASSERT(!allocator()->isAllocated(this));
    ASSERT(!isFreeListed());

    if (!m_newlyAllocated) {
        // This means we had already exhausted the block when we stopped allocation.
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

void MarkedBlock::aboutToMarkSlow(HeapVersion heapVersion)
{
    ASSERT(vm()->heap.objectSpace().isMarking());
    LockHolder locker(m_lock);
    if (needsFlip(heapVersion)) {
        clearMarks(heapVersion);
        // This means we're the first ones to mark any object in this block.
        handle().allocator()->atomicSetAndCheckIsMarkingNotEmpty(&handle(), true);
    }
}

void MarkedBlock::clearMarks()
{
    clearMarks(vm()->heap.objectSpace().version());
}

void MarkedBlock::clearMarks(HeapVersion heapVersion)
{
    m_marks.clearAll();
    clearHasAnyMarked();
    WTF::storeStoreFence();
    m_version = heapVersion;
}

#if !ASSERT_DISABLED
void MarkedBlock::assertFlipped()
{
    ASSERT(m_version == vm()->heap.objectSpace().version());
}
#endif // !ASSERT_DISABLED

bool MarkedBlock::needsFlip()
{
    return needsFlip(vm()->heap.objectSpace().version());
}

bool MarkedBlock::Handle::needsFlip()
{
    return m_block->needsFlip();
}

bool MarkedBlock::isMarked(const void* p)
{
    return isMarked(vm()->heap.objectSpace().version(), p);
}

bool MarkedBlock::Handle::isMarkedOrNewlyAllocated(const HeapCell* cell)
{
    return isMarkedOrNewlyAllocated(vm()->heap.objectSpace().version(), cell);
}

bool MarkedBlock::isMarkedOrNewlyAllocated(const HeapCell* cell)
{
    return isMarkedOrNewlyAllocated(vm()->heap.objectSpace().version(), cell);
}

void MarkedBlock::Handle::didConsumeFreeList()
{
    if (false)
        dataLog(RawPointer(this), ": MarkedBlock::Handle::didConsumeFreeList!\n");
    ASSERT(isFreeListed());
    m_isFreeListed = false;
    allocator()->setIsAllocated(this, true);
}

size_t MarkedBlock::markCount()
{
    return needsFlip() ? 0 : m_marks.count();
}

bool MarkedBlock::Handle::isEmpty()
{
    return m_allocator->isEmpty(this);
}

void MarkedBlock::clearHasAnyMarked()
{
    m_biasedMarkCount = m_markCountBias;
}

void MarkedBlock::noteMarkedSlow()
{
    handle().allocator()->atomicSetAndCheckIsMarkingRetired(&handle(), true);
}

void MarkedBlock::Handle::removeFromAllocator()
{
    if (!m_allocator)
        return;
    
    m_allocator->removeBlock(this);
}

void MarkedBlock::Handle::didAddToAllocator(MarkedAllocator* allocator, size_t index)
{
    ASSERT(m_index == std::numeric_limits<size_t>::max());
    ASSERT(!m_allocator);
    
    m_index = index;
    m_allocator = allocator;
    
    size_t cellSize = allocator->cellSize();
    m_atomsPerCell = (cellSize + atomSize - 1) / atomSize;
    m_endAtom = atomsPerBlock - m_atomsPerCell + 1;
    
    m_attributes = allocator->attributes();

    if (m_attributes.cellKind != HeapCell::JSCell)
        RELEASE_ASSERT(m_attributes.destruction == DoesNotNeedDestruction);
    
    block().m_needsDestruction = needsDestruction();
    
    unsigned cellsPerBlock = MarkedSpace::blockPayload / cellSize;
    double markCountBias = -(Options::minMarkedBlockUtilization() * cellsPerBlock);
    
    // The mark count bias should be comfortably within this range.
    RELEASE_ASSERT(markCountBias > static_cast<double>(std::numeric_limits<int16_t>::min()));
    RELEASE_ASSERT(markCountBias < 0);
    
    // This means we haven't marked anything yet.
    block().m_biasedMarkCount = block().m_markCountBias = static_cast<int16_t>(markCountBias);
}

void MarkedBlock::Handle::didRemoveFromAllocator()
{
    ASSERT(m_index != std::numeric_limits<size_t>::max());
    ASSERT(m_allocator);
    
    m_index = std::numeric_limits<size_t>::max();
    m_allocator = nullptr;
}

bool MarkedBlock::Handle::isLive(const HeapCell* cell)
{
    return isLive(vm()->heap.objectSpace().version(), cell);
}

bool MarkedBlock::Handle::isLiveCell(const void* p)
{
    return isLiveCell(vm()->heap.objectSpace().version(), p);
}

} // namespace JSC

namespace WTF {

void printInternal(PrintStream& out, JSC::MarkedBlock::Handle::SweepMode mode)
{
    switch (mode) {
    case JSC::MarkedBlock::Handle::SweepToFreeList:
        out.print("SweepToFreeList");
        return;
    case JSC::MarkedBlock::Handle::SweepOnly:
        out.print("SweepOnly");
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WTF

