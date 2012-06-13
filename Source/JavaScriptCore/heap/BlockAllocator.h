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

// Simple allocator to reduce VM cost by holding onto blocks of memory for
// short periods of time and then freeing them on a secondary thread.

class BlockAllocator {
public:
    BlockAllocator();
    ~BlockAllocator();

    PageAllocationAligned allocate();
    void deallocate(PageAllocationAligned);

private:
    void waitForRelativeTimeWhileHoldingLock(double relative);
    void waitForRelativeTime(double relative);

    void blockFreeingThreadMain();
    static void blockFreeingThreadStartFunc(void* heap);

    void releaseFreeBlocks();

    DoublyLinkedList<HeapBlock> m_freeBlocks;
    size_t m_numberOfFreeBlocks;
    bool m_isCurrentlyAllocating;
    bool m_blockFreeingThreadShouldQuit;
    SpinLock m_freeBlockLock;
    Mutex m_freeBlockConditionLock;
    ThreadCondition m_freeBlockCondition;
    ThreadIdentifier m_blockFreeingThread;
};

inline PageAllocationAligned BlockAllocator::allocate()
{
    {
        SpinLockHolder locker(&m_freeBlockLock);
        m_isCurrentlyAllocating = true;
        if (m_numberOfFreeBlocks) {
            ASSERT(!m_freeBlocks.isEmpty());
            m_numberOfFreeBlocks--;
            return m_freeBlocks.removeHead()->m_allocation;
        }
    }

    ASSERT(m_freeBlocks.isEmpty());
    PageAllocationAligned allocation = PageAllocationAligned::allocate(HeapBlock::s_blockSize, HeapBlock::s_blockSize, OSAllocator::JSGCHeapPages);
    if (!static_cast<bool>(allocation))
        CRASH();
    return allocation;
}

inline void BlockAllocator::deallocate(PageAllocationAligned allocation)
{
    SpinLockHolder locker(&m_freeBlockLock);
    HeapBlock* heapBlock = new(NotNull, allocation.base()) HeapBlock(allocation);
    m_freeBlocks.push(heapBlock);
    m_numberOfFreeBlocks++;
}

} // namespace JSC

#endif // BlockAllocator_h
