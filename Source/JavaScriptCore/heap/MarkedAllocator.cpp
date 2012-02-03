#include "config.h"
#include "MarkedAllocator.h"

#include "Heap.h"

namespace JSC {

inline void* MarkedAllocator::tryAllocateHelper()
{
    MarkedBlock::FreeCell* firstFreeCell = m_firstFreeCell;
    if (!firstFreeCell) {
        for (MarkedBlock*& block = m_currentBlock; block; block = static_cast<MarkedBlock*>(block->next())) {
            firstFreeCell = block->sweep(MarkedBlock::SweepToFreeList);
            if (firstFreeCell)
                break;
            m_markedSpace->didConsumeFreeList(block);
            block->didConsumeFreeList();
        }
        
        if (!firstFreeCell)
            return 0;
    }
    
    ASSERT(firstFreeCell);
    m_firstFreeCell = firstFreeCell->next;
    return firstFreeCell;
}
    
inline void* MarkedAllocator::tryAllocate()
{
    m_heap->m_operationInProgress = Allocation;
    void* result = tryAllocateHelper();
    m_heap->m_operationInProgress = NoOperation;
    return result;
}
    
void* MarkedAllocator::allocateSlowCase()
{
#if COLLECT_ON_EVERY_ALLOCATION
    m_heap->collectAllGarbage();
    ASSERT(m_heap->m_operationInProgress == NoOperation);
#endif
    
    void* result = tryAllocate();
    
    if (LIKELY(result != 0))
        return result;
    
    AllocationEffort allocationEffort;
    
    if ((
#if ENABLE(GGC)
         nurseryWaterMark() < m_heap->m_minBytesPerCycle
#else
         m_heap->waterMark() < m_heap->highWaterMark()
#endif
         ) || !m_heap->m_isSafeToCollect)
        allocationEffort = AllocationMustSucceed;
    else
        allocationEffort = AllocationCanFail;
    
    MarkedBlock* block = allocateBlock(allocationEffort);
    if (block) {
        addBlock(block);
        void* result = tryAllocate();
        ASSERT(result);
        return result;
    }
    
    m_heap->collect(Heap::DoNotSweep);
    
    result = tryAllocate();
    
    if (result)
        return result;
    
    ASSERT(m_heap->waterMark() < m_heap->highWaterMark());
    
    addBlock(allocateBlock(AllocationMustSucceed));
    
    result = tryAllocate();
    ASSERT(result);
    return result;
}
    
MarkedBlock* MarkedAllocator::allocateBlock(AllocationEffort allocationEffort)
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
        block = MarkedBlock::recycle(block, m_heap, m_cellSize);
    else if (allocationEffort == AllocationCanFail)
        return 0;
    else
        block = MarkedBlock::create(m_heap, m_cellSize);
    
    m_markedSpace->didAddBlock(block);
    
    return block;
}

void MarkedAllocator::addBlock(MarkedBlock* block)
{
    ASSERT(!m_currentBlock);
    ASSERT(!m_firstFreeCell);
    
    m_blockList.append(block);
    m_currentBlock = block;
    m_firstFreeCell = block->sweep(MarkedBlock::SweepToFreeList);
}

void MarkedAllocator::removeBlock(MarkedBlock* block)
{
    if (m_currentBlock == block)
        m_currentBlock = 0;
    m_blockList.remove(block);
}

} // namespace JSC
