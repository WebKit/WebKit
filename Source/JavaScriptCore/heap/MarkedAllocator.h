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

#ifndef MarkedAllocator_h
#define MarkedAllocator_h

#include "AllocatorAttributes.h"
#include "FreeList.h"
#include "MarkedBlock.h"
#include <wtf/SentinelLinkedList.h>

namespace JSC {

class Heap;
class MarkedSpace;
class LLIntOffsetsExtractor;

class MarkedAllocator {
    friend class LLIntOffsetsExtractor;

public:
    static ptrdiff_t offsetOfFreeList();
    static ptrdiff_t offsetOfCellSize();

    MarkedAllocator(Heap*, MarkedSpace*, size_t cellSize, const AllocatorAttributes&);
    void lastChanceToFinalize();
    void reset();
    void stopAllocating();
    void resumeAllocating();
    size_t cellSize() const { return m_cellSize; }
    const AllocatorAttributes& attributes() const { return m_attributes; }
    bool needsDestruction() const { return m_attributes.destruction == NeedsDestruction; }
    DestructionMode destruction() const { return m_attributes.destruction; }
    HeapCell::Kind cellKind() const { return m_attributes.cellKind; }
    void* allocate();
    void* tryAllocate();
    Heap* heap() { return m_heap; }
    MarkedBlock::Handle* takeLastActiveBlock()
    {
        MarkedBlock::Handle* block = m_lastActiveBlock;
        m_lastActiveBlock = 0;
        return block;
    }
    
    template<typename Functor> void forEachBlock(const Functor&);
    
    void addBlock(MarkedBlock::Handle*);
    void removeBlock(MarkedBlock::Handle*);

    bool isPagedOut(double deadline);
    
    static size_t blockSizeForBytes(size_t);
   
private:
    friend class MarkedBlock;
    
    JS_EXPORT_PRIVATE void* allocateSlowCase();
    JS_EXPORT_PRIVATE void* tryAllocateSlowCase();
    void* allocateSlowCaseImpl(bool crashOnFailure);
    void* tryAllocateWithoutCollecting();
    void* tryAllocateWithoutCollectingImpl();
    MarkedBlock::Handle* tryAllocateBlock();
    ALWAYS_INLINE void doTestCollectionsIfNeeded();
    void retire(MarkedBlock::Handle*);
    
    void setFreeList(const FreeList&);
    
    MarkedBlock::Handle* filterNextBlock(MarkedBlock::Handle*);
    void setNextBlockToSweep(MarkedBlock::Handle*);
    
    FreeList m_freeList;
    MarkedBlock::Handle* m_currentBlock;
    MarkedBlock::Handle* m_lastActiveBlock;
    MarkedBlock::Handle* m_nextBlockToSweep;
    SentinelLinkedList<MarkedBlock::Handle, BasicRawSentinelNode<MarkedBlock::Handle>> m_blockList;
    SentinelLinkedList<MarkedBlock::Handle, BasicRawSentinelNode<MarkedBlock::Handle>> m_retiredBlocks;
    Lock m_lock;
    unsigned m_cellSize;
    AllocatorAttributes m_attributes;
    Heap* m_heap;
    MarkedSpace* m_markedSpace;
};

inline ptrdiff_t MarkedAllocator::offsetOfFreeList()
{
    return OBJECT_OFFSETOF(MarkedAllocator, m_freeList);
}

inline ptrdiff_t MarkedAllocator::offsetOfCellSize()
{
    return OBJECT_OFFSETOF(MarkedAllocator, m_cellSize);
}

ALWAYS_INLINE void* MarkedAllocator::tryAllocate()
{
    unsigned remaining = m_freeList.remaining;
    if (remaining) {
        unsigned cellSize = m_cellSize;
        remaining -= cellSize;
        m_freeList.remaining = remaining;
        return m_freeList.payloadEnd - remaining - cellSize;
    }
    
    FreeCell* head = m_freeList.head;
    if (UNLIKELY(!head))
        return tryAllocateSlowCase();
    
    m_freeList.head = head->next;
    return head;
}

ALWAYS_INLINE void* MarkedAllocator::allocate()
{
    unsigned remaining = m_freeList.remaining;
    if (remaining) {
        unsigned cellSize = m_cellSize;
        remaining -= cellSize;
        m_freeList.remaining = remaining;
        return m_freeList.payloadEnd - remaining - cellSize;
    }
    
    FreeCell* head = m_freeList.head;
    if (UNLIKELY(!head))
        return allocateSlowCase();
    
    m_freeList.head = head->next;
    return head;
}

inline void MarkedAllocator::resumeAllocating()
{
    if (!m_lastActiveBlock)
        return;

    m_freeList = m_lastActiveBlock->resumeAllocating();
    m_currentBlock = m_lastActiveBlock;
    m_lastActiveBlock = 0;
}

template <typename Functor> inline void MarkedAllocator::forEachBlock(const Functor& functor)
{
    m_blockList.forEach(functor);
    m_retiredBlocks.forEach(functor);
}

} // namespace JSC

#endif // MarkedAllocator_h
