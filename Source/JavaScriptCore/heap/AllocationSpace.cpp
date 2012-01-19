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

#include "AllocationSpace.h"

#include "Heap.h"

namespace JSC {

inline void* AllocationSpace::tryAllocate(MarkedSpace::SizeClass& sizeClass)
{
    m_heap->m_operationInProgress = Allocation;
    void* result = m_markedSpace.allocate(sizeClass);
    m_heap->m_operationInProgress = NoOperation;
    return result;
}

void* AllocationSpace::allocateSlowCase(MarkedSpace::SizeClass& sizeClass)
{
#if COLLECT_ON_EVERY_ALLOCATION
    m_heap->collectAllGarbage();
    ASSERT(m_heap->m_operationInProgress == NoOperation);
#endif
    
    void* result = tryAllocate(sizeClass);
    
    if (LIKELY(result != 0))
        return result;
    
    AllocationEffort allocationEffort;
    
    if ((
#if ENABLE(GGC)
         m_markedSpace.nurseryWaterMark() < m_heap->m_minBytesPerCycle
#else
         m_heap->waterMark() < m_heap->highWaterMark()
#endif
         ) || !m_heap->m_isSafeToCollect)
        allocationEffort = AllocationMustSucceed;
    else
        allocationEffort = AllocationCanFail;
    
    MarkedBlock* block = allocateBlock(sizeClass.cellSize, allocationEffort);
    if (block) {
        m_markedSpace.addBlock(sizeClass, block);
        void* result = tryAllocate(sizeClass);
        ASSERT(result);
        return result;
    }
    
    m_heap->collect(Heap::DoNotSweep);
    
    result = tryAllocate(sizeClass);
    
    if (result)
        return result;
    
    ASSERT(m_heap->waterMark() < m_heap->highWaterMark());
    
    m_markedSpace.addBlock(sizeClass, allocateBlock(sizeClass.cellSize, AllocationMustSucceed));
    
    result = tryAllocate(sizeClass);
    ASSERT(result);
    return result;
}

MarkedBlock* AllocationSpace::allocateBlock(size_t cellSize, AllocationEffort allocationEffort)
{
    MarkedBlock* block;
    
    {
        MutexLocker locker(m_heap->m_freeBlockLock);
        if (m_heap->m_numberOfFreeBlocks) {
            block = static_cast<MarkedBlock*>(m_heap->m_freeBlocks.removeHead());
            ASSERT(block);
            m_heap->m_numberOfFreeBlocks--;
        } else
            block = 0;
    }
    if (block)
        block = MarkedBlock::recycle(block, m_heap, cellSize);
    else if (allocationEffort == AllocationCanFail)
        return 0;
    else
        block = MarkedBlock::create(m_heap, cellSize);
    
    m_blocks.add(block);
    
    return block;
}

void AllocationSpace::freeBlocks(MarkedBlock* head)
{
    MarkedBlock* next;
    for (MarkedBlock* block = head; block; block = next) {
        next = static_cast<MarkedBlock*>(block->next());
        
        m_blocks.remove(block);
        block->sweep();
        MutexLocker locker(m_heap->m_freeBlockLock);
        m_heap->m_freeBlocks.append(block);
        m_heap->m_numberOfFreeBlocks++;
    }
}

class TakeIfUnmarked {
public:
    typedef MarkedBlock* ReturnType;
    
    TakeIfUnmarked(MarkedSpace*);
    void operator()(MarkedBlock*);
    ReturnType returnValue();
    
private:
    MarkedSpace* m_markedSpace;
    DoublyLinkedList<MarkedBlock> m_empties;
};

inline TakeIfUnmarked::TakeIfUnmarked(MarkedSpace* newSpace)
    : m_markedSpace(newSpace)
{
}

inline void TakeIfUnmarked::operator()(MarkedBlock* block)
{
    if (!block->markCountIsZero())
        return;
    
    m_markedSpace->removeBlock(block);
    m_empties.append(block);
}

inline TakeIfUnmarked::ReturnType TakeIfUnmarked::returnValue()
{
    return m_empties.head();
}

void AllocationSpace::shrink()
{
    // We record a temporary list of empties to avoid modifying m_blocks while iterating it.
    TakeIfUnmarked takeIfUnmarked(&m_markedSpace);
    freeBlocks(forEachBlock(takeIfUnmarked));
}

#if ENABLE(GGC)
class GatherDirtyCells {
    WTF_MAKE_NONCOPYABLE(GatherDirtyCells);
public:
    typedef void* ReturnType;
    
    explicit GatherDirtyCells(MarkedBlock::DirtyCellVector*);
    void operator()(MarkedBlock*);
    ReturnType returnValue() { return 0; }
    
private:
    MarkedBlock::DirtyCellVector* m_dirtyCells;
};

inline GatherDirtyCells::GatherDirtyCells(MarkedBlock::DirtyCellVector* dirtyCells)
    : m_dirtyCells(dirtyCells)
{
}

inline void GatherDirtyCells::operator()(MarkedBlock* block)
{
    block->gatherDirtyCells(*m_dirtyCells);
}

void AllocationSpace::gatherDirtyCells(MarkedBlock::DirtyCellVector& dirtyCells)
{
    GatherDirtyCells gatherDirtyCells(&dirtyCells);
    forEachBlock(gatherDirtyCells);
}
#endif

}
