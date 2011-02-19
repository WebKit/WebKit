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
    m_heap.nextAtom = MarkedBlock::firstAtom();
}

void MarkedSpace::destroy()
{
    clearMarks(); // Make sure weak pointers appear dead during destruction.

    while (m_heap.blocks.size())
        freeBlock(0);
}

NEVER_INLINE MarkedBlock* MarkedSpace::allocateBlock()
{
    MarkedBlock* block = MarkedBlock::create(globalData(), cellSize);
    m_heap.blocks.append(block);
    m_blocks.add(block);
    return block;
}

NEVER_INLINE void MarkedSpace::freeBlock(size_t blockNumber)
{
    MarkedBlock* block = m_heap.blocks[blockNumber];

    // swap with the last block so we compact as we go
    m_heap.blocks[blockNumber] = m_heap.blocks.last();
    m_heap.blocks.removeLast();
    m_blocks.remove(block);

    MarkedBlock::destroy(block);
}

void* MarkedSpace::allocate(size_t)
{
    for ( ; m_heap.nextBlock < m_heap.blocks.size(); ++m_heap.nextBlock) {
        MarkedBlock* block = m_heap.collectorBlock(m_heap.nextBlock);
        if (void* result = block->allocate(m_heap.nextAtom))
            return result;

        m_waterMark += block->capacity();
    }

    if (m_waterMark < m_highWaterMark)
        return allocateBlock()->allocate(m_heap.nextAtom);

    return 0;
}

void MarkedSpace::shrink()
{
    for (size_t i = 0; i != m_heap.blocks.size() && m_heap.blocks.size() > 1; ) { // We assume at least one block exists at all times.
        if (m_heap.collectorBlock(i)->isEmpty()) {
            freeBlock(i);
        } else
            ++i;
    }
}

void MarkedSpace::clearMarks()
{
    BlockIterator end = m_blocks.end();
    for (BlockIterator it = m_blocks.begin(); it != end; ++it)
        (*it)->clearMarks();
}

void MarkedSpace::sweep()
{
    BlockIterator end = m_blocks.end();
    for (BlockIterator it = m_blocks.begin(); it != end; ++it)
        (*it)->sweep();
}

size_t MarkedSpace::objectCount() const
{
    size_t result = 0;
    BlockIterator end = m_blocks.end();
    for (BlockIterator it = m_blocks.begin(); it != end; ++it)
        result += (*it)->markCount();
    return result;
}

size_t MarkedSpace::size() const
{
    size_t result = 0;
    BlockIterator end = m_blocks.end();
    for (BlockIterator it = m_blocks.begin(); it != end; ++it)
        result += (*it)->size();
    return result;
}

size_t MarkedSpace::capacity() const
{
    size_t result = 0;
    BlockIterator end = m_blocks.end();
    for (BlockIterator it = m_blocks.begin(); it != end; ++it)
        result += (*it)->capacity();
    return result;
}

void MarkedSpace::reset()
{
    m_heap.nextBlock = 0;
    m_heap.nextAtom = MarkedBlock::firstAtom();
    m_waterMark = 0;
#if ENABLE(JSC_ZOMBIES)
    sweep();
#endif
}

} // namespace JSC
