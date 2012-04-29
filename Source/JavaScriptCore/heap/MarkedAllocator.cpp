#include "config.h"
#include "MarkedAllocator.h"

#include "GCActivityCallback.h"
#include "Heap.h"

namespace JSC {

inline void* MarkedAllocator::tryAllocateHelper()
{
    if (!m_freeList.head) {
        for (MarkedBlock*& block = m_currentBlock; block; block = static_cast<MarkedBlock*>(block->next())) {
            m_freeList = block->sweep(MarkedBlock::SweepToFreeList);
            if (m_freeList.head)
                break;
            block->didConsumeFreeList();
        }
        
        if (!m_freeList.head)
            return 0;
    }
    
    MarkedBlock::FreeCell* head = m_freeList.head;
    m_freeList.head = head->next;
    ASSERT(head);
    return head;
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
    
    ASSERT(!m_freeList.head);
    m_heap->didAllocate(m_freeList.bytes);
    
    void* result = tryAllocate();
    
    if (LIKELY(result != 0))
        return result;
    
    AllocationEffort allocationEffort;
    
    if (m_heap->shouldCollect())
        allocationEffort = AllocationCanFail;
    else
        allocationEffort = AllocationMustSucceed;
    
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
    
    ASSERT(!m_heap->shouldCollect());
    
    addBlock(allocateBlock(AllocationMustSucceed));
    
    result = tryAllocate();
    ASSERT(result);
    return result;
}
    
MarkedBlock* MarkedAllocator::allocateBlock(AllocationEffort allocationEffort)
{
    MarkedBlock* block = static_cast<MarkedBlock*>(m_heap->blockAllocator().allocate());
    if (block)
        block = MarkedBlock::recycle(block, m_heap, m_cellSize, m_cellsNeedDestruction);
    else if (allocationEffort == AllocationCanFail)
        return 0;
    else
        block = MarkedBlock::create(m_heap, m_cellSize, m_cellsNeedDestruction);
    
    m_markedSpace->didAddBlock(block);
    
    return block;
}

void MarkedAllocator::addBlock(MarkedBlock* block)
{
    ASSERT(!m_currentBlock);
    ASSERT(!m_freeList.head);
    
    m_blockList.append(block);
    m_currentBlock = block;
    m_freeList = block->sweep(MarkedBlock::SweepToFreeList);
}

void MarkedAllocator::removeBlock(MarkedBlock* block)
{
    if (m_currentBlock == block)
        m_currentBlock = 0;
    m_blockList.remove(block);
}

} // namespace JSC
