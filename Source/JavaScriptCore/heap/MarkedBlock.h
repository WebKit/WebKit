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

#ifndef MarkedBlock_h
#define MarkedBlock_h

#include "BlockAllocator.h"
#include "HeapBlock.h"

#include "HeapOperation.h"
#include "WeakSet.h"
#include <wtf/Bitmap.h>
#include <wtf/DataLog.h>
#include <wtf/DoublyLinkedList.h>
#include <wtf/HashFunctions.h>
#include <wtf/PageAllocationAligned.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

// Set to log state transitions of blocks.
#define HEAP_LOG_BLOCK_STATE_TRANSITIONS 0

#if HEAP_LOG_BLOCK_STATE_TRANSITIONS
#define HEAP_LOG_BLOCK_STATE_TRANSITION(block) do {                     \
        dataLogF(                                                    \
            "%s:%d %s: block %s = %p, %d\n",                            \
            __FILE__, __LINE__, __FUNCTION__,                           \
            #block, (block), (block)->m_state);                         \
    } while (false)
#else
#define HEAP_LOG_BLOCK_STATE_TRANSITION(block) ((void)0)
#endif

namespace JSC {
    
    class Heap;
    class JSCell;
    class MarkedAllocator;

    typedef uintptr_t Bits;

    static const size_t MB = 1024 * 1024;
    
    bool isZapped(const JSCell*);
    
    // A marked block is a page-aligned container for heap-allocated objects.
    // Objects are allocated within cells of the marked block. For a given
    // marked block, all cells have the same size. Objects smaller than the
    // cell size may be allocated in the marked block, in which case the
    // allocation suffers from internal fragmentation: wasted space whose
    // size is equal to the difference between the cell size and the object
    // size.

    class MarkedBlock : public HeapBlock<MarkedBlock> {
        friend class LLIntOffsetsExtractor;
        friend struct VerifyMarkedOrRetired;
    public:
        static const size_t atomSize = 16; // bytes
        static const size_t atomShiftAmount = 4; // log_2(atomSize) FIXME: Change atomSize to 16.
        static const size_t blockSize = 64 * KB;
        static const size_t blockMask = ~(blockSize - 1); // blockSize must be a power of two.

        static const size_t atomsPerBlock = blockSize / atomSize;
        static const size_t atomMask = atomsPerBlock - 1;

        static const size_t markByteShiftAmount = 3; // log_2(word size for m_marks) FIXME: Change word size for m_marks to uint8_t.

        struct FreeCell {
            FreeCell* next;
        };
        
        struct FreeList {
            FreeCell* head;
            size_t bytes;

            FreeList();
            FreeList(FreeCell*, size_t);
        };

        struct VoidFunctor {
            typedef void ReturnType;
            void returnValue() { }
        };

        class CountFunctor {
        public:
            typedef size_t ReturnType;

            CountFunctor() : m_count(0) { }
            void count(size_t count) { m_count += count; }
            ReturnType returnValue() { return m_count; }

        private:
            ReturnType m_count;
        };

        enum DestructorType { None, ImmortalStructure, Normal };
        static MarkedBlock* create(DeadBlock*, MarkedAllocator*, size_t cellSize, DestructorType);

        static bool isAtomAligned(const void*);
        static MarkedBlock* blockFor(const void*);
        static size_t firstAtom();
        
        void lastChanceToFinalize();

        MarkedAllocator* allocator() const;
        Heap* heap() const;
        VM* vm() const;
        WeakSet& weakSet();
        
        enum SweepMode { SweepOnly, SweepToFreeList };
        FreeList sweep(SweepMode = SweepOnly);

        void shrink();

        void visitWeakSet(HeapRootVisitor&);
        void reapWeakSet();

        // While allocating from a free list, MarkedBlock temporarily has bogus
        // cell liveness data. To restore accurate cell liveness data, call one
        // of these functions:
        void didConsumeFreeList(); // Call this once you've allocated all the items in the free list.
        void stopAllocating(const FreeList&);
        FreeList resumeAllocating(); // Call this if you canonicalized a block for some non-collection related purpose.
        void didConsumeEmptyFreeList(); // Call this if you sweep a block, but the returned FreeList is empty.
        void didSweepToNoAvail(); // Call this if you sweep a block and get an empty free list back.

        // Returns true if the "newly allocated" bitmap was non-null 
        // and was successfully cleared and false otherwise.
        bool clearNewlyAllocated();
        void clearMarks();
        void clearRememberedSet();
        template <HeapOperation collectionType>
        void clearMarksWithCollectionType();

        size_t markCount();
        bool isEmpty();

        size_t cellSize();
        DestructorType destructorType();

        size_t size();
        size_t capacity();

        bool isMarked(const void*);
        bool testAndSetMarked(const void*);
        bool isLive(const JSCell*);
        bool isLiveCell(const void*);
        void setMarked(const void*);
        void clearMarked(const void*);

        void setRemembered(const void*);
        void clearRemembered(const void*);
        void atomicClearRemembered(const void*);
        bool isRemembered(const void*);

        bool isNewlyAllocated(const void*);
        void setNewlyAllocated(const void*);
        void clearNewlyAllocated(const void*);

        bool needsSweeping();
        void didRetireBlock(const FreeList&);
        void willRemoveBlock();

        template <typename Functor> void forEachCell(Functor&);
        template <typename Functor> void forEachLiveCell(Functor&);
        template <typename Functor> void forEachDeadCell(Functor&);

        static ptrdiff_t offsetOfMarks() { return OBJECT_OFFSETOF(MarkedBlock, m_marks); }

    private:
        static const size_t atomAlignmentMask = atomSize - 1; // atomSize must be a power of two.

        enum BlockState { New, FreeListed, Allocated, Marked, Retired };
        template<DestructorType> FreeList sweepHelper(SweepMode = SweepOnly);

        typedef char Atom[atomSize];

        MarkedBlock(Region*, MarkedAllocator*, size_t cellSize, DestructorType);
        Atom* atoms();
        size_t atomNumber(const void*);
        void callDestructor(JSCell*);
        template<BlockState, SweepMode, DestructorType> FreeList specializedSweep();
        
        size_t m_atomsPerCell;
        size_t m_endAtom; // This is a fuzzy end. Always test for < m_endAtom.
#if ENABLE(PARALLEL_GC)
        WTF::Bitmap<atomsPerBlock, WTF::BitmapAtomic, uint8_t> m_marks;
        WTF::Bitmap<atomsPerBlock, WTF::BitmapAtomic, uint8_t> m_rememberedSet;
#else
        WTF::Bitmap<atomsPerBlock, WTF::BitmapNotAtomic, uint8_t> m_marks;
        WTF::Bitmap<atomsPerBlock, WTF::BitmapNotAtomic, uint8_t> m_rememberedSet;
#endif
        OwnPtr<WTF::Bitmap<atomsPerBlock>> m_newlyAllocated;

        DestructorType m_destructorType;
        MarkedAllocator* m_allocator;
        BlockState m_state;
        WeakSet m_weakSet;
    };

    inline MarkedBlock::FreeList::FreeList()
        : head(0)
        , bytes(0)
    {
    }

    inline MarkedBlock::FreeList::FreeList(FreeCell* head, size_t bytes)
        : head(head)
        , bytes(bytes)
    {
    }

    inline size_t MarkedBlock::firstAtom()
    {
        return WTF::roundUpToMultipleOf<atomSize>(sizeof(MarkedBlock)) / atomSize;
    }

    inline MarkedBlock::Atom* MarkedBlock::atoms()
    {
        return reinterpret_cast<Atom*>(this);
    }

    inline bool MarkedBlock::isAtomAligned(const void* p)
    {
        return !(reinterpret_cast<Bits>(p) & atomAlignmentMask);
    }

    inline MarkedBlock* MarkedBlock::blockFor(const void* p)
    {
        return reinterpret_cast<MarkedBlock*>(reinterpret_cast<Bits>(p) & blockMask);
    }

    inline MarkedAllocator* MarkedBlock::allocator() const
    {
        return m_allocator;
    }

    inline Heap* MarkedBlock::heap() const
    {
        return m_weakSet.heap();
    }

    inline VM* MarkedBlock::vm() const
    {
        return m_weakSet.vm();
    }

    inline WeakSet& MarkedBlock::weakSet()
    {
        return m_weakSet;
    }

    inline void MarkedBlock::shrink()
    {
        m_weakSet.shrink();
    }

    inline void MarkedBlock::visitWeakSet(HeapRootVisitor& heapRootVisitor)
    {
        m_weakSet.visit(heapRootVisitor);
    }

    inline void MarkedBlock::reapWeakSet()
    {
        m_weakSet.reap();
    }

    inline void MarkedBlock::willRemoveBlock()
    {
        ASSERT(m_state != Retired);
    }

    inline void MarkedBlock::didConsumeFreeList()
    {
        HEAP_LOG_BLOCK_STATE_TRANSITION(this);

        ASSERT(m_state == FreeListed);
        m_state = Allocated;
    }

    inline void MarkedBlock::didConsumeEmptyFreeList()
    {
        HEAP_LOG_BLOCK_STATE_TRANSITION(this);

        ASSERT(!m_newlyAllocated);
        ASSERT(m_state == FreeListed);
        m_state = Marked;
    }

    inline size_t MarkedBlock::markCount()
    {
        return m_marks.count();
    }

    inline bool MarkedBlock::isEmpty()
    {
        return m_marks.isEmpty() && m_weakSet.isEmpty() && (!m_newlyAllocated || m_newlyAllocated->isEmpty());
    }

    inline size_t MarkedBlock::cellSize()
    {
        return m_atomsPerCell * atomSize;
    }

    inline MarkedBlock::DestructorType MarkedBlock::destructorType()
    {
        return m_destructorType; 
    }

    inline size_t MarkedBlock::size()
    {
        return markCount() * cellSize();
    }

    inline size_t MarkedBlock::capacity()
    {
        return region()->blockSize();
    }

    inline size_t MarkedBlock::atomNumber(const void* p)
    {
        return (reinterpret_cast<Bits>(p) - reinterpret_cast<Bits>(this)) / atomSize;
    }

    inline void MarkedBlock::setRemembered(const void* p)
    {
        m_rememberedSet.set(atomNumber(p));
    }

    inline void MarkedBlock::clearRemembered(const void* p)
    {
        m_rememberedSet.clear(atomNumber(p));
    }

    inline void MarkedBlock::atomicClearRemembered(const void* p)
    {
        m_rememberedSet.concurrentTestAndClear(atomNumber(p));
    }

    inline bool MarkedBlock::isRemembered(const void* p)
    {
        return m_rememberedSet.get(atomNumber(p));
    }

    inline bool MarkedBlock::isMarked(const void* p)
    {
        return m_marks.get(atomNumber(p));
    }

    inline bool MarkedBlock::testAndSetMarked(const void* p)
    {
        return m_marks.concurrentTestAndSet(atomNumber(p));
    }

    inline void MarkedBlock::setMarked(const void* p)
    {
        m_marks.set(atomNumber(p));
    }

    inline void MarkedBlock::clearMarked(const void* p)
    {
        ASSERT(m_marks.get(atomNumber(p)));
        m_marks.clear(atomNumber(p));
    }

    inline bool MarkedBlock::isNewlyAllocated(const void* p)
    {
        return m_newlyAllocated->get(atomNumber(p));
    }

    inline void MarkedBlock::setNewlyAllocated(const void* p)
    {
        m_newlyAllocated->set(atomNumber(p));
    }

    inline void MarkedBlock::clearNewlyAllocated(const void* p)
    {
        m_newlyAllocated->clear(atomNumber(p));
    }

    inline bool MarkedBlock::clearNewlyAllocated()
    {
        if (m_newlyAllocated) {
            m_newlyAllocated.clear();
            return true;
        }
        return false;
    }

    inline bool MarkedBlock::isLive(const JSCell* cell)
    {
        switch (m_state) {
        case Allocated:
            return true;

        case Retired:
        case Marked:
            return m_marks.get(atomNumber(cell)) || (m_newlyAllocated && isNewlyAllocated(cell));

        case New:
        case FreeListed:
            RELEASE_ASSERT_NOT_REACHED();
            return false;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }

    inline bool MarkedBlock::isLiveCell(const void* p)
    {
        ASSERT(MarkedBlock::isAtomAligned(p));
        size_t atomNumber = this->atomNumber(p);
        size_t firstAtom = this->firstAtom();
        if (atomNumber < firstAtom) // Filters pointers into MarkedBlock metadata.
            return false;
        if ((atomNumber - firstAtom) % m_atomsPerCell) // Filters pointers into cell middles.
            return false;
        if (atomNumber >= m_endAtom) // Filters pointers into invalid cells out of the range.
            return false;

        return isLive(static_cast<const JSCell*>(p));
    }

    template <typename Functor> inline void MarkedBlock::forEachCell(Functor& functor)
    {
        for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
            JSCell* cell = reinterpret_cast_ptr<JSCell*>(&atoms()[i]);
            functor(cell);
        }
    }

    template <typename Functor> inline void MarkedBlock::forEachLiveCell(Functor& functor)
    {
        for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
            JSCell* cell = reinterpret_cast_ptr<JSCell*>(&atoms()[i]);
            if (!isLive(cell))
                continue;

            functor(cell);
        }
    }

    template <typename Functor> inline void MarkedBlock::forEachDeadCell(Functor& functor)
    {
        for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
            JSCell* cell = reinterpret_cast_ptr<JSCell*>(&atoms()[i]);
            if (isLive(cell))
                continue;

            functor(cell);
        }
    }

    inline bool MarkedBlock::needsSweeping()
    {
        return m_state == Marked;
    }

} // namespace JSC

namespace WTF {

    struct MarkedBlockHash : PtrHash<JSC::MarkedBlock*> {
        static unsigned hash(JSC::MarkedBlock* const& key)
        {
            // Aligned VM regions tend to be monotonically increasing integers,
            // which is a great hash function, but we have to remove the low bits,
            // since they're always zero, which is a terrible hash function!
            return reinterpret_cast<JSC::Bits>(key) / JSC::MarkedBlock::blockSize;
        }
    };

    template<> struct DefaultHash<JSC::MarkedBlock*> {
        typedef MarkedBlockHash Hash;
    };

} // namespace WTF

#endif // MarkedBlock_h
