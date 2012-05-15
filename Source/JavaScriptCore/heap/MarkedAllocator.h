#ifndef MarkedAllocator_h
#define MarkedAllocator_h

#include "MarkedBlock.h"
#include <wtf/DoublyLinkedList.h>

namespace JSC {

class Heap;
class MarkedSpace;
class LLIntOffsetsExtractor;

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
    bool cellsNeedDestruction() { return m_cellsNeedDestruction; }
    void* allocate();
    Heap* heap() { return m_heap; }
    
    template<typename Functor> void forEachBlock(Functor&);
    
    void addBlock(MarkedBlock*);
    void removeBlock(MarkedBlock*);
    void init(Heap*, MarkedSpace*, size_t cellSize, bool cellsNeedDestruction);

    bool isPagedOut(double deadline);
   
private:
    friend class LLIntOffsetsExtractor;
    
    JS_EXPORT_PRIVATE void* allocateSlowCase();
    void* tryAllocate();
    void* tryAllocateHelper();
    MarkedBlock* allocateBlock();
    
    MarkedBlock::FreeList m_freeList;
    MarkedBlock* m_currentBlock;
    DoublyLinkedList<HeapBlock> m_blockList;
    size_t m_cellSize;
    bool m_cellsNeedDestruction;
    Heap* m_heap;
    MarkedSpace* m_markedSpace;
};

inline MarkedAllocator::MarkedAllocator()
    : m_currentBlock(0)
    , m_cellSize(0)
    , m_cellsNeedDestruction(true)
    , m_heap(0)
    , m_markedSpace(0)
{
}

inline void MarkedAllocator::init(Heap* heap, MarkedSpace* markedSpace, size_t cellSize, bool cellsNeedDestruction)
{
    m_heap = heap;
    m_markedSpace = markedSpace;
    m_cellSize = cellSize;
    m_cellsNeedDestruction = cellsNeedDestruction;
}

inline void* MarkedAllocator::allocate()
{
    MarkedBlock::FreeCell* head = m_freeList.head;
    // This is a light-weight fast path to cover the most common case.
    if (UNLIKELY(!head))
        return allocateSlowCase();
    
    m_freeList.head = head->next;
    return head;
}

inline void MarkedAllocator::reset()
{
    m_currentBlock = static_cast<MarkedBlock*>(m_blockList.head());
}

inline void MarkedAllocator::zapFreeList()
{
    if (!m_currentBlock) {
        ASSERT(!m_freeList.head);
        return;
    }
    
    m_currentBlock->zapFreeList(m_freeList);
    m_freeList = MarkedBlock::FreeList();
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
