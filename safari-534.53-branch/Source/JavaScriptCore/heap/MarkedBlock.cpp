/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MarkedBlock.h"

#include "JSCell.h"
#include "JSObject.h"
#include "ScopeChain.h"

namespace JSC {

MarkedBlock* MarkedBlock::create(Heap* heap, size_t cellSize)
{
    PageAllocationAligned allocation = PageAllocationAligned::allocate(blockSize, blockSize, OSAllocator::JSGCHeapPages);
    if (!static_cast<bool>(allocation))
        CRASH();
    return new (allocation.base()) MarkedBlock(allocation, heap, cellSize);
}

void MarkedBlock::destroy(MarkedBlock* block)
{
    block->m_allocation.deallocate();
}

MarkedBlock::MarkedBlock(const PageAllocationAligned& allocation, Heap* heap, size_t cellSize)
    : m_inNewSpace(false)
    , m_allocation(allocation)
    , m_heap(heap)
{
    initForCellSize(cellSize);
}

void MarkedBlock::initForCellSize(size_t cellSize)
{
    m_atomsPerCell = (cellSize + atomSize - 1) / atomSize;
    m_endAtom = atomsPerBlock - m_atomsPerCell + 1;
    setDestructorState(SomeFreeCellsStillHaveObjects);
}

template<MarkedBlock::DestructorState specializedDestructorState>
void MarkedBlock::callDestructor(JSCell* cell, void* jsFinalObjectVPtr)
{
    if (specializedDestructorState == FreeCellsDontHaveObjects)
        return;
    void* vptr = cell->vptr();
    if (specializedDestructorState == AllFreeCellsHaveObjects || vptr) {
        if (vptr == jsFinalObjectVPtr) {
            JSFinalObject* object = reinterpret_cast<JSFinalObject*>(cell);
            object->JSFinalObject::~JSFinalObject();
        } else
            cell->~JSCell();
    }
}

template<MarkedBlock::DestructorState specializedDestructorState>
void MarkedBlock::specializedReset()
{
    void* jsFinalObjectVPtr = m_heap->globalData()->jsFinalObjectVPtr;

    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell)
        callDestructor<specializedDestructorState>(reinterpret_cast<JSCell*>(&atoms()[i]), jsFinalObjectVPtr);
}

void MarkedBlock::reset()
{
    switch (destructorState()) {
    case FreeCellsDontHaveObjects:
    case SomeFreeCellsStillHaveObjects:
        specializedReset<SomeFreeCellsStillHaveObjects>();
        break;
    default:
        ASSERT(destructorState() == AllFreeCellsHaveObjects);
        specializedReset<AllFreeCellsHaveObjects>();
        break;
    }
}

template<MarkedBlock::DestructorState specializedDestructorState>
void MarkedBlock::specializedSweep()
{
    if (specializedDestructorState != FreeCellsDontHaveObjects) {
        void* jsFinalObjectVPtr = m_heap->globalData()->jsFinalObjectVPtr;
        
        for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
            if (m_marks.get(i))
                continue;
            
            JSCell* cell = reinterpret_cast<JSCell*>(&atoms()[i]);
            callDestructor<specializedDestructorState>(cell, jsFinalObjectVPtr);
            cell->setVPtr(0);
        }
        
        setDestructorState(FreeCellsDontHaveObjects);
    }
}

void MarkedBlock::sweep()
{
    HEAP_DEBUG_BLOCK(this);
    
    switch (destructorState()) {
    case FreeCellsDontHaveObjects:
        break;
    case SomeFreeCellsStillHaveObjects:
        specializedSweep<SomeFreeCellsStillHaveObjects>();
        break;
    default:
        ASSERT(destructorState() == AllFreeCellsHaveObjects);
        specializedSweep<AllFreeCellsHaveObjects>();
        break;
    }
}

template<MarkedBlock::DestructorState specializedDestructorState>
ALWAYS_INLINE MarkedBlock::FreeCell* MarkedBlock::produceFreeList()
{
    // This returns a free list that is ordered in reverse through the block.
    // This is fine, since the allocation code makes no assumptions about the
    // order of the free list.
    
    void* jsFinalObjectVPtr = m_heap->globalData()->jsFinalObjectVPtr;
    
    FreeCell* result = 0;
    
    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
        if (!m_marks.testAndSet(i)) {
            JSCell* cell = reinterpret_cast<JSCell*>(&atoms()[i]);
            if (specializedDestructorState != FreeCellsDontHaveObjects)
                callDestructor<specializedDestructorState>(cell, jsFinalObjectVPtr);
            FreeCell* freeCell = reinterpret_cast<FreeCell*>(cell);
            freeCell->next = result;
            result = freeCell;
        }
    }
    
    // This is sneaky: if we're producing a free list then we intend to
    // fill up the free cells in the block with objects, which means that
    // if we have a new GC then all of the free stuff in this block will
    // comprise objects rather than empty cells.
    setDestructorState(AllFreeCellsHaveObjects);

    return result;
}

MarkedBlock::FreeCell* MarkedBlock::lazySweep()
{
    // This returns a free list that is ordered in reverse through the block.
    // This is fine, since the allocation code makes no assumptions about the
    // order of the free list.
    
    HEAP_DEBUG_BLOCK(this);
    
    switch (destructorState()) {
    case FreeCellsDontHaveObjects:
        return produceFreeList<FreeCellsDontHaveObjects>();
    case SomeFreeCellsStillHaveObjects:
        return produceFreeList<SomeFreeCellsStillHaveObjects>();
    default:
        ASSERT(destructorState() == AllFreeCellsHaveObjects);
        return produceFreeList<AllFreeCellsHaveObjects>();
    }
}

MarkedBlock::FreeCell* MarkedBlock::blessNewBlockForFastPath()
{
    // This returns a free list that is ordered in reverse through the block,
    // as in lazySweep() above.
    
    HEAP_DEBUG_BLOCK(this);

    FreeCell* result = 0;
    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
        m_marks.set(i);
        FreeCell* freeCell = reinterpret_cast<FreeCell*>(&atoms()[i]);
        freeCell->next = result;
        result = freeCell;
    }
    
    // See produceFreeList(). If we're here then we intend to fill the
    // block with objects, so once a GC happens, all free cells will be
    // occupied by objects.
    setDestructorState(AllFreeCellsHaveObjects);

    return result;
}

void MarkedBlock::blessNewBlockForSlowPath()
{
    HEAP_DEBUG_BLOCK(this);

    m_marks.clearAll();
    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell)
        reinterpret_cast<FreeCell*>(&atoms()[i])->setNoObject();
    
    setDestructorState(FreeCellsDontHaveObjects);
}

void MarkedBlock::canonicalizeBlock(FreeCell* firstFreeCell)
{
    HEAP_DEBUG_BLOCK(this);
    
    ASSERT(destructorState() == AllFreeCellsHaveObjects);
    
    if (firstFreeCell) {
        for (FreeCell* current = firstFreeCell; current;) {
            FreeCell* next = current->next;
            size_t i = atomNumber(current);
            
            m_marks.clear(i);
            
            current->setNoObject();
            
            current = next;
        }
        
        setDestructorState(SomeFreeCellsStillHaveObjects);
    }
}

} // namespace JSC
