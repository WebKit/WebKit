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
    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep) {
        allocatorFor(cellSize).setCellSize(cellSize);
        allocatorFor(cellSize).setHeap(heap);
        allocatorFor(cellSize).setMarkedSpace(this);
    }

    for (size_t cellSize = impreciseStep; cellSize <= impreciseCutoff; cellSize += impreciseStep) {
        allocatorFor(cellSize).setCellSize(cellSize);
        allocatorFor(cellSize).setHeap(heap);
        allocatorFor(cellSize).setMarkedSpace(this);
    }
}

void MarkedSpace::resetAllocators()
{
    m_waterMark = 0;
    m_nurseryWaterMark = 0;

    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep)
        allocatorFor(cellSize).reset();

    for (size_t cellSize = impreciseStep; cellSize <= impreciseCutoff; cellSize += impreciseStep)
        allocatorFor(cellSize).reset();
}

void MarkedSpace::canonicalizeCellLivenessData()
{
    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep)
        allocatorFor(cellSize).zapFreeList();

    for (size_t cellSize = impreciseStep; cellSize <= impreciseCutoff; cellSize += impreciseStep)
        allocatorFor(cellSize).zapFreeList();
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
    
    m_markedSpace->allocatorFor(block->cellSize()).removeBlock(block);
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
