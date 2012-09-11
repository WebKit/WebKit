#include "config.h"
#include "MarkedAllocator.h"

#include "GCActivityCallback.h"
#include "Heap.h"
#include "IncrementalSweeper.h"
#include "JSGlobalData.h"
#include <wtf/CurrentTime.h>

namespace JSC {

bool MarkedAllocator::isPagedOut(double deadline)
{
    unsigned itersSinceLastTimeCheck = 0;
    MarkedBlock* block = m_blockList.head();
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

inline void* MarkedAllocator::tryAllocateHelper(size_t bytes)
{
    if (!m_freeList.head) {
        if (m_onlyContainsStructures && !m_heap->isSafeToSweepStructures()) {
            if (m_currentBlock) {
                m_currentBlock->didConsumeFreeList();
                m_currentBlock = 0;
            }
            // We sweep another random block here so that we can make progress
            // toward being able to sweep Structures.
            m_heap->sweeper()->sweepNextBlock();
            return 0;
        }

        for (MarkedBlock*& block = m_blocksToSweep; block; block = block->next()) {
            MarkedBlock::FreeList freeList = block->sweep(MarkedBlock::SweepToFreeList);
            if (!freeList.head) {
                block->didConsumeFreeList();
                continue;
            }

            if (bytes > block->cellSize()) {
                block->zapFreeList(freeList);
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
    
    MarkedBlock::FreeCell* head = m_freeList.head;
    m_freeList.head = head->next;
    ASSERT(head);
    return head;
}
    
inline void* MarkedAllocator::tryAllocate(size_t bytes)
{
    ASSERT(!m_heap->isBusy());
    m_heap->m_operationInProgress = Allocation;
    void* result = tryAllocateHelper(bytes);
    m_heap->m_operationInProgress = NoOperation;
    return result;
}
    
void* MarkedAllocator::allocateSlowCase(size_t bytes)
{
    ASSERT(m_heap->globalData()->apiLock().currentThreadIsHoldingLock());
#if COLLECT_ON_EVERY_ALLOCATION
    m_heap->collectAllGarbage();
    ASSERT(m_heap->m_operationInProgress == NoOperation);
#endif
    
    ASSERT(!m_freeList.head);
    m_heap->didAllocate(m_freeList.bytes);
    
    void* result = tryAllocate(bytes);
    
    if (LIKELY(result != 0))
        return result;
    
    if (m_heap->shouldCollect()) {
        m_heap->collect(Heap::DoNotSweep);

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

    if (blockSize == MarkedBlock::blockSize) {
        PageAllocationAligned allocation = m_heap->blockAllocator().allocate();
        return MarkedBlock::create(allocation, m_heap, cellSize, m_cellsNeedDestruction, m_onlyContainsStructures);
    }

    PageAllocationAligned allocation = PageAllocationAligned::allocate(blockSize, MarkedBlock::blockSize, OSAllocator::JSGCHeapPages);
    if (!static_cast<bool>(allocation))
        CRASH();
    return MarkedBlock::create(allocation, m_heap, cellSize, m_cellsNeedDestruction, m_onlyContainsStructures);
}

void MarkedAllocator::addBlock(MarkedBlock* block)
{
    ASSERT(!m_currentBlock);
    ASSERT(!m_freeList.head);
    
    m_blockList.append(block);
    m_blocksToSweep = m_currentBlock = block;
    m_freeList = block->sweep(MarkedBlock::SweepToFreeList);
    m_markedSpace->didAddBlock(block);
}

void MarkedAllocator::removeBlock(MarkedBlock* block)
{
    if (m_currentBlock == block) {
        m_currentBlock = m_currentBlock->next();
        m_freeList = MarkedBlock::FreeList();
    }
    if (m_blocksToSweep == block)
        m_blocksToSweep = m_blocksToSweep->next();
    m_blockList.remove(block);
}

} // namespace JSC
