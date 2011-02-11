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

namespace JSC {

class Structure;

MarkedSpace::MarkedSpace(JSGlobalData* globalData)
    : m_waterMark(0)
    , m_highWaterMark(0)
    , m_globalData(globalData)
{
    allocateBlock();
}

void MarkedSpace::destroy()
{
    clearMarkBits(); // Make sure weak pointers appear dead during destruction.

    while (m_heap.blocks.size())
        freeBlock(0);
    m_heap.blocks.clear();
}

NEVER_INLINE MarkedBlock* MarkedSpace::allocateBlock()
{
    MarkedBlock* block = MarkedBlock::create(globalData());
    m_heap.blocks.append(block);
    return block;
}

NEVER_INLINE void MarkedSpace::freeBlock(size_t block)
{
    MarkedBlock::destroy(m_heap.blocks[block]);

    // swap with the last block so we compact as we go
    m_heap.blocks[block] = m_heap.blocks.last();
    m_heap.blocks.removeLast();
}

void* MarkedSpace::allocate(size_t)
{
    do {
        ASSERT(m_heap.nextBlock < m_heap.blocks.size());
        MarkedBlock* block = m_heap.collectorBlock(m_heap.nextBlock);
        if (void* result = block->allocate(m_heap.nextCell))
            return result;

        m_heap.nextCell = 0;
        m_waterMark += BLOCK_SIZE;
    } while (++m_heap.nextBlock != m_heap.blocks.size());

    if (m_waterMark < m_highWaterMark)
        return allocateBlock()->allocate(m_heap.nextCell);

    return 0;
}

void MarkedSpace::shrink()
{
    // Clear the always-on last bit, so isEmpty() isn't fooled by it.
    for (size_t i = 0; i < m_heap.blocks.size(); ++i)
        m_heap.collectorBlock(i)->marked.clear(HeapConstants::cellsPerBlock - 1);

    for (size_t i = 0; i != m_heap.blocks.size() && m_heap.blocks.size() > 1; ) { // We assume at least one block exists at all times.
        if (m_heap.collectorBlock(i)->marked.isEmpty()) {
            freeBlock(i);
        } else
            ++i;
    }

    // Reset the always-on last bit.
    for (size_t i = 0; i < m_heap.blocks.size(); ++i)
        m_heap.collectorBlock(i)->marked.set(HeapConstants::cellsPerBlock - 1);
}

void MarkedSpace::clearMarkBits()
{
    for (size_t i = 0; i < m_heap.blocks.size(); ++i)
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
    ASSERT(startBlock <= m_heap.blocks.size());
    ASSERT(startCell < HeapConstants::cellsPerBlock);

    if (startBlock >= m_heap.blocks.size())
        return 0;

    size_t result = 0;
    result += m_heap.collectorBlock(startBlock)->marked.count(startCell);
    for (size_t i = startBlock + 1; i < m_heap.blocks.size(); ++i)
        result += m_heap.collectorBlock(i)->marked.count();

    return result;
}

void MarkedSpace::sweep()
{
    for (size_t i = 0; i < m_heap.blocks.size(); ++i)
        m_heap.collectorBlock(i)->sweep();
}

size_t MarkedSpace::objectCount() const
{
    return m_heap.nextBlock * HeapConstants::cellsPerBlock // allocated full blocks
           + m_heap.nextCell // allocated cells in current block
           + markedCells(m_heap.nextBlock, m_heap.nextCell) // marked cells in remainder of m_heap
           - m_heap.blocks.size(); // 1 cell per block is a dummy sentinel
}

size_t MarkedSpace::size() const
{
    return objectCount() * HeapConstants::cellSize;
}

size_t MarkedSpace::capacity() const
{
    return m_heap.blocks.size() * BLOCK_SIZE;
}

void MarkedSpace::reset()
{
    m_heap.nextCell = 0;
    m_heap.nextBlock = 0;
    m_waterMark = 0;
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
    return LiveObjectIterator(m_heap, m_heap.blocks.size(), 0);
}

} // namespace JSC
