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

#ifndef CopiedSpaceInlines_h
#define CopiedSpaceInlines_h

#include "CopiedBlock.h"
#include "CopiedSpace.h"
#include "Heap.h"
#include "HeapBlock.h"
#include "VM.h"
#include <wtf/CheckedBoolean.h>

namespace JSC {

inline bool CopiedSpace::contains(CopiedBlock* block)
{
    return (!m_newGen.blockFilter.ruleOut(reinterpret_cast<Bits>(block)) || !m_oldGen.blockFilter.ruleOut(reinterpret_cast<Bits>(block)))
        && m_blockSet.contains(block);
}

inline bool CopiedSpace::contains(void* ptr, CopiedBlock*& result)
{
    CopiedBlock* block = blockFor(ptr);
    if (contains(block)) {
        result = block;
        return true;
    }
    result = 0;
    return false;
}

inline void CopiedSpace::pin(CopiedBlock* block)
{
    block->pin();
}

inline void CopiedSpace::pinIfNecessary(void* opaquePointer)
{
    // Pointers into the copied space come in the following varieties:
    // 1)  Pointers to the start of a span of memory. This is the most
    //     natural though not necessarily the most common.
    // 2)  Pointers to one value-sized (8 byte) word past the end of
    //     a span of memory. This currently occurs with semi-butterflies
    //     and should be fixed soon, once the other half of the
    //     butterfly lands.
    // 3)  Pointers to the innards arising from loop induction variable
    //     optimizations (either manual ones or automatic, by the
    //     compiler).
    // 4)  Pointers to the end of a span of memory in arising from
    //     induction variable optimizations combined with the
    //     GC-to-compiler contract laid out in the C spec: a pointer to
    //     the end of a span of memory must be considered to be a
    //     pointer to that memory.
    
    EncodedJSValue* pointer = reinterpret_cast<EncodedJSValue*>(opaquePointer);
    CopiedBlock* block;

    // Handle (1) and (3).
    if (contains(pointer, block))
        pin(block);
    
    // Handle (4). We don't have to explicitly check and pin the block under this
    // pointer because it cannot possibly point to something that cases (1) and
    // (3) above or case (2) below wouldn't already catch.
    pointer--;
    
    // Handle (2)
    pointer--;
    if (contains(pointer, block))
        pin(block);
}

inline void CopiedSpace::recycleEvacuatedBlock(CopiedBlock* block, HeapOperation collectionType)
{
    ASSERT(block);
    ASSERT(block->canBeRecycled());
    ASSERT(!block->m_isPinned);
    {
        SpinLockHolder locker(&m_toSpaceLock);
        m_blockSet.remove(block);
        if (collectionType == EdenCollection)
            m_newGen.fromSpace->remove(block);
        else
            m_oldGen.fromSpace->remove(block);
    }
    m_heap->blockAllocator().deallocate(CopiedBlock::destroy(block));
}

inline void CopiedSpace::recycleBorrowedBlock(CopiedBlock* block)
{
    m_heap->blockAllocator().deallocate(CopiedBlock::destroy(block));

    {
        MutexLocker locker(m_loanedBlocksLock);
        ASSERT(m_numberOfLoanedBlocks > 0);
        ASSERT(m_inCopyingPhase);
        m_numberOfLoanedBlocks--;
        if (!m_numberOfLoanedBlocks)
            m_loanedBlocksCondition.signal();
    }
}

inline CopiedBlock* CopiedSpace::allocateBlockForCopyingPhase()
{
    ASSERT(m_inCopyingPhase);
    CopiedBlock* block = CopiedBlock::createNoZeroFill(m_heap->blockAllocator().allocate<CopiedBlock>());

    {
        MutexLocker locker(m_loanedBlocksLock);
        m_numberOfLoanedBlocks++;
    }

    ASSERT(!block->dataSize());
    return block;
}

inline void CopiedSpace::allocateBlock()
{
    m_heap->collectIfNecessaryOrDefer();

    m_allocator.resetCurrentBlock();
    
    CopiedBlock* block = CopiedBlock::create(m_heap->blockAllocator().allocate<CopiedBlock>());
        
    m_newGen.toSpace->push(block);
    m_newGen.blockFilter.add(reinterpret_cast<Bits>(block));
    m_blockSet.add(block);
    m_allocator.setCurrentBlock(block);
}

inline CheckedBoolean CopiedSpace::tryAllocate(size_t bytes, void** outPtr)
{
    ASSERT(!m_heap->vm()->isInitializingObject());
    ASSERT(bytes);

    if (!m_allocator.tryAllocate(bytes, outPtr))
        return tryAllocateSlowCase(bytes, outPtr);
    
    ASSERT(*outPtr);
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

inline CopiedBlock* CopiedSpace::blockFor(void* ptr)
{
    return reinterpret_cast<CopiedBlock*>(reinterpret_cast<size_t>(ptr) & s_blockMask);
}

template <HeapOperation collectionType>
inline void CopiedSpace::startedCopying()
{
    DoublyLinkedList<CopiedBlock>* fromSpace;
    DoublyLinkedList<CopiedBlock>* oversizeBlocks;
    TinyBloomFilter* blockFilter;
    if (collectionType == FullCollection) {
        ASSERT(m_oldGen.fromSpace->isEmpty());
        ASSERT(m_newGen.fromSpace->isEmpty());

        m_oldGen.toSpace->append(*m_newGen.toSpace);
        m_oldGen.oversizeBlocks.append(m_newGen.oversizeBlocks);

        ASSERT(m_newGen.toSpace->isEmpty());
        ASSERT(m_newGen.fromSpace->isEmpty());
        ASSERT(m_newGen.oversizeBlocks.isEmpty());

        std::swap(m_oldGen.fromSpace, m_oldGen.toSpace);
        fromSpace = m_oldGen.fromSpace;
        oversizeBlocks = &m_oldGen.oversizeBlocks;
        blockFilter = &m_oldGen.blockFilter;
    } else {
        std::swap(m_newGen.fromSpace, m_newGen.toSpace);
        fromSpace = m_newGen.fromSpace;
        oversizeBlocks = &m_newGen.oversizeBlocks;
        blockFilter = &m_newGen.blockFilter;
    }

    blockFilter->reset();
    m_allocator.resetCurrentBlock();

    CopiedBlock* next = 0;
    size_t totalLiveBytes = 0;
    size_t totalUsableBytes = 0;
    for (CopiedBlock* block = fromSpace->head(); block; block = next) {
        next = block->next();
        if (!block->isPinned() && block->canBeRecycled()) {
            recycleEvacuatedBlock(block, collectionType);
            continue;
        }
        ASSERT(block->liveBytes() <= CopiedBlock::blockSize);
        totalLiveBytes += block->liveBytes();
        totalUsableBytes += block->payloadCapacity();
        block->didPromote();
    }

    CopiedBlock* block = oversizeBlocks->head();
    while (block) {
        CopiedBlock* next = block->next();
        if (block->isPinned()) {
            blockFilter->add(reinterpret_cast<Bits>(block));
            totalLiveBytes += block->payloadCapacity();
            totalUsableBytes += block->payloadCapacity();
            block->didPromote();
        } else {
            oversizeBlocks->remove(block);
            m_blockSet.remove(block);
            m_heap->blockAllocator().deallocateCustomSize(CopiedBlock::destroy(block));
        } 
        block = next;
    }

    double markedSpaceBytes = m_heap->objectSpace().capacity();
    double totalFragmentation = static_cast<double>(totalLiveBytes + markedSpaceBytes) / static_cast<double>(totalUsableBytes + markedSpaceBytes);
    m_shouldDoCopyPhase = m_heap->operationInProgress() == EdenCollection || totalFragmentation <= Options::minHeapUtilization();
    if (!m_shouldDoCopyPhase) {
        if (Options::logGC())
            dataLog("Skipped copying, ");
        return;
    }

    if (Options::logGC())
        dataLogF("Did copy, ");
    ASSERT(m_shouldDoCopyPhase);
    ASSERT(!m_numberOfLoanedBlocks);
    ASSERT(!m_inCopyingPhase);
    m_inCopyingPhase = true;
}

} // namespace JSC

#endif // CopiedSpaceInlines_h

