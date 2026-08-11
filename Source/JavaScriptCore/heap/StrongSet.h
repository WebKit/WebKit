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

#pragma once

#include <JavaScriptCore/SlotVisitorMacros.h>
#include <JavaScriptCore/StrongBlock.h>
#include <wtf/HashCountedSet.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class JSCell;
class VM;

// Owns the StrongBlocks holding a VM's Strong<> handle slots, as a slab allocator
// bump-allocating from one current block whose cursors are hoisted into this
// object. Block selection is cold by construction: every block it picks has at
// least s_reAdmissionWatermark free slots to hand out, and StrongBlock::capacity
// of them when that block is empty.
//
// allocate()/deallocate() and every other mutation must run with the JSLock held.
class StrongSet {
    WTF_MAKE_NONCOPYABLE(StrongSet);
public:
    // Re-admitting a full block on its first free would let the pattern "fill a
    // block, free one slot, immediately reallocate it" transition it from full to
    // eligible once per cycle forever. Waiting for this many free slots also
    // amortizes the cursor save/reload over at least that many allocations, and
    // at ~3% of capacity costs essentially no reuse.
    static constexpr unsigned s_reAdmissionWatermark = 64;
    static_assert(StrongBlock::capacity > s_reAdmissionWatermark);
    static constexpr unsigned s_retiredNotifyThreshold = StrongBlock::capacity - s_reAdmissionWatermark;
    // An empty current block counts against this: it is already warm and serves
    // the same purpose as a spare.
    static constexpr unsigned s_maxIdleBlocks = 1;

    JS_EXPORT_PRIVATE explicit StrongSet(VM&);
    JS_EXPORT_PRIVATE ~StrongSet();

    VM& vm() { return m_vm; }

    static StrongSet* setFor(HandleSlot);

    HandleSlot allocate();
    static void deallocate(HandleSlot);

    DECLARE_VISIT_AGGREGATE;

    // Runs with the JSLock held (see the class comment). The functor must not
    // allocate or deallocate handles. See forEachSlot.
    void forEachStrongHandle(const Invocable<void(JSCell*)> auto&, const HashCountedSet<JSCell*>& skipSet);

    unsigned blockCount() const { return m_blockCount; }
    // Test-only. Counts the transition s_reAdmissionWatermark exists to keep rare.
    unsigned availabilityCount() const { return m_availabilityCount; }
    // Test-only. Walks the chain, so a block freed while still linked here shows
    // up as a count that outlives it rather than as silent corruption.
    unsigned availableBlockCount() const
    {
        unsigned count = 0;
        for (auto it = m_available.begin(); it != m_available.end(); ++it)
            ++count;
        return count;
    }

private:
    JS_EXPORT_PRIVATE HandleSlot allocateSlow();
    JS_EXPORT_PRIVATE void didFreeSlot(StrongBlock*, unsigned usedCount);
    void deallocateFromCurrentBlock(StrongBlock*, HandleSlot);
    void didBecomeEmpty(StrongBlock*);
    void appendAvailable(StrongBlock*);
    StrongBlock* takeAvailable();
    void installCurrentBlock(StrongBlock*);
    void retireCurrentBlock();
    void destroyBlock(StrongBlock*);
    HandleSlot tryAllocateFromCurrent();

    void forEachSlot(const Invocable<void(const HandleSlot&)> auto&);
    void forEachLiveCell(const Invocable<void(JSCell*)> auto&);

    VM& m_vm;
    // The hoisted cursors of m_currentBlock. All three stay null while there is
    // no current block, which is what lets the fast path skip a null check on
    // m_currentBlock: null cursors compare equal, so the first allocate() of a
    // set's life falls through to allocateSlow().
    HandleSlot m_bumpCursor { nullptr };
    HandleSlot m_bumpEnd { nullptr };
    HandleSlot m_freeListHead { nullptr };
    StrongBlock* m_currentBlock { nullptr };
    // Every block for this set's whole life, walked by marking. A block carries a
    // separate embedded node per list, so being on this one does not stop it from
    // also being on m_available.
    StrongBlock::BlockList m_blocks;
    // Blocks with at least s_reAdmissionWatermark free slots, appended at the
    // tail so a block crossing the watermark does not displace the block under
    // the hot cursor.
    StrongBlock::BlockList m_available;
    StrongBlock* m_spareBlock { nullptr };
    unsigned m_blockCount { 0 };
    unsigned m_availabilityCount { 0 };
};

inline StrongSet* StrongSet::setFor(HandleSlot slot)
{
    return StrongBlock::blockFor(slot)->strongSet();
}

inline HandleSlot StrongSet::tryAllocateFromCurrent()
{
    HandleSlot slot = m_freeListHead;
    if (slot)
        m_freeListHead = StrongBlock::decodeFreeListEntry(*slot);
    else if (m_bumpCursor != m_bumpEnd) [[likely]] {
        slot = m_bumpCursor;
        m_bumpCursor = slot + 1;
    } else [[unlikely]]
        return nullptr;

    m_currentBlock->incrementUsedCount();
    *slot = JSValue();
    return slot;
}

inline HandleSlot StrongSet::allocate()
{
    if (HandleSlot slot = tryAllocateFromCurrent()) [[likely]]
        return slot;
    return allocateSlow();
}

// The current block's free list lives in the owning StrongSet, so freeing into
// it has to go through the set. Every other block owns its own free list and its
// own used count, so it is settled here without the set being touched at all --
// which is the whole reason the block carries an isCurrent() bit rather than the
// set being asked to compare against m_currentBlock.
inline void StrongSet::deallocate(HandleSlot slot)
{
    StrongBlock* block = StrongBlock::blockFor(slot);
    ASSERT(slot >= block->payload() && slot < block->payloadEnd());
    if (block->isCurrent()) {
        block->strongSet()->deallocateFromCurrentBlock(block, slot);
        return;
    }

    block->pushFreeSlot(slot);
    unsigned usedCount = block->decrementUsedCount();
    if (usedCount <= block->freeNotifyThreshold()) [[unlikely]]
        block->strongSet()->didFreeSlot(block, usedCount);
}

inline void StrongSet::deallocateFromCurrentBlock(StrongBlock* block, HandleSlot slot)
{
    ASSERT(block == m_currentBlock);
    *slot = StrongBlock::encodeFreeListEntry(m_freeListHead);
    m_freeListHead = slot;

    // The current block's threshold is always zero, so emptiness is the only
    // thing left to notify about and the threshold need not be loaded.
    ASSERT(!block->freeNotifyThreshold());
    unsigned usedCount = block->decrementUsedCount();
    if (!usedCount) [[unlikely]]
        didFreeSlot(block, usedCount);
}

// Visits every slot handed out at least once, allocated or not, so consumers
// must filter to cells. The functor must not allocate or deallocate handles:
// deallocating the last live slot of a non-current block destroys that block
// mid-iteration, and the walk would continue into freed memory.
void StrongSet::forEachSlot(const Invocable<void(const HandleSlot&)> auto& functor)
{
    for (StrongBlock::BlockListNode& node : m_blocks) {
        StrongBlock& block = node.block();
        // An empty block holds nothing live, and skipping it also skips the whole
        // payload of the spare, whose slots are all free-list links.
        if (block.isEmpty())
            continue;
        // Only the current block can have slots that have never been written, and
        // its bump cursor is hoisted here. Any other block was full when it was
        // retired, so every slot is either live or a free-list link.
        HandleSlot end = &block == m_currentBlock ? m_bumpCursor : block.payloadEnd();
        for (HandleSlot slot = block.payload(); slot < end; ++slot)
            functor(slot);
    }
}

void StrongSet::forEachLiveCell(const Invocable<void(JSCell*)> auto& functor)
{
    forEachSlot([&](HandleSlot slot) {
        JSValue value = *slot;
        if (!value || !value.isCell())
            return;
        functor(value.asCell());
    });
}

void StrongSet::forEachStrongHandle(const Invocable<void(JSCell*)> auto& functor, const HashCountedSet<JSCell*>& skipSet)
{
    forEachLiveCell([&](JSCell* cell) {
        if (!skipSet.contains(cell))
            functor(cell);
    });
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
