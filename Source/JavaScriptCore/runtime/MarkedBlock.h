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
#include <wtf/PageAllocationAligned.h>

namespace JSC {

    class Heap;
    class JSCell;
    class JSGlobalData;

    typedef uintptr_t Bits;

    static const size_t KB = 1024;

    // Efficient implementation that takes advantage of powers of two.
    template<size_t divisor> inline size_t roundUpToMultipleOf(size_t x)
    {
        COMPILE_ASSERT(divisor && !(divisor & (divisor - 1)), divisor_is_a_power_of_two);

        size_t remainderMask = divisor - 1;
        return (x + remainderMask) & ~remainderMask;
    }

    class MarkedBlock {
    public:
        static MarkedBlock* create(JSGlobalData*, size_t cellSize);
        static void destroy(MarkedBlock*);

        static bool isAtomAligned(const void*);
        static MarkedBlock* blockFor(const void*);
        
        Heap* heap() const;
        
        void* allocate(size_t& nextCell);
        void sweep();
        
        bool isEmpty();

        void clearMarks();
        size_t markCount();

        size_t size();
        size_t capacity();

        size_t firstAtom();

        size_t atomNumber(const void*);
        bool isMarked(const void*);
        bool testAndSetMarked(const void*);
        void setMarked(const void*);
        
        template <typename Functor> void forEach(Functor&);

    private:
#if OS(WINCE) || OS(SYMBIAN) || PLATFORM(BREWMP)
        static const size_t blockSize = 64 * KB;
#else
        static const size_t blockSize = 256 * KB;
#endif
        static const size_t blockMask = ~(blockSize - 1); // blockSize must be a power of two.

        static const size_t atomSize = 64;
        static const size_t atomMask = ~(atomSize - 1); // atomSize must be a power of two.

        typedef char Atom[atomSize];

        MarkedBlock(const PageAllocationAligned&, JSGlobalData*, size_t cellSize);
        Atom* atoms();

        size_t m_atomsPerCell;
        size_t m_endAtom;
        WTF::Bitmap<blockSize / atomSize> m_marks;
        PageAllocationAligned m_allocation;
        Heap* m_heap;
    };

    inline size_t MarkedBlock::firstAtom()
    {
        return roundUpToMultipleOf<atomSize>(sizeof(MarkedBlock)) / atomSize;
    }

    inline MarkedBlock::Atom* MarkedBlock::atoms()
    {
        return reinterpret_cast<Atom*>(this);
    }

    inline bool MarkedBlock::isAtomAligned(const void* p)
    {
        return !((intptr_t)(p) & ~atomMask);
    }

    inline MarkedBlock* MarkedBlock::blockFor(const void* p)
    {
        return reinterpret_cast<MarkedBlock*>(reinterpret_cast<uintptr_t>(p) & blockMask);
    }

    inline Heap* MarkedBlock::heap() const
    {
        return m_heap;
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

    inline size_t MarkedBlock::size()
    {
        return markCount() * m_atomsPerCell * atomSize;
    }

    inline size_t MarkedBlock::capacity()
    {
        return m_allocation.size();
    }

    inline size_t MarkedBlock::atomNumber(const void* p)
    {
        return (reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(this)) / atomSize;
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

    template <typename Functor> inline void MarkedBlock::forEach(Functor& functor)
    {
        for (size_t i = firstAtom(); i != m_endAtom; i += m_atomsPerCell) {
            if (!m_marks.get(i))
                continue;
            functor(reinterpret_cast<JSCell*>(&atoms()[i]));
        }
    }

} // namespace JSC

#endif // MarkedSpace_h
