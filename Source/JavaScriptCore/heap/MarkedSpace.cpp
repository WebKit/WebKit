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

class Take {
public:
    typedef MarkedBlock* ReturnType;

    enum TakeMode { TakeIfEmpty, TakeAll };

    Take(TakeMode, MarkedSpace*);
    void operator()(MarkedBlock*);
    ReturnType returnValue();
    
private:
    TakeMode m_takeMode;
    MarkedSpace* m_markedSpace;
    DoublyLinkedList<MarkedBlock> m_blocks;
};

inline Take::Take(TakeMode takeMode, MarkedSpace* newSpace)
    : m_takeMode(takeMode)
    , m_markedSpace(newSpace)
{
}

inline void Take::operator()(MarkedBlock* block)
{
    if (m_takeMode == TakeIfEmpty && !block->isEmpty())
        return;
    
    m_markedSpace->allocatorFor(block).removeBlock(block);
    m_blocks.append(block);
}

inline Take::ReturnType Take::returnValue()
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

struct SweepWeakSet : MarkedBlock::VoidFunctor {
    void operator()(MarkedBlock* block) { block->sweepWeakSet(); }
};

MarkedSpace::MarkedSpace(Heap* heap)
    : m_heap(heap)
{
    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep) {
        allocatorFor(cellSize).init(heap, this, cellSize, false);
        destructorAllocatorFor(cellSize).init(heap, this, cellSize, true);
    }

    for (size_t cellSize = impreciseStep; cellSize <= impreciseCutoff; cellSize += impreciseStep) {
        allocatorFor(cellSize).init(heap, this, cellSize, false);
        destructorAllocatorFor(cellSize).init(heap, this, cellSize, true);
    }
}

MarkedSpace::~MarkedSpace()
{
    // We record a temporary list of empties to avoid modifying m_blocks while iterating it.
    Take take(Take::TakeAll, this);
    freeBlocks(forEachBlock(take));
}

struct LastChanceToFinalize : MarkedBlock::VoidFunctor {
    void operator()(MarkedBlock* block) { block->lastChanceToFinalize(); }
};

void MarkedSpace::lastChanceToFinalize()
{
    canonicalizeCellLivenessData();
    forEachBlock<LastChanceToFinalize>();
}

struct ResetAllocator : MarkedBlock::VoidFunctor {
    void operator()(MarkedBlock* block) { block->resetAllocator(); }
};

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

    forEachBlock<ResetAllocator>();
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

void MarkedSpace::sweepWeakSets()
{
    forEachBlock<SweepWeakSet>();
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

    return false;
}

void MarkedSpace::freeBlocks(MarkedBlock* head)
{
    MarkedBlock* next;
    for (MarkedBlock* block = head; block; block = next) {
        next = static_cast<MarkedBlock*>(block->next());
        
        m_blocks.remove(block);
        block->sweep();

        m_heap->blockAllocator().deallocate(MarkedBlock::destroy(block));
    }
}

struct Shrink : MarkedBlock::VoidFunctor {
    void operator()(MarkedBlock* block) { block->shrink(); }
};

void MarkedSpace::shrink()
{
    // We record a temporary list of empties to avoid modifying m_blocks while iterating it.
    Take takeIfEmpty(Take::TakeIfEmpty, this);
    freeBlocks(forEachBlock(takeIfEmpty));

    forEachBlock<Shrink>();
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
