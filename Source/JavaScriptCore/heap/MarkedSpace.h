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

#include "MachineStackMarker.h"
#include "MarkedAllocator.h"
#include "MarkedBlock.h"
#include "MarkedBlockSet.h"
#include "PageAllocationAligned.h"
#include <wtf/Bitmap.h>
#include <wtf/DoublyLinkedList.h>
#include <wtf/FixedArray.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

#define ASSERT_CLASS_FITS_IN_CELL(class) COMPILE_ASSERT(sizeof(class) <= MarkedSpace::maxCellSize, class_fits_in_cell)

namespace JSC {

class Heap;
class JSCell;
class LiveObjectIterator;
class WeakGCHandle;
class SlotVisitor;

class MarkedSpace {
    WTF_MAKE_NONCOPYABLE(MarkedSpace);
public:
    static const size_t maxCellSize = 2048;

    MarkedSpace(Heap*);

    MarkedAllocator& allocatorFor(size_t);
    void* allocate(size_t);
    
    void resetAllocators();

    MarkedBlockSet& blocks() { return m_blocks; }
    
    void canonicalizeCellLivenessData();

    size_t waterMark();
    size_t nurseryWaterMark();

    typedef HashSet<MarkedBlock*>::iterator BlockIterator;
    
    template<typename Functor> typename Functor::ReturnType forEachCell(Functor&);
    template<typename Functor> typename Functor::ReturnType forEachCell();
    template<typename Functor> typename Functor::ReturnType forEachBlock(Functor&);
    template<typename Functor> typename Functor::ReturnType forEachBlock();
    
    void shrink();
    void freeBlocks(MarkedBlock* head);
    void didAddBlock(MarkedBlock*);
    void didConsumeFreeList(MarkedBlock*);

private:
    // [ 32... 256 ]
    static const size_t preciseStep = MarkedBlock::atomSize;
    static const size_t preciseCutoff = 256;
    static const size_t preciseCount = preciseCutoff / preciseStep;

    // [ 512... 2048 ]
    static const size_t impreciseStep = preciseCutoff;
    static const size_t impreciseCutoff = maxCellSize;
    static const size_t impreciseCount = impreciseCutoff / impreciseStep;

    FixedArray<MarkedAllocator, preciseCount> m_preciseSizeClasses;
    FixedArray<MarkedAllocator, impreciseCount> m_impreciseSizeClasses;
    size_t m_waterMark;
    size_t m_nurseryWaterMark;
    Heap* m_heap;
    MarkedBlockSet m_blocks;
};

inline size_t MarkedSpace::waterMark()
{
    return m_waterMark;
}

inline size_t MarkedSpace::nurseryWaterMark()
{
    return m_nurseryWaterMark;
}

template<typename Functor> inline typename Functor::ReturnType MarkedSpace::forEachCell(Functor& functor)
{
    canonicalizeCellLivenessData();

    BlockIterator end = m_blocks.set().end();
    for (BlockIterator it = m_blocks.set().begin(); it != end; ++it)
        (*it)->forEachCell(functor);
    return functor.returnValue();
}

template<typename Functor> inline typename Functor::ReturnType MarkedSpace::forEachCell()
{
    Functor functor;
    return forEachCell(functor);
}

inline MarkedAllocator& MarkedSpace::allocatorFor(size_t bytes)
{
    ASSERT(bytes && bytes <= maxCellSize);
    if (bytes <= preciseCutoff)
        return m_preciseSizeClasses[(bytes - 1) / preciseStep];
    return m_impreciseSizeClasses[(bytes - 1) / impreciseStep];
}

inline void* MarkedSpace::allocate(size_t bytes)
{
    return allocatorFor(bytes).allocate();
}

template <typename Functor> inline typename Functor::ReturnType MarkedSpace::forEachBlock(Functor& functor)
{
    for (size_t i = 0; i < preciseCount; ++i) {
        m_preciseSizeClasses[i].forEachBlock(functor);
    }

    for (size_t i = 0; i < impreciseCount; ++i) {
        m_impreciseSizeClasses[i].forEachBlock(functor);
    }

    return functor.returnValue();
}

template <typename Functor> inline typename Functor::ReturnType MarkedSpace::forEachBlock()
{
    Functor functor;
    return forEachBlock(functor);
}

inline void MarkedSpace::didAddBlock(MarkedBlock* block)
{
    m_blocks.add(block);
}

inline void MarkedSpace::didConsumeFreeList(MarkedBlock* block)
{
    m_nurseryWaterMark += block->capacity() - block->size();
    m_waterMark += block->capacity();
}

} // namespace JSC

#endif // MarkedSpace_h
