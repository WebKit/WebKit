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

#include "CardSet.h"

#include <wtf/Bitmap.h>
#include <wtf/DoublyLinkedList.h>
#include <wtf/HashFunctions.h>
#include <wtf/PageAllocationAligned.h>
#include <wtf/StdLibExtras.h>

// Set to log state transitions of blocks.
#define HEAP_LOG_BLOCK_STATE_TRANSITIONS 0

#if HEAP_LOG_BLOCK_STATE_TRANSITIONS
#define HEAP_LOG_BLOCK_STATE_TRANSITION(block) do {                                  \
        printf("%s:%d %s: block %s = %p, %d\n",                                      \
               __FILE__, __LINE__, __FUNCTION__, #block, (block), (block)->m_state); \
    } while (false)
#else
#define HEAP_LOG_BLOCK_STATE_TRANSITION(block) ((void)0)
#endif

namespace JSC {
    
    class Heap;
    class JSCell;

    typedef uintptr_t Bits;

    static const size_t KB = 1024;
    static const size_t MB = 1024 * 1024;
    
    bool isZapped(const JSCell*);
    
    // A marked block is a page-aligned container for heap-allocated objects.
    // Objects are allocated within cells of the marked block. For a given
    // marked block, all cells have the same size. Objects smaller than the
    // cell size may be allocated in the marked block, in which case the
    // allocation suffers from internal fragmentation: wasted space whose
    // size is equal to the difference between the cell size and the object
    // size.

    class MarkedBlock : public DoublyLinkedListNode<MarkedBlock> {
        friend class WTF::DoublyLinkedListNode<MarkedBlock>;
    public:
        // Ensure natural alignment for native types whilst recognizing that the smallest
        // object the heap will commonly allocate is four words.
        static const size_t atomSize = 4 * sizeof(void*);
        static const size_t blockSize = 16 * KB;
        static const size_t blockMask = ~(blockSize - 1); // blockSize must be a power of two.

        static const size_t atomsPerBlock = blockSize / atomSize; // ~0.4% overhead
        static const size_t bytesPerCard = 512; // 1.6% overhead
        static const int log2CardSize = 9;

        struct FreeCell {
            FreeCell* next;
        };
        
        struct VoidFunctor {
            typedef void ReturnType;
            void returnValue() { }
        };

        static MarkedBlock* create(Heap*, size_t cellSize);
        static MarkedBlock* recycle(MarkedBlock*, size_t cellSize);
        static void destroy(MarkedBlock*);

        static bool isAtomAligned(const void*);
        static MarkedBlock* blockFor(const void*);
        static size_t firstAtom();
        
        Heap* heap() const;
        
        void* allocate();

        enum SweepMode { SweepOnly, SweepToFreeList };
        FreeCell* sweep(SweepMode = SweepOnly);

        // While allocating from a free list, MarkedBlock temporarily has bogus
        // cell liveness data. To restore accurate cell liveness data, call one
        // of these functions:
        void didConsumeFreeList(); // Call this once you've allocated all the items in the free list.
        void zapFreeList(FreeCell* firstFreeCell); // Call this to undo the free list.

        void clearMarks();
        size_t markCount();
        bool markCountIsZero(); // Faster than markCount().

        size_t cellSize();

        size_t size();
        size_t capacity();

        bool isMarked(const void*);
        bool testAndSetMarked(const void*);
        bool isLive(const JSCell*);
        bool isLiveCell(const void*);
        void setMarked(const void*);
        
#if ENABLE(GGC)
        void setDirtyObject(const void* atom)
        {
            m_cards.markCardForAtom(atom);
        }

        uint8_t* addressOfCardFor(const void* atom)
        {
            return &m_cards.cardForAtom(atom);
        }

        static inline size_t offsetOfCards()
        {
            return OBJECT_OFFSETOF(MarkedBlock, m_cards);
        }
#endif

        template <typename Functor> void forEachCell(Functor&);

    private:
        static const size_t atomAlignmentMask = atomSize - 1; // atomSize must be a power of two.

        enum BlockState { New, FreeListed, Allocated, Marked, Zapped };

        typedef char Atom[atomSize];

        MarkedBlock(const PageAllocationAligned&, Heap*, size_t cellSize);
        Atom* atoms();
        size_t atomNumber(const void*);
        void callDestructor(JSCell*, void* jsFinalObjectVPtr);
        template<BlockState, SweepMode> FreeCell* specializedSweep();
        
#if ENABLE(GGC)
        CardSet<bytesPerCard, blockSize> m_cards;
#endif

        size_t m_atomsPerCell;
        size_t m_endAtom; // This is a fuzzy end. Always test for < m_endAtom.
        WTF::Bitmap<atomsPerBlock> m_marks;
        BlockState m_state;
        PageAllocationAligned m_allocation;
        Heap* m_heap;
        MarkedBlock* m_prev;
        MarkedBlock* m_next;
    };

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

    inline Heap* MarkedBlock::heap() const
    {
        return m_heap;
    }

    inline void MarkedBlock::didConsumeFreeList()
    {
        HEAP_LOG_BLOCK_STATE_TRANSITION(this);

        ASSERT(m_state == FreeListed);
        m_state = Allocated;
    }

    inline void MarkedBlock::clearMarks()
    {
        HEAP_LOG_BLOCK_STATE_TRANSITION(this);

        ASSERT(m_state != New && m_state != FreeListed);
        m_marks.clearAll();

        // This will become true at the end of the mark phase. We set it now to
        // avoid an extra pass to do so later.
        m_state = Marked;
    }

    inline size_t MarkedBlock::markCount()
    {
        return m_marks.count();
    }

    inline bool MarkedBlock::markCountIsZero()
    {
        return m_marks.isEmpty();
    }

    inline size_t MarkedBlock::cellSize()
    {
        return m_atomsPerCell * atomSize;
    }

    inline size_t MarkedBlock::size()
    {
        return markCount() * cellSize();
    }

    inline size_t MarkedBlock::capacity()
    {
        return m_allocation.size();
    }

    inline size_t MarkedBlock::atomNumber(const void* p)
    {
        return (reinterpret_cast<Bits>(p) - reinterpret_cast<Bits>(this)) / atomSize;
    }

    inline bool MarkedBlock::isMarked(const void* p)
    {
        return m_marks.get(atomNumber(p));
    }

    inline bool MarkedBlock::testAndSetMarked(const void* p)
    {
        return m_marks.testAndSet(atomNumber(p));
    }

    inline void MarkedBlock::setMarked(const void* p)
    {
        m_marks.set(atomNumber(p));
    }

    inline bool MarkedBlock::isLive(const JSCell* cell)
    {
        switch (m_state) {
        case Allocated:
            return true;
        case Zapped:
            return !isZapped(cell);
        case Marked:
            return m_marks.get(atomNumber(cell));

        case New:
        case FreeListed:
            ASSERT_NOT_REACHED();
            return false;
        }

        ASSERT_NOT_REACHED();
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

        return isLive(static_cast<const JSCell*>(p));
    }

    template <typename Functor> inline void MarkedBlock::forEachCell(Functor& functor)
    {
        for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
            JSCell* cell = reinterpret_cast<JSCell*>(&atoms()[i]);
            if (!isLive(cell))
                continue;

            functor(cell);
        }
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
