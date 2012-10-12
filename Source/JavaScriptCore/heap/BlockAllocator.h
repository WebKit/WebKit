/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef BlockAllocator_h
#define BlockAllocator_h

#include "HeapBlock.h"
#include <wtf/DoublyLinkedList.h>
#include <wtf/Forward.h>
#include <wtf/PageAllocationAligned.h>
#include <wtf/TCSpinLock.h>
#include <wtf/Threading.h>

namespace JSC {

class CopiedBlock;
class MarkedBlock;
class Region;

// Simple allocator to reduce VM cost by holding onto blocks of memory for
// short periods of time and then freeing them on a secondary thread.

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
public:
    ~Region();
    static Region* create(size_t blockSize, size_t blockAlignment, size_t numberOfBlocks);

    size_t blockSize() const { return m_blockSize; }
    bool isFull() const { return m_blocksInUse == m_totalBlocks; }
    bool isEmpty() const { return !m_blocksInUse; }

    DeadBlock* allocate();
    void deallocate(void*);

private:
    Region(PageAllocationAligned&, size_t blockSize, size_t totalBlocks);

    PageAllocationAligned m_allocation;
    size_t m_totalBlocks;
    size_t m_blocksInUse;
    size_t m_blockSize;
    Region* m_prev;
    Region* m_next;
    DoublyLinkedList<DeadBlock> m_deadBlocks;
};

inline Region* Region::create(size_t blockSize, size_t blockAlignment, size_t numberOfBlocks)
{
    size_t regionSize = blockSize * numberOfBlocks;
    PageAllocationAligned allocation = PageAllocationAligned::allocate(regionSize, blockAlignment, OSAllocator::JSGCHeapPages);
    if (!static_cast<bool>(allocation))
        CRASH();
    return new Region(allocation, blockSize, numberOfBlocks);
}

inline Region::Region(PageAllocationAligned& allocation, size_t blockSize, size_t totalBlocks)
    : DoublyLinkedListNode<Region>()
    , m_allocation(allocation)
    , m_totalBlocks(totalBlocks)
    , m_blocksInUse(0)
    , m_blockSize(blockSize)
    , m_prev(0)
    , m_next(0)
{
    ASSERT(allocation);
    char* start = static_cast<char*>(allocation.base());
    char* end = start + allocation.size();
    for (char* current = start; current < end; current += blockSize)
        m_deadBlocks.append(new (NotNull, current) DeadBlock(this));
}

inline Region::~Region()
{
    ASSERT(!m_blocksInUse);
    m_allocation.deallocate();
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

class BlockAllocator {
public:
    BlockAllocator();
    ~BlockAllocator();

    template <typename T> DeadBlock* allocate();
    DeadBlock* allocateCustomSize(size_t blockSize, size_t blockAlignment);
    template <typename T> void deallocate(T*);
    template <typename T> void deallocateCustomSize(T*);

private:
    DeadBlock* tryAllocateFromRegion(DoublyLinkedList<Region>&, size_t&);

    void waitForRelativeTimeWhileHoldingLock(double relative);
    void waitForRelativeTime(double relative);

    void blockFreeingThreadMain();
    static void blockFreeingThreadStartFunc(void* heap);

    void releaseFreeRegions();

    DoublyLinkedList<Region> m_fullRegions;
    DoublyLinkedList<Region> m_partialRegions;
    DoublyLinkedList<Region> m_emptyRegions;
    size_t m_numberOfEmptyRegions;
    size_t m_numberOfPartialRegions;
    bool m_isCurrentlyAllocating;
    bool m_blockFreeingThreadShouldQuit;
    SpinLock m_regionLock;
    Mutex m_emptyRegionConditionLock;
    ThreadCondition m_emptyRegionCondition;
    ThreadIdentifier m_blockFreeingThread;
};

inline DeadBlock* BlockAllocator::tryAllocateFromRegion(DoublyLinkedList<Region>& regions, size_t& numberOfRegions)
{
    if (numberOfRegions) {
        ASSERT(!regions.isEmpty());
        Region* region = regions.head();
        ASSERT(!region->isFull());
        DeadBlock* block = region->allocate();
        if (region->isFull()) {
            numberOfRegions--;
            m_fullRegions.push(regions.removeHead());
        }
        return block;
    }
    return 0;
}

template<typename T>
inline DeadBlock* BlockAllocator::allocate()
{
    DeadBlock* block;
    m_isCurrentlyAllocating = true;
    {
        SpinLockHolder locker(&m_regionLock);
        if ((block = tryAllocateFromRegion(m_partialRegions, m_numberOfPartialRegions)))
            return block;
        if ((block = tryAllocateFromRegion(m_emptyRegions, m_numberOfEmptyRegions)))
            return block;
    }

    Region* newRegion = Region::create(T::blockSize, T::blockSize, 1);

    SpinLockHolder locker(&m_regionLock);
    m_emptyRegions.push(newRegion);
    m_numberOfEmptyRegions++;
    block = tryAllocateFromRegion(m_emptyRegions, m_numberOfEmptyRegions);
    ASSERT(block);
    return block;
}

inline DeadBlock* BlockAllocator::allocateCustomSize(size_t blockSize, size_t blockAlignment)
{
    size_t realSize = WTF::roundUpToMultipleOf(blockAlignment, blockSize);
    Region* newRegion = Region::create(realSize, blockAlignment, 1);
    DeadBlock* block = newRegion->allocate();
    ASSERT(block);
    return block;
}

template<typename T>
inline void BlockAllocator::deallocate(T* block)
{
    bool shouldWakeBlockFreeingThread = false;
    {
        SpinLockHolder locker(&m_regionLock);
        Region* region = block->region();
        if (region->isFull())
            m_fullRegions.remove(region);
        region->deallocate(block);
        if (region->isEmpty()) {
            m_emptyRegions.push(region);
            shouldWakeBlockFreeingThread = !m_numberOfEmptyRegions;
            m_numberOfEmptyRegions++;
        } else {
            m_partialRegions.push(region);
            m_numberOfPartialRegions++;
        }
    }

    if (shouldWakeBlockFreeingThread) {
        MutexLocker mutexLocker(m_emptyRegionConditionLock);
        m_emptyRegionCondition.signal();
    }
}

template<typename T>
inline void BlockAllocator::deallocateCustomSize(T* block)
{
    Region* region = block->region();
    region->deallocate(block);
    delete region;
}

} // namespace JSC

#endif // BlockAllocator_h
