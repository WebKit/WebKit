#ifndef MarkedAllocator_h
#define MarkedAllocator_h

#include "MarkedBlock.h"
#include <wtf/DoublyLinkedList.h>

namespace JSC {

class Heap;
class MarkedSpace;

namespace DFG {
class SpeculativeJIT;
}

class MarkedAllocator {
    friend class JIT;
    friend class DFG::SpeculativeJIT;

public:
    MarkedAllocator();
    void reset();
    void zapFreeList();
    size_t cellSize() { return m_cellSize; }
    void* allocate();
    Heap* heap() { return m_heap; }
    
    template<typename Functor> void forEachBlock(Functor&);
    
    void addBlock(MarkedBlock*);
    void removeBlock(MarkedBlock*);
    void setHeap(Heap* heap) { m_heap = heap; }
    void setCellSize(size_t cellSize) { m_cellSize = cellSize; }
    void setMarkedSpace(MarkedSpace* space) { m_markedSpace = space; }
    
private:
    JS_EXPORT_PRIVATE void* allocateSlowCase();
    void* tryAllocate();
    void* tryAllocateHelper();
    MarkedBlock* allocateBlock(AllocationEffort);
    
    MarkedBlock::FreeCell* m_firstFreeCell;
    MarkedBlock* m_currentBlock;
    DoublyLinkedList<HeapBlock> m_blockList;
    size_t m_cellSize;
    Heap* m_heap;
    MarkedSpace* m_markedSpace;
};

inline MarkedAllocator::MarkedAllocator()
    : m_firstFreeCell(0)
    , m_currentBlock(0)
    , m_cellSize(0)
    , m_heap(0)
    , m_markedSpace(0)
{
}
    
inline void* MarkedAllocator::allocate()
{
    MarkedBlock::FreeCell* firstFreeCell = m_firstFreeCell;
    // This is a light-weight fast path to cover the most common case.
    if (UNLIKELY(!firstFreeCell))
        return allocateSlowCase();
    
    m_firstFreeCell = firstFreeCell->next;
    return firstFreeCell;
}

inline void MarkedAllocator::reset()
{
    m_currentBlock = static_cast<MarkedBlock*>(m_blockList.head());
}

inline void MarkedAllocator::zapFreeList()
{
    if (!m_currentBlock) {
        ASSERT(!m_firstFreeCell);
        return;
    }
    
    m_currentBlock->zapFreeList(m_firstFreeCell);
    m_firstFreeCell = 0;
}

template <typename Functor> inline void MarkedAllocator::forEachBlock(Functor& functor)
{
    HeapBlock* next;
    for (HeapBlock* block = m_blockList.head(); block; block = next) {
        next = block->next();
        functor(static_cast<MarkedBlock*>(block));
    }
}
    
} // namespace JSC

#endif
