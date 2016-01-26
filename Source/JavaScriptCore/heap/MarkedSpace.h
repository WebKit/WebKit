/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2011 Apple Inc. All rights reserved.
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

#ifndef MarkedSpace_h
#define MarkedSpace_h

#include "MarkedAllocator.h"
#include "MarkedBlock.h"
#include "MarkedBlockSet.h"
#include <array>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

namespace JSC {

class Heap;
class HeapIterationScope;
class LLIntOffsetsExtractor;

struct ClearMarks : MarkedBlock::VoidFunctor {
    void operator()(MarkedBlock* block)
    {
        block->clearMarks();
    }
};

struct Sweep : MarkedBlock::VoidFunctor {
    void operator()(MarkedBlock* block) { block->sweep(); }
};

struct ZombifySweep : MarkedBlock::VoidFunctor {
    void operator()(MarkedBlock* block)
    {
        if (block->needsSweeping())
            block->sweep();
    }
};

struct MarkCount : MarkedBlock::CountFunctor {
    void operator()(MarkedBlock* block) { count(block->markCount()); }
};

struct Size : MarkedBlock::CountFunctor {
    void operator()(MarkedBlock* block) { count(block->markCount() * block->cellSize()); }
};

class MarkedSpace {
    WTF_MAKE_NONCOPYABLE(MarkedSpace);
public:
    // [ 16 ... 768 ]
    static const size_t preciseStep = MarkedBlock::atomSize;
    static const size_t preciseCutoff = 768;
    static const size_t preciseCount = preciseCutoff / preciseStep;

    // [ 1024 ... blockSize/2 ]
    static const size_t impreciseStart = 1024;
    static const size_t impreciseStep = 256;
    static const size_t impreciseCutoff = MarkedBlock::blockSize / 2;
    static const size_t impreciseCount = impreciseCutoff / impreciseStep;

    struct Subspace {
        std::array<MarkedAllocator, preciseCount> preciseAllocators;
        std::array<MarkedAllocator, impreciseCount> impreciseAllocators;
        MarkedAllocator largeAllocator;
    };

    MarkedSpace(Heap*);
    ~MarkedSpace();
    void lastChanceToFinalize();

    MarkedAllocator& allocatorFor(size_t);
    MarkedAllocator& destructorAllocatorFor(size_t);
    void* allocateWithDestructor(size_t);
    void* allocateWithoutDestructor(size_t);

    Subspace& subspaceForObjectsWithDestructor() { return m_destructorSpace; }
    Subspace& subspaceForObjectsWithoutDestructor() { return m_normalSpace; }

    void resetAllocators();

    void visitWeakSets(HeapRootVisitor&);
    void reapWeakSets();

    MarkedBlockSet& blocks() { return m_blocks; }

    void willStartIterating();
    bool isIterating() const { return m_isIterating; }
    void didFinishIterating();

    void stopAllocating();
    void resumeAllocating(); // If we just stopped allocation but we didn't do a collection, we need to resume allocation.

    typedef HashSet<MarkedBlock*>::iterator BlockIterator;

    template<typename Functor> typename Functor::ReturnType forEachLiveCell(HeapIterationScope&, Functor&);
    template<typename Functor> typename Functor::ReturnType forEachLiveCell(HeapIterationScope&);
    template<typename Functor> typename Functor::ReturnType forEachDeadCell(HeapIterationScope&, Functor&);
    template<typename Functor> typename Functor::ReturnType forEachDeadCell(HeapIterationScope&);
    template<typename Functor> typename Functor::ReturnType forEachBlock(Functor&);
    template<typename Functor> typename Functor::ReturnType forEachBlock();

    void shrink();
    void freeBlock(MarkedBlock*);
    void freeOrShrinkBlock(MarkedBlock*);

    void didAddBlock(MarkedBlock*);
    void didConsumeFreeList(MarkedBlock*);
    void didAllocateInBlock(MarkedBlock*);

    void clearMarks();
    void clearNewlyAllocated();
    void sweep();
    void zombifySweep();
    size_t objectCount();
    size_t size();
    size_t capacity();

    bool isPagedOut(double deadline);

    const Vector<MarkedBlock*>& blocksWithNewObjects() const { return m_blocksWithNewObjects; }

private:
    friend class LLIntOffsetsExtractor;
    friend class JIT;

    template<typename Functor> void forEachAllocator(Functor&);
    template<typename Functor> void forEachAllocator();

    Subspace m_destructorSpace;
    Subspace m_normalSpace;

    Heap* m_heap;
    size_t m_capacity;
    bool m_isIterating;
    MarkedBlockSet m_blocks;
    Vector<MarkedBlock*> m_blocksWithNewObjects;
};

template<typename Functor> inline typename Functor::ReturnType MarkedSpace::forEachLiveCell(HeapIterationScope&, Functor& functor)
{
    ASSERT(isIterating());
    BlockIterator end = m_blocks.set().end();
    for (BlockIterator it = m_blocks.set().begin(); it != end; ++it) {
        if ((*it)->forEachLiveCell(functor) == IterationStatus::Done)
            break;
    }
    return functor.returnValue();
}

template<typename Functor> inline typename Functor::ReturnType MarkedSpace::forEachLiveCell(HeapIterationScope& scope)
{
    Functor functor;
    return forEachLiveCell(scope, functor);
}

template<typename Functor> inline typename Functor::ReturnType MarkedSpace::forEachDeadCell(HeapIterationScope&, Functor& functor)
{
    ASSERT(isIterating());
    BlockIterator end = m_blocks.set().end();
    for (BlockIterator it = m_blocks.set().begin(); it != end; ++it) {
        if ((*it)->forEachDeadCell(functor) == IterationStatus::Done)
            break;
    }
    return functor.returnValue();
}

template<typename Functor> inline typename Functor::ReturnType MarkedSpace::forEachDeadCell(HeapIterationScope& scope)
{
    Functor functor;
    return forEachDeadCell(scope, functor);
}

inline MarkedAllocator& MarkedSpace::allocatorFor(size_t bytes)
{
    ASSERT(bytes);
    if (bytes <= preciseCutoff)
        return m_normalSpace.preciseAllocators[(bytes - 1) / preciseStep];
    if (bytes <= impreciseCutoff)
        return m_normalSpace.impreciseAllocators[(bytes - 1) / impreciseStep];
    return m_normalSpace.largeAllocator;
}

inline MarkedAllocator& MarkedSpace::destructorAllocatorFor(size_t bytes)
{
    ASSERT(bytes);
    if (bytes <= preciseCutoff)
        return m_destructorSpace.preciseAllocators[(bytes - 1) / preciseStep];
    if (bytes <= impreciseCutoff)
        return m_destructorSpace.impreciseAllocators[(bytes - 1) / impreciseStep];
    return m_destructorSpace.largeAllocator;
}

inline void* MarkedSpace::allocateWithoutDestructor(size_t bytes)
{
    return allocatorFor(bytes).allocate(bytes);
}

inline void* MarkedSpace::allocateWithDestructor(size_t bytes)
{
    return destructorAllocatorFor(bytes).allocate(bytes);
}

template <typename Functor> inline typename Functor::ReturnType MarkedSpace::forEachBlock(Functor& functor)
{
    for (size_t i = 0; i < preciseCount; ++i)
        m_normalSpace.preciseAllocators[i].forEachBlock(functor);
    for (size_t i = 0; i < impreciseCount; ++i)
        m_normalSpace.impreciseAllocators[i].forEachBlock(functor);
    m_normalSpace.largeAllocator.forEachBlock(functor);

    for (size_t i = 0; i < preciseCount; ++i)
        m_destructorSpace.preciseAllocators[i].forEachBlock(functor);
    for (size_t i = 0; i < impreciseCount; ++i)
        m_destructorSpace.impreciseAllocators[i].forEachBlock(functor);
    m_destructorSpace.largeAllocator.forEachBlock(functor);

    return functor.returnValue();
}

template <typename Functor> inline typename Functor::ReturnType MarkedSpace::forEachBlock()
{
    Functor functor;
    return forEachBlock(functor);
}

inline void MarkedSpace::didAddBlock(MarkedBlock* block)
{
    m_capacity += block->capacity();
    m_blocks.add(block);
}

inline void MarkedSpace::didAllocateInBlock(MarkedBlock* block)
{
    m_blocksWithNewObjects.append(block);
}

inline size_t MarkedSpace::objectCount()
{
    return forEachBlock<MarkCount>();
}

inline size_t MarkedSpace::size()
{
    return forEachBlock<Size>();
}

inline size_t MarkedSpace::capacity()
{
    return m_capacity;
}

} // namespace JSC

#endif // MarkedSpace_h
