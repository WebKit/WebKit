/*
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "JSGlobalObject.h"
#include "JSLock.h"
#include "JSObject.h"
#include "ScopeChain.h"

namespace JSC {

class Structure;

MarkedSpace::MarkedSpace(Heap* heap)
    : m_waterMark(0)
    , m_nurseryWaterMark(0)
    , m_heap(heap)
{
    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep)
        sizeClassFor(cellSize).cellSize = cellSize;

    for (size_t cellSize = impreciseStep; cellSize <= impreciseCutoff; cellSize += impreciseStep)
        sizeClassFor(cellSize).cellSize = cellSize;
}

void MarkedSpace::addBlock(SizeClass& sizeClass, MarkedBlock* block)
{
    ASSERT(!sizeClass.currentBlock);
    ASSERT(!sizeClass.firstFreeCell);

    sizeClass.blockList.append(block);
    sizeClass.currentBlock = block;
    sizeClass.firstFreeCell = block->sweep(MarkedBlock::SweepToFreeList);
}

void MarkedSpace::removeBlock(MarkedBlock* block)
{
    SizeClass& sizeClass = sizeClassFor(block->cellSize());
    if (sizeClass.currentBlock == block)
        sizeClass.currentBlock = 0;
    sizeClass.blockList.remove(block);
}

void MarkedSpace::resetAllocator()
{
    m_waterMark = 0;
    m_nurseryWaterMark = 0;

    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep)
        sizeClassFor(cellSize).resetAllocator();

    for (size_t cellSize = impreciseStep; cellSize <= impreciseCutoff; cellSize += impreciseStep)
        sizeClassFor(cellSize).resetAllocator();
}

void MarkedSpace::canonicalizeCellLivenessData()
{
    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep)
        sizeClassFor(cellSize).zapFreeList();

    for (size_t cellSize = impreciseStep; cellSize <= impreciseCutoff; cellSize += impreciseStep)
        sizeClassFor(cellSize).zapFreeList();
}

inline void* MarkedSpace::tryAllocateHelper(MarkedSpace::SizeClass& sizeClass)
{
    MarkedBlock::FreeCell* firstFreeCell = sizeClass.firstFreeCell;
    if (!firstFreeCell) {
        for (MarkedBlock*& block = sizeClass.currentBlock; block; block = static_cast<MarkedBlock*>(block->next())) {
            firstFreeCell = block->sweep(MarkedBlock::SweepToFreeList);
            if (firstFreeCell)
                break;
            m_nurseryWaterMark += block->capacity() - block->size();
            m_waterMark += block->capacity();
            block->didConsumeFreeList();
        }

        if (!firstFreeCell)
            return 0;
    }

    ASSERT(firstFreeCell);
    sizeClass.firstFreeCell = firstFreeCell->next;
    return firstFreeCell;
}

inline void* MarkedSpace::tryAllocate(MarkedSpace::SizeClass& sizeClass)
{
    m_heap->m_operationInProgress = Allocation;
    void* result = tryAllocateHelper(sizeClass);
    m_heap->m_operationInProgress = NoOperation;
    return result;
}

void* MarkedSpace::allocateSlowCase(MarkedSpace::SizeClass& sizeClass)
{
#if COLLECT_ON_EVERY_ALLOCATION
    m_heap->collectAllGarbage();
    ASSERT(m_heap->m_operationInProgress == NoOperation);
#endif
    
    void* result = tryAllocate(sizeClass);
    
    if (LIKELY(result != 0))
        return result;
    
    AllocationEffort allocationEffort;
    
    if ((
#if ENABLE(GGC)
         nurseryWaterMark() < m_heap->m_minBytesPerCycle
#else
         m_heap->waterMark() < m_heap->highWaterMark()
#endif
         ) || !m_heap->m_isSafeToCollect)
        allocationEffort = AllocationMustSucceed;
    else
        allocationEffort = AllocationCanFail;
    
    MarkedBlock* block = allocateBlock(sizeClass.cellSize, allocationEffort);
    if (block) {
        addBlock(sizeClass, block);
        void* result = tryAllocate(sizeClass);
        ASSERT(result);
        return result;
    }
    
    m_heap->collect(Heap::DoNotSweep);
    
    result = tryAllocate(sizeClass);
    
    if (result)
        return result;
    
    ASSERT(m_heap->waterMark() < m_heap->highWaterMark());
    
    addBlock(sizeClass, allocateBlock(sizeClass.cellSize, AllocationMustSucceed));
    
    result = tryAllocate(sizeClass);
    ASSERT(result);
    return result;
}

MarkedBlock* MarkedSpace::allocateBlock(size_t cellSize, AllocationEffort allocationEffort)
{
    MarkedBlock* block;
    
    {
        MutexLocker locker(m_heap->m_freeBlockLock);
        if (m_heap->m_numberOfFreeBlocks) {
            block = static_cast<MarkedBlock*>(m_heap->m_freeBlocks.removeHead());
            ASSERT(block);
            m_heap->m_numberOfFreeBlocks--;
        } else
            block = 0;
    }
    if (block)
        block = MarkedBlock::recycle(block, m_heap, cellSize);
    else if (allocationEffort == AllocationCanFail)
        return 0;
    else
        block = MarkedBlock::create(m_heap, cellSize);
    
    m_blocks.add(block);
    
    return block;
}

void MarkedSpace::freeBlocks(MarkedBlock* head)
{
    MarkedBlock* next;
    for (MarkedBlock* block = head; block; block = next) {
        next = static_cast<MarkedBlock*>(block->next());
        
        m_blocks.remove(block);
        block->sweep();
        MutexLocker locker(m_heap->m_freeBlockLock);
        m_heap->m_freeBlocks.append(block);
        m_heap->m_numberOfFreeBlocks++;
    }
}

class TakeIfUnmarked {
public:
    typedef MarkedBlock* ReturnType;
    
    TakeIfUnmarked(MarkedSpace*);
    void operator()(MarkedBlock*);
    ReturnType returnValue();
    
private:
    MarkedSpace* m_markedSpace;
    DoublyLinkedList<MarkedBlock> m_empties;
};

inline TakeIfUnmarked::TakeIfUnmarked(MarkedSpace* newSpace)
    : m_markedSpace(newSpace)
{
}

inline void TakeIfUnmarked::operator()(MarkedBlock* block)
{
    if (!block->markCountIsZero())
        return;
    
    m_markedSpace->removeBlock(block);
    m_empties.append(block);
}

inline TakeIfUnmarked::ReturnType TakeIfUnmarked::returnValue()
{
    return m_empties.head();
}

void MarkedSpace::shrink()
{
    // We record a temporary list of empties to avoid modifying m_blocks while iterating it.
    TakeIfUnmarked takeIfUnmarked(this);
    freeBlocks(forEachBlock(takeIfUnmarked));
}

#if ENABLE(GGC)
class GatherDirtyCells {
    WTF_MAKE_NONCOPYABLE(GatherDirtyCells);
public:
    typedef void* ReturnType;
    
    explicit GatherDirtyCells(MarkedBlock::DirtyCellVector*);
    void operator()(MarkedBlock*);
    ReturnType returnValue() { return 0; }
    
private:
    MarkedBlock::DirtyCellVector* m_dirtyCells;
};

inline GatherDirtyCells::GatherDirtyCells(MarkedBlock::DirtyCellVector* dirtyCells)
    : m_dirtyCells(dirtyCells)
{
}

inline void GatherDirtyCells::operator()(MarkedBlock* block)
{
    block->gatherDirtyCells(*m_dirtyCells);
}

void MarkedSpace::gatherDirtyCells(MarkedBlock::DirtyCellVector& dirtyCells)
{
    GatherDirtyCells gatherDirtyCells(&dirtyCells);
    forEachBlock(gatherDirtyCells);
}
#endif

} // namespace JSC
