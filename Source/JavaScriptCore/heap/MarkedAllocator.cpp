/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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
#include "MarkedAllocator.h"

#include "DelayedReleaseScope.h"
#include "GCActivityCallback.h"
#include "Heap.h"
#include "IncrementalSweeper.h"
#include "JSCInlines.h"
#include "VM.h"
#include <wtf/CurrentTime.h>

namespace JSC {

static bool isListPagedOut(double deadline, DoublyLinkedList<MarkedBlock>& list)
{
    unsigned itersSinceLastTimeCheck = 0;
    MarkedBlock* block = list.head();
    while (block) {
        block = block->next();
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

bool MarkedAllocator::isPagedOut(double deadline)
{
    if (isListPagedOut(deadline, m_blockList))
        return true;
    return false;
}

inline void* MarkedAllocator::tryAllocateHelper(size_t bytes)
{
    // We need a while loop to check the free list because the DelayedReleaseScope 
    // could cause arbitrary code to execute and exhaust the free list that we 
    // thought had elements in it.
    while (!m_freeList.head) {
        DelayedReleaseScope delayedReleaseScope(*m_markedSpace);
        if (m_currentBlock) {
            ASSERT(m_currentBlock == m_nextBlockToSweep);
            m_currentBlock->didConsumeFreeList();
            m_nextBlockToSweep = m_currentBlock->next();
        }

        MarkedBlock* next;
        for (MarkedBlock*& block = m_nextBlockToSweep; block; block = next) {
            next = block->next();

            MarkedBlock::FreeList freeList = block->sweep(MarkedBlock::SweepToFreeList);
            
            double utilization = ((double)MarkedBlock::blockSize - (double)freeList.bytes) / (double)MarkedBlock::blockSize;
            if (utilization >= Options::minMarkedBlockUtilization()) {
                ASSERT(freeList.bytes || !freeList.head);
                m_blockList.remove(block);
                m_retiredBlocks.push(block);
                block->didRetireBlock(freeList);
                continue;
            }

            if (bytes > block->cellSize()) {
                block->stopAllocating(freeList);
                continue;
            }

            m_currentBlock = block;
            m_freeList = freeList;
            break;
        }
        
        if (!m_freeList.head) {
            m_currentBlock = 0;
            return 0;
        }
    }

    ASSERT(m_freeList.head);
    void* head = tryPopFreeList(bytes);
    ASSERT(head);
    m_markedSpace->didAllocateInBlock(m_currentBlock);
    return head;
}

inline void* MarkedAllocator::tryPopFreeList(size_t bytes)
{
    ASSERT(m_currentBlock);
    if (bytes > m_currentBlock->cellSize())
        return 0;

    MarkedBlock::FreeCell* head = m_freeList.head;
    m_freeList.head = head->next;
    return head;
}

inline void* MarkedAllocator::tryAllocate(size_t bytes)
{
    ASSERT(!m_heap->isBusy());
    m_heap->m_operationInProgress = Allocation;
    void* result = tryAllocateHelper(bytes);

    // Due to the DelayedReleaseScope in tryAllocateHelper, some other thread might have
    // created a new block after we thought we didn't find any free cells. 
    while (!result && m_currentBlock) {
        // A new block was added by another thread so try popping the free list.
        result = tryPopFreeList(bytes);
        if (result)
            break;
        // The free list was empty, so call tryAllocateHelper to do the normal sweeping stuff.
        result = tryAllocateHelper(bytes);
    }

    m_heap->m_operationInProgress = NoOperation;
    ASSERT(result || !m_currentBlock);
    return result;
}
    
void* MarkedAllocator::allocateSlowCase(size_t bytes)
{
    ASSERT(m_heap->vm()->currentThreadIsHoldingAPILock());
#if COLLECT_ON_EVERY_ALLOCATION
    if (!m_heap->isDeferred())
        m_heap->collectAllGarbage();
    ASSERT(m_heap->m_operationInProgress == NoOperation);
#endif
    
    ASSERT(!m_markedSpace->isIterating());
    ASSERT(!m_freeList.head);
    m_heap->didAllocate(m_freeList.bytes);
    
    void* result = tryAllocate(bytes);
    
    if (LIKELY(result != 0))
        return result;
    
    if (m_heap->collectIfNecessaryOrDefer()) {
        result = tryAllocate(bytes);
        if (result)
            return result;
    }

    ASSERT(!m_heap->shouldCollect());
    
    MarkedBlock* block = allocateBlock(bytes);
    ASSERT(block);
    addBlock(block);
        
    result = tryAllocate(bytes);
    ASSERT(result);
    return result;
}

MarkedBlock* MarkedAllocator::allocateBlock(size_t bytes)
{
    size_t minBlockSize = MarkedBlock::blockSize;
    size_t minAllocationSize = WTF::roundUpToMultipleOf(WTF::pageSize(), sizeof(MarkedBlock) + bytes);
    size_t blockSize = std::max(minBlockSize, minAllocationSize);

    size_t cellSize = m_cellSize ? m_cellSize : WTF::roundUpToMultipleOf<MarkedBlock::atomSize>(bytes);

    if (blockSize == MarkedBlock::blockSize)
        return MarkedBlock::create(m_heap->blockAllocator().allocate<MarkedBlock>(), this, cellSize, m_destructorType);
    return MarkedBlock::create(m_heap->blockAllocator().allocateCustomSize(blockSize, MarkedBlock::blockSize), this, cellSize, m_destructorType);
}

void MarkedAllocator::addBlock(MarkedBlock* block)
{
    ASSERT(!m_currentBlock);
    ASSERT(!m_freeList.head);
    
    m_blockList.append(block);
    m_nextBlockToSweep = block;
    m_markedSpace->didAddBlock(block);
}

void MarkedAllocator::removeBlock(MarkedBlock* block)
{
    if (m_currentBlock == block) {
        m_currentBlock = m_currentBlock->next();
        m_freeList = MarkedBlock::FreeList();
    }
    if (m_nextBlockToSweep == block)
        m_nextBlockToSweep = m_nextBlockToSweep->next();

    block->willRemoveBlock();
    m_blockList.remove(block);
}

void MarkedAllocator::reset()
{
    m_lastActiveBlock = 0;
    m_currentBlock = 0;
    m_freeList = MarkedBlock::FreeList();
    if (m_heap->operationInProgress() == FullCollection)
        m_blockList.append(m_retiredBlocks);

    m_nextBlockToSweep = m_blockList.head();
}

struct LastChanceToFinalize : MarkedBlock::VoidFunctor {
    void operator()(MarkedBlock* block) { block->lastChanceToFinalize(); }
};

void MarkedAllocator::lastChanceToFinalize()
{
    m_blockList.append(m_retiredBlocks);
    LastChanceToFinalize functor;
    forEachBlock(functor);
}

} // namespace JSC
