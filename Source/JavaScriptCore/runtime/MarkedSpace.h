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
#include "PageAllocationAligned.h"
#include <wtf/Bitmap.h>
#include <wtf/FixedArray.h>
#include <wtf/HashCountedSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace JSC {

    class CollectorBlock;
    class Heap;
    class JSCell;
    class JSGlobalData;
    class LiveObjectIterator;
    class MarkStack;
    class WeakGCHandle;

#if OS(WINCE) || OS(SYMBIAN) || PLATFORM(BREWMP)
    const size_t BLOCK_SIZE = 64 * 1024; // 64k
#else
    const size_t BLOCK_SIZE = 256 * 1024; // 256k
#endif

    typedef HashCountedSet<JSCell*> ProtectCountSet;

    struct CollectorHeap {
        size_t nextBlock;
        size_t nextCell;
        PageAllocationAligned* blocks;
        
        size_t numBlocks;
        size_t usedBlocks;

        CollectorBlock* collectorBlock(size_t index) const
        {
            return static_cast<CollectorBlock*>(blocks[index].base());
        }
    };

    class MarkedSpace {
        WTF_MAKE_NONCOPYABLE(MarkedSpace);
    public:
        static Heap* heap(JSCell*);

        static bool isCellMarked(const JSCell*);
        static bool checkMarkCell(const JSCell*);
        static void markCell(JSCell*);

        MarkedSpace(JSGlobalData*);
        void destroy(ProtectCountSet&);

        JSGlobalData* globalData() { return m_globalData; }

        void* allocate(size_t);

        void clearMarkBits();
        void markRoots();
        void reset();
        void sweep();

        size_t size() const;
        size_t capacity() const;
        size_t objectCount() const;

        bool contains(void*);

        LiveObjectIterator primaryHeapBegin();
        LiveObjectIterator primaryHeapEnd();

    private:
        bool isCellAligned(void*);
        bool isPossibleCell(void*);
        bool containsSlowCase(void*);

        static CollectorBlock* cellBlock(const JSCell*);
        static size_t cellOffset(const JSCell*);

        NEVER_INLINE CollectorBlock* allocateBlock();
        NEVER_INLINE void freeBlock(size_t);
        void resizeBlocks();
        void growBlocks(size_t neededBlocks);
        void shrinkBlocks(size_t neededBlocks);

        void clearMarkBits(CollectorBlock*);
        size_t markedCells(size_t startBlock = 0, size_t startCell = 0) const;

        CollectorHeap m_heap;
        JSGlobalData* m_globalData;
    };

    // tunable parameters
    // derived constants
    const size_t BLOCK_OFFSET_MASK = BLOCK_SIZE - 1;
    const size_t BLOCK_MASK = ~BLOCK_OFFSET_MASK;
    const size_t MINIMUM_CELL_SIZE = 64;
    const size_t CELL_ARRAY_LENGTH = (MINIMUM_CELL_SIZE / sizeof(double)) + (MINIMUM_CELL_SIZE % sizeof(double) != 0 ? sizeof(double) : 0);
    const size_t CELL_SIZE = CELL_ARRAY_LENGTH * sizeof(double);
    const size_t SMALL_CELL_SIZE = CELL_SIZE / 2;
    const size_t CELL_MASK = CELL_SIZE - 1;
    const size_t CELL_ALIGN_MASK = ~CELL_MASK;
    const size_t CELLS_PER_BLOCK = (BLOCK_SIZE - sizeof(MarkedSpace*)) * 8 * CELL_SIZE / (8 * CELL_SIZE + 1) / CELL_SIZE; // one bitmap byte can represent 8 cells.
    
    struct CollectorCell {
        FixedArray<double, CELL_ARRAY_LENGTH> memory;
    };

    class CollectorBlock {
    public:
        FixedArray<CollectorCell, CELLS_PER_BLOCK> cells;
        WTF::Bitmap<CELLS_PER_BLOCK> marked;
        Heap* heap;
    };

    struct HeapConstants {
        static const size_t cellSize = CELL_SIZE;
        static const size_t cellsPerBlock = CELLS_PER_BLOCK;
        typedef CollectorCell Cell;
        typedef CollectorBlock Block;
    };

    inline CollectorBlock* MarkedSpace::cellBlock(const JSCell* cell)
    {
        return reinterpret_cast<CollectorBlock*>(reinterpret_cast<uintptr_t>(cell) & BLOCK_MASK);
    }

    inline size_t MarkedSpace::cellOffset(const JSCell* cell)
    {
        return (reinterpret_cast<uintptr_t>(cell) & BLOCK_OFFSET_MASK) / CELL_SIZE;
    }

    inline Heap* MarkedSpace::heap(JSCell* cell)
    {
        return cellBlock(cell)->heap;
    }

    inline bool MarkedSpace::isCellMarked(const JSCell* cell)
    {
        return cellBlock(cell)->marked.get(cellOffset(cell));
    }

    inline bool MarkedSpace::checkMarkCell(const JSCell* cell)
    {
        return cellBlock(cell)->marked.testAndSet(cellOffset(cell));
    }

    inline void MarkedSpace::markCell(JSCell* cell)
    {
        cellBlock(cell)->marked.set(cellOffset(cell));
    }

    // Cell size needs to be a power of two for isPossibleCell to be valid.
    COMPILE_ASSERT(!(sizeof(CollectorCell) % 2), Collector_cell_size_is_power_of_two);

    inline bool MarkedSpace::isCellAligned(void *p)
    {
        return !((intptr_t)(p) & CELL_MASK);
    }

    inline bool MarkedSpace::isPossibleCell(void* p)
    {
        return isCellAligned(p) && p;
    }

    inline bool MarkedSpace::contains(void* x)
    {
        if (!isPossibleCell(x))
            return false;
            
        return containsSlowCase(x);
    }

} // namespace JSC

#endif // MarkedSpace_h
