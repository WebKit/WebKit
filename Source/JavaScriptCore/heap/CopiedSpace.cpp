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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CopiedSpace.h"

#include "CopiedSpaceInlineMethods.h"
#include "GCActivityCallback.h"

namespace JSC {

CopiedSpace::CopiedSpace(Heap* heap)
    : m_heap(heap)
    , m_toSpace(0)
    , m_fromSpace(0)
    , m_inCopyingPhase(false)
    , m_numberOfLoanedBlocks(0)
{
}

void CopiedSpace::init()
{
    m_toSpace = &m_blocks1;
    m_fromSpace = &m_blocks2;
    
    if (!addNewBlock())
        CRASH();
}   

CheckedBoolean CopiedSpace::tryAllocateSlowCase(size_t bytes, void** outPtr)
{
    if (isOversize(bytes))
        return tryAllocateOversize(bytes, outPtr);
    
    m_heap->didAllocate(m_allocator.currentCapacity());

    if (!addNewBlock()) {
        *outPtr = 0;
        return false;
    }
    *outPtr = m_allocator.allocate(bytes);
    ASSERT(*outPtr);
    return true;
}

CheckedBoolean CopiedSpace::tryAllocateOversize(size_t bytes, void** outPtr)
{
    ASSERT(isOversize(bytes));
    
    size_t blockSize = WTF::roundUpToMultipleOf(WTF::pageSize(), sizeof(CopiedBlock) + bytes);

    PageAllocationAligned allocation = PageAllocationAligned::allocate(blockSize, WTF::pageSize(), OSAllocator::JSGCHeapPages);
    if (!static_cast<bool>(allocation)) {
        *outPtr = 0;
        return false;
    }

    CopiedBlock* block = new (NotNull, allocation.base()) CopiedBlock(allocation);
    m_oversizeBlocks.push(block);
    m_oversizeFilter.add(reinterpret_cast<Bits>(block));
    
    *outPtr = allocateFromBlock(block, bytes);

    m_heap->didAllocate(blockSize);

    return true;
}

CheckedBoolean CopiedSpace::tryReallocate(void** ptr, size_t oldSize, size_t newSize)
{
    if (oldSize >= newSize)
        return true;
    
    void* oldPtr = *ptr;
    ASSERT(!m_heap->globalData()->isInitializingObject());

    if (isOversize(oldSize) || isOversize(newSize))
        return tryReallocateOversize(ptr, oldSize, newSize);

    if (m_allocator.wasLastAllocation(oldPtr, oldSize)) {
        size_t delta = newSize - oldSize;
        if (m_allocator.fitsInCurrentBlock(delta)) {
            (void)m_allocator.allocate(delta);
            return true;
        }
    }

    void* result = 0;
    if (!tryAllocate(newSize, &result)) {
        *ptr = 0;
        return false;
    }
    memcpy(result, oldPtr, oldSize);
    *ptr = result;
    return true;
}

CheckedBoolean CopiedSpace::tryReallocateOversize(void** ptr, size_t oldSize, size_t newSize)
{
    ASSERT(isOversize(oldSize) || isOversize(newSize));
    ASSERT(newSize > oldSize);

    void* oldPtr = *ptr;
    
    void* newPtr = 0;
    if (!tryAllocateOversize(newSize, &newPtr)) {
        *ptr = 0;
        return false;
    }

    memcpy(newPtr, oldPtr, oldSize);

    if (isOversize(oldSize)) {
        CopiedBlock* oldBlock = oversizeBlockFor(oldPtr);
        m_oversizeBlocks.remove(oldBlock);
        oldBlock->m_allocation.deallocate();
    }
    
    *ptr = newPtr;
    return true;
}

void CopiedSpace::doneFillingBlock(CopiedBlock* block)
{
    ASSERT(block);
    ASSERT(block->m_offset < reinterpret_cast<char*>(block) + HeapBlock::s_blockSize);
    ASSERT(m_inCopyingPhase);

    if (block->m_offset == block->payload()) {
        recycleBlock(block);
        return;
    }

    {
        MutexLocker locker(m_toSpaceLock);
        m_toSpace->push(block);
        m_toSpaceSet.add(block);
        m_toSpaceFilter.add(reinterpret_cast<Bits>(block));
    }

    {
        MutexLocker locker(m_loanedBlocksLock);
        ASSERT(m_numberOfLoanedBlocks > 0);
        m_numberOfLoanedBlocks--;
        if (!m_numberOfLoanedBlocks)
            m_loanedBlocksCondition.signal();
    }
}

void CopiedSpace::doneCopying()
{
    {
        MutexLocker locker(m_loanedBlocksLock);
        while (m_numberOfLoanedBlocks > 0)
            m_loanedBlocksCondition.wait(m_loanedBlocksLock);
    }

    ASSERT(m_inCopyingPhase);
    m_inCopyingPhase = false;
    while (!m_fromSpace->isEmpty()) {
        CopiedBlock* block = static_cast<CopiedBlock*>(m_fromSpace->removeHead());
        if (block->m_isPinned) {
            block->m_isPinned = false;
            m_toSpace->push(block);
            continue;
        }

        m_toSpaceSet.remove(block);
        m_heap->blockAllocator().deallocate(block);
    }

    CopiedBlock* curr = static_cast<CopiedBlock*>(m_oversizeBlocks.head());
    while (curr) {
        CopiedBlock* next = static_cast<CopiedBlock*>(curr->next());
        if (!curr->m_isPinned) {
            m_oversizeBlocks.remove(curr);
            curr->m_allocation.deallocate();
        } else
            curr->m_isPinned = false;
        curr = next;
    }

    if (!m_toSpace->head()) {
        if (!addNewBlock())
            CRASH();
    } else
        m_allocator.resetCurrentBlock(static_cast<CopiedBlock*>(m_toSpace->head()));
}

CheckedBoolean CopiedSpace::getFreshBlock(AllocationEffort allocationEffort, CopiedBlock** outBlock)
{
    CopiedBlock* block = 0;
    if (allocationEffort == AllocationMustSucceed) {
        if (HeapBlock* heapBlock = m_heap->blockAllocator().allocate())
            block = new (NotNull, heapBlock) CopiedBlock(heapBlock->m_allocation);
        else if (!allocateNewBlock(&block)) {
            *outBlock = 0;
            ASSERT_NOT_REACHED();
            return false;
        }
    } else {
        ASSERT(allocationEffort == AllocationCanFail);
        if (m_heap->shouldCollect())
            m_heap->collect(Heap::DoNotSweep);
        
        if (!getFreshBlock(AllocationMustSucceed, &block)) {
            *outBlock = 0;
            ASSERT_NOT_REACHED();
            return false;
        }
    }
    ASSERT(block);
    ASSERT(is8ByteAligned(block->m_offset));
    *outBlock = block;
    return true;
}

void CopiedSpace::freeAllBlocks()
{
    while (!m_toSpace->isEmpty())
        m_heap->blockAllocator().deallocate(m_toSpace->removeHead());

    while (!m_fromSpace->isEmpty())
        m_heap->blockAllocator().deallocate(m_fromSpace->removeHead());

    while (!m_oversizeBlocks.isEmpty())
        m_oversizeBlocks.removeHead()->m_allocation.deallocate();
}

size_t CopiedSpace::size()
{
    size_t calculatedSize = 0;

    for (CopiedBlock* block = static_cast<CopiedBlock*>(m_toSpace->head()); block; block = static_cast<CopiedBlock*>(block->next()))
        calculatedSize += block->size();

    for (CopiedBlock* block = static_cast<CopiedBlock*>(m_fromSpace->head()); block; block = static_cast<CopiedBlock*>(block->next()))
        calculatedSize += block->size();

    for (CopiedBlock* block = static_cast<CopiedBlock*>(m_oversizeBlocks.head()); block; block = static_cast<CopiedBlock*>(block->next()))
        calculatedSize += block->size();

    return calculatedSize;
}

size_t CopiedSpace::capacity()
{
    size_t calculatedCapacity = 0;

    for (CopiedBlock* block = static_cast<CopiedBlock*>(m_toSpace->head()); block; block = static_cast<CopiedBlock*>(block->next()))
        calculatedCapacity += block->capacity();

    for (CopiedBlock* block = static_cast<CopiedBlock*>(m_fromSpace->head()); block; block = static_cast<CopiedBlock*>(block->next()))
        calculatedCapacity += block->capacity();

    for (CopiedBlock* block = static_cast<CopiedBlock*>(m_oversizeBlocks.head()); block; block = static_cast<CopiedBlock*>(block->next()))
        calculatedCapacity += block->capacity();

    return calculatedCapacity;
}

static bool isBlockListPagedOut(double deadline, DoublyLinkedList<HeapBlock>* list)
{
    unsigned itersSinceLastTimeCheck = 0;
    HeapBlock* current = list->head();
    while (current) {
        current = current->next();
        ++itersSinceLastTimeCheck;
        if (itersSinceLastTimeCheck >= Heap::s_timeCheckResolution) {
            double currentTime = WTF::monotonicallyIncreasingTime();
            if (currentTime > deadline)
                return true;
            itersSinceLastTimeCheck = 0;
        }
    }

    return false;
}

bool CopiedSpace::isPagedOut(double deadline)
{
    return isBlockListPagedOut(deadline, m_toSpace) 
        || isBlockListPagedOut(deadline, m_fromSpace) 
        || isBlockListPagedOut(deadline, &m_oversizeBlocks);
}

} // namespace JSC
