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

#include "IncrementalSweeper.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "JSObject.h"


namespace JSC {

class Structure;

class Free {
public:
    typedef MarkedBlock* ReturnType;

    enum FreeMode { FreeOrShrink, FreeAll };

    Free(FreeMode, MarkedSpace*);
    void operator()(MarkedBlock*);
    ReturnType returnValue();
    
private:
    FreeMode m_freeMode;
    MarkedSpace* m_markedSpace;
    DoublyLinkedList<MarkedBlock> m_blocks;
};

inline Free::Free(FreeMode freeMode, MarkedSpace* newSpace)
    : m_freeMode(freeMode)
    , m_markedSpace(newSpace)
{
}

inline void Free::operator()(MarkedBlock* block)
{
    if (m_freeMode == FreeOrShrink)
        m_markedSpace->freeOrShrinkBlock(block);
    else
        m_markedSpace->freeBlock(block);
}

inline Free::ReturnType Free::returnValue()
{
    return m_blocks.head();
}

struct VisitWeakSet : MarkedBlock::VoidFunctor {
    VisitWeakSet(HeapRootVisitor& heapRootVisitor) : m_heapRootVisitor(heapRootVisitor) { }
    void operator()(MarkedBlock* block) { block->visitWeakSet(m_heapRootVisitor); }
private:
    HeapRootVisitor& m_heapRootVisitor;
};

struct ReapWeakSet : MarkedBlock::VoidFunctor {
    void operator()(MarkedBlock* block) { block->reapWeakSet(); }
};

MarkedSpace::MarkedSpace(Heap* heap)
    : m_heap(heap)
{
    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep) {
        allocatorFor(cellSize).init(heap, this, cellSize, false, false);
        destructorAllocatorFor(cellSize).init(heap, this, cellSize, true, false);
    }

    for (size_t cellSize = impreciseStep; cellSize <= impreciseCutoff; cellSize += impreciseStep) {
        allocatorFor(cellSize).init(heap, this, cellSize, false, false);
        destructorAllocatorFor(cellSize).init(heap, this, cellSize, true, false);
    }

    m_largeAllocator.init(heap, this, 0, true, false);
    m_structureAllocator.init(heap, this, WTF::roundUpToMultipleOf(32, sizeof(Structure)), true, true);
}

MarkedSpace::~MarkedSpace()
{
    Free free(Free::FreeAll, this);
    forEachBlock(free);
}

struct LastChanceToFinalize : MarkedBlock::VoidFunctor {
    void operator()(MarkedBlock* block) { block->lastChanceToFinalize(); }
};

void MarkedSpace::lastChanceToFinalize()
{
    canonicalizeCellLivenessData();
    forEachBlock<LastChanceToFinalize>();
}

void MarkedSpace::sweep()
{
    m_heap->sweeper()->willFinishSweeping();
    forEachBlock<Sweep>();
}

void MarkedSpace::resetAllocators()
{
    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep) {
        allocatorFor(cellSize).reset();
        destructorAllocatorFor(cellSize).reset();
    }

    for (size_t cellSize = impreciseStep; cellSize <= impreciseCutoff; cellSize += impreciseStep) {
        allocatorFor(cellSize).reset();
        destructorAllocatorFor(cellSize).reset();
    }

    m_largeAllocator.reset();
    m_structureAllocator.reset();
}

void MarkedSpace::visitWeakSets(HeapRootVisitor& heapRootVisitor)
{
    VisitWeakSet visitWeakSet(heapRootVisitor);
    forEachBlock(visitWeakSet);
}

void MarkedSpace::reapWeakSets()
{
    forEachBlock<ReapWeakSet>();
}

void MarkedSpace::canonicalizeCellLivenessData()
{
    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep) {
        allocatorFor(cellSize).zapFreeList();
        destructorAllocatorFor(cellSize).zapFreeList();
    }

    for (size_t cellSize = impreciseStep; cellSize <= impreciseCutoff; cellSize += impreciseStep) {
        allocatorFor(cellSize).zapFreeList();
        destructorAllocatorFor(cellSize).zapFreeList();
    }

    m_largeAllocator.zapFreeList();
    m_structureAllocator.zapFreeList();
}

bool MarkedSpace::isPagedOut(double deadline)
{
    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep) {
        if (allocatorFor(cellSize).isPagedOut(deadline) || destructorAllocatorFor(cellSize).isPagedOut(deadline))
            return true;
    }

    for (size_t cellSize = impreciseStep; cellSize <= impreciseCutoff; cellSize += impreciseStep) {
        if (allocatorFor(cellSize).isPagedOut(deadline) || destructorAllocatorFor(cellSize).isPagedOut(deadline))
            return true;
    }

    if (m_largeAllocator.isPagedOut(deadline))
        return true;

    if (m_structureAllocator.isPagedOut(deadline))
        return true;

    return false;
}

void MarkedSpace::freeBlock(MarkedBlock* block)
{
    allocatorFor(block).removeBlock(block);
    m_blocks.remove(block);
    if (block->capacity() == MarkedBlock::blockSize) {
        m_heap->blockAllocator().deallocate(MarkedBlock::destroy(block));
        return;
    }

    MarkedBlock::destroy(block).deallocate();
}

void MarkedSpace::freeOrShrinkBlock(MarkedBlock* block)
{
    if (!block->isEmpty()) {
        block->shrink();
        return;
    }

    freeBlock(block);
}

struct Shrink : MarkedBlock::VoidFunctor {
    void operator()(MarkedBlock* block) { block->shrink(); }
};

void MarkedSpace::shrink()
{
    Free freeOrShrink(Free::FreeOrShrink, this);
    forEachBlock(freeOrShrink);
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
