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
    allocateBlock();
}

void MarkedSpace::destroy()
{
    clearMarks(); // Make sure weak pointers appear dead during destruction.

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

        m_waterMark += block->capacity();
    } while (++m_heap.nextBlock != m_heap.blocks.size());

    if (m_waterMark < m_highWaterMark)
        return allocateBlock()->allocate(m_heap.nextCell);

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
    for (size_t i = 0; i < m_heap.blocks.size(); ++i)
        m_heap.collectorBlock(i)->clearMarks();
}

void MarkedSpace::sweep()
{
    for (size_t i = 0; i < m_heap.blocks.size(); ++i)
        m_heap.collectorBlock(i)->sweep();
}

size_t MarkedSpace::objectCount() const
{
    size_t result = 0;
    for (size_t i = 0; i < m_heap.blocks.size(); ++i)
        result += m_heap.collectorBlock(i)->markCount();
    return result;
}

size_t MarkedSpace::size() const
{
    size_t result = 0;
    for (size_t i = 0; i < m_heap.blocks.size(); ++i)
        result += m_heap.collectorBlock(i)->size();
    return result;
}

size_t MarkedSpace::capacity() const
{
    size_t result = 0;
    for (size_t i = 0; i < m_heap.blocks.size(); ++i)
        result += m_heap.collectorBlock(i)->capacity();
    return result;
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

} // namespace JSC
