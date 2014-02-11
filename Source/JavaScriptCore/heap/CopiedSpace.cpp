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

#include "CopiedSpaceInlines.h"
#include "GCActivityCallback.h"
#include "JSCInlines.h"
#include "Options.h"

namespace JSC {

CopiedSpace::CopiedSpace(Heap* heap)
    : m_heap(heap)
    , m_inCopyingPhase(false)
    , m_shouldDoCopyPhase(false)
    , m_numberOfLoanedBlocks(0)
{
    m_toSpaceLock.Init();
}

CopiedSpace::~CopiedSpace()
{
    while (!m_oldGen.toSpace->isEmpty())
        m_heap->blockAllocator().deallocate(CopiedBlock::destroy(m_oldGen.toSpace->removeHead()));

    while (!m_oldGen.fromSpace->isEmpty())
        m_heap->blockAllocator().deallocate(CopiedBlock::destroy(m_oldGen.fromSpace->removeHead()));

    while (!m_oldGen.oversizeBlocks.isEmpty())
        m_heap->blockAllocator().deallocateCustomSize(CopiedBlock::destroy(m_oldGen.oversizeBlocks.removeHead()));

    while (!m_newGen.toSpace->isEmpty())
        m_heap->blockAllocator().deallocate(CopiedBlock::destroy(m_newGen.toSpace->removeHead()));

    while (!m_newGen.fromSpace->isEmpty())
        m_heap->blockAllocator().deallocate(CopiedBlock::destroy(m_newGen.fromSpace->removeHead()));

    while (!m_newGen.oversizeBlocks.isEmpty())
        m_heap->blockAllocator().deallocateCustomSize(CopiedBlock::destroy(m_newGen.oversizeBlocks.removeHead()));

    ASSERT(m_oldGen.toSpace->isEmpty());
    ASSERT(m_oldGen.fromSpace->isEmpty());
    ASSERT(m_oldGen.oversizeBlocks.isEmpty());
    ASSERT(m_newGen.toSpace->isEmpty());
    ASSERT(m_newGen.fromSpace->isEmpty());
    ASSERT(m_newGen.oversizeBlocks.isEmpty());
}

void CopiedSpace::init()
{
    m_oldGen.toSpace = &m_oldGen.blocks1;
    m_oldGen.fromSpace = &m_oldGen.blocks2;
    
    m_newGen.toSpace = &m_newGen.blocks1;
    m_newGen.fromSpace = &m_newGen.blocks2;

    allocateBlock();
}   

CheckedBoolean CopiedSpace::tryAllocateSlowCase(size_t bytes, void** outPtr)
{
    if (isOversize(bytes))
        return tryAllocateOversize(bytes, outPtr);
    
    ASSERT(m_heap->vm()->currentThreadIsHoldingAPILock());
    m_heap->didAllocate(m_allocator.currentCapacity());

    allocateBlock();

    *outPtr = m_allocator.forceAllocate(bytes);
    return true;
}

CheckedBoolean CopiedSpace::tryAllocateOversize(size_t bytes, void** outPtr)
{
    ASSERT(isOversize(bytes));
    
    CopiedBlock* block = CopiedBlock::create(m_heap->blockAllocator().allocateCustomSize(sizeof(CopiedBlock) + bytes, CopiedBlock::blockSize));
    m_newGen.oversizeBlocks.push(block);
    m_newGen.blockFilter.add(reinterpret_cast<Bits>(block));
    m_blockSet.add(block);
    ASSERT(!block->isOld());
    
    CopiedAllocator allocator;
    allocator.setCurrentBlock(block);
    *outPtr = allocator.forceAllocate(bytes);
    allocator.resetCurrentBlock();

    m_heap->didAllocate(block->region()->blockSize());

    return true;
}

CheckedBoolean CopiedSpace::tryReallocate(void** ptr, size_t oldSize, size_t newSize)
{
    if (oldSize >= newSize)
        return true;
    
    void* oldPtr = *ptr;
    ASSERT(!m_heap->vm()->isInitializingObject());
    
    if (CopiedSpace::blockFor(oldPtr)->isOversize() || isOversize(newSize))
        return tryReallocateOversize(ptr, oldSize, newSize);
    
    if (m_allocator.tryReallocate(oldPtr, oldSize, newSize))
        return true;

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

    CopiedBlock* oldBlock = CopiedSpace::blockFor(oldPtr);
    if (oldBlock->isOversize()) {
        if (oldBlock->isOld())
            m_oldGen.oversizeBlocks.remove(oldBlock);
        else
            m_newGen.oversizeBlocks.remove(oldBlock);
        m_blockSet.remove(oldBlock);
        m_heap->blockAllocator().deallocateCustomSize(CopiedBlock::destroy(oldBlock));
    }
    
    *ptr = newPtr;
    return true;
}

void CopiedSpace::doneFillingBlock(CopiedBlock* block, CopiedBlock** exchange)
{
    ASSERT(m_inCopyingPhase);
    
    if (exchange)
        *exchange = allocateBlockForCopyingPhase();

    if (!block)
        return;

    if (!block->dataSize()) {
        recycleBorrowedBlock(block);
        return;
    }

    block->zeroFillWilderness();

    {
        // Always put the block into the old gen because it's being promoted!
        SpinLockHolder locker(&m_toSpaceLock);
        m_oldGen.toSpace->push(block);
        m_blockSet.add(block);
        m_oldGen.blockFilter.add(reinterpret_cast<Bits>(block));
    }

    {
        MutexLocker locker(m_loanedBlocksLock);
        ASSERT(m_numberOfLoanedBlocks > 0);
        ASSERT(m_inCopyingPhase);
        m_numberOfLoanedBlocks--;
        if (!m_numberOfLoanedBlocks)
            m_loanedBlocksCondition.signal();
    }
}

void CopiedSpace::didStartFullCollection()
{
    ASSERT(heap()->operationInProgress() == FullCollection);
    ASSERT(m_oldGen.fromSpace->isEmpty());
    ASSERT(m_newGen.fromSpace->isEmpty());

#ifndef NDEBUG
    for (CopiedBlock* block = m_newGen.toSpace->head(); block; block = block->next())
        ASSERT(!block->liveBytes());

    for (CopiedBlock* block = m_newGen.oversizeBlocks.head(); block; block = block->next())
        ASSERT(!block->liveBytes());
#endif

    for (CopiedBlock* block = m_oldGen.toSpace->head(); block; block = block->next())
        block->didSurviveGC();

    for (CopiedBlock* block = m_oldGen.oversizeBlocks.head(); block; block = block->next())
        block->didSurviveGC();
}

void CopiedSpace::doneCopying()
{
    {
        MutexLocker locker(m_loanedBlocksLock);
        while (m_numberOfLoanedBlocks > 0)
            m_loanedBlocksCondition.wait(m_loanedBlocksLock);
    }

    ASSERT(m_inCopyingPhase == m_shouldDoCopyPhase);
    m_inCopyingPhase = false;

    DoublyLinkedList<CopiedBlock>* toSpace;
    DoublyLinkedList<CopiedBlock>* fromSpace;
    TinyBloomFilter* blockFilter;
    if (heap()->operationInProgress() == FullCollection) {
        toSpace = m_oldGen.toSpace;
        fromSpace = m_oldGen.fromSpace;
        blockFilter = &m_oldGen.blockFilter;
    } else {
        toSpace = m_newGen.toSpace;
        fromSpace = m_newGen.fromSpace;
        blockFilter = &m_newGen.blockFilter;
    }

    while (!fromSpace->isEmpty()) {
        CopiedBlock* block = fromSpace->removeHead();
        // We don't add the block to the blockSet because it was never removed.
        ASSERT(m_blockSet.contains(block));
        blockFilter->add(reinterpret_cast<Bits>(block));
        toSpace->push(block);
    }

    if (heap()->operationInProgress() == EdenCollection) {
        m_oldGen.toSpace->append(*m_newGen.toSpace);
        m_oldGen.oversizeBlocks.append(m_newGen.oversizeBlocks);
        m_oldGen.blockFilter.add(m_newGen.blockFilter);
        m_newGen.blockFilter.reset();
    }

    ASSERT(m_newGen.toSpace->isEmpty());
    ASSERT(m_newGen.fromSpace->isEmpty());
    ASSERT(m_newGen.oversizeBlocks.isEmpty());

    allocateBlock();

    m_shouldDoCopyPhase = false;
}

size_t CopiedSpace::size()
{
    size_t calculatedSize = 0;

    for (CopiedBlock* block = m_oldGen.toSpace->head(); block; block = block->next())
        calculatedSize += block->size();

    for (CopiedBlock* block = m_oldGen.fromSpace->head(); block; block = block->next())
        calculatedSize += block->size();

    for (CopiedBlock* block = m_oldGen.oversizeBlocks.head(); block; block = block->next())
        calculatedSize += block->size();

    for (CopiedBlock* block = m_newGen.toSpace->head(); block; block = block->next())
        calculatedSize += block->size();

    for (CopiedBlock* block = m_newGen.fromSpace->head(); block; block = block->next())
        calculatedSize += block->size();

    for (CopiedBlock* block = m_newGen.oversizeBlocks.head(); block; block = block->next())
        calculatedSize += block->size();

    return calculatedSize;
}

size_t CopiedSpace::capacity()
{
    size_t calculatedCapacity = 0;

    for (CopiedBlock* block = m_oldGen.toSpace->head(); block; block = block->next())
        calculatedCapacity += block->capacity();

    for (CopiedBlock* block = m_oldGen.fromSpace->head(); block; block = block->next())
        calculatedCapacity += block->capacity();

    for (CopiedBlock* block = m_oldGen.oversizeBlocks.head(); block; block = block->next())
        calculatedCapacity += block->capacity();

    for (CopiedBlock* block = m_newGen.toSpace->head(); block; block = block->next())
        calculatedCapacity += block->capacity();

    for (CopiedBlock* block = m_newGen.fromSpace->head(); block; block = block->next())
        calculatedCapacity += block->capacity();

    for (CopiedBlock* block = m_newGen.oversizeBlocks.head(); block; block = block->next())
        calculatedCapacity += block->capacity();

    return calculatedCapacity;
}

static bool isBlockListPagedOut(double deadline, DoublyLinkedList<CopiedBlock>* list)
{
    unsigned itersSinceLastTimeCheck = 0;
    CopiedBlock* current = list->head();
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
    return isBlockListPagedOut(deadline, m_oldGen.toSpace) 
        || isBlockListPagedOut(deadline, m_oldGen.fromSpace) 
        || isBlockListPagedOut(deadline, &m_oldGen.oversizeBlocks)
        || isBlockListPagedOut(deadline, m_newGen.toSpace) 
        || isBlockListPagedOut(deadline, m_newGen.fromSpace) 
        || isBlockListPagedOut(deadline, &m_newGen.oversizeBlocks);
}

} // namespace JSC
