/*
 * Copyright (C) 2012, 2013, 2016 Apple Inc. All rights reserved.
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

#include "GCActivityCallback.h"
#include "Heap.h"
#include "IncrementalSweeper.h"
#include "JSCInlines.h"
#include "SuperSampler.h"
#include "VM.h"
#include <wtf/CurrentTime.h>

namespace JSC {

MarkedAllocator::MarkedAllocator(Heap* heap, MarkedSpace* markedSpace, size_t cellSize, const AllocatorAttributes& attributes)
    : m_currentBlock(0)
    , m_lastActiveBlock(0)
    , m_nextBlockToSweep(nullptr)
    , m_cellSize(static_cast<unsigned>(cellSize))
    , m_attributes(attributes)
    , m_heap(heap)
    , m_markedSpace(markedSpace)
{
}

bool MarkedAllocator::isPagedOut(double deadline)
{
    unsigned itersSinceLastTimeCheck = 0;
    MarkedBlock::Handle* block = m_blockList.begin();
    while (block) {
        block = filterNextBlock(block->next());
        if (block)
            block->flipIfNecessary(); // Forces us to touch the memory of the block, but has no semantic effect.
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

void MarkedAllocator::retire(MarkedBlock::Handle* block)
{
    LockHolder locker(m_lock); // This will be called in parallel during GC.
    if (block == m_currentBlock) {
        // This happens when the mutator is running. We finished a full GC and marked too few things
        // to retire. Then we started allocating in this block. Then a barrier ran, which marked an
        // object in this block, which put it over the retirement threshold. It's OK to simply do
        // nothing in that case.
        return;
    }
    if (block == m_lastActiveBlock) {
        // This can easily happen during marking. It would be easy to handle this case, but it's
        // just as easy to ignore it.
        return;
    }
    RELEASE_ASSERT(block->isOnList());
    if (block == m_nextBlockToSweep)
        m_nextBlockToSweep = filterNextBlock(block->next());
    block->remove();
    m_retiredBlocks.push(block);
}

MarkedBlock::Handle* MarkedAllocator::filterNextBlock(MarkedBlock::Handle* block)
{
    if (block == m_blockList.end())
        return nullptr;
    return block;
}

void MarkedAllocator::setNextBlockToSweep(MarkedBlock::Handle* block)
{
    m_nextBlockToSweep = filterNextBlock(block);
}

void* MarkedAllocator::tryAllocateWithoutCollectingImpl()
{
    SuperSamplerScope superSamplerScope(false);
    
    if (m_currentBlock) {
        ASSERT(m_currentBlock == m_nextBlockToSweep);
        m_currentBlock->didConsumeFreeList();
        setNextBlockToSweep(m_currentBlock->next());
    }
    
    setFreeList(FreeList());

    RELEASE_ASSERT(m_nextBlockToSweep != m_blockList.end());

    MarkedBlock::Handle* next;
    for (MarkedBlock::Handle*& block = m_nextBlockToSweep; block; block = next) {
        next = filterNextBlock(block->next());

        // It would be super weird if the blocks we are sweeping have anything allocated during this
        // cycle.
        ASSERT(!block->hasAnyNewlyAllocated());
        
        FreeList freeList = block->sweep(MarkedBlock::Handle::SweepToFreeList);
        
        // It's possible to stumble on a complete-full block. Marking tries to retire these, but
        // that algorithm is racy and may forget to do it sometimes.
        if (freeList.allocationWillFail()) {
            ASSERT(block->isFreeListed());
            block->unsweepWithNoNewlyAllocated();
            ASSERT(block->isMarked());
            retire(block);
            continue;
        }

        m_currentBlock = block;
        setFreeList(freeList);
        break;
    }
    
    if (!m_freeList) {
        m_currentBlock = 0;
        return 0;
    }

    void* result;
    if (m_freeList.remaining) {
        unsigned cellSize = m_cellSize;
        m_freeList.remaining -= cellSize;
        result = m_freeList.payloadEnd - m_freeList.remaining - cellSize;
    } else {
        FreeCell* head = m_freeList.head;
        m_freeList.head = head->next;
        result = head;
    }
    RELEASE_ASSERT(result);
    m_markedSpace->didAllocateInBlock(m_currentBlock);
    return result;
}

inline void* MarkedAllocator::tryAllocateWithoutCollecting()
{
    ASSERT(!m_heap->isBusy());
    m_heap->m_operationInProgress = Allocation;
    void* result = tryAllocateWithoutCollectingImpl();

    m_heap->m_operationInProgress = NoOperation;
    ASSERT(result || !m_currentBlock);
    return result;
}

ALWAYS_INLINE void MarkedAllocator::doTestCollectionsIfNeeded()
{
    if (!Options::slowPathAllocsBetweenGCs())
        return;

    static unsigned allocationCount = 0;
    if (!allocationCount) {
        if (!m_heap->isDeferred())
            m_heap->collectAllGarbage();
        ASSERT(m_heap->m_operationInProgress == NoOperation);
    }
    if (++allocationCount >= Options::slowPathAllocsBetweenGCs())
        allocationCount = 0;
}

void* MarkedAllocator::allocateSlowCase()
{
    bool crashOnFailure = true;
    return allocateSlowCaseImpl(crashOnFailure);
}

void* MarkedAllocator::tryAllocateSlowCase()
{
    bool crashOnFailure = false;
    return allocateSlowCaseImpl(crashOnFailure);
}

void* MarkedAllocator::allocateSlowCaseImpl(bool crashOnFailure)
{
    SuperSamplerScope superSamplerScope(false);
    ASSERT(m_heap->vm()->currentThreadIsHoldingAPILock());
    doTestCollectionsIfNeeded();

    ASSERT(!m_markedSpace->isIterating());
    m_heap->didAllocate(m_freeList.originalSize);
    
    void* result = tryAllocateWithoutCollecting();
    
    if (LIKELY(result != 0))
        return result;
    
    if (m_heap->collectIfNecessaryOrDefer()) {
        result = tryAllocateWithoutCollecting();
        if (result)
            return result;
    }

    ASSERT(!m_heap->shouldCollect());
    
    MarkedBlock::Handle* block = tryAllocateBlock();
    if (!block) {
        if (crashOnFailure)
            RELEASE_ASSERT_NOT_REACHED();
        else
            return nullptr;
    }
    addBlock(block);
        
    result = tryAllocateWithoutCollecting();
    ASSERT(result);
    return result;
}

static size_t blockHeaderSize()
{
    return WTF::roundUpToMultipleOf<MarkedBlock::atomSize>(sizeof(MarkedBlock));
}

size_t MarkedAllocator::blockSizeForBytes(size_t bytes)
{
    size_t minBlockSize = MarkedBlock::blockSize;
    size_t minAllocationSize = blockHeaderSize() + WTF::roundUpToMultipleOf<MarkedBlock::atomSize>(bytes);
    minAllocationSize = WTF::roundUpToMultipleOf(WTF::pageSize(), minAllocationSize);
    return std::max(minBlockSize, minAllocationSize);
}

MarkedBlock::Handle* MarkedAllocator::tryAllocateBlock()
{
    SuperSamplerScope superSamplerScope(false);
    return MarkedBlock::tryCreate(*m_heap, this, m_cellSize, m_attributes);
}

void MarkedAllocator::addBlock(MarkedBlock::Handle* block)
{
    ASSERT(!m_currentBlock);
    ASSERT(!m_freeList);
    
    m_blockList.append(block);
    setNextBlockToSweep(block);
    m_markedSpace->didAddBlock(block);
}

void MarkedAllocator::removeBlock(MarkedBlock::Handle* block)
{
    if (m_currentBlock == block) {
        m_currentBlock = filterNextBlock(m_currentBlock->next());
        setFreeList(FreeList());
    }
    if (m_nextBlockToSweep == block)
        setNextBlockToSweep(m_nextBlockToSweep->next());

    block->willRemoveBlock();
    m_blockList.remove(block);
}

void MarkedAllocator::stopAllocating()
{
    if (m_heap->operationInProgress() == FullCollection)
        m_blockList.takeFrom(m_retiredBlocks);

    ASSERT(!m_lastActiveBlock);
    if (!m_currentBlock) {
        ASSERT(!m_freeList);
        return;
    }
    
    m_currentBlock->stopAllocating(m_freeList);
    m_lastActiveBlock = m_currentBlock;
    m_currentBlock = 0;
    m_freeList = FreeList();
}

void MarkedAllocator::reset()
{
    m_lastActiveBlock = 0;
    m_currentBlock = 0;
    setFreeList(FreeList());

    setNextBlockToSweep(m_blockList.begin());

    if (UNLIKELY(Options::useImmortalObjects())) {
        MarkedBlock::Handle* next;
        for (MarkedBlock::Handle*& block = m_nextBlockToSweep; block; block = next) {
            next = filterNextBlock(block->next());

            FreeList freeList = block->sweep(MarkedBlock::Handle::SweepToFreeList);
            block->zap(freeList);
            retire(block);
        }
    }
}

void MarkedAllocator::lastChanceToFinalize()
{
    m_blockList.takeFrom(m_retiredBlocks);
    forEachBlock(
        [&] (MarkedBlock::Handle* block) {
            block->lastChanceToFinalize();
        });
}

void MarkedAllocator::setFreeList(const FreeList& freeList)
{
    m_freeList = freeList;
}

} // namespace JSC
