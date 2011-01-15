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

#include "CollectorHeapIterator.h"
#include "JSCell.h"
#include "JSGlobalData.h"
#include "JSLock.h"

using std::max;

namespace JSC {

class Structure;

// tunable parameters

const size_t GROWTH_FACTOR = 2;
const size_t LOW_WATER_FACTOR = 4;
const size_t ALLOCATIONS_PER_COLLECTION = 3600;
// This value has to be a macro to be used in max() without introducing
// a PIC branch in Mach-O binaries, see <rdar://problem/5971391>.
#define MIN_ARRAY_SIZE (static_cast<size_t>(14))

MarkedSpace::MarkedSpace(JSGlobalData* globalData)
    : m_globalData(globalData)
{
    memset(&m_heap, 0, sizeof(CollectorHeap));
    allocateBlock();
}

void MarkedSpace::destroy(ProtectCountSet& protectedValuesCopy)
{
    clearMarkBits();
    ProtectCountSet::iterator protectedValuesEnd = protectedValuesCopy.end();
    for (ProtectCountSet::iterator it = protectedValuesCopy.begin(); it != protectedValuesEnd; ++it)
        markCell(it->first);

    m_heap.nextCell = 0;
    m_heap.nextBlock = 0;
    DeadObjectIterator it(m_heap, m_heap.nextBlock, m_heap.nextCell);
    DeadObjectIterator end(m_heap, m_heap.usedBlocks);
    for ( ; it != end; ++it)
        (*it)->~JSCell();

    protectedValuesEnd = protectedValuesCopy.end();
    for (ProtectCountSet::iterator it = protectedValuesCopy.begin(); it != protectedValuesEnd; ++it)
        it->first->~JSCell();

    for (size_t block = 0; block < m_heap.usedBlocks; ++block)
        m_heap.blocks[block].deallocate();

    fastFree(m_heap.blocks);

    memset(&m_heap, 0, sizeof(CollectorHeap));
}

NEVER_INLINE CollectorBlock* MarkedSpace::allocateBlock()
{
    PageAllocationAligned allocation = PageAllocationAligned::allocate(BLOCK_SIZE, BLOCK_SIZE, OSAllocator::JSGCHeapPages);
    CollectorBlock* block = static_cast<CollectorBlock*>(allocation.base());
    if (!block)
        CRASH();

    // Initialize block.

    block->heap = &globalData()->heap;
    clearMarkBits(block);

    Structure* dummyMarkableCellStructure = globalData()->dummyMarkableCellStructure.get();
    for (size_t i = 0; i < HeapConstants::cellsPerBlock; ++i)
        new (&block->cells[i]) JSCell(dummyMarkableCellStructure);
    
    // Add block to blocks vector.

    size_t numBlocks = m_heap.numBlocks;
    if (m_heap.usedBlocks == numBlocks) {
        static const size_t maxNumBlocks = ULONG_MAX / sizeof(PageAllocationAligned) / GROWTH_FACTOR;
        if (numBlocks > maxNumBlocks)
            CRASH();
        numBlocks = max(MIN_ARRAY_SIZE, numBlocks * GROWTH_FACTOR);
        m_heap.numBlocks = numBlocks;
        m_heap.blocks = static_cast<PageAllocationAligned*>(fastRealloc(m_heap.blocks, numBlocks * sizeof(PageAllocationAligned)));
    }
    m_heap.blocks[m_heap.usedBlocks++] = allocation;

    return block;
}

NEVER_INLINE void MarkedSpace::freeBlock(size_t block)
{
    m_heap.didShrink = true;

    ObjectIterator it(m_heap, block);
    ObjectIterator end(m_heap, block + 1);
    for ( ; it != end; ++it)
        (*it)->~JSCell();
    m_heap.blocks[block].deallocate();

    // swap with the last block so we compact as we go
    m_heap.blocks[block] = m_heap.blocks[m_heap.usedBlocks - 1];
    m_heap.usedBlocks--;

    if (m_heap.numBlocks > MIN_ARRAY_SIZE && m_heap.usedBlocks < m_heap.numBlocks / LOW_WATER_FACTOR) {
        m_heap.numBlocks = m_heap.numBlocks / GROWTH_FACTOR; 
        m_heap.blocks = static_cast<PageAllocationAligned*>(fastRealloc(m_heap.blocks, m_heap.numBlocks * sizeof(PageAllocationAligned)));
    }
}

void* MarkedSpace::allocate(size_t s)
{
    ASSERT(globalData()->identifierTable == wtfThreadData().currentIdentifierTable());
    typedef HeapConstants::Block Block;
    typedef HeapConstants::Cell Cell;
    
    ASSERT(JSLock::lockCount() > 0);
    ASSERT(JSLock::currentThreadIsHoldingLock());
    ASSERT_UNUSED(s, s <= HeapConstants::cellSize);

    // Fast case: find the next garbage cell and recycle it.

    do {
        ASSERT(m_heap.nextBlock < m_heap.usedBlocks);
        Block* block = m_heap.collectorBlock(m_heap.nextBlock);
        do {
            ASSERT(m_heap.nextCell < HeapConstants::cellsPerBlock);
            if (!block->marked.get(m_heap.nextCell)) { // Always false for the last cell in the block
                Cell* cell = &block->cells[m_heap.nextCell];

                JSCell* imp = reinterpret_cast<JSCell*>(cell);
                imp->~JSCell();

                ++m_heap.nextCell;
                return cell;
            }
            block->marked.advanceToNextPossibleFreeCell(m_heap.nextCell);
        } while (m_heap.nextCell != HeapConstants::cellsPerBlock);
        m_heap.nextCell = 0;
    } while (++m_heap.nextBlock != m_heap.usedBlocks);
    
    return 0;
}

void MarkedSpace::resizeBlocks()
{
    m_heap.didShrink = false;

    size_t usedCellCount = markedCells();
    size_t minCellCount = usedCellCount + max(ALLOCATIONS_PER_COLLECTION, usedCellCount);
    size_t minBlockCount = (minCellCount + HeapConstants::cellsPerBlock - 1) / HeapConstants::cellsPerBlock;

    size_t maxCellCount = 1.25f * minCellCount;
    size_t maxBlockCount = (maxCellCount + HeapConstants::cellsPerBlock - 1) / HeapConstants::cellsPerBlock;

    if (m_heap.usedBlocks < minBlockCount)
        growBlocks(minBlockCount);
    else if (m_heap.usedBlocks > maxBlockCount)
        shrinkBlocks(maxBlockCount);
}

void MarkedSpace::growBlocks(size_t neededBlocks)
{
    ASSERT(m_heap.usedBlocks < neededBlocks);
    while (m_heap.usedBlocks < neededBlocks)
        allocateBlock();
}

void MarkedSpace::shrinkBlocks(size_t neededBlocks)
{
    ASSERT(m_heap.usedBlocks > neededBlocks);
    
    // Clear the always-on last bit, so isEmpty() isn't fooled by it.
    for (size_t i = 0; i < m_heap.usedBlocks; ++i)
        m_heap.collectorBlock(i)->marked.clear(HeapConstants::cellsPerBlock - 1);

    for (size_t i = 0; i != m_heap.usedBlocks && m_heap.usedBlocks != neededBlocks; ) {
        if (m_heap.collectorBlock(i)->marked.isEmpty()) {
            freeBlock(i);
        } else
            ++i;
    }

    // Reset the always-on last bit.
    for (size_t i = 0; i < m_heap.usedBlocks; ++i)
        m_heap.collectorBlock(i)->marked.set(HeapConstants::cellsPerBlock - 1);
}

inline bool isPointerAligned(void* p)
{
    return (((intptr_t)(p) & (sizeof(char*) - 1)) == 0);
}

// Cell size needs to be a power of two for isPossibleCell to be valid.
COMPILE_ASSERT(sizeof(CollectorCell) % 2 == 0, Collector_cell_size_is_power_of_two);

static inline bool isCellAligned(void *p)
{
    return (((intptr_t)(p) & CELL_MASK) == 0);
}

static inline bool isPossibleCell(void* p)
{
    return isCellAligned(p) && p;
}

void MarkedSpace::markConservatively(MarkStack& markStack, void* start, void* end)
{
#if OS(WINCE)
    if (start > end) {
        void* tmp = start;
        start = end;
        end = tmp;
    }
#else
    ASSERT(start <= end);
#endif

    ASSERT((static_cast<char*>(end) - static_cast<char*>(start)) < 0x1000000);
    ASSERT(isPointerAligned(start));
    ASSERT(isPointerAligned(end));

    char** p = static_cast<char**>(start);
    char** e = static_cast<char**>(end);

    while (p != e) {
        char* x = *p++;
        if (isPossibleCell(x)) {
            size_t usedBlocks;
            uintptr_t xAsBits = reinterpret_cast<uintptr_t>(x);
            xAsBits &= CELL_ALIGN_MASK;

            uintptr_t offset = xAsBits & BLOCK_OFFSET_MASK;
            const size_t lastCellOffset = sizeof(CollectorCell) * (CELLS_PER_BLOCK - 1);
            if (offset > lastCellOffset)
                continue;

            CollectorBlock* blockAddr = reinterpret_cast<CollectorBlock*>(xAsBits - offset);
            usedBlocks = m_heap.usedBlocks;
            for (size_t block = 0; block < usedBlocks; block++) {
                if (m_heap.collectorBlock(block) != blockAddr)
                    continue;
                markStack.append(reinterpret_cast<JSCell*>(xAsBits));
            }
        }
    }
}

void MarkedSpace::clearMarkBits()
{
    for (size_t i = 0; i < m_heap.usedBlocks; ++i)
        clearMarkBits(m_heap.collectorBlock(i));
}

void MarkedSpace::clearMarkBits(CollectorBlock* block)
{
    // allocate assumes that the last cell in every block is marked.
    block->marked.clearAll();
    block->marked.set(HeapConstants::cellsPerBlock - 1);
}

size_t MarkedSpace::markedCells(size_t startBlock, size_t startCell) const
{
    ASSERT(startBlock <= m_heap.usedBlocks);
    ASSERT(startCell < HeapConstants::cellsPerBlock);

    if (startBlock >= m_heap.usedBlocks)
        return 0;

    size_t result = 0;
    result += m_heap.collectorBlock(startBlock)->marked.count(startCell);
    for (size_t i = startBlock + 1; i < m_heap.usedBlocks; ++i)
        result += m_heap.collectorBlock(i)->marked.count();

    return result;
}

void MarkedSpace::sweep()
{
#if !ENABLE(JSC_ZOMBIES)
    Structure* dummyMarkableCellStructure = globalData()->dummyMarkableCellStructure.get();
#endif

    DeadObjectIterator it(m_heap, m_heap.nextBlock, m_heap.nextCell);
    DeadObjectIterator end(m_heap, m_heap.usedBlocks);
    for ( ; it != end; ++it) {
        JSCell* cell = *it;
#if ENABLE(JSC_ZOMBIES)
        if (!cell->isZombie()) {
            const ClassInfo* info = cell->classInfo();
            cell->~JSCell();
            new (cell) JSZombie(info, JSZombie::leakedZombieStructure());
            Heap::markCell(cell);
        }
#else
        cell->~JSCell();
        // Callers of sweep assume it's safe to mark any cell in the heap.
        new (cell) JSCell(dummyMarkableCellStructure);
#endif
    }
}

size_t MarkedSpace::objectCount() const
{
    return m_heap.nextBlock * HeapConstants::cellsPerBlock // allocated full blocks
           + m_heap.nextCell // allocated cells in current block
           + markedCells(m_heap.nextBlock, m_heap.nextCell) // marked cells in remainder of m_heap
           - m_heap.usedBlocks; // 1 cell per block is a dummy sentinel
}

void MarkedSpace::addToStatistics(Statistics& statistics) const
{
    statistics.size += m_heap.usedBlocks * BLOCK_SIZE;
    statistics.free += m_heap.usedBlocks * BLOCK_SIZE - (objectCount() * HeapConstants::cellSize);
}

MarkedSpace::Statistics MarkedSpace::statistics() const
{
    Statistics statistics = { 0, 0 };
    addToStatistics(statistics);
    return statistics;
}

size_t MarkedSpace::size() const
{
    return m_heap.usedBlocks * BLOCK_SIZE;
}

void MarkedSpace::reset()
{
    m_heap.nextCell = 0;
    m_heap.nextBlock = 0;
#if ENABLE(JSC_ZOMBIES)
    sweep();
#endif
    resizeBlocks();
}

LiveObjectIterator MarkedSpace::primaryHeapBegin()
{
    return LiveObjectIterator(m_heap, 0);
}

LiveObjectIterator MarkedSpace::primaryHeapEnd()
{
    return LiveObjectIterator(m_heap, m_heap.usedBlocks);
}

} // namespace JSC
