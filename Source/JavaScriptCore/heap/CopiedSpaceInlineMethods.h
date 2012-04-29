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
    m_allocator.startedCopying();

    ASSERT(!m_inCopyingPhase);
    ASSERT(!m_numberOfLoanedBlocks);
    m_inCopyingPhase = true;
}

inline void CopiedSpace::recycleBlock(CopiedBlock* block)
{
    m_heap->blockAllocator().deallocate(block);

    {
        MutexLocker locker(m_loanedBlocksLock);
        ASSERT(m_numberOfLoanedBlocks > 0);
        m_numberOfLoanedBlocks--;
        if (!m_numberOfLoanedBlocks)
            m_loanedBlocksCondition.signal();
    }
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

    ASSERT(block->m_offset == block->payload());
    *outBlock = block;
    return true;
}

inline CheckedBoolean CopiedSpace::addNewBlock()
{
    CopiedBlock* block = 0;
    if (!getFreshBlock(AllocationCanFail, &block))
        return false;
        
    m_toSpace->push(block);
    m_toSpaceFilter.add(reinterpret_cast<Bits>(block));
    m_toSpaceSet.add(block);
    m_allocator.resetCurrentBlock(block);
    return true;
}

inline CheckedBoolean CopiedSpace::allocateNewBlock(CopiedBlock** outBlock)
{
    PageAllocationAligned allocation = PageAllocationAligned::allocate(HeapBlock::s_blockSize, HeapBlock::s_blockSize, OSAllocator::JSGCHeapPages);
    if (!static_cast<bool>(allocation)) {
        *outBlock = 0;
        return false;
    }

    *outBlock = new (NotNull, allocation.base()) CopiedBlock(allocation);
    return true;
}

inline bool CopiedSpace::fitsInBlock(CopiedBlock* block, size_t bytes)
{
    return static_cast<char*>(block->m_offset) + bytes < reinterpret_cast<char*>(block) + block->capacity() && static_cast<char*>(block->m_offset) + bytes > block->m_offset;
}

inline CheckedBoolean CopiedSpace::tryAllocate(size_t bytes, void** outPtr)
{
    ASSERT(!m_heap->globalData()->isInitializingObject());

    if (isOversize(bytes) || !m_allocator.fitsInCurrentBlock(bytes))
        return tryAllocateSlowCase(bytes, outPtr);
    
    *outPtr = m_allocator.allocate(bytes);
    ASSERT(*outPtr);
    return true;
}

inline void* CopiedSpace::allocateFromBlock(CopiedBlock* block, size_t bytes)
{
    ASSERT(fitsInBlock(block, bytes));
    ASSERT(is8ByteAligned(block->m_offset));
    
    void* ptr = block->m_offset;
    ASSERT(block->m_offset >= block->payload() && block->m_offset < reinterpret_cast<char*>(block) + block->capacity());
    block->m_offset = static_cast<void*>((static_cast<char*>(ptr) + bytes));
    ASSERT(block->m_offset >= block->payload() && block->m_offset < reinterpret_cast<char*>(block) + block->capacity());

    ASSERT(is8ByteAligned(ptr));
    return ptr;
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
    return reinterpret_cast<CopiedBlock*>(reinterpret_cast<size_t>(ptr) & WTF::pageMask());
}

inline CopiedBlock* CopiedSpace::blockFor(void* ptr)
{
    return reinterpret_cast<CopiedBlock*>(reinterpret_cast<size_t>(ptr) & s_blockMask);
}

} // namespace JSC

#endif
