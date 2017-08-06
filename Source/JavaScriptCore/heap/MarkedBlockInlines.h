/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "JSCell.h"
#include "MarkedAllocator.h"
#include "MarkedBlock.h"
#include "MarkedSpace.h"
#include "Operations.h"
#include "SuperSampler.h"
#include "VM.h"

namespace JSC {

inline unsigned MarkedBlock::Handle::cellsPerBlock()
{
    return MarkedSpace::blockPayload / cellSize();
}

inline bool MarkedBlock::Handle::isNewlyAllocatedStale() const
{
    return m_newlyAllocatedVersion != space()->newlyAllocatedVersion();
}

inline bool MarkedBlock::Handle::hasAnyNewlyAllocated()
{
    return !isNewlyAllocatedStale();
}

inline Heap* MarkedBlock::heap() const
{
    return &vm()->heap;
}

inline MarkedSpace* MarkedBlock::space() const
{
    return &heap()->objectSpace();
}

inline MarkedSpace* MarkedBlock::Handle::space() const
{
    return &heap()->objectSpace();
}

inline bool MarkedBlock::marksConveyLivenessDuringMarking(HeapVersion markingVersion)
{
    // This returns true if any of these is true:
    // - We just created the block and so the bits are clear already.
    // - This block has objects marked during the last GC, and so its version was up-to-date just
    //   before the current collection did beginMarking(). This means that any objects that have 
    //   their mark bit set are valid objects that were never deleted, and so are candidates for
    //   marking in any conservative scan. Using our jargon, they are "live".
    // - We did ~2^32 collections and rotated the version back to null, so we needed to hard-reset
    //   everything. If the marks had been stale, we would have cleared them. So, we can be sure that
    //   any set mark bit reflects objects marked during last GC, i.e. "live" objects.
    // It would be absurd to use this method when not collecting, since this special "one version
    // back" state only makes sense when we're in a concurrent collection and have to be
    // conservative.
    ASSERT(space()->isMarking());
    if (heap()->collectionScope() != CollectionScope::Full)
        return false;
    return m_markingVersion == MarkedSpace::nullVersion
        || MarkedSpace::nextVersion(m_markingVersion) == markingVersion;
}

inline bool MarkedBlock::Handle::isLive(HeapVersion markingVersion, bool isMarking, const HeapCell* cell)
{
    ASSERT(!isFreeListed());
    
    if (UNLIKELY(hasAnyNewlyAllocated())) {
        if (isNewlyAllocated(cell))
            return true;
    }
    
    if (allocator()->isAllocated(NoLockingNecessary, this))
        return true;
    
    MarkedBlock& block = this->block();
    
    if (block.areMarksStale()) {
        if (!isMarking)
            return false;
        if (!block.marksConveyLivenessDuringMarking(markingVersion))
            return false;
    }

    return block.m_marks.get(block.atomNumber(cell));
}

inline bool MarkedBlock::Handle::isLiveCell(HeapVersion markingVersion, bool isMarking, const void* p)
{
    if (!m_block->isAtom(p))
        return false;
    return isLive(markingVersion, isMarking, static_cast<const HeapCell*>(p));
}

// The following has to be true for specialization to kick in:
//
// sweepMode == SweepToFreeList
// scribbleMode == DontScribble
// newlyAllocatedMode == DoesNotHaveNewlyAllocated
// destructionMode != BlockHasDestrictorsAndCollectorIsRunning
//
// emptyMode = IsEmpty
//     destructionMode = DoesNotNeedDestruction
//         marksMode = MarksNotStale (1)
//         marksMode = MarksStale (2)
// emptyMode = NotEmpty
//     destructionMode = DoesNotNeedDestruction
//         marksMode = MarksNotStale (3)
//         marksMode = MarksStale (4)
//     destructionMode = NeedsDestruction
//         marksMode = MarksNotStale (5)
//         marksMode = MarksStale (6)
//
// Only the DoesNotNeedDestruction one should be specialized by MarkedBlock.

template<bool specialize, MarkedBlock::Handle::EmptyMode specializedEmptyMode, MarkedBlock::Handle::SweepMode specializedSweepMode, MarkedBlock::Handle::SweepDestructionMode specializedDestructionMode, MarkedBlock::Handle::ScribbleMode specializedScribbleMode, MarkedBlock::Handle::NewlyAllocatedMode specializedNewlyAllocatedMode, MarkedBlock::Handle::MarksMode specializedMarksMode, typename DestroyFunc>
void MarkedBlock::Handle::specializedSweep(FreeList* freeList, MarkedBlock::Handle::EmptyMode emptyMode, MarkedBlock::Handle::SweepMode sweepMode, MarkedBlock::Handle::SweepDestructionMode destructionMode, MarkedBlock::Handle::ScribbleMode scribbleMode, MarkedBlock::Handle::NewlyAllocatedMode newlyAllocatedMode, MarkedBlock::Handle::MarksMode marksMode, const DestroyFunc& destroyFunc)
{
    if (specialize) {
        emptyMode = specializedEmptyMode;
        sweepMode = specializedSweepMode;
        destructionMode = specializedDestructionMode;
        scribbleMode = specializedScribbleMode;
        newlyAllocatedMode = specializedNewlyAllocatedMode;
        marksMode = specializedMarksMode;
    }
    
    RELEASE_ASSERT(!(destructionMode == BlockHasNoDestructors && sweepMode == SweepOnly));
    
    SuperSamplerScope superSamplerScope(false);

    MarkedBlock& block = this->block();
    
    if (false)
        dataLog(RawPointer(this), "/", RawPointer(&block), ": MarkedBlock::Handle::specializedSweep!\n");
    
    unsigned cellSize = this->cellSize();
    
    VM& vm = *this->vm();
    auto destroy = [&] (void* cell) {
        JSCell* jsCell = static_cast<JSCell*>(cell);
        if (!jsCell->isZapped()) {
            destroyFunc(vm, jsCell);
            jsCell->zap();
        }
    };
    
    m_allocator->setIsDestructible(NoLockingNecessary, this, false);
    
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
        char* payloadEnd = startOfLastCell + cellSize;
        RELEASE_ASSERT(payloadEnd - MarkedBlock::blockSize <= bitwise_cast<char*>(&block));
        char* payloadBegin = bitwise_cast<char*>(block.atoms() + firstAtom());
        
        if (sweepMode == SweepToFreeList)
            setIsFreeListed();
        if (space()->isMarking())
            block.m_lock.unlock();
        if (destructionMode != BlockHasNoDestructors) {
            for (char* cell = payloadBegin; cell < payloadEnd; cell += cellSize)
                destroy(cell);
        }
        if (sweepMode == SweepToFreeList) {
            if (scribbleMode == Scribble)
                scribble(payloadBegin, payloadEnd - payloadBegin);
            freeList->initializeBump(payloadEnd, payloadEnd - payloadBegin);
        }
        if (false)
            dataLog("Quickly swept block ", RawPointer(this), " with cell size ", cellSize, " and attributes ", m_attributes, ": ", pointerDump(freeList), "\n");
        return;
    }

    // This produces a free list that is ordered in reverse through the block.
    // This is fine, since the allocation code makes no assumptions about the
    // order of the free list.
    FreeCell* head = 0;
    size_t count = 0;
    uintptr_t secret;
    cryptographicallyRandomValues(&secret, sizeof(uintptr_t));
    bool isEmpty = true;
    Vector<size_t> deadCells;
    auto handleDeadCell = [&] (size_t i) {
        HeapCell* cell = reinterpret_cast_ptr<HeapCell*>(&block.atoms()[i]);

        if (destructionMode != BlockHasNoDestructors)
            destroy(cell);

        if (sweepMode == SweepToFreeList) {
            FreeCell* freeCell = reinterpret_cast_ptr<FreeCell*>(cell);
            if (scribbleMode == Scribble)
                scribble(freeCell, cellSize);
            freeCell->setNext(head, secret);
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

    if (sweepMode == SweepToFreeList) {
        freeList->initializeList(head, secret, count * cellSize);
        setIsFreeListed();
    } else if (isEmpty)
        m_allocator->setIsEmpty(NoLockingNecessary, this, true);
    if (false)
        dataLog("Slowly swept block ", RawPointer(&block), " with cell size ", cellSize, " and attributes ", m_attributes, ": ", pointerDump(freeList), "\n");
}

template<typename DestroyFunc>
void MarkedBlock::Handle::finishSweepKnowingSubspace(FreeList* freeList, const DestroyFunc& destroyFunc)
{
    SweepMode sweepMode = freeList ? SweepToFreeList : SweepOnly;
    SweepDestructionMode destructionMode = this->sweepDestructionMode();
    EmptyMode emptyMode = this->emptyMode();
    ScribbleMode scribbleMode = this->scribbleMode();
    NewlyAllocatedMode newlyAllocatedMode = this->newlyAllocatedMode();
    MarksMode marksMode = this->marksMode();

    auto trySpecialized = [&] () -> bool {
        if (scribbleMode != DontScribble)
            return false;
        if (newlyAllocatedMode != DoesNotHaveNewlyAllocated)
            return false;
        if (destructionMode != BlockHasDestructors)
            return false;
        
        switch (emptyMode) {
        case IsEmpty:
            switch (sweepMode) {
            case SweepOnly:
                switch (marksMode) {
                case MarksNotStale:
                    specializedSweep<true, IsEmpty, SweepOnly, BlockHasDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksNotStale>(freeList, IsEmpty, SweepOnly, BlockHasDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksNotStale, destroyFunc);
                    return true;
                case MarksStale:
                    specializedSweep<true, IsEmpty, SweepOnly, BlockHasDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksStale>(freeList, IsEmpty, SweepOnly, BlockHasDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksStale, destroyFunc);
                    return true;
                }
            case SweepToFreeList:
                switch (marksMode) {
                case MarksNotStale:
                    specializedSweep<true, IsEmpty, SweepToFreeList, BlockHasDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksNotStale>(freeList, IsEmpty, SweepToFreeList, BlockHasDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksNotStale, destroyFunc);
                    return true;
                case MarksStale:
                    specializedSweep<true, IsEmpty, SweepToFreeList, BlockHasDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksStale>(freeList, IsEmpty, SweepToFreeList, BlockHasDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksStale, destroyFunc);
                    return true;
                }
            }
        case NotEmpty:
            switch (sweepMode) {
            case SweepOnly:
                switch (marksMode) {
                case MarksNotStale:
                    specializedSweep<true, NotEmpty, SweepOnly, BlockHasDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksNotStale>(freeList, NotEmpty, SweepOnly, BlockHasDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksNotStale, destroyFunc);
                    return true;
                case MarksStale:
                    specializedSweep<true, NotEmpty, SweepOnly, BlockHasDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksStale>(freeList, NotEmpty, SweepOnly, BlockHasDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksStale, destroyFunc);
                    return true;
                }
            case SweepToFreeList:
                switch (marksMode) {
                case MarksNotStale:
                    specializedSweep<true, NotEmpty, SweepToFreeList, BlockHasDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksNotStale>(freeList, NotEmpty, SweepToFreeList, BlockHasDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksNotStale, destroyFunc);
                    return true;
                case MarksStale:
                    specializedSweep<true, NotEmpty, SweepToFreeList, BlockHasDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksStale>(freeList, NotEmpty, SweepToFreeList, BlockHasDestructors, DontScribble, DoesNotHaveNewlyAllocated, MarksStale, destroyFunc);
                    return true;
                }
            }
        }
        
        return false;
    };
    
    if (trySpecialized())
        return;
    
    // The template arguments don't matter because the first one is false.
    specializedSweep<false, IsEmpty, SweepOnly, BlockHasNoDestructors, DontScribble, HasNewlyAllocated, MarksStale>(freeList, emptyMode, sweepMode, destructionMode, scribbleMode, newlyAllocatedMode, marksMode, destroyFunc);
}

inline MarkedBlock::Handle::SweepDestructionMode MarkedBlock::Handle::sweepDestructionMode()
{
    if (m_attributes.destruction == NeedsDestruction) {
        if (space()->isMarking())
            return BlockHasDestructorsAndCollectorIsRunning;
        return BlockHasDestructors;
    }
    return BlockHasNoDestructors;
}

inline MarkedBlock::Handle::EmptyMode MarkedBlock::Handle::emptyMode()
{
    // It's not obvious, but this is the only way to know if the block is empty. It's the only
    // bit that captures these caveats:
    // - It's true when the block is freshly allocated.
    // - It's true if the block had been swept in the past, all destructors were called, and that
    //   sweep proved that the block is empty.
    return m_allocator->isEmpty(NoLockingNecessary, this) ? IsEmpty : NotEmpty;
}

inline MarkedBlock::Handle::ScribbleMode MarkedBlock::Handle::scribbleMode()
{
    return scribbleFreeCells() ? Scribble : DontScribble;
}

inline MarkedBlock::Handle::NewlyAllocatedMode MarkedBlock::Handle::newlyAllocatedMode()
{
    return hasAnyNewlyAllocated() ? HasNewlyAllocated : DoesNotHaveNewlyAllocated;
}

inline MarkedBlock::Handle::MarksMode MarkedBlock::Handle::marksMode()
{
    HeapVersion markingVersion = space()->markingVersion();
    bool marksAreUseful = !block().areMarksStale(markingVersion);
    if (space()->isMarking())
        marksAreUseful |= block().marksConveyLivenessDuringMarking(markingVersion);
    return marksAreUseful ? MarksNotStale : MarksStale;
}

template <typename Functor>
inline IterationStatus MarkedBlock::Handle::forEachLiveCell(const Functor& functor)
{
    HeapCell::Kind kind = m_attributes.cellKind;
    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
        HeapCell* cell = reinterpret_cast_ptr<HeapCell*>(&m_block->atoms()[i]);
        if (!isLive(cell))
            continue;

        if (functor(cell, kind) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

template <typename Functor>
inline IterationStatus MarkedBlock::Handle::forEachDeadCell(const Functor& functor)
{
    HeapCell::Kind kind = m_attributes.cellKind;
    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
        HeapCell* cell = reinterpret_cast_ptr<HeapCell*>(&m_block->atoms()[i]);
        if (isLive(cell))
            continue;

        if (functor(cell, kind) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

template <typename Functor>
inline IterationStatus MarkedBlock::Handle::forEachMarkedCell(const Functor& functor)
{
    HeapCell::Kind kind = m_attributes.cellKind;
    MarkedBlock& block = this->block();
    bool areMarksStale = block.areMarksStale();
    WTF::loadLoadFence();
    if (areMarksStale)
        return IterationStatus::Continue;
    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
        HeapCell* cell = reinterpret_cast_ptr<HeapCell*>(&m_block->atoms()[i]);
        if (!block.isMarkedRaw(cell))
            continue;

        if (functor(cell, kind) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

} // namespace JSC

