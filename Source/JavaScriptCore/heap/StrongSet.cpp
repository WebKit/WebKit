/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "StrongSet.h"

#include "JSCJSValueInlines.h"
#include "SlotVisitor.h"
#include "VM.h"
#include <wtf/FastMalloc.h>

namespace JSC {

StrongBlock::StrongBlock(StrongSet* strongSet)
{
    m_header.m_strongSet = strongSet;
}

StrongBlock* StrongBlock::create(StrongSet* strongSet)
{
    void* memory = fastCompactAlignedMalloc(blockSize, blockSize);
    return new (NotNull, memory) StrongBlock(strongSet);
}

StrongSet::StrongSet(VM& vm)
    : m_vm(vm)
{
}

StrongSet::~StrongSet()
{
    while (!m_blocks.isEmpty())
        delete &(*m_blocks.begin()).block();
    ASSERT(m_available.isEmpty());
}

void StrongSet::destroyBlock(StrongBlock* block)
{
    ASSERT(block != m_currentBlock);
    ASSERT(!block->isCurrent());
    ASSERT(!block->isAvailable());
    --m_blockCount;
    delete block;
}

void StrongSet::appendAvailable(StrongBlock* block)
{
    // Release-checked: re-admitting an already-available block would install the
    // same block twice.
    RELEASE_ASSERT(!block->isAvailable());
    ASSERT(block != m_currentBlock);
    ASSERT(block != m_spareBlock);
    m_available.append(&block->availableNode());
    ++m_availabilityCount;
}

StrongBlock* StrongSet::takeAvailable()
{
    if (m_available.isEmpty())
        return nullptr;
    StrongBlock& block = (*m_available.begin()).block();
    block.availableNode().remove();
    return &block;
}

void StrongSet::retireCurrentBlock()
{
    StrongBlock* block = m_currentBlock;
    ASSERT(block);
    // A block is only ever retired because allocation exhausted it.
    ASSERT(!m_freeListHead);
    ASSERT(m_bumpCursor == m_bumpEnd);
    RELEASE_ASSERT(block->isFull());

    block->setFreeListHead(m_freeListHead);
    block->setFreeNotifyThreshold(s_retiredNotifyThreshold);
    block->setCurrent(false);

    m_currentBlock = nullptr;
    m_bumpCursor = nullptr;
    m_bumpEnd = nullptr;
    m_freeListHead = nullptr;
}

void StrongSet::installCurrentBlock(StrongBlock* block)
{
    ASSERT(!m_currentBlock);
    ASSERT(!block->isAvailable());
    ASSERT(!block->isCurrent());
    ASSERT(block != m_spareBlock);
    ASSERT(!block->isFull());

    m_currentBlock = block;
    block->setCurrent(true);
    // A block that still has live slots was full when it was retired, so its bump
    // range is spent and reuse comes off the free list. An empty one has been
    // reset, so the whole payload is available to bump through again.
    m_bumpCursor = block->isEmpty() ? block->payload() : block->payloadEnd();
    m_bumpEnd = block->payloadEnd();
    m_freeListHead = block->freeListHead();
    ASSERT(block->isEmpty() || m_freeListHead);
    // Not a watermark: allocation is already routed here, so there is nothing to
    // re-admit it to. Zero still notifies on empty, which this block needs, since
    // emptying it returns the hoisted cursors to bump mode.
    block->setFreeNotifyThreshold(0);
}

HandleSlot StrongSet::allocateSlow()
{
    if (m_currentBlock)
        retireCurrentBlock();

    // Draining the availability chain before the spare is what keeps the spare
    // free for the oscillating case.
    StrongBlock* block = takeAvailable();
    if (!block) {
        if (m_spareBlock)
            block = std::exchange(m_spareBlock, nullptr);
        else {
            block = StrongBlock::create(this);
            m_blocks.append(&block->blocksNode());
            ++m_blockCount;
        }
    }

    installCurrentBlock(block);

    // Guaranteed to succeed: every source above yields a block with at least
    // s_reAdmissionWatermark free slots, or a completely empty one.
    HandleSlot slot = tryAllocateFromCurrent();
    RELEASE_ASSERT(slot);
    return slot;
}

void StrongSet::didFreeSlot(StrongBlock* block, unsigned usedCount)
{
    if (usedCount) [[likely]] {
        // The block crossed its re-admission watermark. Only emptiness is left
        // to notify about.
        ASSERT(usedCount == s_retiredNotifyThreshold);
        block->setFreeNotifyThreshold(0);
        appendAvailable(block);
        return;
    }
    didBecomeEmpty(block);
}

void StrongSet::didBecomeEmpty(StrongBlock* block)
{
    ASSERT(block->isEmpty());
    ASSERT(!block->freeNotifyThreshold());
    ASSERT(block->isCurrent() == (block == m_currentBlock));

    // The current block is kept in place even when empty, so returning it to the
    // bump path means resetting the hoisted cursors, not the block's own.
    if (block->isCurrent()) {
        m_bumpCursor = block->payload();
        m_bumpEnd = block->payloadEnd();
        m_freeListHead = nullptr;
        if (m_spareBlock)
            destroyBlock(std::exchange(m_spareBlock, nullptr));
        return;
    }

    if (block->isAvailable())
        block->availableNode().remove();
    block->resetToBumpMode();

    if (m_spareBlock || (m_currentBlock && m_currentBlock->isEmpty()))
        destroyBlock(block);
    else
        m_spareBlock = block;
}

template<typename Visitor>
void StrongSet::visitAggregateImpl(Visitor& visitor)
{
    ASSERT(m_vm.heap.worldIsStopped());
    forEachSlot([&](HandleSlot slot) {
        JSValue value = *slot;
        if (!value)
            return;
        visitor.appendUnbarriered(value);
    });
}

DEFINE_VISIT_AGGREGATE(StrongSet);

} // namespace JSC
