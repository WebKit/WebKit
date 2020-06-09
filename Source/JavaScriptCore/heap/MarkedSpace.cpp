/*
 *  Copyright (C) 2003-2018 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "MarkedSpace.h"

#include "BlockDirectoryInlines.h"
#include "HeapInlines.h"
#include "IncrementalSweeper.h"
#include "MarkedBlockInlines.h"
#include "MarkedSpaceInlines.h"
#include <wtf/ListDump.h>

namespace JSC {

std::array<unsigned, MarkedSpace::numSizeClasses> MarkedSpace::s_sizeClassForSizeStep;

namespace {

static Vector<size_t> sizeClasses()
{
    Vector<size_t> result;

    if (UNLIKELY(Options::dumpSizeClasses())) {
        dataLog("Block size: ", MarkedBlock::blockSize, "\n");
        dataLog("Footer size: ", sizeof(MarkedBlock::Footer), "\n");
    }
    
    auto add = [&] (size_t sizeClass) {
        sizeClass = WTF::roundUpToMultipleOf<MarkedBlock::atomSize>(sizeClass);
        dataLogLnIf(Options::dumpSizeClasses(), "Adding JSC MarkedSpace size class: ", sizeClass);
        // Perform some validation as we go.
        RELEASE_ASSERT(!(sizeClass % MarkedSpace::sizeStep));
        if (result.isEmpty())
            RELEASE_ASSERT(sizeClass == MarkedSpace::sizeStep);
        result.append(sizeClass);
    };
    
    // This is a definition of the size classes in our GC. It must define all of the
    // size classes from sizeStep up to largeCutoff.

    // Have very precise size classes for the small stuff. This is a loop to make it easy to reduce
    // atomSize.
    for (size_t size = MarkedSpace::sizeStep; size < MarkedSpace::preciseCutoff; size += MarkedSpace::sizeStep)
        add(size);
    
    // We want to make sure that the remaining size classes minimize internal fragmentation (i.e.
    // the wasted space at the tail end of a MarkedBlock) while proceeding roughly in an exponential
    // way starting at just above the precise size classes to four cells per block.
    
    dataLogLnIf(Options::dumpSizeClasses(), "    Marked block payload size: ", static_cast<size_t>(MarkedSpace::blockPayload));
    
    for (unsigned i = 0; ; ++i) {
        double approximateSize = MarkedSpace::preciseCutoff * pow(Options::sizeClassProgression(), i);
        dataLogLnIf(Options::dumpSizeClasses(), "    Next size class as a double: ", approximateSize);

        size_t approximateSizeInBytes = static_cast<size_t>(approximateSize);
        dataLogLnIf(Options::dumpSizeClasses(), "    Next size class as bytes: ", approximateSizeInBytes);

        // Make sure that the computer did the math correctly.
        RELEASE_ASSERT(approximateSizeInBytes >= MarkedSpace::preciseCutoff);
        
        if (approximateSizeInBytes > MarkedSpace::largeCutoff)
            break;
        
        size_t sizeClass =
            WTF::roundUpToMultipleOf<MarkedSpace::sizeStep>(approximateSizeInBytes);
        dataLogLnIf(Options::dumpSizeClasses(), "    Size class: ", sizeClass);
        
        // Optimize the size class so that there isn't any slop at the end of the block's
        // payload.
        unsigned cellsPerBlock = MarkedSpace::blockPayload / sizeClass;
        size_t possiblyBetterSizeClass = (MarkedSpace::blockPayload / cellsPerBlock) & ~(MarkedSpace::sizeStep - 1);
        dataLogLnIf(Options::dumpSizeClasses(), "    Possibly better size class: ", possiblyBetterSizeClass);

        // The size class we just came up with is better than the other one if it reduces
        // total wastage assuming we only allocate cells of that size.
        size_t originalWastage = MarkedSpace::blockPayload - cellsPerBlock * sizeClass;
        size_t newWastage = (possiblyBetterSizeClass - sizeClass) * cellsPerBlock;
        dataLogLnIf(Options::dumpSizeClasses(), "    Original wastage: ", originalWastage, ", new wastage: ", newWastage);
        
        size_t betterSizeClass;
        if (newWastage > originalWastage)
            betterSizeClass = sizeClass;
        else
            betterSizeClass = possiblyBetterSizeClass;
        
        dataLogLnIf(Options::dumpSizeClasses(), "    Choosing size class: ", betterSizeClass);
        
        if (betterSizeClass == result.last()) {
            // Defense for when expStep is small.
            continue;
        }
        
        // This is usually how we get out of the loop.
        if (betterSizeClass > MarkedSpace::largeCutoff
            || betterSizeClass > Options::preciseAllocationCutoff())
            break;
        
        add(betterSizeClass);
    }

    // Manually inject size classes for objects we know will be allocated in high volume.
    // FIXME: All of these things should have IsoSubspaces.
    // https://bugs.webkit.org/show_bug.cgi?id=179876
    add(256);

    {
        // Sort and deduplicate.
        std::sort(result.begin(), result.end());
        auto it = std::unique(result.begin(), result.end());
        result.shrinkCapacity(it - result.begin());
    }

    dataLogLnIf(Options::dumpSizeClasses(), "JSC Heap MarkedSpace size class dump: ", listDump(result));

    // We have an optimization in MarkedSpace::optimalSizeFor() that assumes things about
    // the size class table. This checks our results against that function's assumptions.
    for (size_t size = MarkedSpace::sizeStep, i = 0; size <= MarkedSpace::preciseCutoff; size += MarkedSpace::sizeStep, i++)
        RELEASE_ASSERT(result.at(i) == size);

    return result;
}

template<typename TableType, typename SizeClassCons, typename DefaultCons>
void buildSizeClassTable(TableType& table, const SizeClassCons& cons, const DefaultCons& defaultCons)
{
    size_t nextIndex = 0;
    for (size_t sizeClass : sizeClasses()) {
        auto entry = cons(sizeClass);
        size_t index = MarkedSpace::sizeClassToIndex(sizeClass);
        for (size_t i = nextIndex; i <= index; ++i)
            table[i] = entry;
        nextIndex = index + 1;
    }
    ASSERT(MarkedSpace::sizeClassToIndex(MarkedSpace::largeCutoff - 1) < MarkedSpace::numSizeClasses);
    for (size_t i = nextIndex; i < MarkedSpace::numSizeClasses; ++i)
        table[i] = defaultCons(MarkedSpace::indexToSizeClass(i));
}

} // anonymous namespace

void MarkedSpace::initializeSizeClassForStepSize()
{
    static std::once_flag flag;
    std::call_once(
        flag,
        [] {
            buildSizeClassTable(
                s_sizeClassForSizeStep,
                [&] (size_t sizeClass) -> size_t {
                    RELEASE_ASSERT(sizeClass <= UINT32_MAX);
                    return sizeClass;
                },
                [&] (size_t sizeClass) -> size_t {
                    RELEASE_ASSERT(sizeClass <= UINT32_MAX);
                    return sizeClass;
                });
        });
}

MarkedSpace::MarkedSpace(Heap* heap)
{
    ASSERT_UNUSED(heap, heap == &this->heap());
    initializeSizeClassForStepSize();
}

MarkedSpace::~MarkedSpace()
{
    ASSERT(!m_blocks.set().size());
}

void MarkedSpace::freeMemory()
{
    forEachBlock(
        [&] (MarkedBlock::Handle* block) {
            freeBlock(block);
        });
    for (PreciseAllocation* allocation : m_preciseAllocations)
        allocation->destroy();
    forEachSubspace([&](Subspace& subspace) {
        if (subspace.isIsoSubspace())
            static_cast<IsoSubspace&>(subspace).destroyLowerTierFreeList();
        return IterationStatus::Continue;
    });
}

void MarkedSpace::lastChanceToFinalize()
{
    forEachDirectory(
        [&] (BlockDirectory& directory) -> IterationStatus {
            directory.lastChanceToFinalize();
            return IterationStatus::Continue;
        });
    for (PreciseAllocation* allocation : m_preciseAllocations)
        allocation->lastChanceToFinalize();
    // We do not call lastChanceToFinalize for lower-tier swept cells since we need nothing to do.
}

void MarkedSpace::sweepBlocks()
{
    heap().sweeper().stopSweeping();
    forEachDirectory(
        [&] (BlockDirectory& directory) -> IterationStatus {
            directory.sweep();
            return IterationStatus::Continue;
        });
}

void MarkedSpace::sweepPreciseAllocations()
{
    RELEASE_ASSERT(m_preciseAllocationsNurseryOffset == m_preciseAllocations.size());
    unsigned srcIndex = m_preciseAllocationsNurseryOffsetForSweep;
    unsigned dstIndex = srcIndex;
    while (srcIndex < m_preciseAllocations.size()) {
        PreciseAllocation* allocation = m_preciseAllocations[srcIndex++];
        allocation->sweep();
        if (allocation->isEmpty()) {
            if (auto* set = preciseAllocationSet())
                set->remove(allocation->cell());
            if (allocation->isLowerTier())
                static_cast<IsoSubspace*>(allocation->subspace())->sweepLowerTierCell(allocation);
            else {
                m_capacity -= allocation->cellSize();
                allocation->destroy();
            }
            continue;
        }
        allocation->setIndexInSpace(dstIndex);
        m_preciseAllocations[dstIndex++] = allocation;
    }
    m_preciseAllocations.shrink(dstIndex);
    m_preciseAllocationsNurseryOffset = m_preciseAllocations.size();
}

void MarkedSpace::prepareForAllocation()
{
    ASSERT(!Thread::mayBeGCThread() || heap().worldIsStopped());
    for (Subspace* subspace : m_subspaces)
        subspace->prepareForAllocation();

    m_activeWeakSets.takeFrom(m_newActiveWeakSets);
    
    if (heap().collectionScope() == CollectionScope::Eden)
        m_preciseAllocationsNurseryOffsetForSweep = m_preciseAllocationsNurseryOffset;
    else
        m_preciseAllocationsNurseryOffsetForSweep = 0;
    m_preciseAllocationsNurseryOffset = m_preciseAllocations.size();
}

void MarkedSpace::enablePreciseAllocationTracking()
{
    m_preciseAllocationSet = makeUnique<HashSet<HeapCell*>>();
    for (auto* allocation : m_preciseAllocations)
        m_preciseAllocationSet->add(allocation->cell());
}

void MarkedSpace::visitWeakSets(SlotVisitor& visitor)
{
    auto visit = [&] (WeakSet* weakSet) {
        weakSet->visit(visitor);
    };
    
    m_newActiveWeakSets.forEach(visit);
    
    if (heap().collectionScope() == CollectionScope::Full)
        m_activeWeakSets.forEach(visit);
}

void MarkedSpace::reapWeakSets()
{
    auto visit = [&] (WeakSet* weakSet) {
        weakSet->reap();
    };
    
    m_newActiveWeakSets.forEach(visit);
    
    if (heap().collectionScope() == CollectionScope::Full)
        m_activeWeakSets.forEach(visit);
}

void MarkedSpace::stopAllocating()
{
    ASSERT(!isIterating());
    forEachDirectory(
        [&] (BlockDirectory& directory) -> IterationStatus {
            directory.stopAllocating();
            return IterationStatus::Continue;
        });
}

void MarkedSpace::stopAllocatingForGood()
{
    ASSERT(!isIterating());
    forEachDirectory(
        [&] (BlockDirectory& directory) -> IterationStatus {
            directory.stopAllocatingForGood();
            return IterationStatus::Continue;
        });
}

void MarkedSpace::prepareForConservativeScan()
{
    m_preciseAllocationsForThisCollectionBegin = m_preciseAllocations.begin() + m_preciseAllocationsOffsetForThisCollection;
    m_preciseAllocationsForThisCollectionSize = m_preciseAllocations.size() - m_preciseAllocationsOffsetForThisCollection;
    m_preciseAllocationsForThisCollectionEnd = m_preciseAllocations.end();
    RELEASE_ASSERT(m_preciseAllocationsForThisCollectionEnd == m_preciseAllocationsForThisCollectionBegin + m_preciseAllocationsForThisCollectionSize);
    
    std::sort(
        m_preciseAllocationsForThisCollectionBegin, m_preciseAllocationsForThisCollectionEnd,
        [&] (PreciseAllocation* a, PreciseAllocation* b) {
            return a < b;
        });
    unsigned index = m_preciseAllocationsOffsetForThisCollection;
    for (auto* start = m_preciseAllocationsForThisCollectionBegin; start != m_preciseAllocationsForThisCollectionEnd; ++start, ++index) {
        (*start)->setIndexInSpace(index);
        ASSERT(m_preciseAllocations[index] == *start);
        ASSERT(m_preciseAllocations[index]->indexInSpace() == index);
    }
}

void MarkedSpace::prepareForMarking()
{
    if (heap().collectionScope() == CollectionScope::Eden)
        m_preciseAllocationsOffsetForThisCollection = m_preciseAllocationsNurseryOffset;
    else
        m_preciseAllocationsOffsetForThisCollection = 0;
}

void MarkedSpace::resumeAllocating()
{
    forEachDirectory(
        [&] (BlockDirectory& directory) -> IterationStatus {
            directory.resumeAllocating();
            return IterationStatus::Continue;
        });
    // Nothing to do for PreciseAllocations.
}

bool MarkedSpace::isPagedOut(MonotonicTime deadline)
{
    bool result = false;
    forEachDirectory(
        [&] (BlockDirectory& directory) -> IterationStatus {
            if (directory.isPagedOut(deadline)) {
                result = true;
                return IterationStatus::Done;
            }
            return IterationStatus::Continue;
        });
    // FIXME: Consider taking PreciseAllocations into account here.
    return result;
}

void MarkedSpace::freeBlock(MarkedBlock::Handle* block)
{
    block->directory()->removeBlock(block);
    m_capacity -= MarkedBlock::blockSize;
    m_blocks.remove(&block->block());
    delete block;
}

void MarkedSpace::freeOrShrinkBlock(MarkedBlock::Handle* block)
{
    if (!block->isEmpty()) {
        block->shrink();
        return;
    }

    freeBlock(block);
}

void MarkedSpace::shrink()
{
    forEachDirectory(
        [&] (BlockDirectory& directory) -> IterationStatus {
            directory.shrink();
            return IterationStatus::Continue;
        });
}

void MarkedSpace::beginMarking()
{
    if (heap().collectionScope() == CollectionScope::Full) {
        forEachDirectory(
            [&] (BlockDirectory& directory) -> IterationStatus {
                directory.beginMarkingForFullCollection();
                return IterationStatus::Continue;
            });

        if (UNLIKELY(nextVersion(m_markingVersion) == initialVersion)) {
            forEachBlock(
                [&] (MarkedBlock::Handle* handle) {
                    handle->block().resetMarks();
                });
        }
        
        m_markingVersion = nextVersion(m_markingVersion);
        
        for (PreciseAllocation* allocation : m_preciseAllocations)
            allocation->flip();
    }

    if (ASSERT_ENABLED) {
        forEachBlock(
            [&] (MarkedBlock::Handle* block) {
                if (block->areMarksStale())
                    return;
                ASSERT(!block->isFreeListed());
            });
    }
    
    m_isMarking = true;
}

void MarkedSpace::endMarking()
{
    if (UNLIKELY(nextVersion(m_newlyAllocatedVersion) == initialVersion)) {
        forEachBlock(
            [&] (MarkedBlock::Handle* handle) {
                handle->block().resetAllocated();
            });
    }
    
    m_newlyAllocatedVersion = nextVersion(m_newlyAllocatedVersion);
    
    for (unsigned i = m_preciseAllocationsOffsetForThisCollection; i < m_preciseAllocations.size(); ++i)
        m_preciseAllocations[i]->clearNewlyAllocated();

    if (ASSERT_ENABLED) {
        for (PreciseAllocation* allocation : m_preciseAllocations)
            ASSERT_UNUSED(allocation, !allocation->isNewlyAllocated());
    }

    forEachDirectory(
        [&] (BlockDirectory& directory) -> IterationStatus {
            directory.endMarking();
            return IterationStatus::Continue;
        });
    
    m_isMarking = false;
}

void MarkedSpace::willStartIterating()
{
    ASSERT(!isIterating());
    stopAllocating();
    m_isIterating = true;
}

void MarkedSpace::didFinishIterating()
{
    ASSERT(isIterating());
    resumeAllocating();
    m_isIterating = false;
}

size_t MarkedSpace::objectCount()
{
    size_t result = 0;
    forEachBlock(
        [&] (MarkedBlock::Handle* block) {
            result += block->markCount();
        });
    for (PreciseAllocation* allocation : m_preciseAllocations) {
        if (allocation->isMarked())
            result++;
    }
    return result;
}

size_t MarkedSpace::size()
{
    size_t result = 0;
    forEachBlock(
        [&] (MarkedBlock::Handle* block) {
            result += block->markCount() * block->cellSize();
        });
    for (PreciseAllocation* allocation : m_preciseAllocations) {
        if (allocation->isMarked())
            result += allocation->cellSize();
    }
    return result;
}

size_t MarkedSpace::capacity()
{
    return m_capacity;
}

void MarkedSpace::addActiveWeakSet(WeakSet* weakSet)
{
    // We conservatively assume that the WeakSet should belong in the new set. In fact, some weak
    // sets might contain new weak handles even though they are tied to old objects. This slightly
    // increases the amount of scanning that an eden collection would have to do, but the effect
    // ought to be small.
    m_newActiveWeakSets.append(weakSet);
}

void MarkedSpace::didAddBlock(MarkedBlock::Handle* block)
{
    // WARNING: This function is called before block is fully initialized. The block will not know
    // its cellSize() or attributes(). The latter implies that you can't ask things like
    // needsDestruction().
    m_capacity += MarkedBlock::blockSize;
    m_blocks.add(&block->block());
}

void MarkedSpace::didAllocateInBlock(MarkedBlock::Handle* block)
{
    if (block->weakSet().isOnList()) {
        block->weakSet().remove();
        m_newActiveWeakSets.append(&block->weakSet());
    }
}

void MarkedSpace::snapshotUnswept()
{
    if (heap().collectionScope() == CollectionScope::Eden) {
        forEachDirectory(
            [&] (BlockDirectory& directory) -> IterationStatus {
                directory.snapshotUnsweptForEdenCollection();
                return IterationStatus::Continue;
            });
    } else {
        forEachDirectory(
            [&] (BlockDirectory& directory) -> IterationStatus {
                directory.snapshotUnsweptForFullCollection();
                return IterationStatus::Continue;
            });
    }
}

void MarkedSpace::assertNoUnswept()
{
    if (!ASSERT_ENABLED)
        return;
    forEachDirectory(
        [&] (BlockDirectory& directory) -> IterationStatus {
            directory.assertNoUnswept();
            return IterationStatus::Continue;
        });
}

void MarkedSpace::dumpBits(PrintStream& out)
{
    forEachDirectory(
        [&] (BlockDirectory& directory) -> IterationStatus {
            out.print("Bits for ", directory, ":\n");
            directory.dumpBits(out);
            return IterationStatus::Continue;
        });
}

void MarkedSpace::addBlockDirectory(const AbstractLocker&, BlockDirectory* directory)
{
    directory->setNextDirectory(nullptr);
    
    WTF::storeStoreFence();

    m_directories.append(std::mem_fn(&BlockDirectory::setNextDirectory), directory);
}

} // namespace JSC
