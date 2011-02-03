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

#ifndef MarkedBlock_h
#define MarkedBlock_h

#include <wtf/Bitmap.h>
#include <wtf/FixedArray.h>

namespace JSC {

    class Heap;
    class JSCell;

#if OS(WINCE) || OS(SYMBIAN) || PLATFORM(BREWMP)
    const size_t BLOCK_SIZE = 64 * 1024; // 64k
#else
    const size_t BLOCK_SIZE = 256 * 1024; // 256k
#endif

    const size_t BLOCK_OFFSET_MASK = BLOCK_SIZE - 1;
    const size_t BLOCK_MASK = ~BLOCK_OFFSET_MASK;
    const size_t MINIMUM_CELL_SIZE = 64;
    const size_t CELL_ARRAY_LENGTH = (MINIMUM_CELL_SIZE / sizeof(double)) + (MINIMUM_CELL_SIZE % sizeof(double) != 0 ? sizeof(double) : 0);
    const size_t CELL_SIZE = CELL_ARRAY_LENGTH * sizeof(double);
    const size_t SMALL_CELL_SIZE = CELL_SIZE / 2;
    const size_t CELL_MASK = CELL_SIZE - 1;
    const size_t CELL_ALIGN_MASK = ~CELL_MASK;
    const size_t CELLS_PER_BLOCK = (BLOCK_SIZE - sizeof(Heap*)) * 8 * CELL_SIZE / (8 * CELL_SIZE + 1) / CELL_SIZE; // one bitmap byte can represent 8 cells.
    
    struct CollectorCell {
        FixedArray<double, CELL_ARRAY_LENGTH> memory;
    };

    // Cell size needs to be a power of two for CELL_MASK to be valid.
    COMPILE_ASSERT(!(sizeof(CollectorCell) % 2), Collector_cell_size_is_power_of_two);

    class MarkedBlock {
    public:
        static bool isCellAligned(const void*);
        static bool isPossibleCell(const void*);
        static MarkedBlock* blockFor(const void*);

        size_t cellNumber(const JSCell*);
        bool isMarked(const JSCell*);
        bool testAndSetMarked(const JSCell*);
        void setMarked(const JSCell*);

        FixedArray<CollectorCell, CELLS_PER_BLOCK> cells;
        WTF::Bitmap<CELLS_PER_BLOCK> marked;
        Heap* heap;
    };

    struct HeapConstants {
        static const size_t cellSize = CELL_SIZE;
        static const size_t cellsPerBlock = CELLS_PER_BLOCK;
        typedef CollectorCell Cell;
        typedef MarkedBlock Block;
    };

    inline MarkedBlock* MarkedBlock::blockFor(const void* p)
    {
        return reinterpret_cast<MarkedBlock*>(reinterpret_cast<uintptr_t>(p) & BLOCK_MASK);
    }

    inline size_t MarkedBlock::cellNumber(const JSCell* cell)
    {
        return (reinterpret_cast<uintptr_t>(cell) & BLOCK_OFFSET_MASK) / CELL_SIZE;
    }

    inline bool MarkedBlock::isMarked(const JSCell* cell)
    {
        return marked.get(cellNumber(cell));
    }

    inline bool MarkedBlock::testAndSetMarked(const JSCell* cell)
    {
        return marked.testAndSet(cellNumber(cell));
    }

    inline void MarkedBlock::setMarked(const JSCell* cell)
    {
        marked.set(cellNumber(cell));
    }

    inline bool MarkedBlock::isCellAligned(const void* p)
    {
        return !((intptr_t)(p) & CELL_MASK);
    }

    inline bool MarkedBlock::isPossibleCell(const void* p)
    {
        return isCellAligned(p) && p;
    }

} // namespace JSC

#endif // MarkedSpace_h
