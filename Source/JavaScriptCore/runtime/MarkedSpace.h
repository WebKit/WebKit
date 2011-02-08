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
        size_t nextBlock;
        size_t nextCell;
        PageAllocationAligned* blocks;
        
        size_t numBlocks;
        size_t usedBlocks;
        
        size_t waterMark;
        size_t highWaterMark;

        MarkedBlock* collectorBlock(size_t index) const
        {
            return static_cast<MarkedBlock*>(blocks[index].base());
        }
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

        size_t highWaterMark() { return m_heap.highWaterMark; }
        void setHighWaterMark(size_t highWaterMark) { m_heap.highWaterMark = highWaterMark; }

        void* allocate(size_t);

        void clearMarkBits();
        void markRoots();
        void reset();
        void sweep();
        size_t markedCells(size_t startBlock = 0, size_t startCell = 0) const;

        size_t size() const;
        size_t capacity() const;
        size_t objectCount() const;

        bool contains(const void*);

        LiveObjectIterator primaryHeapBegin();
        LiveObjectIterator primaryHeapEnd();

    private:
        bool containsSlowCase(const void*);

        NEVER_INLINE MarkedBlock* allocateBlock();
        NEVER_INLINE void freeBlock(size_t);
        void shrink();

        void clearMarkBits(MarkedBlock*);

        CollectorHeap m_heap;
        JSGlobalData* m_globalData;
    };

    inline Heap* MarkedSpace::heap(JSCell* cell)
    {
        return MarkedBlock::blockFor(cell)->heap;
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
        if (!MarkedBlock::isPossibleCell(x))
            return false;
            
        return containsSlowCase(x);
    }

} // namespace JSC

#endif // MarkedSpace_h
