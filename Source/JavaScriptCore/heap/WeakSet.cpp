/*
 * Copyright (C) 2012, 2016, 2026 Apple Inc. All rights reserved.
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
#include "WeakSetInlines.h"

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

bool WeakSet::isEmpty() const
{
    Locker locker { weakBlockLock() };
    for (WeakBlock* block = m_blocks.head(); block; block = block->next()) {
        if (!block->isEmpty())
            return false;
    }

    return true;
}

bool WeakSet::isTriviallyDestructible() const
{
    Locker locker { weakBlockLock() };
    if (!m_blocks.isEmpty())
        return false;
    if (isOnList())
        return false;
    return true;
}

void WeakSet::lastChanceToFinalize()
{
    // A finalizer may allocate a handle into this WeakSet, and that handle has to be finalized too.
    auto hasLiveHandles = [&] {
        Locker locker { weakBlockLock() };
        for (WeakBlock* block = m_blocks.head(); block; block = block->next()) {
            if (block->hasLiveHandles(locker))
                return true;
        }
        return false;
    };
    do {
        {
            Locker locker { weakBlockLock() };
            for (WeakBlock* block = m_blocks.head(); block; block = block->next())
                block->reapAllLiveHandles(locker);
        }
        sweep();
    } while (hasLiveHandles());
}

void WeakSet::reap()
{
    Locker locker { weakBlockLock() };
    forEachBlock([&](WeakBlock& block) {
        block.reap(locker);
    });
}

void WeakSet::tryReleaseBlock(Locker<Lock>& locker, WeakBlock* block, IsConcurrent isConcurrent)
{
    locker.assertIsHolding(heap()->weakBlockLock());
    if (block->isIterationTarget())
        return;

    // The fast path allocates out of m_currentBlock without the lock.
    if (block == m_currentBlock)
        return;

    // No other block can be allocated from while the lock is held, since changing m_currentBlock
    // takes it. A concurrent caller releases only empty blocks; the mutator detaches the rest.
    bool isEmpty = block->isEmpty();
    if (!isEmpty && (isConcurrent == IsConcurrent::Yes || !block->hasOnlyFinalizedHandles(locker)))
        return;

    if (block == m_nextAllocator)
        m_nextAllocator = block->next();

    m_blocks.remove(block);
    if (isEmpty)
        heap()->returnWeakBlockToPool(locker, block, isConcurrent);
    else
        heap()->addDetachedWeakBlock(locker, block);

    // Only the mutator changes the active lists. releaseBlocks() and shrink() drop a WeakSet that a
    // concurrent caller emptied.
    if (isConcurrent == IsConcurrent::No && m_blocks.isEmpty() && isOnList())
        remove();
}

void WeakSet::sweep()
{
    // A WeakSet with blocks is always on one of MarkedSpace's active lists.
    if (!isOnList())
        return;

    finalizeDeadHandles();
    releaseBlocks();
}

void WeakSet::finalizeDeadHandles()
{
    // Finalizers run during sweep() and may take weakBlockLock(). Marking the swept block as the
    // iteration target keeps tryReleaseBlock() from unlinking it while the lock is dropped. A block a
    // finalizer appends is still reached through next().
    WeakBlock* block;
    {
        Locker locker { weakBlockLock() };
        block = m_blocks.head();
        if (!block)
            return;
        ASSERT(!block->isIterationTarget());
        block->setIterationTarget(locker, true);
    }

    while (block) {
        block->sweep();

        Locker locker { weakBlockLock() };
        block->setIterationTarget(locker, false);
        block = block->next();
        if (block) {
            ASSERT(!block->isIterationTarget());
            block->setIterationTarget(locker, true);
        }
    }
}

void WeakSet::releaseBlocks()
{
    Locker locker { weakBlockLock() };
    assertIsHoldingWeakBlockLock();

    // A finalizer can clear the last live handle in a block a sweep already let go of. Nothing
    // revisits a block when its last live handle is cleared, so classify every block, including the one
    // the allocator was using.
    m_currentBlock = nullptr;
    WeakBlock* next;
    for (WeakBlock* block = m_blocks.head(); block; block = next) {
        next = block->next();
        tryReleaseBlock(locker, block, IsConcurrent::No);
    }
    resetAllocator();

    if (m_blocks.isEmpty() && isOnList())
        remove();
}

void WeakSet::shrink()
{
    Locker locker { weakBlockLock() };
    assertIsHoldingWeakBlockLock();
    m_currentBlock = nullptr;

    WeakBlock* next;
    for (WeakBlock* block = m_blocks.head(); block; block = next) {
        next = block->next();

        if (block->isEmpty())
            tryReleaseBlock(locker, block, IsConcurrent::No);
    }

    resetAllocator();

    if (m_blocks.isEmpty() && isOnList())
        remove();
}

WeakBlock* WeakSet::findAllocator(CellContainer container)
{
    if (WeakBlock* block = tryFindAllocator())
        return block;

    return addAllocator(container);
}

WeakBlock* WeakSet::tryFindAllocator()
{
    Locker locker { weakBlockLock() };
    assertIsHoldingWeakBlockLock();
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

    WeakBlock* block;
    {
        Locker locker { heap()->weakBlockLock() };
        block = heap()->takeWeakBlockFromPool();
        if (block)
            block->reattach(locker, container);
    }
    if (!block) {
        block = WeakBlock::create(*heap(), container);
        // Only a block that is new memory counts against the eden trigger. One taken from the
        // pool adds no footprint.
        heap()->didAllocate(WeakBlock::blockSize);
    }

    Locker locker { weakBlockLock() };
    assertIsHoldingWeakBlockLock();
    m_blocks.append(block);
    ASSERT(block->hasFreeCell());
    return m_currentBlock = block;
}

} // namespace JSC
