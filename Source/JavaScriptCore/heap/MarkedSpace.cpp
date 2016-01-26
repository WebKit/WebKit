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
#include "JSObject.h"
#include "JSCInlines.h"

namespace JSC {

struct Free : MarkedBlock::VoidFunctor {
    Free(MarkedSpace& space) : m_markedSpace(space) { }
    void operator()(MarkedBlock* block) { m_markedSpace.freeBlock(block); }
private:
    MarkedSpace& m_markedSpace;
};

struct FreeOrShrink : MarkedBlock::VoidFunctor {
    FreeOrShrink(MarkedSpace& space) : m_markedSpace(space) { }
    void operator()(MarkedBlock* block) { m_markedSpace.freeOrShrinkBlock(block); }
private:
    MarkedSpace& m_markedSpace;
};

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
    , m_capacity(0)
    , m_isIterating(false)
{
    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep) {
        allocatorFor(cellSize).init(heap, this, cellSize, false);
        destructorAllocatorFor(cellSize).init(heap, this, cellSize, true);
    }

    for (size_t cellSize = impreciseStart; cellSize <= impreciseCutoff; cellSize += impreciseStep) {
        allocatorFor(cellSize).init(heap, this, cellSize, false);
        destructorAllocatorFor(cellSize).init(heap, this, cellSize, true);
    }

    m_normalSpace.largeAllocator.init(heap, this, 0, false);
    m_destructorSpace.largeAllocator.init(heap, this, 0, true);
}

MarkedSpace::~MarkedSpace()
{
    Free free(*this);
    forEachBlock(free);
    ASSERT(!m_blocks.set().size());
}

struct LastChanceToFinalize {
    void operator()(MarkedAllocator& allocator) { allocator.lastChanceToFinalize(); }
};

void MarkedSpace::lastChanceToFinalize()
{
    stopAllocating();
    forEachAllocator<LastChanceToFinalize>();
}

void MarkedSpace::sweep()
{
    m_heap->sweeper()->willFinishSweeping();
    forEachBlock<Sweep>();
}

void MarkedSpace::zombifySweep()
{
    if (Options::logGC())
        dataLog("Zombifying sweep...");
    m_heap->sweeper()->willFinishSweeping();
    forEachBlock<ZombifySweep>();
}

void MarkedSpace::resetAllocators()
{
    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep) {
        allocatorFor(cellSize).reset();
        destructorAllocatorFor(cellSize).reset();
    }

    for (size_t cellSize = impreciseStart; cellSize <= impreciseCutoff; cellSize += impreciseStep) {
        allocatorFor(cellSize).reset();
        destructorAllocatorFor(cellSize).reset();
    }

    m_normalSpace.largeAllocator.reset();
    m_destructorSpace.largeAllocator.reset();

    m_blocksWithNewObjects.clear();
}

void MarkedSpace::visitWeakSets(HeapRootVisitor& heapRootVisitor)
{
    VisitWeakSet visitWeakSet(heapRootVisitor);
    if (m_heap->operationInProgress() == EdenCollection) {
        for (unsigned i = 0; i < m_blocksWithNewObjects.size(); ++i)
            visitWeakSet(m_blocksWithNewObjects[i]);
    } else
        forEachBlock(visitWeakSet);
}

void MarkedSpace::reapWeakSets()
{
    if (m_heap->operationInProgress() == EdenCollection) {
        for (unsigned i = 0; i < m_blocksWithNewObjects.size(); ++i)
            m_blocksWithNewObjects[i]->reapWeakSet();
    } else
        forEachBlock<ReapWeakSet>();
}

template <typename Functor>
void MarkedSpace::forEachAllocator()
{
    Functor functor;
    forEachAllocator(functor);
}

template <typename Functor>
void MarkedSpace::forEachAllocator(Functor& functor)
{
    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep) {
        functor(allocatorFor(cellSize));
        functor(destructorAllocatorFor(cellSize));
    }

    for (size_t cellSize = impreciseStart; cellSize <= impreciseCutoff; cellSize += impreciseStep) {
        functor(allocatorFor(cellSize));
        functor(destructorAllocatorFor(cellSize));
    }

    functor(m_normalSpace.largeAllocator);
    functor(m_destructorSpace.largeAllocator);
}

struct StopAllocatingFunctor {
    void operator()(MarkedAllocator& allocator) { allocator.stopAllocating(); }
};

void MarkedSpace::stopAllocating()
{
    ASSERT(!isIterating());
    forEachAllocator<StopAllocatingFunctor>();
}

struct ResumeAllocatingFunctor {
    void operator()(MarkedAllocator& allocator) { allocator.resumeAllocating(); }
};

void MarkedSpace::resumeAllocating()
{
    ASSERT(isIterating());
    forEachAllocator<ResumeAllocatingFunctor>();
}

bool MarkedSpace::isPagedOut(double deadline)
{
    for (size_t cellSize = preciseStep; cellSize <= preciseCutoff; cellSize += preciseStep) {
        if (allocatorFor(cellSize).isPagedOut(deadline) 
            || destructorAllocatorFor(cellSize).isPagedOut(deadline))
            return true;
    }

    for (size_t cellSize = impreciseStart; cellSize <= impreciseCutoff; cellSize += impreciseStep) {
        if (allocatorFor(cellSize).isPagedOut(deadline) 
            || destructorAllocatorFor(cellSize).isPagedOut(deadline))
            return true;
    }

    if (m_normalSpace.largeAllocator.isPagedOut(deadline)
        || m_destructorSpace.largeAllocator.isPagedOut(deadline))
        return true;

    return false;
}

void MarkedSpace::freeBlock(MarkedBlock* block)
{
    block->allocator()->removeBlock(block);
    m_capacity -= block->capacity();
    m_blocks.remove(block);
    MarkedBlock::destroy(*m_heap, block);
}

void MarkedSpace::freeOrShrinkBlock(MarkedBlock* block)
{
    if (!block->isEmpty()) {
        block->shrink();
        return;
    }

    freeBlock(block);
}

void MarkedSpace::shrink()
{
    FreeOrShrink freeOrShrink(*this);
    forEachBlock(freeOrShrink);
}

static void clearNewlyAllocatedInBlock(MarkedBlock* block)
{
    if (!block)
        return;
    block->clearNewlyAllocated();
}

struct ClearNewlyAllocated : MarkedBlock::VoidFunctor {
    void operator()(MarkedBlock* block) { block->clearNewlyAllocated(); }
};

#ifndef NDEBUG
struct VerifyNewlyAllocated : MarkedBlock::VoidFunctor {
    void operator()(MarkedBlock* block) { ASSERT(!block->clearNewlyAllocated()); }
};
#endif

void MarkedSpace::clearNewlyAllocated()
{
    for (size_t i = 0; i < preciseCount; ++i) {
        clearNewlyAllocatedInBlock(m_normalSpace.preciseAllocators[i].takeLastActiveBlock());
        clearNewlyAllocatedInBlock(m_destructorSpace.preciseAllocators[i].takeLastActiveBlock());
    }

    for (size_t i = 0; i < impreciseCount; ++i) {
        clearNewlyAllocatedInBlock(m_normalSpace.impreciseAllocators[i].takeLastActiveBlock());
        clearNewlyAllocatedInBlock(m_destructorSpace.impreciseAllocators[i].takeLastActiveBlock());
    }

    // We have to iterate all of the blocks in the large allocators because they are
    // canonicalized as they are used up (see MarkedAllocator::tryAllocateHelper)
    // which creates the m_newlyAllocated bitmap.
    ClearNewlyAllocated functor;
    m_normalSpace.largeAllocator.forEachBlock(functor);
    m_destructorSpace.largeAllocator.forEachBlock(functor);

#ifndef NDEBUG
    VerifyNewlyAllocated verifyFunctor;
    forEachBlock(verifyFunctor);
#endif
}

#ifndef NDEBUG 
struct VerifyMarkedOrRetired : MarkedBlock::VoidFunctor { 
    void operator()(MarkedBlock* block)
    {
        switch (block->m_state) {
        case MarkedBlock::Marked:
        case MarkedBlock::Retired:
            return;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
}; 
#endif 

void MarkedSpace::clearMarks()
{
    if (m_heap->operationInProgress() == EdenCollection) {
        for (unsigned i = 0; i < m_blocksWithNewObjects.size(); ++i)
            m_blocksWithNewObjects[i]->clearMarks();
    } else
        forEachBlock<ClearMarks>();

#ifndef NDEBUG
    VerifyMarkedOrRetired verifyFunctor;
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

} // namespace JSC
