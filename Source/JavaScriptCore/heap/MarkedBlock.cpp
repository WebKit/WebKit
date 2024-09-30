/*
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
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
#include "FreeListInlines.h"
#include "JSCJSValueInlines.h"
#include "MarkedBlockInlines.h"
#include "SweepingScope.h"
#include "VMInspector.h"
#include <wtf/CommaPrinter.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/CrashReporter.h>
#endif

namespace JSC {
namespace MarkedBlockInternal {
static constexpr bool verbose = false;
}

static constexpr bool computeBalance = false;
static size_t balance;

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MarkedBlock);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MarkedBlockHandle);

MarkedBlock::Handle* MarkedBlock::tryCreate(JSC::Heap& heap, AlignedMemoryAllocator* alignedMemoryAllocator)
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

MarkedBlock::Handle::Handle(JSC::Heap& heap, AlignedMemoryAllocator* alignedMemoryAllocator, void* blockSpace)
    : m_alignedMemoryAllocator(alignedMemoryAllocator)
    , m_weakSet(heap.vm())
    , m_block(new (NotNull, blockSpace) MarkedBlock(heap.vm(), *this))
{
    heap.didAllocateBlock(blockSize);
}

MarkedBlock::Handle::~Handle()
{
    JSC::Heap& heap = *this->heap();
    if (computeBalance) {
        balance--;
        if (!(balance % 10))
            dataLog("MarkedBlock Balance: ", balance, "\n");
    }
    m_directory->removeBlock(this, BlockDirectory::WillDeleteBlock::Yes);
    m_block->~MarkedBlock();
    m_alignedMemoryAllocator->freeAlignedMemory(m_block);
    heap.didFreeBlock(blockSize);
}

MarkedBlock::MarkedBlock(VM& vm, Handle& handle)
{
    new (&header()) Header(vm, handle);
    if (MarkedBlockInternal::verbose)
        dataLog(RawPointer(this), ": Allocated.\n");
}

MarkedBlock::~MarkedBlock()
{
    header().~Header();
}

MarkedBlock::Header::Header(VM& vm, Handle& handle)
    : m_handle(handle)
    , m_vm(&vm)
    , m_markingVersion(MarkedSpace::nullVersion)
    , m_newlyAllocatedVersion(MarkedSpace::nullVersion)
{
}

MarkedBlock::Header::~Header() = default;

void MarkedBlock::Handle::unsweepWithNoNewlyAllocated()
{
    RELEASE_ASSERT(m_isFreeListed);
    m_isFreeListed = false;
    m_directory->didFinishUsingBlock(this);
}

void MarkedBlock::Handle::stopAllocating(const FreeList& freeList)
{
    Locker locker { blockHeader().m_lock };
    
    if (MarkedBlockInternal::verbose)
        dataLog(RawPointer(this), ": MarkedBlock::Handle::stopAllocating!\n");
    m_directory->assertIsMutatorOrMutatorIsStopped();
    ASSERT(!m_directory->isAllocated(this));

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
    
    blockHeader().m_newlyAllocated.clearAll();
    blockHeader().m_newlyAllocatedVersion = heap()->objectSpace().newlyAllocatedVersion();

    forEachCell(
        [&] (size_t, HeapCell* cell, HeapCell::Kind) -> IterationStatus {
            block().setNewlyAllocated(cell);
            return IterationStatus::Continue;
        });

    freeList.forEach(
        [&] (HeapCell* cell) {
            if constexpr (MarkedBlockInternal::verbose)
                dataLog("Free cell: ", RawPointer(cell), "\n");
            if (m_attributes.destruction == NeedsDestruction)
                cell->zap(HeapCell::StopAllocating);
            block().clearNewlyAllocated(cell);
        });
    
    m_isFreeListed = false;
    directory()->didFinishUsingBlock(this);
}

void MarkedBlock::Handle::lastChanceToFinalize()
{
    // Concurrent sweeper is shut down at this point.
    m_directory->assertSweeperIsSuspended();
    m_directory->setIsAllocated(this, false);
    m_directory->setIsDestructible(this, true);
    blockHeader().m_marks.clearAll();
    block().clearHasAnyMarked();
    blockHeader().m_markingVersion = heap()->objectSpace().markingVersion();
    m_weakSet.lastChanceToFinalize();
    blockHeader().m_newlyAllocated.clearAll();
    blockHeader().m_newlyAllocatedVersion = heap()->objectSpace().newlyAllocatedVersion();
    m_directory->setIsInUse(this, true);
    sweep(nullptr);
}

void MarkedBlock::Handle::resumeAllocating(FreeList& freeList)
{
    BlockDirectory* directory = this->directory();
    directory->assertSweeperIsSuspended();
    {
        Locker locker { blockHeader().m_lock };
        
        if (MarkedBlockInternal::verbose)
            dataLog(RawPointer(this), ": MarkedBlock::Handle::resumeAllocating!\n");


        ASSERT(!directory->isAllocated(this));
        ASSERT(!isFreeListed());
        
        if (!block().hasAnyNewlyAllocated()) {
            if (MarkedBlockInternal::verbose)
                dataLog("There ain't no newly allocated.\n");
            // This means we had already exhausted the block when we stopped allocation.
            freeList.clear();
            return;
        }
    }

    directory->setIsInUse(this, true);

    // Re-create our free list from before stopping allocation. Note that this may return an empty
    // freelist, in which case the block will still be Marked!
    sweep(&freeList);
}

void MarkedBlock::aboutToMarkSlow(HeapVersion markingVersion, HeapCell* cell)
{
    ASSERT(vm().heap.objectSpace().isMarking());
    Locker locker { header().m_lock };
    
    if (!areMarksStale(markingVersion))
        return;

    MarkedBlock::Handle* handle = header().handlePointerForNullCheck();
    if (UNLIKELY(!handle))
        dumpInfoAndCrashForInvalidHandle(locker, cell);

    BlockDirectory* directory = handle->directory();
    bool isAllocated;
    {
        Locker bitLocker { directory->bitvectorLock() };
        isAllocated = directory->isAllocated(handle);
    }

    if (isAllocated || !marksConveyLivenessDuringMarking(markingVersion)) {
        if (MarkedBlockInternal::verbose)
            dataLog(RawPointer(this), ": Clearing marks without doing anything else.\n");
        // We already know that the block is full and is already recognized as such, or that the
        // block did not survive the previous GC. So, we can clear mark bits the old fashioned
        // way. Note that it's possible for such a block to have newlyAllocated with an up-to-
        // date version! If it does, then we want to leave the newlyAllocated alone, since that
        // means that we had allocated in this previously empty block but did not fill it up, so
        // we created a newlyAllocated.
        header().m_marks.clearAll();
    } else {
        if (MarkedBlockInternal::verbose)
            dataLog(RawPointer(this), ": Doing things.\n");
        HeapVersion newlyAllocatedVersion = space()->newlyAllocatedVersion();
        if (header().m_newlyAllocatedVersion == newlyAllocatedVersion) {
            // When do we get here? The block could not have been filled up. The newlyAllocated bits would
            // have had to be created since the end of the last collection. The only things that create
            // them are aboutToMarkSlow, lastChanceToFinalize, and stopAllocating. If it had been
            // aboutToMarkSlow, then we shouldn't be here since the marks wouldn't be stale anymore. It
            // cannot be lastChanceToFinalize. So it must be stopAllocating. That means that we just
            // computed the newlyAllocated bits just before the start of an increment. When we are in that
            // mode, it seems as if newlyAllocated should subsume marks.
            ASSERT(header().m_newlyAllocated.subsumes(header().m_marks));
            header().m_marks.clearAll();
        } else {
            header().m_newlyAllocated.setAndClear(header().m_marks);
            header().m_newlyAllocatedVersion = newlyAllocatedVersion;
        }
    }
    clearHasAnyMarked();
    WTF::storeStoreFence();
    header().m_markingVersion = markingVersion;
    
    // Workaround for a clang regression <rdar://111818130>.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif
    // This means we're the first ones to mark any object in this block.
    Locker bitLocker { directory->bitvectorLock() };
    directory->setIsMarkingNotEmpty(handle, true);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

void MarkedBlock::resetAllocated()
{
    header().m_newlyAllocated.clearAll();
    header().m_newlyAllocatedVersion = MarkedSpace::nullVersion;
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
        header().m_marks.clearAll();
    header().m_markingVersion = MarkedSpace::nullVersion;
}

#if ASSERT_ENABLED
void MarkedBlock::assertMarksNotStale()
{
    ASSERT(header().m_markingVersion == vm().heap.objectSpace().markingVersion());
}
#endif // ASSERT_ENABLED

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
    Locker locker { blockHeader().m_lock };
    if (MarkedBlockInternal::verbose)
        dataLog(RawPointer(this), ": MarkedBlock::Handle::didConsumeFreeList!\n");
    ASSERT(isFreeListed());
    m_isFreeListed = false;
    Locker bitLocker(m_directory->bitvectorLock());
    m_directory->setIsAllocated(this, true);
    m_directory->didFinishUsingBlock(bitLocker, this);
}

size_t MarkedBlock::markCount()
{
    return areMarksStale() ? 0 : header().m_marks.count();
}

void MarkedBlock::clearHasAnyMarked()
{
    header().m_biasedMarkCount = header().m_markCountBias;
}

void MarkedBlock::noteMarkedSlow()
{
    BlockDirectory* directory = handle().directory();
    Locker locker { directory->bitvectorLock() };
    directory->setIsMarkingRetired(&handle(), true);
}

void MarkedBlock::Handle::removeFromDirectory()
{
    if (!m_directory)
        return;
    
    m_directory->removeBlock(this);
}

void MarkedBlock::Handle::didAddToDirectory(BlockDirectory* directory, unsigned index)
{
    ASSERT(m_index == std::numeric_limits<unsigned>::max());
    ASSERT(!m_directory);
    
    RELEASE_ASSERT(directory->subspace()->alignedMemoryAllocator() == m_alignedMemoryAllocator);
    
    m_index = index;
    m_directory = directory;
    blockHeader().m_subspace = directory->subspace();
    
    size_t cellSize = directory->cellSize();
    m_atomsPerCell = (cellSize + atomSize - 1) / atomSize;

    // Discount the payload atoms at the front so that m_startAtom can start on an atom such that
    // m_atomsPerCell increments from m_startAtom will get us exactly to endAtom when we have filled
    // up the payload region using bump allocation. This makes simplifies the computation of the
    // termination condition for iteration later.
    size_t numberOfUnallocatableAtoms = numberOfPayloadAtoms % m_atomsPerCell;
    m_startAtom = firstPayloadRegionAtom + numberOfUnallocatableAtoms;
    ASSERT(m_startAtom < firstPayloadRegionAtom + m_atomsPerCell);

    m_attributes = directory->attributes();

    if (!isJSCellKind(m_attributes.cellKind))
        RELEASE_ASSERT(m_attributes.destruction == DoesNotNeedDestruction);
    
    double markCountBias = -(Options::minMarkedBlockUtilization() * cellsPerBlock());
    
    // The mark count bias should be comfortably within this range.
    RELEASE_ASSERT(markCountBias > static_cast<double>(std::numeric_limits<int16_t>::min()));
    RELEASE_ASSERT(markCountBias < 0);
    
    // This means we haven't marked anything yet.
    blockHeader().m_biasedMarkCount = blockHeader().m_markCountBias = static_cast<int16_t>(markCountBias);
}

void MarkedBlock::Handle::didRemoveFromDirectory()
{
    ASSERT(m_index != std::numeric_limits<unsigned>::max());
    ASSERT(m_directory);
    
    m_index = std::numeric_limits<unsigned>::max();
    m_directory = nullptr;
    blockHeader().m_subspace = nullptr;
}

#if ASSERT_ENABLED
void MarkedBlock::assertValidCell(VM& vm, HeapCell* cell) const
{
    RELEASE_ASSERT(&vm == &this->vm());
    RELEASE_ASSERT(const_cast<MarkedBlock*>(this)->handle().cellAlign(cell) == cell);
}
#endif // ASSERT_ENABLED

void MarkedBlock::Handle::dumpState(PrintStream& out)
{
    CommaPrinter comma;
    Locker locker { directory()->bitvectorLock() };
    directory()->forEachBitVectorWithName(
        [&](auto vectorRef, const char* name) {
            out.print(comma, name, ":"_s, vectorRef[index()] ? "YES"_s : "no"_s);
        });
}

Subspace* MarkedBlock::Handle::subspace() const
{
    return directory()->subspace();
}

void MarkedBlock::Handle::sweep(FreeList* freeList)
{
    SweepingScope sweepingScope(*heap());
    m_directory->assertIsMutatorOrMutatorIsStopped();
    ASSERT(m_directory->isInUse(this));

    SweepMode sweepMode = freeList ? SweepToFreeList : SweepOnly;
    bool needsDestruction = m_attributes.destruction == NeedsDestruction
        && m_directory->isDestructible(this);

    m_weakSet.sweep();

    // If we don't "release" our read access without locking then the ThreadSafetyAnalysis code gets upset with the locker below.
    m_directory->releaseAssertAcquiredBitVectorLock();

    if (sweepMode == SweepOnly && !needsDestruction) {
        Locker locker(m_directory->bitvectorLock());
        m_directory->setIsUnswept(this, false);
        return;
    }

    if (m_isFreeListed) {
        dataLog("FATAL: ", RawPointer(this), "->sweep: block is free-listed.\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    if (isAllocated()) {
        dataLog("FATAL: ", RawPointer(this), "->sweep: block is allocated.\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    if (space()->isMarking())
        blockHeader().m_lock.lock();
    
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

#if PLATFORM(COCOA)
#define LOG_INVALID_HANDLE_DETAILS(s, ...) do { \
    out.printf("INVALID HANDLE: " s, __VA_ARGS__); \
    WTF::setCrashLogMessage(out.toCString().data()); \
} while (false)
#else
#define LOG_INVALID_HANDLE_DETAILS(s, ...) do { \
    out.printf("INVALID HANDLE: " s, __VA_ARGS__); \
    dataLog(out.toCString().data()); \
} while (false)
#endif

#if CPU(ARM64) && !COMPILER(GCC)
#define DEFINE_SAVED_VALUE(name, reg, value) \
    volatile register decltype(value) name asm(reg) = value; \
    WTF::opaque(name); \
    WTF::compilerFence();
#else
#define DEFINE_SAVED_VALUE(name, reg, value) \
    decltype(value) name = value; \
    UNUSED_VARIABLE(reg);
#endif

#define SAVE_TO_REG(name, value) do { \
    name = WTF::opaque(value); \
    WTF::compilerFence(); \
} while (false)

NO_RETURN_DUE_TO_CRASH NEVER_INLINE void MarkedBlock::dumpInfoAndCrashForInvalidHandle(AbstractLocker&, HeapCell* heapCell)
{
    JSCell* cell = bitwise_cast<JSCell*>(heapCell);
    JSType cellType = cell->type();

    StringPrintStream out;
    LOG_INVALID_HANDLE_DETAILS("MarkedBlock = %p ; heapCell = %p ; type = %d\n", this, heapCell, cellType);

    DEFINE_SAVED_VALUE(savedMarkedBlock, "x19", this);
    DEFINE_SAVED_VALUE(savedType, "x20", cellType);
    DEFINE_SAVED_VALUE(savedHeapCell, "x21", heapCell);

    static_assert(!offsetOfHeader);
    static_assert(!OBJECT_OFFSETOF(Header, m_handle));
    size_t contiguousZeroCountAfterHandle = 0;
    {
        char* mem = WTF::bitwise_cast<char*>(&header());
        for (; contiguousZeroCountAfterHandle < MarkedBlock::blockSize; ++contiguousZeroCountAfterHandle) {
            if (*mem)
                break;
            ++mem;
        }
    }
    LOG_INVALID_HANDLE_DETAILS("found %zd 0s at beginning of block\n", contiguousZeroCountAfterHandle);
    SAVE_TO_REG(savedMarkedBlock, this);
    SAVE_TO_REG(savedHeapCell, heapCell);
    SAVE_TO_REG(savedType, cellType);
    DEFINE_SAVED_VALUE(savedCount, "x22", contiguousZeroCountAfterHandle);

    bool isValidBlockVM = false;
    bool foundBlockInThisVM = false;
    bool isBlockInVM = false;
    bool isBlockHandleInVM = false;

    VM* blockVM = header().m_vm;
    VM* actualVM = nullptr;
    DEFINE_SAVED_VALUE(savedBlockVM, "x23", blockVM);
    DEFINE_SAVED_VALUE(savedActualVM, "x24", actualVM);
    DEFINE_SAVED_VALUE(savedBitfield, "x25", 0L);

    {
        VMInspector::forEachVM([&](VM& vm) {
            if (blockVM == &vm) {
                isValidBlockVM = true;
                SAVE_TO_REG(savedActualVM, &vm);
                SAVE_TO_REG(savedBitfield, 8);
                LOG_INVALID_HANDLE_DETAILS("block VM %p is valid\n", &vm);
                return IterationStatus::Done;
            }
            return IterationStatus::Continue;
        });
    }

    SAVE_TO_REG(savedMarkedBlock, this);
    SAVE_TO_REG(savedHeapCell, heapCell);
    SAVE_TO_REG(savedType, cellType);
    SAVE_TO_REG(savedCount, contiguousZeroCountAfterHandle);
    SAVE_TO_REG(savedBlockVM, blockVM);

    if (isValidBlockVM) {
        MarkedSpace& objectSpace = blockVM->heap.objectSpace();
        isBlockInVM = objectSpace.blocks().set().contains(this);
        isBlockHandleInVM = !!objectSpace.findMarkedBlockHandleDebug(this);
        foundBlockInThisVM = isBlockInVM || isBlockHandleInVM;
        LOG_INVALID_HANDLE_DETAILS("block in our VM = %d, block handle in our VM = %d\n", isBlockInVM, isBlockHandleInVM);

        SAVE_TO_REG(savedBitfield, (isValidBlockVM ? 8 : 0) | (isBlockInVM ? 4 : 0) | (isBlockHandleInVM ? 2 : 0) | (foundBlockInThisVM ? 1 : 0));
    }

    SAVE_TO_REG(savedMarkedBlock, this);
    SAVE_TO_REG(savedHeapCell, heapCell);
    SAVE_TO_REG(savedType, cellType);
    SAVE_TO_REG(savedCount, contiguousZeroCountAfterHandle);
    SAVE_TO_REG(savedBlockVM, blockVM);

    if (!isBlockInVM && !isBlockHandleInVM) {
        // worst case path
        VMInspector::forEachVM([&](VM& vm) {
            MarkedSpace& objectSpace = vm.heap.objectSpace();
            isBlockInVM = objectSpace.blocks().set().contains(this);
            isBlockHandleInVM = !!objectSpace.findMarkedBlockHandleDebug(this);
            // Either of them is true indicates that the block belongs or used to belong to the VM.
            if (isBlockInVM || isBlockHandleInVM) {
                actualVM = &vm;
                LOG_INVALID_HANDLE_DETAILS("block in another VM: %d, block in another VM: %d; other VM is %p\n", isBlockInVM, isBlockHandleInVM, &vm);

                SAVE_TO_REG(savedActualVM, actualVM);
                SAVE_TO_REG(savedBitfield, (isValidBlockVM ? 8 : 0) | (isBlockInVM ? 4 : 0) | (isBlockHandleInVM ? 2 : 0) | (foundBlockInThisVM ? 1 : 0));

                return IterationStatus::Done;
            }
            return IterationStatus::Continue;
        });
    }

    SAVE_TO_REG(savedMarkedBlock, this);
    SAVE_TO_REG(savedHeapCell, heapCell);
    SAVE_TO_REG(savedType, cellType);
    SAVE_TO_REG(savedCount, contiguousZeroCountAfterHandle);
    SAVE_TO_REG(savedBlockVM, blockVM);
    SAVE_TO_REG(savedActualVM, savedActualVM);
    SAVE_TO_REG(savedBitfield, savedBitfield);

    uint64_t bitfield = 0xab00ab01ab020000;
    if (!isValidBlockVM)
        bitfield |= 1 << 7;
    if (!isBlockInVM)
        bitfield |= 1 << 6;
    if (!isBlockHandleInVM)
        bitfield |= 1 << 5;
    if (!foundBlockInThisVM)
        bitfield |= 1 << 4;

    // Make sure that the compiler doesn't think of these as "unused"
    WTF::compilerFence();
    WTF::opaque(savedMarkedBlock);
    WTF::opaque(savedHeapCell);
    WTF::opaque(savedType);
    WTF::opaque(savedCount);
    WTF::opaque(savedBlockVM);

    CRASH_WITH_INFO(cell, cellType, contiguousZeroCountAfterHandle, bitfield, this, blockVM, actualVM);
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

