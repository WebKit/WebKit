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

void MarkedSpace::destroy()
{
    clearMarkBits(); // Make sure weak pointers appear dead during destruction.

    while (m_heap.usedBlocks)
        freeBlock(0);
    fastFree(m_heap.blocks);

    memset(&m_heap, 0, sizeof(CollectorHeap));
}

NEVER_INLINE MarkedBlock* MarkedSpace::allocateBlock()
{
    PageAllocationAligned allocation = PageAllocationAligned::allocate(BLOCK_SIZE, BLOCK_SIZE, OSAllocator::JSGCHeapPages);
    MarkedBlock* block = static_cast<MarkedBlock*>(allocation.base());
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
    ObjectIterator it(m_heap, block, 0);
    ObjectIterator end(m_heap, block + 1, 0);
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
            if (!block->marked.testAndSet(m_heap.nextCell)) { // Always false for the last cell in the block
                Cell* cell = &block->cells[m_heap.nextCell];

                JSCell* imp = reinterpret_cast<JSCell*>(cell);
                imp->~JSCell();

                ++m_heap.nextCell;
                return cell;
            }
            m_heap.nextCell = block->marked.nextPossiblyUnset(m_heap.nextCell);
        } while (m_heap.nextCell != HeapConstants::cellsPerBlock);
        m_heap.nextCell = 0;
        m_heap.waterMark += BLOCK_SIZE;
    } while (++m_heap.nextBlock != m_heap.usedBlocks);

    if (m_heap.waterMark < m_heap.highWaterMark) {
        MarkedBlock* block = allocateBlock();
        ASSERT(!block->marked.get(m_heap.nextCell));
        block->marked.set(m_heap.nextCell);
        return &block->cells[m_heap.nextCell++];
    }

    return 0;
}

void MarkedSpace::shrink()
{
    // Clear the always-on last bit, so isEmpty() isn't fooled by it.
    for (size_t i = 0; i < m_heap.usedBlocks; ++i)
        m_heap.collectorBlock(i)->marked.clear(HeapConstants::cellsPerBlock - 1);

    for (size_t i = 0; i != m_heap.usedBlocks && m_heap.usedBlocks > 1; ) { // We assume at least one block exists at all times.
        if (m_heap.collectorBlock(i)->marked.isEmpty()) {
            freeBlock(i);
        } else
            ++i;
    }

    // Reset the always-on last bit.
    for (size_t i = 0; i < m_heap.usedBlocks; ++i)
        m_heap.collectorBlock(i)->marked.set(HeapConstants::cellsPerBlock - 1);
}

bool MarkedSpace::containsSlowCase(const void* x)
{
    uintptr_t xAsBits = reinterpret_cast<uintptr_t>(x);
    xAsBits &= CELL_ALIGN_MASK;

    uintptr_t offset = xAsBits & BLOCK_OFFSET_MASK;
    const size_t lastCellOffset = sizeof(CollectorCell) * (CELLS_PER_BLOCK - 1);
    if (offset > lastCellOffset)
        return false;

    MarkedBlock* blockAddr = reinterpret_cast<MarkedBlock*>(xAsBits - offset);
    size_t usedBlocks = m_heap.usedBlocks;
    for (size_t block = 0; block < usedBlocks; block++) {
        if (m_heap.collectorBlock(block) != blockAddr)
            continue;

        // x is a pointer into the heap. Now, verify that the cell it
        // points to is live. (If the cell is dead, we must not mark it,
        // since that would revive it in a zombie state.)
        size_t cellOffset = offset / CELL_SIZE;
        return blockAddr->marked.get(cellOffset);
    }
    
    return false;
}

void MarkedSpace::clearMarkBits()
{
    for (size_t i = 0; i < m_heap.usedBlocks; ++i)
        clearMarkBits(m_heap.collectorBlock(i));
}

void MarkedSpace::clearMarkBits(MarkedBlock* block)
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

    DeadObjectIterator it(m_heap, 0, 0);
    DeadObjectIterator end(m_heap, m_heap.usedBlocks, 0);
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
    
    shrink();
}

size_t MarkedSpace::objectCount() const
{
    return m_heap.nextBlock * HeapConstants::cellsPerBlock // allocated full blocks
           + m_heap.nextCell // allocated cells in current block
           + markedCells(m_heap.nextBlock, m_heap.nextCell) // marked cells in remainder of m_heap
           - m_heap.usedBlocks; // 1 cell per block is a dummy sentinel
}

size_t MarkedSpace::size() const
{
    return objectCount() * HeapConstants::cellSize;
}

size_t MarkedSpace::capacity() const
{
    return m_heap.usedBlocks * BLOCK_SIZE;
}

void MarkedSpace::reset()
{
    m_heap.nextCell = 0;
    m_heap.nextBlock = 0;
    m_heap.waterMark = 0;
#if ENABLE(JSC_ZOMBIES)
    sweep();
#endif
}

LiveObjectIterator MarkedSpace::primaryHeapBegin()
{
    return LiveObjectIterator(m_heap, 0, 0);
}

LiveObjectIterator MarkedSpace::primaryHeapEnd()
{
    return LiveObjectIterator(m_heap, m_heap.usedBlocks, 0);
}

} // namespace JSC
