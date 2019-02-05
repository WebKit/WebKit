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
#include "FunctionCodeBlock.h"
#include "IncrementalSweeper.h"
#include "JSObject.h"
#include "JSCInlines.h"
#include "MarkedBlockInlines.h"
#include <wtf/ListDump.h>

namespace JSC {

std::array<size_t, MarkedSpace::numSizeClasses> MarkedSpace::s_sizeClassForSizeStep;

namespace {

const Vector<size_t>& sizeClasses()
{
    static Vector<size_t>* result;
    static std::once_flag once;
    std::call_once(
        once,
        [] {
            result = new Vector<size_t>();
            
            if (Options::dumpSizeClasses()) {
                dataLog("Block size: ", MarkedBlock::blockSize, "\n");
                dataLog("Footer size: ", sizeof(MarkedBlock::Footer), "\n");
            }
            
            auto add = [&] (size_t sizeClass) {
                sizeClass = WTF::roundUpToMultipleOf<MarkedBlock::atomSize>(sizeClass);
                if (Options::dumpSizeClasses())
                    dataLog("Adding JSC MarkedSpace size class: ", sizeClass, "\n");
                // Perform some validation as we go.
                RELEASE_ASSERT(!(sizeClass % MarkedSpace::sizeStep));
                if (result->isEmpty())
                    RELEASE_ASSERT(sizeClass == MarkedSpace::sizeStep);
                result->append(sizeClass);
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
            
            if (Options::dumpSizeClasses())
                dataLog("    Marked block payload size: ", static_cast<size_t>(MarkedSpace::blockPayload), "\n");
            
            for (unsigned i = 0; ; ++i) {
                double approximateSize = MarkedSpace::preciseCutoff * pow(Options::sizeClassProgression(), i);
                
                if (Options::dumpSizeClasses())
                    dataLog("    Next size class as a double: ", approximateSize, "\n");
        
                size_t approximateSizeInBytes = static_cast<size_t>(approximateSize);
        
                if (Options::dumpSizeClasses())
                    dataLog("    Next size class as bytes: ", approximateSizeInBytes, "\n");
        
                // Make sure that the computer did the math correctly.
                RELEASE_ASSERT(approximateSizeInBytes >= MarkedSpace::preciseCutoff);
                
                if (approximateSizeInBytes > MarkedSpace::largeCutoff)
                    break;
                
                size_t sizeClass =
                    WTF::roundUpToMultipleOf<MarkedSpace::sizeStep>(approximateSizeInBytes);
                
                if (Options::dumpSizeClasses())
                    dataLog("    Size class: ", sizeClass, "\n");
                
                // Optimize the size class so that there isn't any slop at the end of the block's
                // payload.
                unsigned cellsPerBlock = MarkedSpace::blockPayload / sizeClass;
                size_t possiblyBetterSizeClass = (MarkedSpace::blockPayload / cellsPerBlock) & ~(MarkedSpace::sizeStep - 1);
                
                if (Options::dumpSizeClasses())
                    dataLog("    Possibly better size class: ", possiblyBetterSizeClass, "\n");

                // The size class we just came up with is better than the other one if it reduces
                // total wastage assuming we only allocate cells of that size.
                size_t originalWastage = MarkedSpace::blockPayload - cellsPerBlock * sizeClass;
                size_t newWastage = (possiblyBetterSizeClass - sizeClass) * cellsPerBlock;
                
                if (Options::dumpSizeClasses())
                    dataLog("    Original wastage: ", originalWastage, ", new wastage: ", newWastage, "\n");
                
                size_t betterSizeClass;
                if (newWastage > originalWastage)
                    betterSizeClass = sizeClass;
                else
                    betterSizeClass = possiblyBetterSizeClass;
                
                if (Options::dumpSizeClasses())
                    dataLog("    Choosing size class: ", betterSizeClass, "\n");
                
                if (betterSizeClass == result->last()) {
                    // Defense for when expStep is small.
                    continue;
                }
                
                // This is usually how we get out of the loop.
                if (betterSizeClass > MarkedSpace::largeCutoff
                    || betterSizeClass > Options::largeAllocationCutoff())
                    break;
                
                add(betterSizeClass);
            }

            // Manually inject size classes for objects we know will be allocated in high volume.
            // FIXME: All of these things should have IsoSubspaces.
            // https://bugs.webkit.org/show_bug.cgi?id=179876
            add(sizeof(UnlinkedFunctionCodeBlock));
            add(sizeof(JSString));

            {
                // Sort and deduplicate.
                std::sort(result->begin(), result->end());
                auto it = std::unique(result->begin(), result->end());
                result->shrinkCapacity(it - result->begin());
            }

            if (Options::dumpSizeClasses())
                dataLog("JSC Heap MarkedSpace size class dump: ", listDump(*result), "\n");

            // We have an optimiation in MarkedSpace::optimalSizeFor() that assumes things about
            // the size class table. This checks our results against that function's assumptions.
            for (size_t size = MarkedSpace::sizeStep, i = 0; size <= MarkedSpace::preciseCutoff; size += MarkedSpace::sizeStep, i++)
                RELEASE_ASSERT(result->at(i) == size);
        });
    return *result;
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
                    return sizeClass;
                },
                [&] (size_t sizeClass) -> size_t {
                    return sizeClass;
                });
        });
}

MarkedSpace::MarkedSpace(Heap* heap)
    : m_heap(heap)
{
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
    for (LargeAllocation* allocation : m_largeAllocations)
        allocation->destroy();
}

void MarkedSpace::lastChanceToFinalize()
{
    forEachDirectory(
        [&] (BlockDirectory& directory) -> IterationStatus {
            directory.lastChanceToFinalize();
            return IterationStatus::Continue;
        });
    for (LargeAllocation* allocation : m_largeAllocations)
        allocation->lastChanceToFinalize();
}

void MarkedSpace::sweep()
{
    m_heap->sweeper().stopSweeping();
    forEachDirectory(
        [&] (BlockDirectory& directory) -> IterationStatus {
            directory.sweep();
            return IterationStatus::Continue;
        });
}

void MarkedSpace::sweepLargeAllocations()
{
    RELEASE_ASSERT(m_largeAllocationsNurseryOffset == m_largeAllocations.size());
    unsigned srcIndex = m_largeAllocationsNurseryOffsetForSweep;
    unsigned dstIndex = srcIndex;
    while (srcIndex < m_largeAllocations.size()) {
        LargeAllocation* allocation = m_largeAllocations[srcIndex++];
        allocation->sweep();
        if (allocation->isEmpty()) {
            m_capacity -= allocation->cellSize();
            allocation->destroy();
            continue;
        }
        m_largeAllocations[dstIndex++] = allocation;
    }
    m_largeAllocations.shrink(dstIndex);
    m_largeAllocationsNurseryOffset = m_largeAllocations.size();
}

void MarkedSpace::prepareForAllocation()
{
    ASSERT(!mayBeGCThread() || m_heap->worldIsStopped());
    for (Subspace* subspace : m_subspaces)
        subspace->prepareForAllocation();

    m_activeWeakSets.takeFrom(m_newActiveWeakSets);
    
    if (m_heap->collectionScope() == CollectionScope::Eden)
        m_largeAllocationsNurseryOffsetForSweep = m_largeAllocationsNurseryOffset;
    else
        m_largeAllocationsNurseryOffsetForSweep = 0;
    m_largeAllocationsNurseryOffset = m_largeAllocations.size();
}

void MarkedSpace::visitWeakSets(SlotVisitor& visitor)
{
    auto visit = [&] (WeakSet* weakSet) {
        weakSet->visit(visitor);
    };
    
    m_newActiveWeakSets.forEach(visit);
    
    if (m_heap->collectionScope() == CollectionScope::Full)
        m_activeWeakSets.forEach(visit);
}

void MarkedSpace::reapWeakSets()
{
    auto visit = [&] (WeakSet* weakSet) {
        weakSet->reap();
    };
    
    m_newActiveWeakSets.forEach(visit);
    
    if (m_heap->collectionScope() == CollectionScope::Full)
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
    m_largeAllocationsForThisCollectionBegin = m_largeAllocations.begin() + m_largeAllocationsOffsetForThisCollection;
    m_largeAllocationsForThisCollectionSize = m_largeAllocations.size() - m_largeAllocationsOffsetForThisCollection;
    m_largeAllocationsForThisCollectionEnd = m_largeAllocations.end();
    RELEASE_ASSERT(m_largeAllocationsForThisCollectionEnd == m_largeAllocationsForThisCollectionBegin + m_largeAllocationsForThisCollectionSize);
    
    std::sort(
        m_largeAllocationsForThisCollectionBegin, m_largeAllocationsForThisCollectionEnd,
        [&] (LargeAllocation* a, LargeAllocation* b) {
            return a < b;
        });
}

void MarkedSpace::prepareForMarking()
{
    if (m_heap->collectionScope() == CollectionScope::Eden)
        m_largeAllocationsOffsetForThisCollection = m_largeAllocationsNurseryOffset;
    else
        m_largeAllocationsOffsetForThisCollection = 0;
}

void MarkedSpace::resumeAllocating()
{
    forEachDirectory(
        [&] (BlockDirectory& directory) -> IterationStatus {
            directory.resumeAllocating();
            return IterationStatus::Continue;
        });
    // Nothing to do for LargeAllocations.
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
    // FIXME: Consider taking LargeAllocations into account here.
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
    if (m_heap->collectionScope() == CollectionScope::Full) {
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
        
        for (LargeAllocation* allocation : m_largeAllocations)
            allocation->flip();
    }

    if (!ASSERT_DISABLED) {
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
    
    for (unsigned i = m_largeAllocationsOffsetForThisCollection; i < m_largeAllocations.size(); ++i)
        m_largeAllocations[i]->clearNewlyAllocated();

    if (!ASSERT_DISABLED) {
        for (LargeAllocation* allocation : m_largeAllocations)
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
    for (LargeAllocation* allocation : m_largeAllocations) {
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
    for (LargeAllocation* allocation : m_largeAllocations) {
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
    if (m_heap->collectionScope() == CollectionScope::Eden) {
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
    if (ASSERT_DISABLED)
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
