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

const size_t MarkedBlock::blockSize;

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
    , m_newlyAllocatedVersion(MarkedSpace::nullVersion)
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
    : m_markingVersion(MarkedSpace::nullVersion)
    , m_handle(handle)
    , m_vm(&vm)
{
    if (false)
        dataLog(RawPointer(this), ": Allocated.\n");
}

template<MarkedBlock::Handle::EmptyMode emptyMode, MarkedBlock::Handle::SweepMode sweepMode, MarkedBlock::Handle::SweepDestructionMode destructionMode, MarkedBlock::Handle::ScribbleMode scribbleMode, MarkedBlock::Handle::NewlyAllocatedMode newlyAllocatedMode, MarkedBlock::Handle::MarksMode marksMode>
FreeList MarkedBlock::Handle::specializedSweep()
{
    RELEASE_ASSERT(!(destructionMode == BlockHasNoDestructors && sweepMode == SweepOnly));
    
    SuperSamplerScope superSamplerScope(false);

    MarkedBlock& block = this->block();
    
    if (false)
        dataLog(RawPointer(this), "/", RawPointer(&block), ": MarkedBlock::Handle::specializedSweep!\n");
    
    if (Options::useBumpAllocator()
        && emptyMode == IsEmpty
        && newlyAllocatedMode == DoesNotHaveNewlyAllocated) {
        
        // This is an incredibly powerful assertion that checks the sanity of our block bits.
        if (marksMode == MarksNotStale && !block.m_marks.isEmpty()) {
            WTF::dataFile().atomically(
                [&] (PrintStream& out) {
                    out.print("Block ", RawPointer(&block), ": marks not empty!\n");
                    out.print("Block lock is held: ", block.m_lock.isHeld(), "\n");
                    out.print("Marking version of block: ", block.m_markingVersion, "\n");
                    out.print("Marking version of heap: ", space()->markingVersion(), "\n");
                    UNREACHABLE_FOR_PLATFORM();
                });
        }
        
        char* startOfLastCell = static_cast<char*>(cellAlign(block.atoms() + m_endAtom - 1));
        char* payloadEnd = startOfLastCell + cellSize();
        RELEASE_ASSERT(payloadEnd - MarkedBlock::blockSize <= bitwise_cast<char*>(&block));
        char* payloadBegin = bitwise_cast<char*>(block.atoms() + firstAtom());
        if (scribbleMode == Scribble)
            scribble(payloadBegin, payloadEnd - payloadBegin);
        if (sweepMode == SweepToFreeList)
            setIsFreeListed();
        else
            m_allocator->setIsEmpty(NoLockingNecessary, this, true);
        if (space()->isMarking())
            block.m_lock.unlock();
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
    Vector<size_t> deadCells;
    auto handleDeadCell = [&] (size_t i) {
        HeapCell* cell = reinterpret_cast_ptr<HeapCell*>(&block.atoms()[i]);

        if (destructionMode != BlockHasNoDestructors && emptyMode == NotEmpty)
            static_cast<JSCell*>(cell)->callDestructor(*vm());

        if (sweepMode == SweepToFreeList) {
            FreeCell* freeCell = reinterpret_cast_ptr<FreeCell*>(cell);
            if (scribbleMode == Scribble)
                scribble(freeCell, cellSize());
            freeCell->next = head;
            head = freeCell;
            ++count;
        }
    };
    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
        if (emptyMode == NotEmpty
            && ((marksMode == MarksNotStale && block.m_marks.get(i))
                || (newlyAllocatedMode == HasNewlyAllocated && m_newlyAllocated.get(i)))) {
            isEmpty = false;
            continue;
        }
        
        if (destructionMode == BlockHasDestructorsAndCollectorIsRunning)
            deadCells.append(i);
        else
            handleDeadCell(i);
    }
    
    // We only want to discard the newlyAllocated bits if we're creating a FreeList,
    // otherwise we would lose information on what's currently alive.
    if (sweepMode == SweepToFreeList && newlyAllocatedMode == HasNewlyAllocated)
        m_newlyAllocatedVersion = MarkedSpace::nullVersion;
    
    if (space()->isMarking())
        block.m_lock.unlock();
    
    if (destructionMode == BlockHasDestructorsAndCollectorIsRunning) {
        for (size_t i : deadCells)
            handleDeadCell(i);
    }

    FreeList result = FreeList::list(head, count * cellSize());
    if (sweepMode == SweepToFreeList)
        setIsFreeListed();
    else if (isEmpty)
        m_allocator->setIsEmpty(NoLockingNecessary, this, true);
    if (false)
        dataLog("Slowly swept block ", RawPointer(&block), " with cell size ", cellSize(), " and attributes ", m_attributes, ": ", result, "\n");
    return result;
}

FreeList MarkedBlock::Handle::sweep(SweepMode sweepMode)
{
    m_allocator->setIsUnswept(NoLockingNecessary, this, false);
    
    m_weakSet.sweep();

    if (sweepMode == SweepOnly && m_attributes.destruction == DoesNotNeedDestruction)
        return FreeList();

    if (UNLIKELY(m_isFreeListed)) {
        RELEASE_ASSERT(sweepMode == SweepToFreeList);
        return FreeList();
    }
    
    ASSERT(!m_allocator->isAllocated(NoLockingNecessary, this));
    
    if (space()->isMarking())
        block().m_lock.lock();
    
    if (m_attributes.destruction == NeedsDestruction) {
        if (space()->isMarking())
            return sweepHelperSelectScribbleMode<BlockHasDestructorsAndCollectorIsRunning>(sweepMode);
        return sweepHelperSelectScribbleMode<BlockHasDestructors>(sweepMode);
    }
    return sweepHelperSelectScribbleMode<BlockHasNoDestructors>(sweepMode);
}

template<MarkedBlock::Handle::SweepDestructionMode destructionMode>
FreeList MarkedBlock::Handle::sweepHelperSelectScribbleMode(SweepMode sweepMode)
{
    if (scribbleFreeCells())
        return sweepHelperSelectEmptyMode<destructionMode, Scribble>(sweepMode);
    return sweepHelperSelectEmptyMode<destructionMode, DontScribble>(sweepMode);
}

template<MarkedBlock::Handle::SweepDestructionMode destructionMode, MarkedBlock::Handle::ScribbleMode scribbleMode>
FreeList MarkedBlock::Handle::sweepHelperSelectEmptyMode(SweepMode sweepMode)
{
    // It's not obvious, but this is the only way to know if the block is empty. It's the only
    // bit that captures these caveats:
    // - It's true when the block is freshly allocated.
    // - It's true if the block had been swept in the past, all destructors were called, and that
    //   sweep proved that the block is empty.
    // - It's false if there are any destructors that need to be called, even if the block has no
    //   live objects.
    if (m_allocator->isEmpty(NoLockingNecessary, this))
        return sweepHelperSelectHasNewlyAllocated<IsEmpty, destructionMode, scribbleMode>(sweepMode);
    return sweepHelperSelectHasNewlyAllocated<NotEmpty, destructionMode, scribbleMode>(sweepMode);
}

template<MarkedBlock::Handle::EmptyMode emptyMode, MarkedBlock::Handle::SweepDestructionMode destructionMode, MarkedBlock::Handle::ScribbleMode scribbleMode>
FreeList MarkedBlock::Handle::sweepHelperSelectHasNewlyAllocated(SweepMode sweepMode)
{
    if (hasAnyNewlyAllocated())
        return sweepHelperSelectSweepMode<emptyMode, destructionMode, scribbleMode, HasNewlyAllocated>(sweepMode);
    return sweepHelperSelectSweepMode<emptyMode, destructionMode, scribbleMode, DoesNotHaveNewlyAllocated>(sweepMode);
}

template<MarkedBlock::Handle::EmptyMode emptyMode, MarkedBlock::Handle::SweepDestructionMode destructionMode, MarkedBlock::Handle::ScribbleMode scribbleMode, MarkedBlock::Handle::NewlyAllocatedMode newlyAllocatedMode>
FreeList MarkedBlock::Handle::sweepHelperSelectSweepMode(SweepMode sweepMode)
{
    if (sweepMode == SweepToFreeList)
        return sweepHelperSelectMarksMode<emptyMode, SweepToFreeList, destructionMode, scribbleMode, newlyAllocatedMode>();
    return sweepHelperSelectMarksMode<emptyMode, SweepOnly, destructionMode, scribbleMode, newlyAllocatedMode>();
}

template<MarkedBlock::Handle::EmptyMode emptyMode, MarkedBlock::Handle::SweepMode sweepMode, MarkedBlock::Handle::SweepDestructionMode destructionMode, MarkedBlock::Handle::ScribbleMode scribbleMode, MarkedBlock::Handle::NewlyAllocatedMode newlyAllocatedMode>
FreeList MarkedBlock::Handle::sweepHelperSelectMarksMode()
{
    HeapVersion markingVersion = space()->markingVersion();
    bool marksAreUseful = !block().areMarksStale(markingVersion);
    
    if (space()->isMarking())
        marksAreUseful |= block().marksConveyLivenessDuringMarking(markingVersion);
    
    if (!marksAreUseful)
        return specializedSweep<emptyMode, sweepMode, destructionMode, scribbleMode, newlyAllocatedMode, MarksStale>();
    return specializedSweep<emptyMode, sweepMode, destructionMode, scribbleMode, newlyAllocatedMode, MarksNotStale>();
}

void MarkedBlock::Handle::unsweepWithNoNewlyAllocated()
{
    RELEASE_ASSERT(m_isFreeListed);
    m_isFreeListed = false;
}

void MarkedBlock::Handle::setIsFreeListed()
{
    m_allocator->setIsEmpty(NoLockingNecessary, this, false);
    m_isFreeListed = true;
}

void MarkedBlock::Handle::stopAllocating(const FreeList& freeList)
{
    auto locker = holdLock(block().m_lock);
    
    if (false)
        dataLog(RawPointer(this), ": MarkedBlock::Handle::stopAllocating!\n");
    ASSERT(!allocator()->isAllocated(NoLockingNecessary, this));

    if (!isFreeListed()) {
        if (false)
            dataLog("There ain't no newly allocated.\n");
        // This means that we either didn't use this block at all for allocation since last GC,
        // or someone had already done stopAllocating() before.
        ASSERT(freeList.allocationWillFail());
        return;
    }
    
    if (false)
        dataLog("Free list: ", freeList, "\n");
    
    // Roll back to a coherent state for Heap introspection. Cells newly
    // allocated from our free list are not currently marked, so we need another
    // way to tell what's live vs dead. 
    
    m_newlyAllocated.clearAll();
    m_newlyAllocatedVersion = heap()->objectSpace().newlyAllocatedVersion();

    forEachCell(
        [&] (HeapCell* cell, HeapCell::Kind) -> IterationStatus {
            setNewlyAllocated(cell);
            return IterationStatus::Continue;
        });

    forEachFreeCell(
        freeList,
        [&] (HeapCell* cell) {
            if (false)
                dataLog("Free cell: ", RawPointer(cell), "\n");
            if (m_attributes.destruction == NeedsDestruction)
                cell->zap();
            clearNewlyAllocated(cell);
        });
    
    m_isFreeListed = false;
}

void MarkedBlock::Handle::lastChanceToFinalize()
{
    allocator()->setIsAllocated(NoLockingNecessary, this, false);
    m_block->m_marks.clearAll();
    m_block->clearHasAnyMarked();
    m_block->m_markingVersion = heap()->objectSpace().markingVersion();
    m_weakSet.lastChanceToFinalize();
    m_newlyAllocated.clearAll();
    m_newlyAllocatedVersion = heap()->objectSpace().newlyAllocatedVersion();
    sweep();
}

FreeList MarkedBlock::Handle::resumeAllocating()
{
    {
        auto locker = holdLock(block().m_lock);
        
        if (false)
            dataLog(RawPointer(this), ": MarkedBlock::Handle::resumeAllocating!\n");
        ASSERT(!allocator()->isAllocated(NoLockingNecessary, this));
        ASSERT(!isFreeListed());
        
        if (!hasAnyNewlyAllocated()) {
            if (false)
                dataLog("There ain't no newly allocated.\n");
            // This means we had already exhausted the block when we stopped allocation.
            return FreeList();
        }
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

void MarkedBlock::aboutToMarkSlow(HeapVersion markingVersion)
{
    ASSERT(vm()->heap.objectSpace().isMarking());
    LockHolder locker(m_lock);
    
    if (!areMarksStale(markingVersion))
        return;
    
    MarkedAllocator* allocator = handle().allocator();

    if (handle().allocator()->isAllocated(holdLock(allocator->bitvectorLock()), &handle())
        || !marksConveyLivenessDuringMarking(markingVersion)) {
        if (false)
            dataLog(RawPointer(this), ": Clearing marks without doing anything else.\n");
        // We already know that the block is full and is already recognized as such, or that the
        // block did not survive the previous GC. So, we can clear mark bits the old fashioned
        // way. Note that it's possible for such a block to have newlyAllocated with an up-to-
        // date version! If it does, then we want to leave the newlyAllocated alone, since that
        // means that we had allocated in this previously empty block but did not fill it up, so
        // we created a newlyAllocated.
        m_marks.clearAll();
    } else {
        if (false)
            dataLog(RawPointer(this), ": Doing things.\n");
        HeapVersion newlyAllocatedVersion = space()->newlyAllocatedVersion();
        if (handle().m_newlyAllocatedVersion == newlyAllocatedVersion) {
            // Merge the contents of marked into newlyAllocated. If we get the full set of bits
            // then invalidate newlyAllocated and set allocated.
            handle().m_newlyAllocated.mergeAndClear(m_marks);
        } else {
            // Replace the contents of newlyAllocated with marked. If we get the full set of
            // bits then invalidate newlyAllocated and set allocated.
            handle().m_newlyAllocated.setAndClear(m_marks);
        }
        handle().m_newlyAllocatedVersion = newlyAllocatedVersion;
    }
    clearHasAnyMarked();
    WTF::storeStoreFence();
    m_markingVersion = markingVersion;
    
    // This means we're the first ones to mark any object in this block.
    allocator->setIsMarkingNotEmpty(holdLock(allocator->bitvectorLock()), &handle(), true);
}

void MarkedBlock::Handle::resetAllocated()
{
    m_newlyAllocated.clearAll();
    m_newlyAllocatedVersion = MarkedSpace::nullVersion;
}

void MarkedBlock::resetMarks()
{
    // We want aboutToMarkSlow() to see what the mark bits were after the last collection. It uses
    // the version number to distinguish between the marks having already been stale before
    // beginMarking(), or just stale now that beginMarking() bumped the version. If we have a version
    // wraparound, then we will call this method before resetting the version to null. When the
    // version is null, aboutToMarkSlow() will assume that the marks were not stale as of before
    // beginMarking(). Hence the need to whip the marks into shape.
    if (areMarksStale())
        m_marks.clearAll();
    m_markingVersion = MarkedSpace::nullVersion;
}

#if !ASSERT_DISABLED
void MarkedBlock::assertMarksNotStale()
{
    ASSERT(m_markingVersion == vm()->heap.objectSpace().markingVersion());
}
#endif // !ASSERT_DISABLED

bool MarkedBlock::areMarksStale()
{
    return areMarksStale(vm()->heap.objectSpace().markingVersion());
}

bool MarkedBlock::Handle::areMarksStale()
{
    return m_block->areMarksStale();
}

bool MarkedBlock::isMarked(const void* p)
{
    return isMarked(vm()->heap.objectSpace().markingVersion(), p);
}

void MarkedBlock::Handle::didConsumeFreeList()
{
    auto locker = holdLock(block().m_lock);
    if (false)
        dataLog(RawPointer(this), ": MarkedBlock::Handle::didConsumeFreeList!\n");
    ASSERT(isFreeListed());
    m_isFreeListed = false;
    allocator()->setIsAllocated(NoLockingNecessary, this, true);
}

size_t MarkedBlock::markCount()
{
    return areMarksStale() ? 0 : m_marks.count();
}

bool MarkedBlock::Handle::isEmpty()
{
    return m_allocator->isEmpty(NoLockingNecessary, this);
}

void MarkedBlock::clearHasAnyMarked()
{
    m_biasedMarkCount = m_markCountBias;
}

void MarkedBlock::noteMarkedSlow()
{
    MarkedAllocator* allocator = handle().allocator();
    allocator->setIsMarkingRetired(holdLock(allocator->bitvectorLock()), &handle(), true);
}

void MarkedBlock::Handle::removeFromAllocator()
{
    if (!m_allocator)
        return;
    
    m_allocator->removeBlock(this);
}

void MarkedBlock::updateNeedsDestruction()
{
    m_needsDestruction = handle().needsDestruction();
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
    
    block().updateNeedsDestruction();
    
    double markCountBias = -(Options::minMarkedBlockUtilization() * cellsPerBlock());
    
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
    return isLive(space()->markingVersion(), space()->isMarking(), cell);
}

bool MarkedBlock::Handle::isLiveCell(const void* p)
{
    return isLiveCell(space()->markingVersion(), space()->isMarking(), p);
}

#if !ASSERT_DISABLED
void MarkedBlock::assertValidCell(VM& vm, HeapCell* cell) const
{
    RELEASE_ASSERT(&vm == this->vm());
    RELEASE_ASSERT(const_cast<MarkedBlock*>(this)->handle().cellAlign(cell) == cell);
}
#endif

void MarkedBlock::Handle::dumpState(PrintStream& out)
{
    CommaPrinter comma;
    allocator()->forEachBitVectorWithName(
        holdLock(allocator()->bitvectorLock()),
        [&] (FastBitVector& bitvector, const char* name) {
            out.print(comma, name, ":", bitvector[index()] ? "YES" : "no");
        });
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

