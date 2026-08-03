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

#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/JSString.h>
#include <JavaScriptCore/MarkedBlock.h>
#include <JavaScriptCore/ObjectConstructor.h>
#include <JavaScriptCore/StrongBlock.h>
#include <JavaScriptCore/StrongInlines.h>
#include <JavaScriptCore/StrongSet.h>
#include <JavaScriptCore/VM.h>
#include <limits>
#include <optional>
#include <wtf/HashSet.h>
#include <wtf/MainThread.h>
#include <wtf/Vector.h>
#include <wtf/text/MakeString.h>

namespace TestWebKitAPI {

using JSC::HandleSlot;
using JSC::HeapType;
using JSC::JSLockHolder;
using JSC::Strong;
using JSC::StrongBlock;
using JSC::StrongSet;
using JSC::VM;
using JSC::jsString;

TEST(JavaScriptCore_StrongBlock, Geometry)
{
    EXPECT_EQ(StrongBlock::blockSize, JSC::MarkedBlock::blockSize);

#if USE(JSVALUE64)
    // Pinned so a toolchain or ABI change cannot silently shrink capacity. A
    // 64KB-page target, and every non-64-bit target, solves for its own capacity
    // and is checked only against the invariants below, which hold everywhere.
    EXPECT_EQ(StrongBlock::headerSize, 56u);
    if (StrongBlock::blockSize == 16 * KB)
        EXPECT_EQ(StrongBlock::capacity, 2041u);
#endif
    EXPECT_LE(StrongBlock::capacity, std::numeric_limits<uint16_t>::max());
    EXPECT_EQ(StrongBlock::headerSize + StrongBlock::capacity * sizeof(JSC::JSValue), StrongBlock::blockSize);
    EXPECT_EQ(sizeof(StrongBlock), StrongBlock::headerSize);

    StrongBlock* block = StrongBlock::create(nullptr);
    EXPECT_EQ(block->usedCount(), 0u);
    EXPECT_TRUE(block->isEmpty());
    EXPECT_FALSE(block->isFull());
    EXPECT_EQ(std::bit_cast<uintptr_t>(block->slotAtIndex(0)) - std::bit_cast<uintptr_t>(block), StrongBlock::headerSize);
    EXPECT_EQ(block->payloadEnd(), block->slotAtIndex(0) + StrongBlock::capacity);
    delete block;
}

// An untagged slot address has none of NotCellMask's bits set, so marking would
// read it as a live cell and follow it into the middle of a StrongBlock.
TEST(JavaScriptCore_StrongBlock, FreeListEntriesAreTaggedNonCells)
{
    StrongBlock* block = StrongBlock::create(nullptr);

    EXPECT_FALSE(StrongBlock::encodeFreeListEntry(nullptr).isCell());
    EXPECT_EQ(StrongBlock::decodeFreeListEntry(StrongBlock::encodeFreeListEntry(nullptr)), nullptr);
    for (unsigned i = 0; i < StrongBlock::capacity; ++i) {
        HandleSlot slot = block->slotAtIndex(i);
        JSC::JSValue encoded = StrongBlock::encodeFreeListEntry(slot);
        EXPECT_FALSE(encoded.isCell());
        EXPECT_EQ(StrongBlock::decodeFreeListEntry(encoded), slot);
    }

    delete block;
}

class StrongSetTest : public testing::Test {
protected:
    void SetUp() final
    {
        WTF::initializeMainThread();
        JSC::initialize();
        m_vm = VM::create(HeapType::Large);
        m_locker.emplace(*m_vm);
        m_set.emplace(*m_vm);
    }

    void TearDown() final
    {
        m_set.reset();
        // VM::~VM asserts the API lock is held, so the VM must go before the
        // locker does.
        m_vm = nullptr;
        m_locker.reset();
    }

    VM& vm() { return *m_vm; }
    StrongSet& set() { return *m_set; }

    Vector<HandleSlot> fill(unsigned count = StrongBlock::capacity)
    {
        Vector<HandleSlot> slots(count, [&](size_t) {
            return m_set->allocate();
        });
        return slots;
    }

    void drain(const Vector<HandleSlot>& slots)
    {
        for (HandleSlot slot : slots)
            StrongSet::deallocate(slot);
    }

    // Frees in a non-monotonic order, so a bug that only preserved a monotonic
    // run would show up. The stride is coprime with capacity's small factors.
    Vector<HandleSlot> drainScattered(const Vector<HandleSlot>& slots, unsigned count)
    {
        Vector<HandleSlot> freed;
        HashSet<unsigned> freedIndices;
        unsigned index = 0;
        while (freed.size() < count) {
            index = (index + 977) % slots.size();
            if (!freedIndices.add(index).isNewEntry)
                continue;
            freed.append(slots[index]);
            StrongSet::deallocate(slots[index]);
        }
        return freed;
    }

private:
    RefPtr<VM> m_vm;
    std::optional<JSLockHolder> m_locker;
    std::optional<StrongSet> m_set;
};

TEST_F(StrongSetTest, Addressing)
{
    Vector<HandleSlot> slots = fill();
    StrongBlock* block = StrongBlock::blockFor(slots[0]);
    for (unsigned i = 0; i < slots.size(); ++i) {
        EXPECT_EQ(StrongBlock::blockFor(slots[i]), block);
        EXPECT_EQ(StrongSet::setFor(slots[i]), &set());
        EXPECT_EQ(block->indexOf(slots[i]), i);
    }
    EXPECT_TRUE(block->isFull());
    EXPECT_EQ(set().blockCount(), 1u);
    drain(slots);
    EXPECT_TRUE(block->isEmpty());
}

TEST_F(StrongSetTest, FirstBlockIsLazy)
{
    EXPECT_EQ(set().blockCount(), 0u);
    HandleSlot slot = set().allocate();
    EXPECT_EQ(set().blockCount(), 1u);
    StrongSet::deallocate(slot);
}

TEST_F(StrongSetTest, GrowsAcrossBlocks)
{
    Vector<HandleSlot> slots = fill(StrongBlock::capacity + 5);
    EXPECT_EQ(set().blockCount(), 2u);
    drain(slots);
}

// A free that went onto the block's own list instead of the hoisted one, which
// allocate() does not read, would leak the slot rather than reuse it.
TEST_F(StrongSetTest, FreeWhileStillBumpIsReusedImmediately)
{
    HandleSlot first = set().allocate();
    HandleSlot second = set().allocate();
    StrongBlock* block = StrongBlock::blockFor(first);
    EXPECT_EQ(block->indexOf(first), 0u);
    EXPECT_EQ(block->indexOf(second), 1u);

    StrongSet::deallocate(first);
    EXPECT_EQ(block->usedCount(), 1u);

    HandleSlot third = set().allocate();
    EXPECT_EQ(third, first);
    EXPECT_EQ(block->usedCount(), 2u);

    // The free list is drained, so the bump run resumes at index 2.
    HandleSlot fourth = set().allocate();
    EXPECT_EQ(block->indexOf(fourth), 2u);

    // Freeing two and allocating three repeatedly alternates between draining the
    // free list in LIFO order and resuming the bump run.
    HashSet<HandleSlot> live { second, third, fourth };
    Vector<HandleSlot> liveList({ second, third, fourth });
    for (unsigned round = 0; round < 100; ++round) {
        for (unsigned i = 0; i < 2; ++i) {
            HandleSlot slot = liveList.takeLast();
            EXPECT_TRUE(live.remove(slot));
            StrongSet::deallocate(slot);
        }
        for (unsigned i = 0; i < 3; ++i) {
            HandleSlot slot = set().allocate();
            EXPECT_TRUE(live.add(slot).isNewEntry);
            liveList.append(slot);
        }
    }
    EXPECT_EQ(live.size(), liveList.size());
    EXPECT_EQ(block->usedCount(), live.size());
    EXPECT_EQ(set().blockCount(), 1u);
    for (HandleSlot slot : live)
        EXPECT_EQ(StrongBlock::blockFor(slot), block);

    drain(liveList);
}

TEST_F(StrongSetTest, FreeListHandsBackEverySlotExactlyOnce)
{
    Vector<HandleSlot> slots = fill();
    StrongBlock* block = StrongBlock::blockFor(slots[0]);
    EXPECT_TRUE(block->isFull());

    Vector<HandleSlot> freed = drainScattered(slots, 200);
    EXPECT_EQ(block->usedCount(), StrongBlock::capacity - freed.size());

    HashSet<HandleSlot> expected;
    for (HandleSlot slot : freed)
        expected.add(slot);
    // Never the same slot twice, and never a slot that was not free.
    for (unsigned i = 0; i < freed.size(); ++i)
        EXPECT_TRUE(expected.remove(set().allocate()));
    EXPECT_TRUE(expected.isEmpty());

    EXPECT_TRUE(block->isFull());
    EXPECT_EQ(set().blockCount(), 1u);

    drain(slots);
}

TEST_F(StrongSetTest, DrainToEmptyRestoresBumpMode)
{
    Vector<HandleSlot> slots = fill();
    StrongBlock* block = StrongBlock::blockFor(slots[0]);

    // Index 7 is freed last, so a surviving free list would hand it out first.
    for (unsigned i = 0; i < slots.size(); ++i)
        StrongSet::deallocate(slots[(i + 8) % slots.size()]);
    EXPECT_TRUE(block->isEmpty());

    for (unsigned i = 0; i < 3; ++i)
        EXPECT_EQ(block->indexOf(set().allocate()), i);

    // The reset must not have lost capacity either.
    HashSet<HandleSlot> seen;
    for (unsigned i = 3; i < StrongBlock::capacity; ++i)
        EXPECT_TRUE(seen.add(set().allocate()).isNewEntry);
    EXPECT_EQ(block->usedCount(), StrongBlock::capacity);
    EXPECT_EQ(set().blockCount(), 1u);
}

// The second block is still current when it empties, so it is kept in place
// rather than cached as the spare or freed.
TEST_F(StrongSetTest, ReclamationHysteresis)
{
    Vector<HandleSlot> slots = fill();
    EXPECT_EQ(set().blockCount(), 1u);

    for (unsigned iteration = 0; iteration < 10; ++iteration) {
        HandleSlot overflow = set().allocate();
        EXPECT_EQ(set().blockCount(), 2u);
        StrongSet::deallocate(overflow);
        EXPECT_EQ(set().blockCount(), 2u);
    }

    drain(slots);
}

// ReclamationHysteresis above always drains the block under the cursor, which is
// kept for a different reason, so it never reaches this branch.
TEST_F(StrongSetTest, NonCurrentEmptyBlockBecomesSpare)
{
    Vector<HandleSlot> firstBlockSlots = fill();
    HandleSlot secondBlockSlot = set().allocate();
    EXPECT_EQ(set().blockCount(), 2u);

    drain(firstBlockSlots);
    EXPECT_EQ(set().blockCount(), 2u);

    StrongSet::deallocate(secondBlockSlot);
}

// The reverse of NonCurrentEmptyBlockBecomesSpare's drain order. Without a check
// on the non-current path, this leaves an empty current block and a cached spare
// alive at once.
TEST_F(StrongSetTest, CurrentEmptyBeforeSpareLeavesOnlyOneIdleBlock)
{
    Vector<HandleSlot> firstBlockSlots = fill();
    HandleSlot secondBlockSlot = set().allocate();
    EXPECT_EQ(set().blockCount(), 2u);

    // Empty while current with no spare cached yet, so it is kept in place.
    StrongSet::deallocate(secondBlockSlot);
    EXPECT_EQ(set().blockCount(), 2u);

    // Now a second block empties while the current one already is.
    drain(firstBlockSlots);
    EXPECT_EQ(set().blockCount(), 1u);
}

TEST_F(StrongSetTest, EmptyBlocksBeyondSpareAreFreed)
{
    Vector<HandleSlot> slots = fill(StrongBlock::capacity * 4);
    EXPECT_EQ(set().blockCount(), 4u);

    drain(slots);
    EXPECT_LE(set().blockCount(), StrongSet::s_maxIdleBlocks);
}

// A free into a non-current block is settled inside that block. If it reached
// the owning set instead, the slot would land on the current block's hoisted
// free list and be handed out as if it belonged to a different block.
TEST_F(StrongSetTest, FreeIntoNonCurrentBlockLeavesCurrentBlockAlone)
{
    Vector<HandleSlot> aSlots = fill();
    StrongBlock* blockA = StrongBlock::blockFor(aSlots[0]);
    HandleSlot bSlot = set().allocate();
    StrongBlock* blockB = StrongBlock::blockFor(bSlot);
    ASSERT_NE(blockA, blockB);
    EXPECT_FALSE(blockA->isCurrent());
    EXPECT_TRUE(blockB->isCurrent());

    StrongSet::deallocate(aSlots[10]);
    EXPECT_EQ(blockA->usedCount(), StrongBlock::capacity - 1);
    EXPECT_EQ(blockB->usedCount(), 1u);

    // B is still bumping, so it owes index 1 and not A's freed slot.
    HandleSlot next = set().allocate();
    EXPECT_EQ(StrongBlock::blockFor(next), blockB);
    EXPECT_EQ(blockB->indexOf(next), 1u);

    StrongSet::deallocate(next);
    StrongSet::deallocate(bSlot);
    for (unsigned i = 0; i < aSlots.size(); ++i) {
        if (i != 10)
            StrongSet::deallocate(aSlots[i]);
    }
}

TEST_F(StrongSetTest, RetiredFullBlockRejoinsOnlyAtWatermark)
{
    constexpr unsigned watermark = StrongSet::s_reAdmissionWatermark;

    // Spilling into B retires A as full and non-current.
    Vector<HandleSlot> aSlots = fill();
    StrongBlock* blockA = StrongBlock::blockFor(aSlots[0]);
    HandleSlot bSlot = set().allocate();
    EXPECT_NE(StrongBlock::blockFor(bSlot), blockA);
    EXPECT_EQ(set().availabilityCount(), 0u);

    for (unsigned i = 0; i < watermark - 1; ++i) {
        StrongSet::deallocate(aSlots[i]);
        EXPECT_EQ(set().availabilityCount(), 0u);
        EXPECT_FALSE(blockA->isAvailable());
    }
    EXPECT_EQ(blockA->usedCount(), StrongBlock::capacity - (watermark - 1));

    // The watermark-th free is.
    StrongSet::deallocate(aSlots[watermark - 1]);
    EXPECT_EQ(set().availabilityCount(), 1u);
    EXPECT_TRUE(blockA->isAvailable());

    // An already-available block must not be re-admitted again.
    for (unsigned i = watermark; i < watermark + 100; ++i)
        StrongSet::deallocate(aSlots[i]);
    EXPECT_EQ(set().availabilityCount(), 1u);

    for (unsigned i = watermark + 100; i < aSlots.size(); ++i)
        StrongSet::deallocate(aSlots[i]);
    StrongSet::deallocate(bSlot);
}

// A non-current block is taken to have handed out its whole payload, so a
// re-admitted one must allocate off its free list only. Bumping instead would
// hand out a slot past the payload end.
TEST_F(StrongSetTest, ReAdmittedBlockAllocatesOnlyFreedSlots)
{
    constexpr unsigned watermark = StrongSet::s_reAdmissionWatermark;

    Vector<HandleSlot> aSlots = fill();
    StrongBlock* blockA = StrongBlock::blockFor(aSlots[0]);
    HandleSlot bSlot = set().allocate();
    StrongBlock* blockB = StrongBlock::blockFor(bSlot);
    EXPECT_NE(blockB, blockA);

    HashSet<HandleSlot> freed;
    for (HandleSlot slot : drainScattered(aSlots, watermark))
        freed.add(slot);
    EXPECT_EQ(freed.size(), watermark);
    EXPECT_TRUE(blockA->isAvailable());
    EXPECT_EQ(blockA->usedCount(), StrongBlock::capacity - watermark);

    // Retiring B installs A, which has exactly `watermark` slots to give back.
    Vector<HandleSlot> bSlots = fill(StrongBlock::capacity - 1);
    Vector<HandleSlot> reused(watermark, [&](size_t) {
        return set().allocate();
    });
    for (HandleSlot slot : reused) {
        EXPECT_EQ(StrongBlock::blockFor(slot), blockA);
        EXPECT_TRUE(freed.remove(slot));
    }
    EXPECT_TRUE(freed.isEmpty());
    EXPECT_TRUE(blockA->isFull());

    // A is spent, so the next allocation cannot come from it.
    HandleSlot overflow = set().allocate();
    EXPECT_NE(StrongBlock::blockFor(overflow), blockA);

    // Enumerated before anything is freed: A is destroyed by its own last free.
    Vector<HandleSlot> survivors;
    for (unsigned i = 0; i < StrongBlock::capacity; ++i) {
        HandleSlot slot = blockA->slotAtIndex(i);
        if (!reused.contains(slot))
            survivors.append(slot);
    }

    StrongSet::deallocate(overflow);
    drain(bSlots);
    StrongSet::deallocate(bSlot);
    drain(reused);
    drain(survivors);
}

// The spare's counterpart to DrainToEmptyRestoresBumpMode: that one keeps the
// emptied block current, this one reinstalls it after a switch.
TEST_F(StrongSetTest, ReinstalledSpareBumpsFromTheStart)
{
    Vector<HandleSlot> aSlots = fill();
    StrongBlock* blockA = StrongBlock::blockFor(aSlots[0]);
    HandleSlot bSlot = set().allocate();
    EXPECT_NE(StrongBlock::blockFor(bSlot), blockA);

    // A empties while non-current, so it is cached as the spare.
    drain(aSlots);
    EXPECT_TRUE(blockA->isEmpty());
    EXPECT_EQ(set().blockCount(), 2u);

    // Retiring B installs the spare, which must bump from index 0 rather than
    // from wherever it stood when it was retired full.
    Vector<HandleSlot> bSlots = fill(StrongBlock::capacity - 1);
    for (unsigned i = 0; i < 3; ++i) {
        HandleSlot slot = set().allocate();
        EXPECT_EQ(StrongBlock::blockFor(slot), blockA);
        EXPECT_EQ(blockA->indexOf(slot), i);
    }

    EXPECT_EQ(set().blockCount(), 2u);
    for (unsigned i = 0; i < 3; ++i)
        StrongSet::deallocate(blockA->slotAtIndex(i));
    drain(bSlots);
    StrongSet::deallocate(bSlot);
}

// Two blocks on the chain at once need a third, current block to keep both off
// the cursor. A head-insert bug would hand them back in the reverse order.
TEST_F(StrongSetTest, AvailableChainDrainsInAppendOrder)
{
    constexpr unsigned watermark = StrongSet::s_reAdmissionWatermark;

    Vector<HandleSlot> aSlots = fill();
    StrongBlock* blockA = StrongBlock::blockFor(aSlots[0]);
    Vector<HandleSlot> bSlots = fill();
    StrongBlock* blockB = StrongBlock::blockFor(bSlots[0]);
    Vector<HandleSlot> cSlots = fill();
    StrongBlock* blockC = StrongBlock::blockFor(cSlots[0]);
    EXPECT_NE(blockA, blockB);
    EXPECT_NE(blockC, blockA);
    EXPECT_NE(blockC, blockB);
    EXPECT_EQ(set().blockCount(), 3u);

    // A and B cross the watermark while C is current, in that order.
    for (unsigned i = 0; i < watermark; ++i)
        StrongSet::deallocate(aSlots[i]);
    for (unsigned i = 0; i < watermark; ++i)
        StrongSet::deallocate(bSlots[i]);
    EXPECT_EQ(set().availabilityCount(), 2u);

    // C is full and current, so these fall through to the chain.
    Vector<HandleSlot> fromChain;
    for (unsigned i = 0; i < watermark; ++i) {
        HandleSlot slot = set().allocate();
        EXPECT_EQ(StrongBlock::blockFor(slot), blockA);
        fromChain.append(slot);
    }
    // That refilled A exactly, so these fall through to the chain again.
    for (unsigned i = 0; i < watermark; ++i) {
        HandleSlot slot = set().allocate();
        EXPECT_EQ(StrongBlock::blockFor(slot), blockB);
        fromChain.append(slot);
    }
    EXPECT_EQ(set().blockCount(), 3u);

    for (unsigned i = watermark; i < aSlots.size(); ++i)
        StrongSet::deallocate(aSlots[i]);
    for (unsigned i = watermark; i < bSlots.size(); ++i)
        StrongSet::deallocate(bSlots[i]);
    drain(fromChain);
    drain(cSlots);
}

// Leaving the block on the chain as well as cached as the spare would let a later
// allocateSlow() pull it off the chain and make it current while the spare still
// points at it; the next empty would then free the live current block.
TEST_F(StrongSetTest, AvailableChainUnlinksBlockThatEmptiesWhileAvailable)
{
    // B stays current for the rest of the test, so nothing forces allocateSlow()
    // to consult the chain until the explicit allocate() below.
    Vector<HandleSlot> aSlots = fill();
    StrongBlock* blockA = StrongBlock::blockFor(aSlots[0]);
    HandleSlot bSlot = set().allocate();
    EXPECT_NE(StrongBlock::blockFor(bSlot), blockA);
    ASSERT_EQ(set().blockCount(), 2u);

    // A joins the chain partway through this, and the last free then empties it
    // while it is still linked there.
    drain(aSlots);
    EXPECT_EQ(set().availabilityCount(), 1u);
    EXPECT_EQ(set().availableBlockCount(), 0u);
    EXPECT_FALSE(blockA->isAvailable());
    // A is the spare now, and still linked on the block list.
    ASSERT_EQ(set().blockCount(), 2u);

    Vector<HandleSlot> bSlots = fill(StrongBlock::capacity - 1);
    bSlots.append(bSlot);
    HandleSlot fromA = set().allocate();
    ASSERT_EQ(StrongBlock::blockFor(fromA), blockA);
    ASSERT_EQ(set().blockCount(), 2u);

    StrongSet::deallocate(fromA);
    ASSERT_EQ(set().blockCount(), 2u);

    drain(bSlots);
}

// The other exit from the chain: a linked block empties and is freed outright
// rather than cached as the spare. A block left linked here would leave the
// chain holding a node in freed memory.
TEST_F(StrongSetTest, ChainIsEmptyAfterLinkedBlocksAreDestroyed)
{
    constexpr unsigned watermark = StrongSet::s_reAdmissionWatermark;

    auto drainRemaining = [&](const Vector<HandleSlot>& all, const Vector<HandleSlot>& alreadyFreed) {
        HashSet<HandleSlot> freed;
        for (HandleSlot slot : alreadyFreed)
            freed.add(slot);
        for (HandleSlot slot : all) {
            if (!freed.contains(slot))
                StrongSet::deallocate(slot);
        }
    };

    Vector<HandleSlot> aSlots = fill();
    StrongBlock* blockA = StrongBlock::blockFor(aSlots[0]);
    Vector<HandleSlot> bSlots = fill();
    StrongBlock* blockB = StrongBlock::blockFor(bSlots[0]);
    HandleSlot cSlot = set().allocate();
    ASSERT_NE(blockA, blockB);
    ASSERT_NE(StrongBlock::blockFor(cSlot), blockB);
    ASSERT_EQ(set().blockCount(), 3u);
    EXPECT_EQ(set().availableBlockCount(), 0u);

    // Both retired blocks cross the watermark, so both are on the chain at once.
    Vector<HandleSlot> aFreed = drainScattered(aSlots, watermark);
    EXPECT_EQ(set().availableBlockCount(), 1u);
    Vector<HandleSlot> bFreed = drainScattered(bSlots, watermark);
    EXPECT_EQ(set().availableBlockCount(), 2u);
    EXPECT_TRUE(blockA->isAvailable());
    EXPECT_TRUE(blockB->isAvailable());

    // A empties while linked. C is current and non-empty and there is no spare
    // yet, so A is unlinked and cached rather than freed.
    drainRemaining(aSlots, aFreed);
    EXPECT_EQ(set().availableBlockCount(), 1u);
    EXPECT_FALSE(blockA->isAvailable());
    ASSERT_EQ(set().blockCount(), 3u);

    // B empties while linked with a spare already cached, so it is unlinked and
    // then destroyed.
    drainRemaining(bSlots, bFreed);
    EXPECT_EQ(set().availableBlockCount(), 0u);
    ASSERT_EQ(set().blockCount(), 2u);

    // Nothing on the chain, so this must come from the spare or a fresh block,
    // never from a freed one.
    HandleSlot next = set().allocate();
    EXPECT_NE(StrongBlock::blockFor(next), blockB);

    StrongSet::deallocate(next);
    StrongSet::deallocate(cSlot);
}

// The pattern s_reAdmissionWatermark exists to defeat: keep a block full, free one
// slot, immediately take it back. The thrashed block must not be the current one,
// which is never re-admitted, so both A and B are held full; each cycle's
// re-allocation can then only come from a block the allocator re-admitted.
TEST_F(StrongSetTest, ThrashPatternCausesNoRepeatedEligibilityTransitions)
{
    Vector<HandleSlot> aSlots = fill();
    StrongBlock* blockA = StrongBlock::blockFor(aSlots[0]);
    Vector<HandleSlot> bSlots = fill();
    EXPECT_NE(StrongBlock::blockFor(bSlots[0]), blockA);
    EXPECT_EQ(set().blockCount(), 2u);
    EXPECT_EQ(set().availabilityCount(), 0u);

    constexpr unsigned cycles = 20000;
    for (unsigned i = 0; i < cycles; ++i) {
        unsigned index = i % StrongBlock::capacity;
        StrongSet::deallocate(aSlots[index]);
        StrongSet::deallocate(bSlots[index]);
        aSlots[index] = set().allocate();
        bSlots[index] = set().allocate();
    }

    // Deliberately absolute: a bound of cycles / watermark would relax when the
    // watermark was lowered, and would still pass at a watermark of 1, which is
    // the regression this test exists to catch.
    EXPECT_LE(set().availabilityCount(), cycles / 100);
    EXPECT_LE(set().blockCount(), 4u);

    HashSet<HandleSlot> distinct;
    for (HandleSlot slot : aSlots)
        EXPECT_TRUE(distinct.add(slot).isNewEntry);
    for (HandleSlot slot : bSlots)
        EXPECT_TRUE(distinct.add(slot).isNewEntry);
    EXPECT_EQ(distinct.size(), StrongBlock::capacity * 2);

    drain(aSlots);
    drain(bSlots);
}

// A skipped cursor save or reload across a block switch makes a block resume from
// a stale cursor and either hand out a slot that is already live or forget its
// free list. Both are checked as they would happen.
TEST_F(StrongSetTest, CursorHandoffAcrossBlockSwitchLosesNoSlot)
{
    constexpr unsigned watermark = StrongSet::s_reAdmissionWatermark;

    // Tracks every live slot, so a duplicate is caught as it is handed out.
    HashSet<HandleSlot> live;
    auto allocateLive = [&]() -> HandleSlot {
        HandleSlot slot = set().allocate();
        EXPECT_TRUE(live.add(slot).isNewEntry);
        return slot;
    };

    Vector<HandleSlot> aSlots = fill();
    live.addAll(aSlots);
    StrongBlock* blockA = StrongBlock::blockFor(aSlots[0]);
    HandleSlot bFirst = allocateLive();
    StrongBlock* blockB = StrongBlock::blockFor(bFirst);
    EXPECT_NE(blockA, blockB);

    // Scattered, so A's free list is a genuine chain rather than a run the bump
    // cursor could reproduce by accident. This also carries A onto the chain.
    constexpr unsigned freedFromA = watermark + 37;
    Vector<HandleSlot> freedSlots = drainScattered(aSlots, freedFromA);
    for (HandleSlot slot : freedSlots)
        EXPECT_TRUE(live.remove(slot));
    EXPECT_TRUE(blockA->isAvailable());
    EXPECT_EQ(blockA->usedCount(), StrongBlock::capacity - freedFromA);

    // Exhausting B switches the next allocation back to A.
    for (unsigned i = 1; i < StrongBlock::capacity; ++i)
        allocateLive();
    EXPECT_EQ(set().blockCount(), 2u);

    // A must hand back exactly the slots freed from it: all of them, none twice,
    // nothing else.
    HashSet<HandleSlot> expected;
    for (HandleSlot slot : freedSlots)
        expected.add(slot);
    for (unsigned i = 0; i < freedFromA; ++i) {
        HandleSlot slot = allocateLive();
        EXPECT_EQ(StrongBlock::blockFor(slot), blockA);
        EXPECT_TRUE(expected.remove(slot));
    }
    EXPECT_TRUE(expected.isEmpty());
    EXPECT_TRUE(blockA->isFull());
    EXPECT_TRUE(blockB->isFull());
    EXPECT_EQ(set().blockCount(), 2u);
    EXPECT_EQ(live.size(), StrongBlock::capacity * 2);

    // Bounce back and forth, forcing repeated save/reload cycles.
    for (unsigned round = 0; round < 5; ++round) {
        Vector<HandleSlot> victims;
        for (HandleSlot slot : live) {
            if (victims.size() >= watermark)
                break;
            victims.append(slot);
        }
        for (HandleSlot slot : victims) {
            EXPECT_TRUE(live.remove(slot));
            StrongSet::deallocate(slot);
        }
        for (unsigned i = 0; i < watermark; ++i)
            allocateLive();
        EXPECT_EQ(live.size(), StrongBlock::capacity * 2);
        EXPECT_LE(set().blockCount(), 3u);
    }

    drain(copyToVector(live));
}

TEST_F(StrongSetTest, ForEachStrongHandleAcrossBlocks)
{
    auto* globalObject = JSC::JSGlobalObject::create(vm(), JSC::JSGlobalObject::createStructure(vm(), JSC::jsNull()));

    Vector<HandleSlot> slots = fill(StrongBlock::capacity + 10);
    for (HandleSlot slot : slots)
        *slot = globalObject;

    // Neither a non-cell value nor a freed slot's free-list link may be reported.
    HandleSlot numberSlot = set().allocate();
    *numberSlot = JSC::jsNumber(42);
    Vector<HandleSlot> freed;
    for (unsigned i = 0; i < 10; ++i)
        freed.append(slots[i]);
    for (HandleSlot slot : freed) {
        StrongSet::deallocate(slot);
        JSC::JSValue link = *slot;
        EXPECT_FALSE(link.isCell());
    }
    unsigned expectedCount = slots.size() - freed.size();

    WTF::HashCountedSet<JSC::JSCell*> emptySkipSet;
    unsigned count = 0;
    set().forEachStrongHandle([&](JSC::JSCell*) { ++count; }, emptySkipSet);
    EXPECT_EQ(count, expectedCount);

    WTF::HashCountedSet<JSC::JSCell*> skipSet;
    skipSet.add(globalObject);
    unsigned skipped = 0;
    set().forEachStrongHandle([&](JSC::JSCell*) { ++skipped; }, skipSet);
    EXPECT_EQ(skipped, 0u);

    for (unsigned i = freed.size(); i < slots.size(); ++i)
        StrongSet::deallocate(slots[i]);
    StrongSet::deallocate(numberSlot);
}

// allocate() leaves a slot as JSValue(), and on USE(JSVALUE64) that all-zero
// pattern satisfies isCell() with asCell() == nullptr, so the iteration must
// skip it on the emptiness test, not on isCell().
TEST_F(StrongSetTest, ForEachStrongHandleSkipsEmptySlots)
{
    auto* globalObject = JSC::JSGlobalObject::create(vm(), JSC::JSGlobalObject::createStructure(vm(), JSC::jsNull()));

    Vector<HandleSlot> slots = fill(5);
    *slots[0] = globalObject;
    *slots[1] = globalObject;
    *slots[2] = JSC::constructEmptyObject(globalObject);
    *slots[3] = JSC::jsNumber(42);
    // slots[4] is allocated but never assigned.

    unsigned count = 0;
    HashCountedSet<JSC::JSCell*> skipSet;
    set().forEachStrongHandle([&](JSC::JSCell*) {
        ++count;
    }, skipSet);
    EXPECT_EQ(count, 3u);

    drain(slots);
}

TEST_F(StrongSetTest, StrongHandlesSurviveGCAcrossBlocks)
{
    constexpr unsigned count = StrongBlock::capacity + 100;
    Vector<Strong<JSC::JSString>> handles;
    handles.reserveInitialCapacity(count);
    for (unsigned i = 0; i < count; ++i)
        handles.append(Strong<JSC::JSString>(vm(), jsString(vm(), String::number(i))));

    // Unreferenced garbage, to make the GC do real work.
    for (unsigned i = 0; i < 1000; ++i)
        jsString(vm(), makeString("noise"_s, i));

    vm().heap.collectSync(JSC::CollectionScope::Full);

    for (unsigned i = 0; i < count; ++i) {
        ASSERT_TRUE(!!handles[i]);
        auto value = handles[i]->tryGetValue();
        EXPECT_EQ(static_cast<const String&>(value), String::number(i));
    }
}

// Without a write barrier, the plain slot walk is all that marks these
// transitions.
TEST_F(StrongSetTest, SetTransitionsAreMarkedCorrectly)
{
    JSC::Strong<JSC::Unknown> handle(vm(), jsString(vm(), String("survivor"_s)));
    vm().heap.collectSync(JSC::CollectionScope::Full);
    ASSERT_TRUE(handle.get().isCell());
    {
        auto value = JSC::asString(handle.get())->tryGetValue();
        EXPECT_EQ(static_cast<const String&>(value), "survivor"_s);
    }

    // cell -> non-cell. appendUnbarriered(JSValue) must discard the non-cell.
    handle.set(vm(), JSC::jsNumber(7));
    vm().heap.collectSync(JSC::CollectionScope::Full);
    EXPECT_TRUE(handle.get() == JSC::jsNumber(7));

    // non-cell -> cell.
    handle.set(vm(), jsString(vm(), String("again"_s)));
    vm().heap.collectSync(JSC::CollectionScope::Full);
    ASSERT_TRUE(handle.get().isCell());
    {
        auto value = JSC::asString(handle.get())->tryGetValue();
        EXPECT_EQ(static_cast<const String&>(value), "again"_s);
    }

    // cell -> empty. deallocate() overwrites the slot with a non-cell link.
    handle.clear();
    vm().heap.collectSync(JSC::CollectionScope::Full);
    EXPECT_FALSE(!!handle);
}

TEST_F(StrongSetTest, ChurnDoesNotGrowBlockCount)
{
    auto churn = [&] {
        Vector<Strong<JSC::JSString>> handles;
        for (unsigned i = 0; i < 500; ++i)
            handles.append(Strong<JSC::JSString>(vm(), jsString(vm(), String::number(i))));
    };

    // The baseline is measured, not assumed: a real VM has other subsystems
    // holding handles, and only unbounded growth is a failure.
    churn();
    unsigned baseline = vm().heap.strongSet()->blockCount();

    for (unsigned iteration = 0; iteration < 50; ++iteration)
        churn();

    EXPECT_LE(vm().heap.strongSet()->blockCount(), baseline + 2);
}

} // namespace TestWebKitAPI
