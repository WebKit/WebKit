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
    void canonicalizeCellLivenessData();
    size_t cellSize() { return m_cellSize; }
    bool cellsNeedDestruction() { return m_cellsNeedDestruction; }
    bool onlyContainsStructures() { return m_onlyContainsStructures; }
    void* allocate(size_t);
    Heap* heap() { return m_heap; }
    
    template<typename Functor> void forEachBlock(Functor&);
    
    void addBlock(MarkedBlock*);
    void removeBlock(MarkedBlock*);
    void init(Heap*, MarkedSpace*, size_t cellSize, bool cellsNeedDestruction, bool onlyContainsStructures);

    bool isPagedOut(double deadline);
   
private:
    friend class LLIntOffsetsExtractor;
    
    JS_EXPORT_PRIVATE void* allocateSlowCase(size_t);
    void* tryAllocate(size_t);
    void* tryAllocateHelper(size_t);
    MarkedBlock* allocateBlock(size_t);
    
    MarkedBlock::FreeList m_freeList;
    MarkedBlock* m_currentBlock;
    MarkedBlock* m_blocksToSweep;
    DoublyLinkedList<MarkedBlock> m_blockList;
    size_t m_cellSize;
    bool m_cellsNeedDestruction;
    bool m_onlyContainsStructures;
    Heap* m_heap;
    MarkedSpace* m_markedSpace;
};

inline MarkedAllocator::MarkedAllocator()
    : m_currentBlock(0)
    , m_blocksToSweep(0)
    , m_cellSize(0)
    , m_cellsNeedDestruction(true)
    , m_onlyContainsStructures(false)
    , m_heap(0)
    , m_markedSpace(0)
{
}

inline void MarkedAllocator::init(Heap* heap, MarkedSpace* markedSpace, size_t cellSize, bool cellsNeedDestruction, bool onlyContainsStructures)
{
    m_heap = heap;
    m_markedSpace = markedSpace;
    m_cellSize = cellSize;
    m_cellsNeedDestruction = cellsNeedDestruction;
    m_onlyContainsStructures = onlyContainsStructures;
}

inline void* MarkedAllocator::allocate(size_t bytes)
{
    MarkedBlock::FreeCell* head = m_freeList.head;
    if (UNLIKELY(!head))
        return allocateSlowCase(bytes);
    
    m_freeList.head = head->next;
    return head;
}

inline void MarkedAllocator::reset()
{
    m_currentBlock = 0;
    m_freeList = MarkedBlock::FreeList();
    m_blocksToSweep = m_blockList.head();
}

inline void MarkedAllocator::canonicalizeCellLivenessData()
{
    if (!m_currentBlock) {
        ASSERT(!m_freeList.head);
        return;
    }
    
    m_currentBlock->canonicalizeCellLivenessData(m_freeList);
    m_currentBlock = 0;
    m_freeList = MarkedBlock::FreeList();
}

template <typename Functor> inline void MarkedAllocator::forEachBlock(Functor& functor)
{
    MarkedBlock* next;
    for (MarkedBlock* block = m_blockList.head(); block; block = next) {
        next = block->next();
        functor(block);
    }
}
    
} // namespace JSC

#endif
