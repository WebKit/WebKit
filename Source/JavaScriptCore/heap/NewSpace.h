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
    class WeakGCHandle;
    class SlotVisitor;

    class NewSpace {
        WTF_MAKE_NONCOPYABLE(NewSpace);
    public:
        static const size_t maxCellSize = 1024;

        struct SizeClass {
            SizeClass();
            void resetAllocator();
            void canonicalizeBlock();

            MarkedBlock::FreeCell* firstFreeCell;
            MarkedBlock* currentBlock;
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
        
        void canonicalizeBlocks();

        size_t waterMark();
        size_t highWaterMark();
        void setHighWaterMark(size_t);

        template<typename Functor> typename Functor::ReturnType forEachBlock(Functor&); // Safe to remove the current item while iterating.
        template<typename Functor> typename Functor::ReturnType forEachBlock();

    private:
        // [ 8, 16... 128 )
        static const size_t preciseStep = MarkedBlock::atomSize;
        static const size_t preciseCutoff = 128;
        static const size_t maximumPreciseAllocationSize = preciseCutoff - preciseStep;
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
        if (bytes <= maximumPreciseAllocationSize)
            return m_preciseSizeClasses[(bytes - 1) / preciseStep];
        return m_impreciseSizeClasses[(bytes - 1) / impreciseStep];
    }

    inline void* NewSpace::allocate(SizeClass& sizeClass)
    {
        MarkedBlock::FreeCell* firstFreeCell = sizeClass.firstFreeCell;
        if (!firstFreeCell) {
            // There are two possibilities for why we got here:
            // 1) We've exhausted the allocation cache for currentBlock, in which case
            //    currentBlock == nextBlock, and we know that there is no reason to
            //    repeat a lazy sweep of nextBlock because we won't find anything.
            // 2) Allocation caches have been cleared, in which case nextBlock may
            //    have (and most likely does have) free cells, so we almost certainly
            //    should do a lazySweep for nextBlock. This also implies that
            //    currentBlock == 0.
            
            if (sizeClass.currentBlock) {
                ASSERT(sizeClass.currentBlock == sizeClass.nextBlock);
                m_waterMark += sizeClass.nextBlock->capacity();
                sizeClass.nextBlock = sizeClass.nextBlock->next();
                sizeClass.currentBlock = 0;
            }
            
            for (MarkedBlock*& block = sizeClass.nextBlock ; block; block = block->next()) {
                firstFreeCell = block->lazySweep();
                if (firstFreeCell) {
                    sizeClass.firstFreeCell = firstFreeCell;
                    sizeClass.currentBlock = block;
                    break;
                }
                
                m_waterMark += block->capacity();
            }
            
            if (!firstFreeCell)
                return 0;
        }
        
        ASSERT(firstFreeCell);
        
        sizeClass.firstFreeCell = firstFreeCell->next;
        return firstFreeCell;
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
        : firstFreeCell(0)
        , currentBlock(0)
        , nextBlock(0)
        , cellSize(0)
    {
    }

    inline void NewSpace::SizeClass::resetAllocator()
    {
        nextBlock = blockList.head();
    }
    
    inline void NewSpace::SizeClass::canonicalizeBlock()
    {
        if (currentBlock) {
            currentBlock->canonicalizeBlock(firstFreeCell);
            firstFreeCell = 0;
        }
        
        ASSERT(!firstFreeCell);
        
        currentBlock = 0;
        firstFreeCell = 0;
    }

} // namespace JSC

#endif // NewSpace_h
