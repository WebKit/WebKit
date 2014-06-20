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

#include "GCActivityCallback.h"
#include "HeapBlock.h"
#include "Region.h"
#include <condition_variable>
#include <wtf/DoublyLinkedList.h>
#include <wtf/Forward.h>
#include <wtf/PageAllocationAligned.h>
#include <wtf/TCSpinLock.h>
#include <wtf/Threading.h>

namespace JSC {

class BlockAllocator;
class CodeBlock;
class CopiedBlock;
class CopyWorkListSegment;
template <typename T> class GCArraySegment;
class HandleBlock;
class JSCell;
class VM;
class MarkedBlock;
class WeakBlock;

// Simple allocator to reduce VM cost by holding onto blocks of memory for
// short periods of time and then freeing them on a secondary thread.

class BlockAllocator {
public:
    BlockAllocator();
    ~BlockAllocator();

    template <typename T> DeadBlock* allocate();
    DeadBlock* allocateCustomSize(size_t blockSize, size_t blockAlignment);
    template <typename T> void deallocate(T*);
    template <typename T> void deallocateCustomSize(T*);

    JS_EXPORT_PRIVATE void releaseFreeRegions();

private:
    void waitForDuration(std::chrono::milliseconds);

    friend ThreadIdentifier createBlockFreeingThread(BlockAllocator*);
    void blockFreeingThreadMain();
    static void blockFreeingThreadStartFunc(void* heap);

    struct RegionSet {
        RegionSet(size_t blockSize)
            : m_numberOfPartialRegions(0)
            , m_blockSize(blockSize)
        {
        }

        bool isEmpty() const
        {
            return m_fullRegions.isEmpty() && m_partialRegions.isEmpty();
        }

        DoublyLinkedList<Region> m_fullRegions;
        DoublyLinkedList<Region> m_partialRegions;
        size_t m_numberOfPartialRegions;
        size_t m_blockSize;
    };

    DeadBlock* tryAllocateFromRegion(RegionSet&, DoublyLinkedList<Region>&, size_t&);

    bool allRegionSetsAreEmpty() const;

    template <typename T> RegionSet& regionSetFor();

    SuperRegion m_superRegion;
    RegionSet m_copiedRegionSet;
    RegionSet m_markedRegionSet;
    // WeakBlocks and GCArraySegments use the same RegionSet since they're the same size.
    RegionSet m_fourKBBlockRegionSet;
    RegionSet m_workListRegionSet;

    DoublyLinkedList<Region> m_emptyRegions;
    size_t m_numberOfEmptyRegions;

    bool m_isCurrentlyAllocating;
    bool m_blockFreeingThreadShouldQuit;
    SpinLock m_regionLock;
    std::mutex m_emptyRegionConditionMutex;
    std::condition_variable m_emptyRegionCondition;
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

    Region* newRegion = Region::create(&m_superRegion, T::blockSize);

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
    Region* newRegion = Region::createCustomSize(&m_superRegion, realSize, blockAlignment);
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
        std::lock_guard<std::mutex> lock(m_emptyRegionConditionMutex);
        m_emptyRegionCondition.notify_one();
    }

    if (!m_blockFreeingThread)
        releaseFreeRegions();
}

template<typename T>
inline void BlockAllocator::deallocateCustomSize(T* block)
{
    Region* region = block->region();
    ASSERT(region->isCustomSize());
    region->deallocate(block);
    region->destroy();
}

#define REGION_SET_FOR(blockType, set) \
    template <> \
    inline BlockAllocator::RegionSet& BlockAllocator::regionSetFor<blockType>() \
    { \
        return set; \
    } \
    template <> \
    inline BlockAllocator::RegionSet& BlockAllocator::regionSetFor<HeapBlock<blockType>>() \
    { \
        return set; \
    } \

REGION_SET_FOR(MarkedBlock, m_markedRegionSet);
REGION_SET_FOR(CopiedBlock, m_copiedRegionSet);
REGION_SET_FOR(WeakBlock, m_fourKBBlockRegionSet);
REGION_SET_FOR(GCArraySegment<const JSCell*>, m_fourKBBlockRegionSet);
REGION_SET_FOR(GCArraySegment<CodeBlock*>, m_fourKBBlockRegionSet);
REGION_SET_FOR(CopyWorkListSegment, m_workListRegionSet);
REGION_SET_FOR(HandleBlock, m_fourKBBlockRegionSet);

#undef REGION_SET_FOR

template <typename T>
inline BlockAllocator::RegionSet& BlockAllocator::regionSetFor()
{
    RELEASE_ASSERT_NOT_REACHED();
    return *(RegionSet*)0;
}

} // namespace JSC

#endif // BlockAllocator_h
