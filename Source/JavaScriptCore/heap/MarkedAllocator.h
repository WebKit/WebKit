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

#ifndef MarkedAllocator_h
#define MarkedAllocator_h

#include "MarkedBlock.h"
#include <wtf/DoublyLinkedList.h>

namespace JSC {

class Heap;
class MarkedSpace;
class LLIntOffsetsExtractor;

class MarkedAllocator {
    friend class LLIntOffsetsExtractor;

public:
    static ptrdiff_t offsetOfFreeListHead();

    MarkedAllocator();
    void lastChanceToFinalize();
    void reset();
    void stopAllocating();
    void resumeAllocating();
    size_t cellSize() { return m_cellSize; }
    bool needsDestruction() { return m_needsDestruction; }
    void* allocate(size_t);
    Heap* heap() { return m_heap; }
    MarkedBlock* takeLastActiveBlock()
    {
        MarkedBlock* block = m_lastActiveBlock;
        m_lastActiveBlock = 0;
        return block;
    }
    
    template<typename Functor> void forEachBlock(Functor&);
    
    void addBlock(MarkedBlock*);
    void removeBlock(MarkedBlock*);
    void init(Heap*, MarkedSpace*, size_t cellSize, bool needsDestruction);

    bool isPagedOut(double deadline);
   
private:
    JS_EXPORT_PRIVATE void* allocateSlowCase(size_t);
    void* tryAllocate(size_t);
    void* tryAllocateHelper(size_t);
    void* tryPopFreeList(size_t);
    MarkedBlock* allocateBlock(size_t);
    ALWAYS_INLINE void doTestCollectionsIfNeeded();
    void retire(MarkedBlock*, MarkedBlock::FreeList&);
    
    MarkedBlock::FreeList m_freeList;
    MarkedBlock* m_currentBlock;
    MarkedBlock* m_lastActiveBlock;
    MarkedBlock* m_nextBlockToSweep;
    DoublyLinkedList<MarkedBlock> m_blockList;
    DoublyLinkedList<MarkedBlock> m_retiredBlocks;
    size_t m_cellSize;
    bool m_needsDestruction { false };
    Heap* m_heap;
    MarkedSpace* m_markedSpace;
};

inline ptrdiff_t MarkedAllocator::offsetOfFreeListHead()
{
    return OBJECT_OFFSETOF(MarkedAllocator, m_freeList) + OBJECT_OFFSETOF(MarkedBlock::FreeList, head);
}

inline MarkedAllocator::MarkedAllocator()
    : m_currentBlock(0)
    , m_lastActiveBlock(0)
    , m_nextBlockToSweep(0)
    , m_cellSize(0)
    , m_heap(0)
    , m_markedSpace(0)
{
}

inline void MarkedAllocator::init(Heap* heap, MarkedSpace* markedSpace, size_t cellSize, bool needsDestruction)
{
    m_heap = heap;
    m_markedSpace = markedSpace;
    m_cellSize = cellSize;
    m_needsDestruction = needsDestruction;
}

inline void* MarkedAllocator::allocate(size_t bytes)
{
    MarkedBlock::FreeCell* head = m_freeList.head;
    if (UNLIKELY(!head)) {
        void* result = allocateSlowCase(bytes);
#ifndef NDEBUG
        memset(result, 0xCD, bytes);
#endif
        return result;
    }
    
    m_freeList.head = head->next;
#ifndef NDEBUG
    memset(head, 0xCD, bytes);
#endif
    return head;
}

inline void MarkedAllocator::stopAllocating()
{
    ASSERT(!m_lastActiveBlock);
    if (!m_currentBlock) {
        ASSERT(!m_freeList.head);
        return;
    }
    
    m_currentBlock->stopAllocating(m_freeList);
    m_lastActiveBlock = m_currentBlock;
    m_currentBlock = 0;
    m_freeList = MarkedBlock::FreeList();
}

inline void MarkedAllocator::resumeAllocating()
{
    if (!m_lastActiveBlock)
        return;

    m_freeList = m_lastActiveBlock->resumeAllocating();
    m_currentBlock = m_lastActiveBlock;
    m_lastActiveBlock = 0;
}

template <typename Functor> inline void MarkedAllocator::forEachBlock(Functor& functor)
{
    MarkedBlock* next;
    for (MarkedBlock* block = m_blockList.head(); block; block = next) {
        next = block->next();
        functor(block);
    }

    for (MarkedBlock* block = m_retiredBlocks.head(); block; block = next) {
        next = block->next();
        functor(block);
    }
}

} // namespace JSC

#endif // MarkedAllocator_h
