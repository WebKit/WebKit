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

#ifndef NewSpace_h
#define NewSpace_h

#include "MachineStackMarker.h"
#include "MarkedBlock.h"
#include "PageAllocationAligned.h"
#include <wtf/Bitmap.h>
#include <wtf/DoublyLinkedList.h>
#include <wtf/FixedArray.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

#define ASSERT_CLASS_FITS_IN_CELL(class) COMPILE_ASSERT(sizeof(class) < NewSpace::maxCellSize, class_fits_in_cell)

namespace JSC {

    class Heap;
    class JSCell;
    class LiveObjectIterator;
    class MarkStack;
    class WeakGCHandle;
    typedef MarkStack SlotVisitor;

    class NewSpace {
        WTF_MAKE_NONCOPYABLE(NewSpace);
    public:
        static const size_t maxCellSize = 1024;

        struct SizeClass {
            SizeClass();
            void resetAllocator();

            MarkedBlock* nextBlock;
            DoublyLinkedList<MarkedBlock> blockList;
            size_t cellSize;
        };

        NewSpace(Heap*);

        SizeClass& sizeClassFor(size_t);
        void* allocate(SizeClass&);
        void resetAllocator();

        void addBlock(SizeClass&, MarkedBlock*);
        void removeBlock(MarkedBlock*);

        size_t waterMark();
        size_t highWaterMark();
        void setHighWaterMark(size_t);

        template<typename Functor> typename Functor::ReturnType forEachBlock(Functor&); // Safe to remove the current item while iterating.
        template<typename Functor> typename Functor::ReturnType forEachBlock();

    private:
        // [ 8, 16... 128 )
        static const size_t preciseStep = MarkedBlock::atomSize;
        static const size_t preciseCutoff = 128;
        static const size_t preciseCount = preciseCutoff / preciseStep - 1;

        // [ 128, 256... 1024 )
        static const size_t impreciseStep = preciseCutoff;
        static const size_t impreciseCutoff = maxCellSize;
        static const size_t impreciseCount = impreciseCutoff / impreciseStep - 1;

        SizeClass m_preciseSizeClasses[preciseCount];
        SizeClass m_impreciseSizeClasses[impreciseCount];
        size_t m_waterMark;
        size_t m_highWaterMark;
        Heap* m_heap;
    };

    inline size_t NewSpace::waterMark()
    {
        return m_waterMark;
    }

    inline size_t NewSpace::highWaterMark()
    {
        return m_highWaterMark;
    }

    inline void NewSpace::setHighWaterMark(size_t highWaterMark)
    {
        m_highWaterMark = highWaterMark;
    }

    inline NewSpace::SizeClass& NewSpace::sizeClassFor(size_t bytes)
    {
        ASSERT(bytes && bytes < maxCellSize);
        if (bytes < preciseCutoff)
            return m_preciseSizeClasses[(bytes - 1) / preciseStep];
        return m_impreciseSizeClasses[(bytes - 1) / impreciseStep];
    }

    inline void* NewSpace::allocate(SizeClass& sizeClass)
    {
        for (MarkedBlock*& block = sizeClass.nextBlock ; block; block = block->next()) {
            if (void* result = block->allocate())
                return result;

            m_waterMark += block->capacity();
        }

        return 0;
    }

    template <typename Functor> inline typename Functor::ReturnType NewSpace::forEachBlock(Functor& functor)
    {
        for (size_t i = 0; i < preciseCount; ++i) {
            SizeClass& sizeClass = m_preciseSizeClasses[i];
            MarkedBlock* next;
            for (MarkedBlock* block = sizeClass.blockList.head(); block; block = next) {
                next = block->next();
                functor(block);
            }
        }

        for (size_t i = 0; i < impreciseCount; ++i) {
            SizeClass& sizeClass = m_impreciseSizeClasses[i];
            MarkedBlock* next;
            for (MarkedBlock* block = sizeClass.blockList.head(); block; block = next) {
                next = block->next();
                functor(block);
            }
        }

        return functor.returnValue();
    }

    template <typename Functor> inline typename Functor::ReturnType NewSpace::forEachBlock()
    {
        Functor functor;
        return forEachBlock(functor);
    }

    inline NewSpace::SizeClass::SizeClass()
        : nextBlock(0)
        , cellSize(0)
    {
    }

    inline void NewSpace::SizeClass::resetAllocator()
    {
        nextBlock = blockList.head();
    }

} // namespace JSC

#endif // NewSpace_h
