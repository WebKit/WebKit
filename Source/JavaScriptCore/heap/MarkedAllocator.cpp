#include "config.h"
#include "MarkedAllocator.h"

#include "GCActivityCallback.h"
#include "Heap.h"
#include "JSGlobalData.h"
#include <wtf/CurrentTime.h>

namespace JSC {

bool MarkedAllocator::isPagedOut(double deadline)
{
    unsigned itersSinceLastTimeCheck = 0;
    HeapBlock* block = m_blockList.head();
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
    ASSERT(m_heap->globalData()->apiLock().currentThreadIsHoldingLock());
#if COLLECT_ON_EVERY_ALLOCATION
    m_heap->collectAllGarbage();
    ASSERT(m_heap->m_operationInProgress == NoOperation);
#endif
    
    ASSERT(!m_freeList.head);
    m_heap->didAllocate(m_freeList.bytes);
    
    void* result = tryAllocate();
    
    if (LIKELY(result != 0))
        return result;
    
    if (m_heap->shouldCollect()) {
        m_heap->collect(Heap::DoNotSweep);

        result = tryAllocate();
        if (result)
            return result;
    }

    ASSERT(!m_heap->shouldCollect());
    
    MarkedBlock* block = allocateBlock();
    ASSERT(block);
    addBlock(block);
        
    result = tryAllocate();
    ASSERT(result);
    return result;
}
    
MarkedBlock* MarkedAllocator::allocateBlock()
{
    MarkedBlock* block = static_cast<MarkedBlock*>(m_heap->blockAllocator().allocate());
    if (block)
        block = MarkedBlock::recycle(block, m_heap, m_cellSize, m_cellsNeedDestruction);
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
