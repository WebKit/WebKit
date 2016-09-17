/*
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2016 Apple Inc. All rights reserved.
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

#include "IncrementalSweeper.h"
#include "JSObject.h"
#include "JSCInlines.h"
#include "SuperSampler.h"
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
            
            auto add = [&] (size_t sizeClass) {
                if (Options::dumpSizeClasses())
                    dataLog("Adding JSC MarkedSpace size class: ", sizeClass, "\n");
                // Perform some validation as we go.
                RELEASE_ASSERT(!(sizeClass % MarkedSpace::sizeStep));
                if (result->isEmpty())
                    RELEASE_ASSERT(sizeClass == MarkedSpace::sizeStep);
                else
                    RELEASE_ASSERT(sizeClass > result->last());
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
    for (size_t i = nextIndex; i < MarkedSpace::numSizeClasses; ++i)
        table[i] = defaultCons(MarkedSpace::indexToSizeClass(i));
}

} // anonymous namespace

void MarkedSpace::initializeSizeClassForStepSize()
{
    // We call this multiple times and we may call it simultaneously from multiple threads. That's
    // OK, since it always stores the same values into the table.
    
    buildSizeClassTable(
        s_sizeClassForSizeStep,
        [&] (size_t sizeClass) -> size_t {
            return sizeClass;
        },
        [&] (size_t sizeClass) -> size_t {
            return sizeClass;
        });
}

MarkedSpace::MarkedSpace(Heap* heap)
    : m_heap(heap)
    , m_capacity(0)
    , m_isIterating(false)
{
    initializeSizeClassForStepSize();
    
    forEachSubspace(
        [&] (Subspace& subspace, AllocatorAttributes attributes) -> IterationStatus {
            subspace.attributes = attributes;
            
            buildSizeClassTable(
                subspace.allocatorForSizeStep,
                [&] (size_t sizeClass) -> MarkedAllocator* {
                    return subspace.bagOfAllocators.add(heap, this, sizeClass, attributes);
                },
                [&] (size_t) -> MarkedAllocator* {
                    return nullptr;
                });
            
            return IterationStatus::Continue;
        });
}

MarkedSpace::~MarkedSpace()
{
    forEachBlock(
        [&] (MarkedBlock::Handle* block) {
            freeBlock(block);
        });
    for (LargeAllocation* allocation : m_largeAllocations)
        allocation->destroy();
    ASSERT(!m_blocks.set().size());
}

void MarkedSpace::lastChanceToFinalize()
{
    stopAllocating();
    forEachAllocator(
        [&] (MarkedAllocator& allocator) -> IterationStatus {
            allocator.lastChanceToFinalize();
            return IterationStatus::Continue;
        });
    for (LargeAllocation* allocation : m_largeAllocations)
        allocation->lastChanceToFinalize();
}

void* MarkedSpace::allocate(Subspace& subspace, size_t bytes)
{
    if (MarkedAllocator* allocator = allocatorFor(subspace, bytes))
        return allocator->allocate();
    return allocateLarge(subspace, bytes);
}

void* MarkedSpace::tryAllocate(Subspace& subspace, size_t bytes)
{
    if (MarkedAllocator* allocator = allocatorFor(subspace, bytes))
        return allocator->tryAllocate();
    return tryAllocateLarge(subspace, bytes);
}

void* MarkedSpace::allocateLarge(Subspace& subspace, size_t size)
{
    void* result = tryAllocateLarge(subspace, size);
    RELEASE_ASSERT(result);
    return result;
}

void* MarkedSpace::tryAllocateLarge(Subspace& subspace, size_t size)
{
    m_heap->collectIfNecessaryOrDefer();
    
    size = WTF::roundUpToMultipleOf<sizeStep>(size);
    LargeAllocation* allocation = LargeAllocation::tryCreate(*m_heap, size, subspace.attributes);
    if (!allocation)
        return nullptr;
    
    m_largeAllocations.append(allocation);
    m_heap->didAllocate(size);
    m_capacity += size;
    return allocation->cell();
}

void MarkedSpace::sweep()
{
    m_heap->sweeper()->willFinishSweeping();
    forEachBlock(
        [&] (MarkedBlock::Handle* block) {
            block->sweep();
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
    m_largeAllocations.resize(dstIndex);
    m_largeAllocationsNurseryOffset = m_largeAllocations.size();
}

void MarkedSpace::zombifySweep()
{
    if (Options::logGC())
        dataLog("Zombifying sweep...");
    m_heap->sweeper()->willFinishSweeping();
    forEachBlock(
        [&] (MarkedBlock::Handle* block) {
            if (block->needsSweeping())
                block->sweep();
        });
}

void MarkedSpace::resetAllocators()
{
    forEachAllocator(
        [&] (MarkedAllocator& allocator) -> IterationStatus {
            allocator.reset();
            return IterationStatus::Continue;
        });

    m_blocksWithNewObjects.clear();
    m_activeWeakSets.takeFrom(m_newActiveWeakSets);
    if (m_heap->operationInProgress() == EdenCollection)
        m_largeAllocationsNurseryOffsetForSweep = m_largeAllocationsNurseryOffset;
    else
        m_largeAllocationsNurseryOffsetForSweep = 0;
    m_largeAllocationsNurseryOffset = m_largeAllocations.size();
}

void MarkedSpace::visitWeakSets(HeapRootVisitor& heapRootVisitor)
{
    auto visit = [&] (WeakSet* weakSet) {
        weakSet->visit(heapRootVisitor);
    };
    
    m_newActiveWeakSets.forEach(visit);
    
    if (m_heap->operationInProgress() == FullCollection)
        m_activeWeakSets.forEach(visit);
}

void MarkedSpace::reapWeakSets()
{
    auto visit = [&] (WeakSet* weakSet) {
        weakSet->reap();
    };
    
    m_newActiveWeakSets.forEach(visit);
    
    if (m_heap->operationInProgress() == FullCollection)
        m_activeWeakSets.forEach(visit);
}

void MarkedSpace::stopAllocating()
{
    ASSERT(!isIterating());
    forEachAllocator(
        [&] (MarkedAllocator& allocator) -> IterationStatus {
            allocator.stopAllocating();
            return IterationStatus::Continue;
        });
}

void MarkedSpace::prepareForMarking()
{
    if (m_heap->operationInProgress() == EdenCollection)
        m_largeAllocationsOffsetForThisCollection = m_largeAllocationsNurseryOffset;
    else
        m_largeAllocationsOffsetForThisCollection = 0;
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

void MarkedSpace::resumeAllocating()
{
    ASSERT(isIterating());
    forEachAllocator(
        [&] (MarkedAllocator& allocator) -> IterationStatus {
            allocator.resumeAllocating();
            return IterationStatus::Continue;
        });
    // Nothing to do for LargeAllocations.
}

bool MarkedSpace::isPagedOut(double deadline)
{
    bool result = false;
    forEachAllocator(
        [&] (MarkedAllocator& allocator) -> IterationStatus {
            if (allocator.isPagedOut(deadline)) {
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
    block->allocator()->removeBlock(block);
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
    forEachBlock(
        [&] (MarkedBlock::Handle* block) {
            freeOrShrinkBlock(block);
        });
    // For LargeAllocations, we do the moral equivalent in sweepLargeAllocations().
}

void MarkedSpace::clearNewlyAllocated()
{
    forEachAllocator(
        [&] (MarkedAllocator& allocator) -> IterationStatus {
            if (MarkedBlock::Handle* block = allocator.takeLastActiveBlock())
                block->clearNewlyAllocated();
            return IterationStatus::Continue;
        });
    
    for (unsigned i = m_largeAllocationsOffsetForThisCollection; i < m_largeAllocations.size(); ++i)
        m_largeAllocations[i]->clearNewlyAllocated();

#if !ASSERT_DISABLED
    forEachBlock(
        [&] (MarkedBlock::Handle* block) {
            ASSERT(!block->clearNewlyAllocated());
        });

    for (LargeAllocation* allocation : m_largeAllocations)
        ASSERT(!allocation->isNewlyAllocated());
#endif // !ASSERT_DISABLED
}

#ifndef NDEBUG 
struct VerifyMarked : MarkedBlock::VoidFunctor { 
    void operator()(MarkedBlock::Handle* block) const
    {
        if (block->needsFlip())
            return;
        switch (block->m_state) {
        case MarkedBlock::Marked:
            return;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
}; 
#endif 

void MarkedSpace::flip()
{
    if (m_heap->operationInProgress() == EdenCollection) {
        for (unsigned i = 0; i < m_blocksWithNewObjects.size(); ++i)
            m_blocksWithNewObjects[i]->flipForEdenCollection();
    } else {
        m_version++; // Henceforth, flipIfNecessary() will trigger on all blocks.
        for (LargeAllocation* allocation : m_largeAllocations)
            allocation->flip();
    }

#ifndef NDEBUG
    VerifyMarked verifyFunctor;
    forEachBlock(verifyFunctor);
#endif
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
    m_capacity += MarkedBlock::blockSize;
    m_blocks.add(&block->block());
}

void MarkedSpace::didAllocateInBlock(MarkedBlock::Handle* block)
{
    block->assertFlipped();
    m_blocksWithNewObjects.append(block);
    
    if (block->weakSet().isOnList()) {
        block->weakSet().remove();
        m_newActiveWeakSets.append(&block->weakSet());
    }
}

} // namespace JSC
