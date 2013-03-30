/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JSC_Region_h
#define JSC_Region_h

#include "HeapBlock.h"
#include <wtf/DoublyLinkedList.h>
#include <wtf/PageAllocationAligned.h>

namespace JSC {

class DeadBlock : public HeapBlock<DeadBlock> {
public:
    DeadBlock(Region*);
};

inline DeadBlock::DeadBlock(Region* region)
    : HeapBlock<DeadBlock>(region)
{
}

class Region : public DoublyLinkedListNode<Region> {
    friend CLASS_IF_GCC DoublyLinkedListNode<Region>;
    friend class BlockAllocator;
public:
    ~Region();
    static Region* create(size_t blockSize);
    static Region* createCustomSize(size_t blockSize, size_t blockAlignment);
    Region* reset(size_t blockSize);

    size_t blockSize() const { return m_blockSize; }
    bool isFull() const { return m_blocksInUse == m_totalBlocks; }
    bool isEmpty() const { return !m_blocksInUse; }
    bool isCustomSize() const { return m_isCustomSize; }

    DeadBlock* allocate();
    void deallocate(void*);

    static const size_t s_regionSize = 64 * KB;

private:
    Region(PageAllocationAligned&, size_t blockSize, size_t totalBlocks);

    PageAllocationAligned m_allocation;
    size_t m_totalBlocks;
    size_t m_blocksInUse;
    size_t m_blockSize;
    bool m_isCustomSize;
    Region* m_prev;
    Region* m_next;
    DoublyLinkedList<DeadBlock> m_deadBlocks;
};

inline Region* Region::create(size_t blockSize)
{
    ASSERT(blockSize <= s_regionSize);
    ASSERT(!(s_regionSize % blockSize));
    PageAllocationAligned allocation = PageAllocationAligned::allocate(s_regionSize, s_regionSize, OSAllocator::JSGCHeapPages);
    RELEASE_ASSERT(static_cast<bool>(allocation));
    return new Region(allocation, blockSize, s_regionSize / blockSize);
}

inline Region* Region::createCustomSize(size_t blockSize, size_t blockAlignment)
{
    PageAllocationAligned allocation = PageAllocationAligned::allocate(blockSize, blockAlignment, OSAllocator::JSGCHeapPages);
    RELEASE_ASSERT(static_cast<bool>(allocation));
    Region* region = new Region(allocation, blockSize, 1);
    region->m_isCustomSize = true;
    return region;
}

inline Region::Region(PageAllocationAligned& allocation, size_t blockSize, size_t totalBlocks)
    : DoublyLinkedListNode<Region>()
    , m_allocation(allocation)
    , m_totalBlocks(totalBlocks)
    , m_blocksInUse(0)
    , m_blockSize(blockSize)
    , m_isCustomSize(false)
    , m_prev(0)
    , m_next(0)
{
    ASSERT(allocation);
    char* start = static_cast<char*>(m_allocation.base());
    char* end = start + m_allocation.size();
    for (char* current = start; current < end; current += blockSize)
        m_deadBlocks.append(new (NotNull, current) DeadBlock(this));
}

inline Region::~Region()
{
    ASSERT(isEmpty());
    m_allocation.deallocate();
}

inline Region* Region::reset(size_t blockSize)
{
    ASSERT(isEmpty());
    PageAllocationAligned allocation = m_allocation;
    return new (NotNull, this) Region(allocation, blockSize, s_regionSize / blockSize);
}

inline DeadBlock* Region::allocate()
{
    ASSERT(!isFull());
    m_blocksInUse++;
    return m_deadBlocks.removeHead();
}

inline void Region::deallocate(void* base)
{
    ASSERT(base);
    ASSERT(m_blocksInUse);
    ASSERT(base >= m_allocation.base() && base < static_cast<char*>(m_allocation.base()) + m_allocation.size());
    DeadBlock* block = new (NotNull, base) DeadBlock(this);
    m_deadBlocks.push(block);
    m_blocksInUse--;
}

} // namespace JSC

#endif // JSC_Region_h
