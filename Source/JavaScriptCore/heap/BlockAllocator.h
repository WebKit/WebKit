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

class BlockAllocator;
class CopiedBlock;
class CopyWorkListSegment;
class HandleBlock;
class MarkStackSegment;
class MarkedBlock;
class Region;
class WeakBlock;

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

class BlockAllocator {
public:
    BlockAllocator();
    ~BlockAllocator();

    template <typename T> DeadBlock* allocate();
    DeadBlock* allocateCustomSize(size_t blockSize, size_t blockAlignment);
    template <typename T> void deallocate(T*);
    template <typename T> void deallocateCustomSize(T*);

private:
    void waitForRelativeTimeWhileHoldingLock(double relative);
    void waitForRelativeTime(double relative);

    void blockFreeingThreadMain();
    static void blockFreeingThreadStartFunc(void* heap);

    struct RegionSet {
        RegionSet(size_t blockSize)
            : m_numberOfPartialRegions(0)
            , m_blockSize(blockSize)
        {
        }
        DoublyLinkedList<Region> m_fullRegions;
        DoublyLinkedList<Region> m_partialRegions;
        size_t m_numberOfPartialRegions;
        size_t m_blockSize;
    };

    DeadBlock* tryAllocateFromRegion(RegionSet&, DoublyLinkedList<Region>&, size_t&);

    void releaseFreeRegions();

    template <typename T> RegionSet& regionSetFor();

    RegionSet m_copiedRegionSet;
    RegionSet m_markedRegionSet;
    // WeakBlocks and MarkStackSegments use the same RegionSet since they're the same size.
    RegionSet m_fourKBBlockRegionSet;
    RegionSet m_workListRegionSet;

    DoublyLinkedList<Region> m_emptyRegions;
    size_t m_numberOfEmptyRegions;

    bool m_isCurrentlyAllocating;
    bool m_blockFreeingThreadShouldQuit;
    SpinLock m_regionLock;
    Mutex m_emptyRegionConditionLock;
    ThreadCondition m_emptyRegionCondition;
    ThreadIdentifier m_blockFreeingThread;
};

inline DeadBlock* BlockAllocator::tryAllocateFromRegion(RegionSet& set, DoublyLinkedList<Region>& regions, size_t& numberOfRegions)
{
    if (numberOfRegions) {
        ASSERT(!regions.isEmpty());
        Region* region = regions.head();
        ASSERT(!region->isFull());

        if (region->isEmpty()) {
            ASSERT(region == m_emptyRegions.head());
            m_numberOfEmptyRegions--;
            set.m_numberOfPartialRegions++;
            region = m_emptyRegions.removeHead()->reset(set.m_blockSize);
            set.m_partialRegions.push(region);
        }

        DeadBlock* block = region->allocate();

        if (region->isFull()) {
            set.m_numberOfPartialRegions--;
            set.m_fullRegions.push(set.m_partialRegions.removeHead());
        }

        return block;
    }
    return 0;
}

template<typename T>
inline DeadBlock* BlockAllocator::allocate()
{
    RegionSet& set = regionSetFor<T>();
    DeadBlock* block;
    m_isCurrentlyAllocating = true;
    {
        SpinLockHolder locker(&m_regionLock);
        if ((block = tryAllocateFromRegion(set, set.m_partialRegions, set.m_numberOfPartialRegions)))
            return block;
        if ((block = tryAllocateFromRegion(set, m_emptyRegions, m_numberOfEmptyRegions)))
            return block;
    }

    Region* newRegion = Region::create(T::blockSize);

    SpinLockHolder locker(&m_regionLock);
    m_emptyRegions.push(newRegion);
    m_numberOfEmptyRegions++;
    block = tryAllocateFromRegion(set, m_emptyRegions, m_numberOfEmptyRegions);
    ASSERT(block);
    return block;
}

inline DeadBlock* BlockAllocator::allocateCustomSize(size_t blockSize, size_t blockAlignment)
{
    size_t realSize = WTF::roundUpToMultipleOf(blockAlignment, blockSize);
    Region* newRegion = Region::createCustomSize(realSize, blockAlignment);
    DeadBlock* block = newRegion->allocate();
    ASSERT(block);
    return block;
}

template<typename T>
inline void BlockAllocator::deallocate(T* block)
{
    RegionSet& set = regionSetFor<T>();
    bool shouldWakeBlockFreeingThread = false;
    {
        SpinLockHolder locker(&m_regionLock);
        Region* region = block->region();
        ASSERT(!region->isEmpty());
        if (region->isFull())
            set.m_fullRegions.remove(region);
        else {
            set.m_partialRegions.remove(region);
            set.m_numberOfPartialRegions--;
        }

        region->deallocate(block);

        if (region->isEmpty()) {
            m_emptyRegions.push(region);
            shouldWakeBlockFreeingThread = !m_numberOfEmptyRegions;
            m_numberOfEmptyRegions++;
        } else {
            set.m_partialRegions.push(region);
            set.m_numberOfPartialRegions++;
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
    ASSERT(region->isCustomSize());
    region->deallocate(block);
    delete region;
}

template <>
inline BlockAllocator::RegionSet& BlockAllocator::regionSetFor<CopiedBlock>()
{
    return m_copiedRegionSet;
}

template <>
inline BlockAllocator::RegionSet& BlockAllocator::regionSetFor<MarkedBlock>()
{
    return m_markedRegionSet;
}

template <>
inline BlockAllocator::RegionSet& BlockAllocator::regionSetFor<WeakBlock>()
{
    return m_fourKBBlockRegionSet;
}

template <>
inline BlockAllocator::RegionSet& BlockAllocator::regionSetFor<MarkStackSegment>()
{
    return m_fourKBBlockRegionSet;
}

template <>
inline BlockAllocator::RegionSet& BlockAllocator::regionSetFor<CopyWorkListSegment>()
{
    return m_workListRegionSet;
}

template <>
inline BlockAllocator::RegionSet& BlockAllocator::regionSetFor<HandleBlock>()
{
    return m_fourKBBlockRegionSet;
}

template <>
inline BlockAllocator::RegionSet& BlockAllocator::regionSetFor<HeapBlock<CopiedBlock> >()
{
    return m_copiedRegionSet;
}

template <>
inline BlockAllocator::RegionSet& BlockAllocator::regionSetFor<HeapBlock<MarkedBlock> >()
{
    return m_markedRegionSet;
}

template <>
inline BlockAllocator::RegionSet& BlockAllocator::regionSetFor<HeapBlock<WeakBlock> >()
{
    return m_fourKBBlockRegionSet;
}

template <>
inline BlockAllocator::RegionSet& BlockAllocator::regionSetFor<HeapBlock<MarkStackSegment> >()
{
    return m_fourKBBlockRegionSet;
}

template <>
inline BlockAllocator::RegionSet& BlockAllocator::regionSetFor<HeapBlock<CopyWorkListSegment> >()
{
    return m_workListRegionSet;
}

template <>
inline BlockAllocator::RegionSet& BlockAllocator::regionSetFor<HeapBlock<HandleBlock> >()
{
    return m_fourKBBlockRegionSet;
}

template <typename T>
inline BlockAllocator::RegionSet& BlockAllocator::regionSetFor()
{
    RELEASE_ASSERT_NOT_REACHED();
    return *(RegionSet*)0;
}

} // namespace JSC

#endif // BlockAllocator_h
