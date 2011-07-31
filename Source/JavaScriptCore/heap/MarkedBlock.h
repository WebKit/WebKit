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

namespace JSC {

    class Heap;
    class JSCell;

    typedef uintptr_t Bits;

    static const size_t KB = 1024;
    
    void destructor(JSCell*); // Defined in JSCell.h.

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

        typedef char Atom[atomSize];

        MarkedBlock(const PageAllocationAligned&, Heap*, size_t cellSize);
        Atom* atoms();

        size_t atomNumber(const void*);
        size_t ownerSetNumber(const JSCell*);

        size_t m_endAtom; // This is a fuzzy end. Always test for < m_endAtom.
        size_t m_atomsPerCell;
        WTF::Bitmap<blockSize / atomSize> m_marks;
        bool m_inNewSpace;
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
