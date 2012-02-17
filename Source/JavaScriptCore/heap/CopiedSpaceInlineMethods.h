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

#ifndef CopiedSpaceInlineMethods_h
#define CopiedSpaceInlineMethods_h

#include "CopiedBlock.h"
#include "CopiedSpace.h"
#include "Heap.h"
#include "HeapBlock.h"
#include "JSGlobalData.h"
#include <wtf/CheckedBoolean.h>

namespace JSC {

inline CopiedSpace::CopiedSpace(Heap* heap)
    : m_heap(heap)
    , m_currentBlock(0)
    , m_toSpace(0)
    , m_fromSpace(0)
    , m_totalMemoryAllocated(0)
    , m_totalMemoryUtilized(0)
    , m_inCopyingPhase(false)
    , m_numberOfLoanedBlocks(0)
{
}

inline void CopiedSpace::init()
{
    m_toSpace = &m_blocks1;
    m_fromSpace = &m_blocks2;
    
    m_totalMemoryAllocated += s_blockSize * s_initialBlockNum;

    if (!addNewBlock())
        CRASH();
}   

inline bool CopiedSpace::contains(void* ptr, CopiedBlock*& result)
{
    CopiedBlock* block = blockFor(ptr);
    result = block;
    return !m_toSpaceFilter.ruleOut(reinterpret_cast<Bits>(block)) && m_toSpaceSet.contains(block);
}

inline void CopiedSpace::pin(CopiedBlock* block)
{
    block->m_isPinned = true;
}

inline void CopiedSpace::startedCopying()
{
    DoublyLinkedList<HeapBlock>* temp = m_fromSpace;
    m_fromSpace = m_toSpace;
    m_toSpace = temp;

    m_toSpaceFilter.reset();

    m_totalMemoryUtilized = 0;

    ASSERT(!m_inCopyingPhase);
    ASSERT(!m_numberOfLoanedBlocks);
    m_inCopyingPhase = true;
}

inline void CopiedSpace::doneCopying()
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
        {
            MutexLocker locker(m_heap->m_freeBlockLock);
            m_heap->m_freeBlocks.push(block);
            m_heap->m_numberOfFreeBlocks++;
        }
    }

    CopiedBlock* curr = static_cast<CopiedBlock*>(m_oversizeBlocks.head());
    while (curr) {
        CopiedBlock* next = static_cast<CopiedBlock*>(curr->next());
        if (!curr->m_isPinned) {
            m_oversizeBlocks.remove(curr);
            m_totalMemoryAllocated -= curr->m_allocation.size();
            m_totalMemoryUtilized -= curr->m_allocation.size() - sizeof(CopiedBlock);
            curr->m_allocation.deallocate();
        } else
            curr->m_isPinned = false;
        curr = next;
    }

    if (!(m_currentBlock = static_cast<CopiedBlock*>(m_toSpace->head())))
        if (!addNewBlock())
            CRASH();
}

inline void CopiedSpace::doneFillingBlock(CopiedBlock* block)
{
    ASSERT(block);
    ASSERT(block->m_offset < reinterpret_cast<char*>(block) + s_blockSize);
    ASSERT(m_inCopyingPhase);

    if (block->m_offset == block->m_payload) {
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
        MutexLocker locker(m_memoryStatsLock);
        m_totalMemoryUtilized += static_cast<size_t>(static_cast<char*>(block->m_offset) - block->m_payload);
    }

    {
        MutexLocker locker(m_loanedBlocksLock);
        ASSERT(m_numberOfLoanedBlocks > 0);
        m_numberOfLoanedBlocks--;
        if (!m_numberOfLoanedBlocks)
            m_loanedBlocksCondition.signal();
    }
}

inline void CopiedSpace::recycleBlock(CopiedBlock* block)
{
    {
        MutexLocker locker(m_heap->m_freeBlockLock);
        m_heap->m_freeBlocks.push(block);
        m_heap->m_numberOfFreeBlocks++;
    }

    {
        MutexLocker locker(m_loanedBlocksLock);
        ASSERT(m_numberOfLoanedBlocks > 0);
        m_numberOfLoanedBlocks--;
        if (!m_numberOfLoanedBlocks)
            m_loanedBlocksCondition.signal();
    }
}

inline CheckedBoolean CopiedSpace::getFreshBlock(AllocationEffort allocationEffort, CopiedBlock** outBlock)
{
    HeapBlock* heapBlock = 0;
    CopiedBlock* block = 0;
    {
        MutexLocker locker(m_heap->m_freeBlockLock);
        if (!m_heap->m_freeBlocks.isEmpty()) {
            heapBlock = m_heap->m_freeBlocks.removeHead();
            m_heap->m_numberOfFreeBlocks--;
        }
    }
    if (heapBlock)
        block = new (NotNull, heapBlock) CopiedBlock(heapBlock->m_allocation);
    else if (allocationEffort == AllocationMustSucceed) {
        if (!allocateNewBlock(&block)) {
            *outBlock = 0;
            ASSERT_NOT_REACHED();
            return false;
        }
    } else {
        ASSERT(allocationEffort == AllocationCanFail);
        if (m_heap->waterMark() >= m_heap->highWaterMark() && m_heap->m_isSafeToCollect)
            m_heap->collect(Heap::DoNotSweep);
        
        if (!getFreshBlock(AllocationMustSucceed, &block)) {
            *outBlock = 0;
            ASSERT_NOT_REACHED();
            return false;
        }
    }
    ASSERT(block);
    ASSERT(isPointerAligned(block->m_offset));
    *outBlock = block;
    return true;
}

inline CheckedBoolean CopiedSpace::borrowBlock(CopiedBlock** outBlock)
{
    CopiedBlock* block = 0;
    if (!getFreshBlock(AllocationMustSucceed, &block)) {
        *outBlock = 0;
        return false;
    }

    ASSERT(m_inCopyingPhase);
    MutexLocker locker(m_loanedBlocksLock);
    m_numberOfLoanedBlocks++;

    ASSERT(block->m_offset == block->m_payload);
    *outBlock = block;
    return true;
}

inline CheckedBoolean CopiedSpace::addNewBlock()
{
    CopiedBlock* block = 0;
    if (!getFreshBlock(AllocationCanFail, &block))
        return false;
        
    m_toSpace->push(block);
    m_currentBlock = block;
    return true;
}

inline CheckedBoolean CopiedSpace::allocateNewBlock(CopiedBlock** outBlock)
{
    PageAllocationAligned allocation = PageAllocationAligned::allocate(s_blockSize, s_blockSize, OSAllocator::JSGCHeapPages);
    if (!static_cast<bool>(allocation)) {
        *outBlock = 0;
        return false;
    }

    {
        MutexLocker locker(m_memoryStatsLock);
        m_totalMemoryAllocated += s_blockSize;
    }

    *outBlock = new (NotNull, allocation.base()) CopiedBlock(allocation);
    return true;
}

inline bool CopiedSpace::fitsInBlock(CopiedBlock* block, size_t bytes)
{
    return static_cast<char*>(block->m_offset) + bytes < reinterpret_cast<char*>(block) + s_blockSize && static_cast<char*>(block->m_offset) + bytes > block->m_offset;
}

inline bool CopiedSpace::fitsInCurrentBlock(size_t bytes)
{
    return fitsInBlock(m_currentBlock, bytes);
}

inline CheckedBoolean CopiedSpace::tryAllocate(size_t bytes, void** outPtr)
{
    ASSERT(!m_heap->globalData()->isInitializingObject());

    if (isOversize(bytes) || !fitsInCurrentBlock(bytes))
        return tryAllocateSlowCase(bytes, outPtr);
    
    *outPtr = allocateFromBlock(m_currentBlock, bytes);
    return true;
}

inline CheckedBoolean CopiedSpace::tryAllocateOversize(size_t bytes, void** outPtr)
{
    ASSERT(isOversize(bytes));
    
    size_t blockSize = WTF::roundUpToMultipleOf<s_pageSize>(sizeof(CopiedBlock) + bytes);
    PageAllocationAligned allocation = PageAllocationAligned::allocate(blockSize, s_pageSize, OSAllocator::JSGCHeapPages);
    if (!static_cast<bool>(allocation)) {
        *outPtr = 0;
        return false;
    }
    CopiedBlock* block = new (NotNull, allocation.base()) CopiedBlock(allocation);
    m_oversizeBlocks.push(block);
    ASSERT(isPointerAligned(block->m_offset));

    m_oversizeFilter.add(reinterpret_cast<Bits>(block));
    
    m_totalMemoryAllocated += blockSize;
    m_totalMemoryUtilized += bytes;

    *outPtr = block->m_offset;
    return true;
}

inline void* CopiedSpace::allocateFromBlock(CopiedBlock* block, size_t bytes)
{
    ASSERT(!isOversize(bytes));
    ASSERT(fitsInBlock(block, bytes));
    ASSERT(isPointerAligned(block->m_offset));
    
    void* ptr = block->m_offset;
    ASSERT(block->m_offset >= block->m_payload && block->m_offset < reinterpret_cast<char*>(block) + s_blockSize);
    block->m_offset = static_cast<void*>((static_cast<char*>(ptr) + bytes));
    ASSERT(block->m_offset >= block->m_payload && block->m_offset < reinterpret_cast<char*>(block) + s_blockSize);

    ASSERT(isPointerAligned(ptr));
    return ptr;
}

inline CheckedBoolean CopiedSpace::tryReallocate(void** ptr, size_t oldSize, size_t newSize)
{
    if (oldSize >= newSize)
        return true;
    
    void* oldPtr = *ptr;
    ASSERT(!m_heap->globalData()->isInitializingObject());

    if (isOversize(oldSize) || isOversize(newSize))
        return tryReallocateOversize(ptr, oldSize, newSize);

    if (static_cast<char*>(oldPtr) + oldSize == m_currentBlock->m_offset && oldPtr > m_currentBlock && oldPtr < reinterpret_cast<char*>(m_currentBlock) + s_blockSize) {
        m_currentBlock->m_offset = oldPtr;
        if (fitsInCurrentBlock(newSize)) {
            m_totalMemoryUtilized += newSize - oldSize;
            return allocateFromBlock(m_currentBlock, newSize);
        }
    }
    m_totalMemoryUtilized -= oldSize;

    void* result = 0;
    if (!tryAllocate(newSize, &result)) {
        *ptr = 0;
        return false;
    }
    memcpy(result, oldPtr, oldSize);
    *ptr = result;
    return true;
}

inline CheckedBoolean CopiedSpace::tryReallocateOversize(void** ptr, size_t oldSize, size_t newSize)
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
        m_totalMemoryAllocated -= oldSize + sizeof(CopiedBlock);
    }
    
    m_totalMemoryUtilized -= oldSize;

    *ptr = newPtr;
    return true;
}

inline bool CopiedSpace::isOversize(size_t bytes)
{
    return bytes > s_maxAllocationSize;
}

inline bool CopiedSpace::isPinned(void* ptr)
{
    return blockFor(ptr)->m_isPinned;
}

inline CopiedBlock* CopiedSpace::oversizeBlockFor(void* ptr)
{
    return reinterpret_cast<CopiedBlock*>(reinterpret_cast<size_t>(ptr) & s_pageMask);
}

inline CopiedBlock* CopiedSpace::blockFor(void* ptr)
{
    return reinterpret_cast<CopiedBlock*>(reinterpret_cast<size_t>(ptr) & s_blockMask);
}

} // namespace JSC

#endif
