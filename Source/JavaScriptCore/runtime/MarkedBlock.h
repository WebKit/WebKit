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
#include <wtf/PageAllocationAligned.h>

#define ASSERT_CLASS_FITS_IN_CELL(class) COMPILE_ASSERT(sizeof(class) <= MarkedBlock::CELL_SIZE, class_fits_in_cell)

namespace JSC {

    class Heap;
    class JSCell;
    class JSGlobalData;

    class MarkedBlock {
#if OS(WINCE) || OS(SYMBIAN) || PLATFORM(BREWMP)
        static const size_t BLOCK_SIZE = 64 * 1024; // 64k
#else
        static const size_t BLOCK_SIZE = 256 * 1024; // 256k
#endif

        static const size_t BLOCK_OFFSET_MASK = BLOCK_SIZE - 1;
        static const size_t BLOCK_MASK = ~BLOCK_OFFSET_MASK;
        static const size_t MINIMUM_CELL_SIZE = 64;
        static const size_t CELL_ARRAY_LENGTH = (MINIMUM_CELL_SIZE / sizeof(double)) + (MINIMUM_CELL_SIZE % sizeof(double) != 0 ? sizeof(double) : 0);
    public:
        // This is still public for now, for use in assertions.
        static const size_t CELL_SIZE = CELL_ARRAY_LENGTH * sizeof(double);
    private:
        static const size_t SMALL_CELL_SIZE = CELL_SIZE / 2;
        static const size_t CELL_MASK = CELL_SIZE - 1;
        static const size_t CELL_ALIGN_MASK = ~CELL_MASK;
        static const size_t BITS_PER_BLOCK = BLOCK_SIZE / CELL_SIZE;
        static const size_t CELLS_PER_BLOCK = (BLOCK_SIZE - sizeof(Heap*) - sizeof(WTF::Bitmap<BITS_PER_BLOCK>)) / CELL_SIZE; // Division rounds down intentionally.
        
        struct CollectorCell {
            FixedArray<double, CELL_ARRAY_LENGTH> memory;
        };

        // Cell size needs to be a power of two for CELL_MASK to be valid.
        COMPILE_ASSERT(!(sizeof(CollectorCell) % 2), Collector_cell_size_is_power_of_two);

    public:
        static MarkedBlock* create(JSGlobalData*);
        static void destroy(MarkedBlock*);

        static bool isCellAligned(const void*);
        static MarkedBlock* blockFor(const void*);
        
        Heap* heap() const;
        
        void* allocate(size_t& nextCell);
        void sweep();
        
        bool isEmpty();

        void clearMarks();
        size_t markCount();
        size_t size();
        size_t capacity();

        size_t cellNumber(const void*);
        bool isMarked(const void*);
        bool testAndSetMarked(const void*);
        void setMarked(const void*);
        
        template <typename Functor> void forEach(Functor&);

        FixedArray<CollectorCell, CELLS_PER_BLOCK> cells;

    private:
        MarkedBlock(const PageAllocationAligned&, JSGlobalData*);

        WTF::Bitmap<BITS_PER_BLOCK> marked;
        PageAllocationAligned m_allocation;
        Heap* m_heap;
    };

    inline bool MarkedBlock::isCellAligned(const void* p)
    {
        return !((intptr_t)(p) & CELL_MASK);
    }

    inline MarkedBlock* MarkedBlock::blockFor(const void* p)
    {
        return reinterpret_cast<MarkedBlock*>(reinterpret_cast<uintptr_t>(p) & BLOCK_MASK);
    }

    inline Heap* MarkedBlock::heap() const
    {
        return m_heap;
    }

    inline bool MarkedBlock::isEmpty()
    {
        marked.clear(CELLS_PER_BLOCK - 1); // Clear the always-set last bit to avoid confusing isEmpty().
        bool result = marked.isEmpty();
        marked.set(CELLS_PER_BLOCK - 1);
        return result;
    }

    inline void MarkedBlock::clearMarks()
    {
        // allocate() assumes that the last mark bit is always set.
        marked.clearAll();
        marked.set(CELLS_PER_BLOCK - 1);
    }
    
    inline size_t MarkedBlock::markCount()
    {
        return marked.count() - 1; // The last mark bit is always set.
    }

    inline size_t MarkedBlock::size()
    {
        return markCount() * CELL_SIZE;
    }

    inline size_t MarkedBlock::capacity()
    {
        return BLOCK_SIZE;
    }

    inline size_t MarkedBlock::cellNumber(const void* cell)
    {
        return (reinterpret_cast<uintptr_t>(cell) & BLOCK_OFFSET_MASK) / CELL_SIZE;
    }

    inline bool MarkedBlock::isMarked(const void* cell)
    {
        return marked.get(cellNumber(cell));
    }

    inline bool MarkedBlock::testAndSetMarked(const void* cell)
    {
        return marked.testAndSet(cellNumber(cell));
    }

    inline void MarkedBlock::setMarked(const void* cell)
    {
        marked.set(cellNumber(cell));
    }

    template <typename Functor> inline void MarkedBlock::forEach(Functor& functor)
    {
        for (size_t i = 0; i < CELLS_PER_BLOCK - 1; ++i) { // The last cell is a dummy place-holder.
            if (!marked.get(i))
                continue;
            functor(reinterpret_cast<JSCell*>(&cells[i]));
        }
    }

} // namespace JSC

#endif // MarkedSpace_h
