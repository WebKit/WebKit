/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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

#include "AlignedMemoryAllocator.h"
#include "BlockDirectoryInlines.h"
#include "FreeListInlines.h"
#include "JSCast.h"
#include "JSDestructibleObject.h"
#include "JSCInlines.h"
#include "MarkedBlockInlines.h"
#include "SuperSampler.h"
#include "SweepingScope.h"
#include <wtf/CommaPrinter.h>

namespace JSC {
namespace MarkedBlockInternal {
static constexpr bool verbose = false;
}

const size_t MarkedBlock::blockSize;

static const bool computeBalance = false;
static size_t balance;

MarkedBlock::Handle* MarkedBlock::tryCreate(Heap& heap, AlignedMemoryAllocator* alignedMemoryAllocator)
{
    if (computeBalance) {
        balance++;
        if (!(balance % 10))
            dataLog("MarkedBlock Balance: ", balance, "\n");
    }
    void* blockSpace = alignedMemoryAllocator->tryAllocateAlignedMemory(blockSize, blockSize);
    if (!blockSpace)
        return nullptr;
    if (scribbleFreeCells())
        scribble(blockSpace, blockSize);
    return new Handle(heap, alignedMemoryAllocator, blockSpace);
}

MarkedBlock::Handle::Handle(Heap& heap, AlignedMemoryAllocator* alignedMemoryAllocator, void* blockSpace)
    : m_alignedMemoryAllocator(alignedMemoryAllocator)
    , m_weakSet(heap.vm(), CellContainer())
{
    m_block = new (NotNull, blockSpace) MarkedBlock(heap.vm(), *this);
    
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
    removeFromDirectory();
    m_block->~MarkedBlock();
    m_alignedMemoryAllocator->freeAlignedMemory(m_block);
    heap.didFreeBlock(blockSize);
}

MarkedBlock::MarkedBlock(VM& vm, Handle& handle)
{
    new (&footer()) Footer(vm, handle);
    if (MarkedBlockInternal::verbose)
        dataLog(RawPointer(this), ": Allocated.\n");
}

MarkedBlock::~MarkedBlock()
{
    footer().~Footer();
}

MarkedBlock::Footer::Footer(VM& vm, Handle& handle)
    : m_handle(handle)
    , m_vm(&vm)
    , m_markingVersion(MarkedSpace::nullVersion)
    , m_newlyAllocatedVersion(MarkedSpace::nullVersion)
{
}

MarkedBlock::Footer::~Footer()
{
}

void MarkedBlock::Handle::unsweepWithNoNewlyAllocated()
{
    RELEASE_ASSERT(m_isFreeListed);
    m_isFreeListed = false;
}

void MarkedBlock::Handle::setIsFreeListed()
{
    m_directory->setIsEmpty(NoLockingNecessary, this, false);
    m_isFreeListed = true;
}

void MarkedBlock::Handle::stopAllocating(const FreeList& freeList)
{
    auto locker = holdLock(blockFooter().m_lock);
    
    if (MarkedBlockInternal::verbose)
        dataLog(RawPointer(this), ": MarkedBlock::Handle::stopAllocating!\n");
    ASSERT(!directory()->isAllocated(NoLockingNecessary, this));

    if (!isFreeListed()) {
        if (MarkedBlockInternal::verbose)
            dataLog("There ain't no newly allocated.\n");
        // This means that we either didn't use this block at all for allocation since last GC,
        // or someone had already done stopAllocating() before.
        ASSERT(freeList.allocationWillFail());
        return;
    }
    
    if (MarkedBlockInternal::verbose)
        dataLog("Free list: ", freeList, "\n");
    
    // Roll back to a coherent state for Heap introspection. Cells newly
    // allocated from our free list are not currently marked, so we need another
    // way to tell what's live vs dead. 
    
    blockFooter().m_newlyAllocated.clearAll();
    blockFooter().m_newlyAllocatedVersion = heap()->objectSpace().newlyAllocatedVersion();

    forEachCell(
        [&] (HeapCell* cell, HeapCell::Kind) -> IterationStatus {
            block().setNewlyAllocated(cell);
            return IterationStatus::Continue;
        });

    freeList.forEach(
        [&] (HeapCell* cell) {
            if (MarkedBlockInternal::verbose)
                dataLog("Free cell: ", RawPointer(cell), "\n");
            if (m_attributes.destruction == NeedsDestruction)
                cell->zap(HeapCell::StopAllocating);
            block().clearNewlyAllocated(cell);
        });
    
    m_isFreeListed = false;
}

void MarkedBlock::Handle::lastChanceToFinalize()
{
    directory()->setIsAllocated(NoLockingNecessary, this, false);
    directory()->setIsDestructible(NoLockingNecessary, this, true);
    blockFooter().m_marks.clearAll();
    block().clearHasAnyMarked();
    blockFooter().m_markingVersion = heap()->objectSpace().markingVersion();
    m_weakSet.lastChanceToFinalize();
    blockFooter().m_newlyAllocated.clearAll();
    blockFooter().m_newlyAllocatedVersion = heap()->objectSpace().newlyAllocatedVersion();
    sweep(nullptr);
}

void MarkedBlock::Handle::resumeAllocating(FreeList& freeList)
{
    {
        auto locker = holdLock(blockFooter().m_lock);
        
        if (MarkedBlockInternal::verbose)
            dataLog(RawPointer(this), ": MarkedBlock::Handle::resumeAllocating!\n");
        ASSERT(!directory()->isAllocated(NoLockingNecessary, this));
        ASSERT(!isFreeListed());
        
        if (!block().hasAnyNewlyAllocated()) {
            if (MarkedBlockInternal::verbose)
                dataLog("There ain't no newly allocated.\n");
            // This means we had already exhausted the block when we stopped allocation.
            freeList.clear();
            return;
        }
    }

    // Re-create our free list from before stopping allocation. Note that this may return an empty
    // freelist, in which case the block will still be Marked!
    sweep(&freeList);
}

void MarkedBlock::aboutToMarkSlow(HeapVersion markingVersion)
{
    ASSERT(vm().heap.objectSpace().isMarking());
    auto locker = holdLock(footer().m_lock);
    
    if (!areMarksStale(markingVersion))
        return;
    
    BlockDirectory* directory = handle().directory();

    if (handle().directory()->isAllocated(holdLock(directory->bitvectorLock()), &handle())
        || !marksConveyLivenessDuringMarking(markingVersion)) {
        if (MarkedBlockInternal::verbose)
            dataLog(RawPointer(this), ": Clearing marks without doing anything else.\n");
        // We already know that the block is full and is already recognized as such, or that the
        // block did not survive the previous GC. So, we can clear mark bits the old fashioned
        // way. Note that it's possible for such a block to have newlyAllocated with an up-to-
        // date version! If it does, then we want to leave the newlyAllocated alone, since that
        // means that we had allocated in this previously empty block but did not fill it up, so
        // we created a newlyAllocated.
        footer().m_marks.clearAll();
    } else {
        if (MarkedBlockInternal::verbose)
            dataLog(RawPointer(this), ": Doing things.\n");
        HeapVersion newlyAllocatedVersion = space()->newlyAllocatedVersion();
        if (footer().m_newlyAllocatedVersion == newlyAllocatedVersion) {
            // When do we get here? The block could not have been filled up. The newlyAllocated bits would
            // have had to be created since the end of the last collection. The only things that create
            // them are aboutToMarkSlow, lastChanceToFinalize, and stopAllocating. If it had been
            // aboutToMarkSlow, then we shouldn't be here since the marks wouldn't be stale anymore. It
            // cannot be lastChanceToFinalize. So it must be stopAllocating. That means that we just
            // computed the newlyAllocated bits just before the start of an increment. When we are in that
            // mode, it seems as if newlyAllocated should subsume marks.
            ASSERT(footer().m_newlyAllocated.subsumes(footer().m_marks));
            footer().m_marks.clearAll();
        } else {
            footer().m_newlyAllocated.setAndClear(footer().m_marks);
            footer().m_newlyAllocatedVersion = newlyAllocatedVersion;
        }
    }
    clearHasAnyMarked();
    WTF::storeStoreFence();
    footer().m_markingVersion = markingVersion;
    
    // This means we're the first ones to mark any object in this block.
    directory->setIsMarkingNotEmpty(holdLock(directory->bitvectorLock()), &handle(), true);
}

void MarkedBlock::resetAllocated()
{
    footer().m_newlyAllocated.clearAll();
    footer().m_newlyAllocatedVersion = MarkedSpace::nullVersion;
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
        footer().m_marks.clearAll();
    footer().m_markingVersion = MarkedSpace::nullVersion;
}

#if !ASSERT_DISABLED
void MarkedBlock::assertMarksNotStale()
{
    ASSERT(footer().m_markingVersion == vm().heap.objectSpace().markingVersion());
}
#endif // !ASSERT_DISABLED

bool MarkedBlock::areMarksStale()
{
    return areMarksStale(vm().heap.objectSpace().markingVersion());
}

bool MarkedBlock::Handle::areMarksStale()
{
    return m_block->areMarksStale();
}

bool MarkedBlock::isMarked(const void* p)
{
    return isMarked(vm().heap.objectSpace().markingVersion(), p);
}

void MarkedBlock::Handle::didConsumeFreeList()
{
    auto locker = holdLock(blockFooter().m_lock);
    if (MarkedBlockInternal::verbose)
        dataLog(RawPointer(this), ": MarkedBlock::Handle::didConsumeFreeList!\n");
    ASSERT(isFreeListed());
    m_isFreeListed = false;
    directory()->setIsAllocated(NoLockingNecessary, this, true);
}

size_t MarkedBlock::markCount()
{
    return areMarksStale() ? 0 : footer().m_marks.count();
}

void MarkedBlock::clearHasAnyMarked()
{
    footer().m_biasedMarkCount = footer().m_markCountBias;
}

void MarkedBlock::noteMarkedSlow()
{
    BlockDirectory* directory = handle().directory();
    directory->setIsMarkingRetired(holdLock(directory->bitvectorLock()), &handle(), true);
}

void MarkedBlock::Handle::removeFromDirectory()
{
    if (!m_directory)
        return;
    
    m_directory->removeBlock(this);
}

void MarkedBlock::Handle::didAddToDirectory(BlockDirectory* directory, size_t index)
{
    ASSERT(m_index == std::numeric_limits<size_t>::max());
    ASSERT(!m_directory);
    
    RELEASE_ASSERT(directory->subspace()->alignedMemoryAllocator() == m_alignedMemoryAllocator);
    
    m_index = index;
    m_directory = directory;
    blockFooter().m_subspace = directory->subspace();
    
    size_t cellSize = directory->cellSize();
    m_atomsPerCell = (cellSize + atomSize - 1) / atomSize;
    m_endAtom = endAtom - m_atomsPerCell + 1;
    
    m_attributes = directory->attributes();

    if (!isJSCellKind(m_attributes.cellKind))
        RELEASE_ASSERT(m_attributes.destruction == DoesNotNeedDestruction);
    
    double markCountBias = -(Options::minMarkedBlockUtilization() * cellsPerBlock());
    
    // The mark count bias should be comfortably within this range.
    RELEASE_ASSERT(markCountBias > static_cast<double>(std::numeric_limits<int16_t>::min()));
    RELEASE_ASSERT(markCountBias < 0);
    
    // This means we haven't marked anything yet.
    blockFooter().m_biasedMarkCount = blockFooter().m_markCountBias = static_cast<int16_t>(markCountBias);
}

void MarkedBlock::Handle::didRemoveFromDirectory()
{
    ASSERT(m_index != std::numeric_limits<size_t>::max());
    ASSERT(m_directory);
    
    m_index = std::numeric_limits<size_t>::max();
    m_directory = nullptr;
    blockFooter().m_subspace = nullptr;
}

#if !ASSERT_DISABLED
void MarkedBlock::assertValidCell(VM& vm, HeapCell* cell) const
{
    RELEASE_ASSERT(&vm == &this->vm());
    RELEASE_ASSERT(const_cast<MarkedBlock*>(this)->handle().cellAlign(cell) == cell);
}
#endif

void MarkedBlock::Handle::dumpState(PrintStream& out)
{
    CommaPrinter comma;
    directory()->forEachBitVectorWithName(
        holdLock(directory()->bitvectorLock()),
        [&] (FastBitVector& bitvector, const char* name) {
            out.print(comma, name, ":", bitvector[index()] ? "YES" : "no");
        });
}

Subspace* MarkedBlock::Handle::subspace() const
{
    return directory()->subspace();
}

void MarkedBlock::Handle::sweep(FreeList* freeList)
{
    SweepingScope sweepingScope(*heap());
    
    SweepMode sweepMode = freeList ? SweepToFreeList : SweepOnly;
    
    m_directory->setIsUnswept(NoLockingNecessary, this, false);
    
    m_weakSet.sweep();
    
    bool needsDestruction = m_attributes.destruction == NeedsDestruction
        && m_directory->isDestructible(NoLockingNecessary, this);

    if (sweepMode == SweepOnly && !needsDestruction)
        return;

    if (m_isFreeListed) {
        dataLog("FATAL: ", RawPointer(this), "->sweep: block is free-listed.\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    if (isAllocated()) {
        dataLog("FATAL: ", RawPointer(this), "->sweep: block is allocated.\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    if (space()->isMarking())
        blockFooter().m_lock.lock();
    
    subspace()->didBeginSweepingToFreeList(this);
    
    if (needsDestruction) {
        subspace()->finishSweep(*this, freeList);
        return;
    }
    
    // Handle the no-destructor specializations here, since we have the most of those. This
    // ensures that they don't get re-specialized for every destructor space.
    
    EmptyMode emptyMode = this->emptyMode();
    ScribbleMode scribbleMode = this->scribbleMode();
    NewlyAllocatedMode newlyAllocatedMode = this->newlyAllocatedMode();
    MarksMode marksMode = this->marksMode();
    
    auto trySpecialized = [&] () -> bool {
        if (sweepMode != SweepToFreeList)
            return false;
        if (scribbleMode != DontScribble)
            return false;
        if (newlyAllocatedMode != DoesNotHaveNewlyAllocated)
            return false;
        
        switch (emptyMode) {
        case IsEmpty:
            switch (marksMode) {
            case MarksNotStale:
                specializedSweep<true, IsEmpty, SweepToFreeList, BlockHasNoDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksNotStale>(freeList, IsEmpty, SweepToFreeList, BlockHasNoDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksNotStale, [] (VM&, JSCell*) { });
                return true;
            case MarksStale:
                specializedSweep<true, IsEmpty, SweepToFreeList, BlockHasNoDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksStale>(freeList, IsEmpty, SweepToFreeList, BlockHasNoDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksStale, [] (VM&, JSCell*) { });
                return true;
            }
            break;
        case NotEmpty:
            switch (marksMode) {
            case MarksNotStale:
                specializedSweep<true, NotEmpty, SweepToFreeList, BlockHasNoDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksNotStale>(freeList, IsEmpty, SweepToFreeList, BlockHasNoDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksNotStale, [] (VM&, JSCell*) { });
                return true;
            case MarksStale:
                specializedSweep<true, NotEmpty, SweepToFreeList, BlockHasNoDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksStale>(freeList, IsEmpty, SweepToFreeList, BlockHasNoDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksStale, [] (VM&, JSCell*) { });
                return true;
            }
            break;
        }
        
        return false;
    };
    
    if (trySpecialized())
        return;

    // The template arguments don't matter because the first one is false.
    specializedSweep<false, IsEmpty, SweepOnly, BlockHasNoDestructors, DontScribble, HasNewlyAllocated, MarksStale>(freeList, emptyMode, sweepMode, BlockHasNoDestructors, scribbleMode, newlyAllocatedMode, marksMode, [] (VM&, JSCell*) { });
}

bool MarkedBlock::Handle::isFreeListedCell(const void* target) const
{
    ASSERT(isFreeListed());
    return m_directory->isFreeListedCell(target);
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

