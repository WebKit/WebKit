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

#include <wtf/Bitmap.h>
#include <wtf/DoublyLinkedList.h>
#include <wtf/HashFunctions.h>
#include <wtf/PageAllocationAligned.h>
#include <wtf/StdLibExtras.h>

// Set to log state transitions of blocks.
#define HEAP_LOG_BLOCK_STATE_TRANSITIONS 0

#if HEAP_LOG_BLOCK_STATE_TRANSITIONS
#define HEAP_DEBUG_BLOCK(block) do {                                    \
        printf("%s:%d %s: block %s = %p\n",                             \
               __FILE__, __LINE__, __FUNCTION__, #block, (block));      \
    } while (false)
#else
#define HEAP_DEBUG_BLOCK(block) ((void)0)
#endif

namespace JSC {
    
    class Heap;
    class JSCell;

    typedef uintptr_t Bits;

    static const size_t KB = 1024;
    
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

        static const size_t atomsPerBlock = blockSize / atomSize; // ~1.5% overhead
        static const size_t ownerSetsPerBlock = 8; // ~2% overhead.

        struct FreeCell {
            FreeCell* next;
            
            void setNoObject()
            {
                // This relies on FreeCell not having a vtable, and the next field
                // falling exactly where a vtable would have been.
                next = 0;
            }
        };
        
        struct VoidFunctor {
            typedef void ReturnType;
            void returnValue() { }
        };

        class OwnerSet {
        public:
            OwnerSet();
            
            void add(const JSCell*);
            void clear();

            size_t size();
            bool didOverflow();
            
            const JSCell** owners();

        private:
            static const size_t capacity = 5;
            unsigned char m_size;
            const JSCell* m_owners[capacity];
        };

        static MarkedBlock* create(Heap*, size_t cellSize);
        static void destroy(MarkedBlock*);

        static bool isAtomAligned(const void*);
        static MarkedBlock* blockFor(const void*);
        static size_t firstAtom();
        
        Heap* heap() const;
        
        bool inNewSpace();
        void setInNewSpace(bool);

        void* allocate();
        void sweep();
        
        // This invokes destructors on all cells that are not marked, marks
        // them, and returns a linked list of those cells.
        FreeCell* lazySweep();
        
        // Notify the block that destructors may have to be called again.
        void notifyMayHaveFreshFreeCells();
        
        void initForCellSize(size_t cellSize);
        
        // These should be called immediately after a block is created.
        // Blessing for fast path creates a linked list, while blessing for
        // slow path creates dummy cells.
        FreeCell* blessNewBlockForFastPath();
        void blessNewBlockForSlowPath();
        
        void reset();
        
        // This unmarks all cells on the free list, and allocates dummy JSCells
        // in their place.
        void canonicalizeBlock(FreeCell* firstFreeCell);
        
        bool isEmpty();

        void clearMarks();
        size_t markCount();

        size_t cellSize();

        size_t size();
        size_t capacity();

        bool isMarked(const void*);
        bool testAndSetMarked(const void*);
        bool testAndClearMarked(const void*);
        void setMarked(const void*);

        void addOldSpaceOwner(const JSCell* owner, const JSCell*);

        template <typename Functor> void forEachCell(Functor&);

    private:
        static const size_t blockMask = ~(blockSize - 1); // blockSize must be a power of two.
        static const size_t atomMask = ~(atomSize - 1); // atomSize must be a power of two.
        
        enum DestructorState { FreeCellsDontHaveObjects, SomeFreeCellsStillHaveObjects, AllFreeCellsHaveObjects };

        typedef char Atom[atomSize];

        MarkedBlock(const PageAllocationAligned&, Heap*, size_t cellSize);
        Atom* atoms();

        size_t atomNumber(const void*);
        size_t ownerSetNumber(const JSCell*);
        
        template<DestructorState destructorState>
        static void callDestructor(JSCell*, void* jsFinalObjectVPtr);
        
        template<DestructorState destructorState>
        void specializedReset();
        
        template<DestructorState destructorState>
        void specializedSweep();
        
        template<DestructorState destructorState>
        MarkedBlock::FreeCell* produceFreeList();
        
        void setDestructorState(DestructorState destructorState)
        {
            m_destructorState = static_cast<int8_t>(destructorState);
        }
        
        DestructorState destructorState()
        {
            return static_cast<DestructorState>(m_destructorState);
        }
        
        size_t m_endAtom; // This is a fuzzy end. Always test for < m_endAtom.
        size_t m_atomsPerCell;
        WTF::Bitmap<blockSize / atomSize> m_marks;
        bool m_inNewSpace;
        int8_t m_destructorState; // use getters/setters for this, particularly since we may want to compact this (effectively log(3)/log(2)-bit) field into other fields
        OwnerSet m_ownerSets[ownerSetsPerBlock];
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
        return !(reinterpret_cast<Bits>(p) & ~atomMask);
    }

    inline MarkedBlock* MarkedBlock::blockFor(const void* p)
    {
        return reinterpret_cast<MarkedBlock*>(reinterpret_cast<Bits>(p) & blockMask);
    }

    inline Heap* MarkedBlock::heap() const
    {
        return m_heap;
    }

    inline bool MarkedBlock::inNewSpace()
    {
        return m_inNewSpace;
    }
    
    inline void MarkedBlock::setInNewSpace(bool inNewSpace)
    {
        m_inNewSpace = inNewSpace;
    }
    
    inline void MarkedBlock::notifyMayHaveFreshFreeCells()
    {
        HEAP_DEBUG_BLOCK(this);
        
        // This is called at the beginning of GC. If this block is
        // AllFreeCellsHaveObjects, then it means that we filled up
        // the block in this collection. If it's in any other state,
        // then this collection will potentially produce new free
        // cells; new free cells always have objects. Hence if the
        // state currently claims that there are no objects in free
        // cells then we need to bump it over. Otherwise leave it be.
        // This all crucially relies on the collector canonicalizing
        // blocks before doing anything else, as canonicalizeBlocks
        // will correctly set SomeFreeCellsStillHaveObjects for
        // blocks that were only partially filled during this
        // mutation cycle.
        
        if (destructorState() == FreeCellsDontHaveObjects)
            setDestructorState(SomeFreeCellsStillHaveObjects);
    }

    inline bool MarkedBlock::isEmpty()
    {
        return m_marks.isEmpty();
    }

    inline void MarkedBlock::clearMarks()
    {
        m_marks.clearAll();
    }
    
    inline size_t MarkedBlock::markCount()
    {
        return m_marks.count();
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

    inline bool MarkedBlock::testAndClearMarked(const void* p)
    {
        return m_marks.testAndClear(atomNumber(p));
    }

    inline void MarkedBlock::setMarked(const void* p)
    {
        m_marks.set(atomNumber(p));
    }

    template <typename Functor> inline void MarkedBlock::forEachCell(Functor& functor)
    {
        for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
            if (!m_marks.get(i))
                continue;
            functor(reinterpret_cast<JSCell*>(&atoms()[i]));
        }
    }
    
    inline size_t MarkedBlock::ownerSetNumber(const JSCell* cell)
    {
        return (reinterpret_cast<Bits>(cell) - reinterpret_cast<Bits>(this)) * ownerSetsPerBlock / blockSize;
    }

    inline void MarkedBlock::addOldSpaceOwner(const JSCell* owner, const JSCell* cell)
    {
        OwnerSet& ownerSet = m_ownerSets[ownerSetNumber(cell)];
        ownerSet.add(owner);
    }

    inline MarkedBlock::OwnerSet::OwnerSet()
        : m_size(0)
    {
    }

    inline void MarkedBlock::OwnerSet::add(const JSCell* owner)
    {
        if (m_size < capacity) {
            m_owners[m_size++] = owner;
            return;
        }
        m_size = capacity + 1; // Signals overflow.
    }

    inline void MarkedBlock::OwnerSet::clear()
    {
        m_size = 0;
    }

    inline size_t MarkedBlock::OwnerSet::size()
    {
        return m_size;
    }

    inline bool MarkedBlock::OwnerSet::didOverflow()
    {
        return m_size > capacity;
    }

    inline const JSCell** MarkedBlock::OwnerSet::owners()
    {
        return m_owners;
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
