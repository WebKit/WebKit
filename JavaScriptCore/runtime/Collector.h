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

#ifndef Collector_h
#define Collector_h

#include "AlignedMemoryAllocator.h"
#include "GCHandle.h"
#include <stddef.h>
#include <string.h>
#include <wtf/Bitmap.h>
#include <wtf/FixedArray.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PageAllocation.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>

#if ENABLE(JSC_MULTIPLE_THREADS)
#include <pthread.h>
#endif

#define ASSERT_CLASS_FITS_IN_CELL(class) COMPILE_ASSERT(sizeof(class) <= CELL_SIZE, class_fits_in_cell)

namespace JSC {

    class CollectorBlock;
    class GCActivityCallback;
    class JSCell;
    class JSGlobalData;
    class JSValue;
    class MarkedArgumentBuffer;
    class MarkStack;

    enum OperationInProgress { NoOperation, Allocation, Collection };

    class LiveObjectIterator;

#if OS(WINCE) || OS(SYMBIAN) || PLATFORM(BREWMP)
    const size_t BLOCK_SIZE = 64 * 1024; // 64k
#else
    const size_t BLOCK_SIZE = 256 * 1024; // 256k
#endif

    typedef AlignedMemoryAllocator<BLOCK_SIZE> CollectorBlockAllocator;
    typedef AlignedMemory<BLOCK_SIZE> AlignedCollectorBlock;

    struct CollectorHeap {
        size_t nextBlock;
        size_t nextCell;
        AlignedCollectorBlock* blocks;
        
        void* nextNumber;

        size_t numBlocks;
        size_t usedBlocks;

        size_t extraCost;
        bool didShrink;

        OperationInProgress operationInProgress;

        CollectorBlock* collectorBlock(size_t index) const
        {
            return static_cast<CollectorBlock*>(blocks[index].base());
        }
    };

    class Heap : public Noncopyable {
    public:
        class Thread;

        void destroy();

        void* allocateNumber(size_t);
        void* allocate(size_t);

        bool isBusy(); // true if an allocation or collection is in progress
        void collectAllGarbage();
        void setActivityCallback(PassOwnPtr<GCActivityCallback>);

        static const size_t minExtraCost = 256;
        static const size_t maxExtraCost = 1024 * 1024;

        void reportExtraMemoryCost(size_t cost);

        size_t objectCount() const;
        struct Statistics {
            size_t size;
            size_t free;
        };
        Statistics statistics() const;
        size_t size() const;

        void protect(JSValue);
        // Returns true if the value is no longer protected by any protect pointers
        // (though it may still be alive due to heap/stack references).
        bool unprotect(JSValue);

        static Heap* heap(JSValue); // 0 for immediate values
        static Heap* heap(JSCell*);

        size_t globalObjectCount();
        size_t protectedObjectCount();
        size_t protectedGlobalObjectCount();
        HashCountedSet<const char*>* protectedObjectTypeCounts();
        HashCountedSet<const char*>* objectTypeCounts();

        void registerThread(); // Only needs to be called by clients that can use the same heap from multiple threads.

        static bool isCellMarked(const JSCell*);
        static bool checkMarkCell(const JSCell*);
        static void markCell(JSCell*);

        WeakGCHandle* addWeakGCHandle(JSCell*);

        void markConservatively(MarkStack&, void* start, void* end);

        HashSet<MarkedArgumentBuffer*>& markListSet() { if (!m_markListSet) m_markListSet = new HashSet<MarkedArgumentBuffer*>; return *m_markListSet; }

        JSGlobalData* globalData() const { return m_globalData; }
        static bool isNumber(JSCell*);
        
        LiveObjectIterator primaryHeapBegin();
        LiveObjectIterator primaryHeapEnd();

    private:
        void reset();
        void sweep();
        static CollectorBlock* cellBlock(const JSCell*);
        static size_t cellOffset(const JSCell*);

        friend class JSGlobalData;
        Heap(JSGlobalData*);
        ~Heap();

        NEVER_INLINE CollectorBlock* allocateBlock();
        NEVER_INLINE void freeBlock(size_t);
        void freeBlocks();
        void resizeBlocks();
        void growBlocks(size_t neededBlocks);
        void shrinkBlocks(size_t neededBlocks);
        void clearMarkBits();
        void clearMarkBits(CollectorBlock*);
        size_t markedCells(size_t startBlock = 0, size_t startCell = 0) const;

        void recordExtraCost(size_t);

        void addToStatistics(Statistics&) const;

        void markRoots();
        void markProtectedObjects(MarkStack&);
        void markCurrentThreadConservatively(MarkStack&);
        void markCurrentThreadConservativelyInternal(MarkStack&);
        void markOtherThreadConservatively(MarkStack&, Thread*);
        void markStackObjectsConservatively(MarkStack&);

        void updateWeakGCHandles();
        WeakGCHandlePool* weakGCHandlePool(size_t index);

        typedef HashCountedSet<JSCell*> ProtectCountSet;

        CollectorHeap m_heap;

        ProtectCountSet m_protectedValues;
        WTF::Vector<AlignedMemory<WeakGCHandlePool::poolSize> > m_weakGCHandlePools;

        HashSet<MarkedArgumentBuffer*>* m_markListSet;

        OwnPtr<GCActivityCallback> m_activityCallback;

#if ENABLE(JSC_MULTIPLE_THREADS)
        void makeUsableFromMultipleThreads();

        static void unregisterThread(void*);
        void unregisterThread();

        Mutex m_registeredThreadsMutex;
        Thread* m_registeredThreads;
        pthread_key_t m_currentThreadRegistrar;
#endif

        // Allocates collector blocks with correct alignment
        CollectorBlockAllocator m_blockallocator; 
        WeakGCHandlePool::Allocator m_weakGCHandlePoolAllocator; 
        
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
    const size_t CELLS_PER_BLOCK = (BLOCK_SIZE - sizeof(Heap*)) * 8 * CELL_SIZE / (8 * CELL_SIZE + 1) / CELL_SIZE; // one bitmap byte can represent 8 cells.
    
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

    inline CollectorBlock* Heap::cellBlock(const JSCell* cell)
    {
        return reinterpret_cast<CollectorBlock*>(reinterpret_cast<uintptr_t>(cell) & BLOCK_MASK);
    }

    inline size_t Heap::cellOffset(const JSCell* cell)
    {
        return (reinterpret_cast<uintptr_t>(cell) & BLOCK_OFFSET_MASK) / CELL_SIZE;
    }

    inline bool Heap::isCellMarked(const JSCell* cell)
    {
        return cellBlock(cell)->marked.get(cellOffset(cell));
    }

    inline bool Heap::checkMarkCell(const JSCell* cell)
    {
        return cellBlock(cell)->marked.getset(cellOffset(cell));
    }

    inline void Heap::markCell(JSCell* cell)
    {
        cellBlock(cell)->marked.set(cellOffset(cell));
    }

    inline void Heap::reportExtraMemoryCost(size_t cost)
    {
        if (cost > minExtraCost) 
            recordExtraCost(cost);
    }
    
    inline void* Heap::allocateNumber(size_t s)
    {
        if (void* result = m_heap.nextNumber) {
            m_heap.nextNumber = 0;
            return result;
        }

        void* result = allocate(s);
        m_heap.nextNumber = static_cast<char*>(result) + (CELL_SIZE / 2);
        return result;
    }


    inline WeakGCHandlePool* Heap::weakGCHandlePool(size_t index)
    {
        return static_cast<WeakGCHandlePool*>(m_weakGCHandlePools[index].base());
    }
} // namespace JSC

#endif /* Collector_h */
