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

        bool didShrink;

        CollectorBlock* collectorBlock(size_t index) const
        {
            return static_cast<CollectorBlock*>(blocks[index].base());
        }
    };

    class MarkedSpace : public Noncopyable {
    public:
        MarkedSpace(JSGlobalData*);
        void destroy(ProtectCountSet&);

        void* allocate(size_t);

        size_t objectCount() const;
        struct Statistics {
            size_t size;
            size_t free;
        };
        Statistics statistics() const;
        size_t size() const;

        static Heap* heap(JSCell*);

        static bool isCellMarked(const JSCell*);
        static bool checkMarkCell(const JSCell*);
        static void markCell(JSCell*);

        WeakGCHandle* addWeakGCHandle(JSCell*);

        void markConservatively(MarkStack&, void* start, void* end);

        static bool isNumber(JSCell*);
        
        LiveObjectIterator primaryHeapBegin();
        LiveObjectIterator primaryHeapEnd();

        JSGlobalData* globalData() { return m_globalData; }

        static CollectorBlock* cellBlock(const JSCell*);
        static size_t cellOffset(const JSCell*);

        void reset();
        void sweep();

        NEVER_INLINE CollectorBlock* allocateBlock();
        NEVER_INLINE void freeBlock(size_t);
        void resizeBlocks();
        void growBlocks(size_t neededBlocks);
        void shrinkBlocks(size_t neededBlocks);
        void clearMarkBits();
        void clearMarkBits(CollectorBlock*);
        size_t markedCells(size_t startBlock = 0, size_t startCell = 0) const;

        void addToStatistics(Statistics&) const;

        void markRoots();

        bool didShrink() { return m_heap.didShrink; }
        
    private:
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
    
    const size_t BITMAP_SIZE = (CELLS_PER_BLOCK + 7) / 8;
    const size_t BITMAP_WORDS = (BITMAP_SIZE + 3) / sizeof(uint32_t);

    struct CollectorBitmap {
        FixedArray<uint32_t, BITMAP_WORDS> bits;
        bool get(size_t n) const { return !!(bits[n >> 5] & (1 << (n & 0x1F))); } 
        void set(size_t n) { bits[n >> 5] |= (1 << (n & 0x1F)); } 
        bool getset(size_t n)
        {
            unsigned i = (1 << (n & 0x1F));
            uint32_t& b = bits[n >> 5];
            bool r = !!(b & i);
            b |= i;
            return r;
        } 
        void clear(size_t n) { bits[n >> 5] &= ~(1 << (n & 0x1F)); } 
        void clearAll() { memset(bits.data(), 0, sizeof(bits)); }
        ALWAYS_INLINE void advanceToNextPossibleFreeCell(size_t& startCell)
        {
            if (!~bits[startCell >> 5])
                startCell = (startCell & (~0x1F)) + 32;
            else
                ++startCell;
        }
        size_t count(size_t startCell = 0)
        {
            size_t result = 0;
            for ( ; (startCell & 0x1F) != 0; ++startCell) {
                if (get(startCell))
                    ++result;
            }
            for (size_t i = startCell >> 5; i < BITMAP_WORDS; ++i)
                result += WTF::bitCount(bits[i]);
            return result;
        }
        size_t isEmpty() // Much more efficient than testing count() == 0.
        {
            for (size_t i = 0; i < BITMAP_WORDS; ++i)
                if (bits[i] != 0)
                    return false;
            return true;
        }
    };
  
    struct CollectorCell {
        FixedArray<double, CELL_ARRAY_LENGTH> memory;
    };

    class CollectorBlock {
    public:
        FixedArray<CollectorCell, CELLS_PER_BLOCK> cells;
        CollectorBitmap marked;
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

    inline bool MarkedSpace::isCellMarked(const JSCell* cell)
    {
        return cellBlock(cell)->marked.get(cellOffset(cell));
    }

    inline bool MarkedSpace::checkMarkCell(const JSCell* cell)
    {
        return cellBlock(cell)->marked.getset(cellOffset(cell));
    }

    inline void MarkedSpace::markCell(JSCell* cell)
    {
        cellBlock(cell)->marked.set(cellOffset(cell));
    }

} // namespace JSC

#endif // MarkedSpace_h
