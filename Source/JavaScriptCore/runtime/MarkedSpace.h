/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "MarkedBlock.h"
#include "PageAllocationAligned.h"
#include <wtf/Bitmap.h>
#include <wtf/FixedArray.h>
#include <wtf/HashCountedSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace JSC {

    class Heap;
    class JSCell;
    class JSGlobalData;
    class LiveObjectIterator;
    class MarkStack;
    class WeakGCHandle;

    struct CollectorHeap {
        CollectorHeap()
            : nextBlock(0)
            , nextCell(0)
        {
        }
        
        MarkedBlock* collectorBlock(size_t index) const
        {
            return blocks[index];
        }

        size_t nextBlock;
        size_t nextCell;
        Vector<MarkedBlock*> blocks;
    };

    class MarkedSpace {
        WTF_MAKE_NONCOPYABLE(MarkedSpace);
    public:
        static Heap* heap(JSCell*);

        static bool isMarked(const JSCell*);
        static bool testAndSetMarked(const JSCell*);
        static void setMarked(const JSCell*);

        MarkedSpace(JSGlobalData*);
        void destroy();

        JSGlobalData* globalData() { return m_globalData; }

        size_t highWaterMark() { return m_highWaterMark; }
        void setHighWaterMark(size_t highWaterMark) { m_highWaterMark = highWaterMark; }

        void* allocate(size_t);

        void clearMarks();
        void markRoots();
        void reset();
        void sweep();
        void shrink();

        size_t size() const;
        size_t capacity() const;
        size_t objectCount() const;

        bool contains(const void*);

        template<typename Functor> void forEach(Functor&);

    private:
        NEVER_INLINE MarkedBlock* allocateBlock();
        NEVER_INLINE void freeBlock(size_t);

        void clearMarks(MarkedBlock*);

        CollectorHeap m_heap;
        size_t m_waterMark;
        size_t m_highWaterMark;
        JSGlobalData* m_globalData;
    };

    inline Heap* MarkedSpace::heap(JSCell* cell)
    {
        return MarkedBlock::blockFor(cell)->heap();
    }

    inline bool MarkedSpace::isMarked(const JSCell* cell)
    {
        return MarkedBlock::blockFor(cell)->isMarked(cell);
    }

    inline bool MarkedSpace::testAndSetMarked(const JSCell* cell)
    {
        return MarkedBlock::blockFor(cell)->testAndSetMarked(cell);
    }

    inline void MarkedSpace::setMarked(const JSCell* cell)
    {
        MarkedBlock::blockFor(cell)->setMarked(cell);
    }

    inline bool MarkedSpace::contains(const void* x)
    {
        if (!MarkedBlock::isCellAligned(x))
            return false;

        MarkedBlock* block = MarkedBlock::blockFor(x);
        if (!block)
            return false;

        size_t size = m_heap.blocks.size();
        for (size_t i = 0; i < size; i++) {
            if (block != m_heap.collectorBlock(i))
                continue;

            // x is a pointer into the heap. Now, verify that the cell it
            // points to is live. (If the cell is dead, we must not mark it,
            // since that would revive it in a zombie state.)
            return block->isMarked(x);
        }
        
        return false;
    }

    template <typename Functor> inline void MarkedSpace::forEach(Functor& functor)
    {
        for (size_t i = 0; i < m_heap.blocks.size(); ++i)
            m_heap.collectorBlock(i)->forEach(functor);
    }

} // namespace JSC

#endif // MarkedSpace_h
