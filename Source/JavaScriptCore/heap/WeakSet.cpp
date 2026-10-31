/*
 * Copyright (C) 2012, 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WeakSet.h"

#include "Heap.h"
#include "VM.h"

namespace JSC {

WeakSet::~WeakSet()
{
    if (isOnList())
        remove();

    // Sweeping a container hands back every block in its WeakSet, and a container is always swept
    // before it is freed - at VM teardown through lastChanceToFinalize as much as in a collection.
    // Destroying a block here instead would dangle the Weak<>s still pointing into it.
    RELEASE_ASSERT(m_blocks.isEmpty());
}

void WeakSet::lastChanceToFinalize()
{
    // A finalizer may allocate a handle into this WeakSet, and that handle has to be finalized too.
    auto hasLiveHandles = [&] {
        for (WeakBlock* block = m_blocks.head(); block; block = block->next()) {
            if (block->hasLiveHandles())
                return true;
        }
        return false;
    };
    do {
        for (WeakBlock* block = m_blocks.head(); block; block = block->next())
            block->reapAllLiveHandles();
        sweep();
    } while (hasLiveHandles());
}

void WeakSet::reap()
{
    forEachBlock([](WeakBlock& block) {
        block.reap();
    });
}

void WeakSet::didBecomeEmpty(WeakBlock* block)
{
    Locker locker { heap()->weakBlockLock() };
    tryReleaseBlock(locker, block);
}

void WeakSet::tryReleaseBlock(const AbstractLocker&, WeakBlock* block)
{
    if (block->m_isIterationTarget)
        return;

    // Past this point the caller must be the thread with heap access, or the world must be stopped.
    // Nothing tracks which thread has heap access, and many clients clear Weak<>s without holding the
    // API lock

    // The fast path allocates out of m_currentBlock without consulting the list, so that block
    // can't be changed concurrently; a later shrink() will collect it.
    if (block == m_currentBlock)
        return;

    bool isEmpty = block->isEmpty();
    if (!isEmpty && !block->hasOnlyFinalizedHandles())
        return;

    if (block == m_nextAllocator)
        m_nextAllocator = block->next();

    m_blocks.remove(block);
    if (isEmpty)
        heap()->returnWeakBlockToPool(block);
    else
        heap()->addDetachedWeakBlock(block);

    if (m_blocks.isEmpty() && isOnList())
        remove();
}

void WeakSet::sweep()
{
    // WeakBlock::sweep calls finalizer and it can allocate/deallocate WeakImpls. This means,
    //
    // 1. New WeakBlock can be allocated and chained to m_blocks during iteration.
    // 2. WeakBlocks get empty and removed from m_blocks.
    //
    // So our approach is,
    //
    // 1. We are iterating doubly-linked list, so it is fine when a new WeakBlock is appended to m_blocks.
    // 2. During sweeping, we mark WeakBlock via WeakBlock::IterationScope. This defers empty / logically-empty chaining.
    //    Thus we do not unchain the currently swept block from m_blocks.
    for (WeakBlock* block = m_blocks.head(); block; block = block->next()) {
        WeakBlock::IterationScope iterationScope(*block);
        block->sweep();
    }

    releaseBlocks();
}

void WeakSet::finalizeDeadHandles()
{
    Lock& lock = heap()->weakBlockLock();
    WeakBlock* block;
    {
        Locker locker { lock };
        block = m_blocks.head();
        if (!block)
            return;
        ASSERT(!block->m_isIterationTarget);
        block->m_isIterationTarget = true;
    }

    while (block) {
        block->sweep();

        Locker locker { lock };
        block->m_isIterationTarget = false;
        block = block->next();
        if (block) {
            ASSERT(!block->m_isIterationTarget);
            block->m_isIterationTarget = true;
        }
    }
}

void WeakSet::releaseBlocks()
{
    if (m_blocks.isEmpty())
        return;

    // A finalizer can clear the last live handle in a block a sweep already let go of. Nothing
    // revisits a block when its live count reaches zero, so classify every block, including the one
    // the allocator was using.
    Locker locker { heap()->weakBlockLock() };
    m_currentBlock = nullptr;
    WeakBlock* next;
    for (WeakBlock* block = m_blocks.head(); block; block = next) {
        next = block->next();
        tryReleaseBlock(locker, block);
    }
    resetAllocator(locker);
}

void WeakSet::shrink()
{
    Locker locker { heap()->weakBlockLock() };
    m_currentBlock = nullptr;

    WeakBlock* next;
    for (WeakBlock* block = m_blocks.head(); block; block = next) {
        next = block->next();

        if (block->isEmpty())
            tryReleaseBlock(locker, block);
    }

    resetAllocator(locker);
}

WeakBlock* WeakSet::findAllocator(CellContainer container)
{
    if (WeakBlock* block = tryFindAllocator())
        return block;

    return addAllocator(container);
}

WeakBlock* WeakSet::tryFindAllocator()
{
    while (m_nextAllocator) {
        WeakBlock* block = m_nextAllocator;
        m_nextAllocator = m_nextAllocator->next();

        if (block->hasFreeCell())
            return m_currentBlock = block;
    }

    return nullptr;
}

WeakBlock* WeakSet::addAllocator(CellContainer container)
{
    if (!isOnList())
        heap()->objectSpace().addActiveWeakSet(this);

    WeakBlock* block = heap()->takeWeakBlockFromPool();
    if (block)
        block->reattach(container);
    else {
        block = WeakBlock::create(*heap(), container);
        // Only a block that is new memory counts against the eden trigger. One taken from the
        // pool adds no footprint.
        heap()->didAllocate(WeakBlock::blockSize);
    }

    {
        Locker locker { heap()->weakBlockLock() };
        m_blocks.append(block);
    }
    ASSERT(block->hasFreeCell());
    return m_currentBlock = block;
}

} // namespace JSC
