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

#include <JavaScriptCore/DeferGC.h>
#include <JavaScriptCore/HeapCellInlines.h>
#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSString.h>
#include <JavaScriptCore/StrongInlines.h>
#include <JavaScriptCore/VM.h>
#include <JavaScriptCore/WeakBlock.h>
#include <JavaScriptCore/WeakHandleOwner.h>
#include <JavaScriptCore/WeakInlines.h>
#include <JavaScriptCore/WeakSet.h>
#include <atomic>
#include <limits>
#include <optional>
#include <wtf/Condition.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Threading.h>
#include <wtf/UniqueArray.h>
#include <wtf/Vector.h>
#include <wtf/WeakRandom.h>
#include <wtf/text/MakeString.h>

namespace TestWebKitAPI {

using JSC::DeferGC;
using JSC::HeapType;
using JSC::JSCell;
using JSC::JSLockHolder;
using JSC::JSValue;
using JSC::Strong;
using JSC::VM;
using JSC::Weak;
using JSC::WeakBlock;
using JSC::WeakHandleOwner;
using JSC::WeakImpl;
using JSC::WeakSet;
using JSC::jsString;

static constexpr unsigned offThreadSweepRounds = 100;
static constexpr unsigned operationsPerRound = 400;
static constexpr unsigned operationsPerSweepWait = 50;
static constexpr unsigned stringCount = 4000;

static constexpr size_t slotsPerBlock = WeakBlock::weakImplCount();
static constexpr size_t maxTrackedHandlesPerMutator = 8 * slotsPerBlock;

// Records every finalize() call and checks it against what the handle's history allows. A handle
// allocated in round r is made Dead by the reap that ends round r.
class FinalizeLedger final : public WeakHandleOwner {
    WTF_MAKE_TZONE_ALLOCATED(FinalizeLedger);
    WTF_MAKE_NONCOPYABLE(FinalizeLedger);
public:
    explicit FinalizeLedger(unsigned capacity)
        : m_entries(makeUniqueArray<Entry>(capacity))
        , m_capacity(capacity)
    {
    }

    unsigned round() const { return m_round.load(); }
    void didReap() { ++m_round; }

    void* add(JSCell* cell)
    {
        unsigned index = m_size.fetch_add(1);
        RELEASE_ASSERT(index < m_capacity);
        Entry& entry = m_entries[index];
        entry.cell.store(cell);
        entry.allocatedRound.store(round());
        return std::bit_cast<void*>(static_cast<uintptr_t>(index) + 1);
    }

    void didClear(void* context)
    {
        entryFor(context).clearedRound.store(round());
    }

    unsigned finalizeCount(void* context) const
    {
        return entryFor(context).finalizeCount.load();
    }

    FinalizeResult finalize(JSC::Handle<JSC::Unknown> handle, void* context) final
    {
        uintptr_t index = std::bit_cast<uintptr_t>(context) - 1;
        if (index >= m_size.load()) {
            ++m_unknownContexts;
            return FinalizeResult::KeepImpl;
        }
        Entry& entry = m_entries[index];
        unsigned round = this->round();
        if (handle.get() != JSValue(entry.cell.load()))
            ++m_wrongValues;
        if (entry.allocatedRound.load() >= round)
            ++m_finalizedBeforeReap;
        if (entry.clearedRound.load() < round)
            ++m_finalizedAfterClear;
        if (entry.finalizeCount.fetch_add(1))
            ++m_finalizedTwice;
        return FinalizeResult::KeepImpl;
    }

    void expectNoViolations() const
    {
        EXPECT_EQ(m_unknownContexts.load(), 0u);
        EXPECT_EQ(m_wrongValues.load(), 0u);
        EXPECT_EQ(m_finalizedBeforeReap.load(), 0u);
        EXPECT_EQ(m_finalizedAfterClear.load(), 0u);
        EXPECT_EQ(m_finalizedTwice.load(), 0u);
    }

private:
    static constexpr unsigned notCleared = std::numeric_limits<unsigned>::max();

    struct Entry {
        std::atomic<JSCell*> cell { nullptr };
        std::atomic<unsigned> allocatedRound { 0 };
        std::atomic<unsigned> clearedRound { notCleared };
        std::atomic<unsigned> finalizeCount { 0 };
    };

    Entry& entryFor(void* context) const
    {
        uintptr_t index = std::bit_cast<uintptr_t>(context) - 1;
        RELEASE_ASSERT(index < m_size.load());
        return m_entries[index];
    }

    UniqueArray<Entry> m_entries;
    unsigned m_capacity;
    std::atomic<unsigned> m_size { 0 };
    std::atomic<unsigned> m_round { 0 };
    std::atomic<unsigned> m_unknownContexts { 0 };
    std::atomic<unsigned> m_wrongValues { 0 };
    std::atomic<unsigned> m_finalizedBeforeReap { 0 };
    std::atomic<unsigned> m_finalizedAfterClear { 0 };
    std::atomic<unsigned> m_finalizedTwice { 0 };
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(FinalizeLedger);

struct Population {
    WeakSet* weakSet;
    Vector<JSCell*> cells;
};

struct TrackedHandle {
    WeakImpl* impl;
    void* context;
    unsigned allocatedRound;
};

struct TrackedHandles {
    WeakSet* weakSet;
    const Vector<TrackedHandle>* handles;
};

class JavaScriptCore_WeakSet : public testing::Test {
protected:
    void SetUp() final
    {
        WTF::initializeMainThread();
        JSC::initialize();
        m_vm = VM::create(HeapType::Large);
        m_locker.emplace(*m_vm);
    }

    void TearDown() final
    {
        m_roots.clear();
        // VM::~VM asserts the API lock is held, so the VM must go before the
        // locker does. Tearing the VM down finalizes any handle a failed test
        // left behind, so the ledger has to outlive it.
        m_vm = nullptr;
        m_locker.reset();
        m_ledger = nullptr;
    }

    VM& vm() { return *m_vm; }
    JSC::Heap& heap() { return m_vm->heap; }

    FinalizeLedger& makeLedger(unsigned capacity)
    {
        m_ledger = makeUnique<FinalizeLedger>(capacity);
        return *m_ledger;
    }

    // Largest population first. Strong roots keep the cells alive whether or not GC is deferred.
    Vector<Population> populationsByWeakSet()
    {
        HashMap<WeakSet*, Vector<JSCell*>> cellsByWeakSet;
        for (unsigned i = 0; i < stringCount; ++i) {
            JSC::JSString* string = jsString(vm(), makeString("weak-set-test-"_s, i));
            m_roots.append(Strong<JSC::JSString>(vm(), string));
            cellsByWeakSet.add(&string->cellContainer().weakSet(), Vector<JSCell*> { }).iterator->value.append(string);
        }

        Vector<Population> populations;
        for (auto& entry : cellsByWeakSet)
            populations.append({ entry.key, WTF::move(entry.value) });
        std::ranges::sort(populations, std::ranges::greater { }, [](const Population& population) {
            return population.cells.size();
        });
        for (auto& population : populations)
            EXPECT_FALSE(population.weakSet->head());
        return populations;
    }

private:
    RefPtr<VM> m_vm;
    std::optional<JSLockHolder> m_locker;
    Vector<Strong<JSC::JSString>> m_roots;
    std::unique_ptr<FinalizeLedger> m_ledger;
};

static HashSet<WeakBlock*> attachedBlocks(WeakSet& weakSet)
{
    HashSet<WeakBlock*> blocks;
    weakSet.forEachBlock([&](WeakBlock& block) {
        EXPECT_TRUE(blocks.add(&block).isNewEntry);
    });
    return blocks;
}

static Vector<WeakImpl*> allocateHandles(JSCell* cell, size_t count, WeakHandleOwner* owner = nullptr, FinalizeLedger* ledger = nullptr)
{
    return Vector<WeakImpl*>(count, [&](size_t) {
        return WeakSet::allocate(cell, owner, ledger ? ledger->add(cell) : nullptr);
    });
}

static void deallocateHandles(const Vector<WeakImpl*>& handles)
{
    for (WeakImpl* handle : handles)
        WeakSet::deallocate(handle);
}

static void churnOnce(const Population& population, Vector<TrackedHandle>& handles, FinalizeLedger& ledger, WeakRandom& random)
{
    bool shouldAllocate = handles.isEmpty() || (handles.size() < maxTrackedHandlesPerMutator && random.getUint32(5) < 3);
    if (shouldAllocate) {
        JSCell* cell = population.cells[random.getUint32(population.cells.size())];
        bool hasOwner = random.getUint32(4);
        void* context = hasOwner ? ledger.add(cell) : nullptr;
        WeakImpl* impl = WeakSet::allocate(cell, hasOwner ? &ledger : nullptr, context);
        handles.append({ impl, context, ledger.round() });
        return;
    }

    unsigned index = random.getUint32(handles.size());
    TrackedHandle handle = handles[index];
    handles[index] = handles.last();
    handles.removeLast();
    WeakSet::deallocate(handle.impl);
    if (handle.context)
        ledger.didClear(handle.context);
}

static void clearTrackedHandles(Vector<TrackedHandle>& handles, FinalizeLedger& ledger)
{
    for (auto& handle : handles) {
        WeakSet::deallocate(handle.impl);
        if (handle.context)
            ledger.didClear(handle.context);
    }
    handles.clear();
}

struct BlockTally {
    unsigned allocated { 0 };
    unsigned live { 0 };
    unsigned finalized { 0 };
};

// Call only when nothing else is touching these WeakSets, right after the calling thread swept each
// of them so that no handle is left Dead. Every handle allocated into these WeakSets must be listed.
static void expectQuiescentConsistency(JSC::Heap& heap, const FinalizeLedger& ledger, const Vector<TrackedHandles>& lists)
{
    unsigned round = ledger.round();

    HashMap<WeakBlock*, WeakSet*> attachedTo;
    HashSet<WeakSet*> walkedWeakSets;
    for (auto& list : lists) {
        if (!walkedWeakSets.add(list.weakSet).isNewEntry)
            continue;
        list.weakSet->forEachBlock([&](WeakBlock& block) {
            EXPECT_TRUE(attachedTo.add(&block, list.weakSet).isNewEntry) << "a block is attached twice";
        });
    }

    unsigned wrongStates = 0;
    unsigned wrongFinalizeCounts = 0;
    HashMap<WeakBlock*, BlockTally> tallies;
    HashMap<WeakBlock*, WeakSet*> blockUsers;
    for (auto& list : lists) {
        for (auto& handle : *list.handles) {
            bool wasReaped = handle.allocatedRound < round;
            WeakImpl::State state = handle.impl->state();
            if (state != (wasReaped ? WeakImpl::Finalized : WeakImpl::Live))
                ++wrongStates;
            if (handle.context && ledger.finalizeCount(handle.context) != (wasReaped ? 1u : 0u))
                ++wrongFinalizeCounts;

            WeakBlock* block = WeakBlock::blockFor(handle.impl);
            auto addResult = blockUsers.add(block, list.weakSet);
            EXPECT_EQ(addResult.iterator->value, list.weakSet) << "one block holds handles of two WeakSets";

            BlockTally& tally = tallies.add(block, BlockTally { }).iterator->value;
            ++tally.allocated;
            if (state == WeakImpl::Live)
                ++tally.live;
            else if (state == WeakImpl::Finalized)
                ++tally.finalized;
        }
    }
    EXPECT_EQ(wrongStates, 0u);
    EXPECT_EQ(wrongFinalizeCounts, 0u);

    unsigned detachedBlockCount = 0;
    for (auto& entry : tallies) {
        WeakBlock* block = entry.key;
        auto iterator = attachedTo.find(block);
        if (iterator == attachedTo.end()) {
            ++detachedBlockCount;
            EXPECT_EQ(entry.value.live, 0u) << "a block holding live handles is not attached";
            EXPECT_TRUE(block->hasOnlyFinalizedHandles());
            continue;
        }
        EXPECT_EQ(iterator->value, blockUsers.get(block)) << "a block is attached to a WeakSet other than the one its handles belong to";
    }

    HashMap<WeakSet*, unsigned> idleBlocks;
    for (auto& entry : attachedTo) {
        WeakBlock* block = entry.key;
        BlockTally tally = tallies.get(block);
        EXPECT_EQ(block->isEmpty(), !tally.allocated);
        EXPECT_EQ(block->hasLiveHandles(), !!tally.live);
        EXPECT_EQ(block->hasOnlyFinalizedHandles(), tally.allocated && tally.finalized == tally.allocated);
        if (!tally.live)
            ++idleBlocks.add(entry.value, 0).iterator->value;
    }
    // A sweep may leave the allocator's current block in place even though nothing in it is live.
    for (unsigned count : idleBlocks.values())
        EXPECT_LE(count, 1u);

    EXPECT_LE(attachedTo.size() + detachedBlockCount, heap.weakBlockCount());
}

class SweeperThread {
    WTF_MAKE_NONCOPYABLE(SweeperThread);
public:
    explicit SweeperThread(WeakSet& weakSet)
        : m_weakSet(weakSet)
        , m_thread(Thread::create("WeakSet sweeper"_s, [this] { run(); }))
    {
    }

    ~SweeperThread()
    {
        {
            Locker locker { m_lock };
            m_state = State::Stopping;
            m_condition.notifyAll();
        }
        m_thread->waitForCompletion();
    }

    void resume()
    {
        Locker locker { m_lock };
        RELEASE_ASSERT(m_state == State::Paused);
        m_state = State::Running;
        m_condition.notifyAll();
    }

    // Returns once the sweeper is parked between sweeps, so the caller has the WeakSet to itself.
    void pause()
    {
        Locker locker { m_lock };
        RELEASE_ASSERT(m_state == State::Running);
        m_state = State::Pausing;
        while (m_state != State::Paused)
            m_condition.wait(m_lock);
    }

    // Keeps a mutator from finishing its round before the sweeper has had a chance to interleave.
    void waitForNextSweep() const
    {
        unsigned target = m_completedSweeps.load() + 1;
        while (m_completedSweeps.load() < target)
            Thread::yield();
    }

    unsigned completedSweeps() const { return m_completedSweeps.load(); }

private:
    enum class State : uint8_t { Running, Pausing, Paused, Stopping };

    void run()
    {
        while (true) {
            {
                Locker locker { m_lock };
                if (m_state == State::Pausing) {
                    m_state = State::Paused;
                    m_condition.notifyAll();
                }
                while (m_state == State::Paused)
                    m_condition.wait(m_lock);
                if (m_state == State::Stopping)
                    return;
            }
            m_weakSet.finalizeDeadHandles();
            ++m_completedSweeps;
        }
    }

    WeakSet& m_weakSet;
    Lock m_lock;
    Condition m_condition;
    State m_state WTF_GUARDED_BY_LOCK(m_lock) { State::Paused };
    std::atomic<unsigned> m_completedSweeps { 0 };
    Ref<Thread> m_thread;
};

TEST_F(JavaScriptCore_WeakSet, AllocateAndClearAcrossBlocks)
{
    DeferGC deferGC(vm());
    auto populations = populationsByWeakSet();
    WeakSet& weakSet = *populations[0].weakSet;
    JSCell* cell = populations[0].cells[0];

    Vector<WeakImpl*> handles = allocateHandles(cell, 3 * slotsPerBlock + 5);
    HashSet<WeakBlock*> handleBlocks;
    for (WeakImpl* handle : handles) {
        EXPECT_EQ(handle->state(), WeakImpl::Live);
        EXPECT_EQ(handle->jsValue(), JSValue(cell));
        handleBlocks.add(WeakBlock::blockFor(handle));
    }
    EXPECT_EQ(handleBlocks.size(), 4u);
    EXPECT_EQ(attachedBlocks(weakSet), handleBlocks);
    EXPECT_FALSE(weakSet.isEmpty());
    for (WeakBlock* block : handleBlocks) {
        EXPECT_FALSE(block->isEmpty());
        EXPECT_TRUE(block->hasLiveHandles());
        EXPECT_FALSE(block->hasOnlyFinalizedHandles());
    }

    deallocateHandles(handles);
    EXPECT_TRUE(weakSet.isEmpty());
    EXPECT_LE(attachedBlocks(weakSet).size(), 1u);

    weakSet.sweep();
    EXPECT_TRUE(weakSet.isEmpty());
    EXPECT_LE(attachedBlocks(weakSet).size(), 1u);
}

TEST_F(JavaScriptCore_WeakSet, FreedSlotsAreReused)
{
    DeferGC deferGC(vm());
    auto populations = populationsByWeakSet();
    WeakSet& weakSet = *populations[0].weakSet;
    JSCell* cell = populations[0].cells[0];

    Vector<WeakImpl*> handles = allocateHandles(cell, slotsPerBlock);
    WeakBlock* block = WeakBlock::blockFor(handles[0]);
    for (WeakImpl* handle : handles)
        EXPECT_EQ(WeakBlock::blockFor(handle), block);
    EXPECT_FALSE(block->hasFreeCell());

    HashSet<WeakImpl*> freed;
    for (size_t i = 0; i < handles.size(); i += 3) {
        WeakSet::deallocate(handles[i]);
        freed.add(handles[i]);
    }

    // The block is still the allocator's current block, so its own freed slots come back first.
    for (size_t i = 0; i < handles.size(); i += 3) {
        WeakImpl* handle = WeakSet::allocate(cell);
        EXPECT_TRUE(freed.remove(handle));
        handles[i] = handle;
    }
    EXPECT_TRUE(freed.isEmpty());
    EXPECT_EQ(attachedBlocks(weakSet).size(), 1u);

    deallocateHandles(handles);
}

TEST_F(JavaScriptCore_WeakSet, WeakClearsOnDestruction)
{
    DeferGC deferGC(vm());
    auto populations = populationsByWeakSet();
    WeakSet& weakSet = *populations[0].weakSet;
    auto* string = JSC::asString(populations[0].cells[0]);

    {
        Weak<JSC::JSString> weak(string);
        EXPECT_EQ(weak.get(), string);
        EXPECT_FALSE(weakSet.isEmpty());

        weakSet.reap();
        EXPECT_FALSE(weak.get());
        EXPECT_FALSE(weak);
        EXPECT_TRUE(weak.was(string));

        weakSet.sweep();
        EXPECT_EQ(weak.unsafeImpl()->state(), WeakImpl::Finalized);
    }
    EXPECT_TRUE(weakSet.isEmpty());
}

// WeakSet::reap is what the concurrent tests use to make handles Dead without running a collector.
// That only works if a cell nothing has marked reads as unmarked.
TEST_F(JavaScriptCore_WeakSet, ReapKillsHandlesToUnmarkedCells)
{
    DeferGC deferGC(vm());
    auto populations = populationsByWeakSet();
    WeakSet& weakSet = *populations[0].weakSet;

    Vector<WeakImpl*> handles;
    for (JSCell* cell : populations[0].cells)
        handles.append(WeakSet::allocate(cell));

    weakSet.reap();
    for (WeakImpl* handle : handles)
        EXPECT_EQ(handle->state(), WeakImpl::Dead);

    weakSet.sweep();
    for (WeakImpl* handle : handles)
        EXPECT_EQ(handle->state(), WeakImpl::Finalized);

    deallocateHandles(handles);
}

TEST_F(JavaScriptCore_WeakSet, ReapSparesMarkedCells)
{
    auto populations = populationsByWeakSet();
    WeakSet& weakSet = *populations[0].weakSet;
    JSCell* cell = populations[0].cells[0];

    WeakImpl* handle = WeakSet::allocate(cell);
    heap().collectSync(JSC::CollectionScope::Full);
    EXPECT_EQ(handle->state(), WeakImpl::Live);

    weakSet.reap();
    EXPECT_EQ(handle->state(), WeakImpl::Live);

    WeakSet::deallocate(handle);
}

TEST_F(JavaScriptCore_WeakSet, SweepFinalizesEachDeadHandleOnce)
{
    DeferGC deferGC(vm());
    auto populations = populationsByWeakSet();
    WeakSet& weakSet = *populations[0].weakSet;
    JSCell* cell = populations[0].cells[0];
    FinalizeLedger& ledger = makeLedger(2 * slotsPerBlock);

    Vector<void*> contexts;
    Vector<WeakImpl*> handles(2 * slotsPerBlock, [&](size_t) {
        void* context = ledger.add(cell);
        contexts.append(context);
        return WeakSet::allocate(cell, &ledger, context);
    });

    weakSet.sweep();
    for (void* context : contexts)
        EXPECT_EQ(ledger.finalizeCount(context), 0u);

    weakSet.reap();
    ledger.didReap();
    for (WeakImpl* handle : handles)
        EXPECT_EQ(handle->state(), WeakImpl::Dead);

    weakSet.sweep();
    for (size_t i = 0; i < handles.size(); ++i) {
        EXPECT_EQ(handles[i]->state(), WeakImpl::Finalized);
        EXPECT_EQ(ledger.finalizeCount(contexts[i]), 1u);
    }

    weakSet.reap();
    ledger.didReap();
    weakSet.sweep();
    for (size_t i = 0; i < handles.size(); ++i) {
        EXPECT_EQ(handles[i]->state(), WeakImpl::Finalized);
        EXPECT_EQ(ledger.finalizeCount(contexts[i]), 1u);
    }

    for (size_t i = 0; i < handles.size(); ++i) {
        WeakSet::deallocate(handles[i]);
        ledger.didClear(contexts[i]);
    }
    weakSet.sweep();
    for (void* context : contexts)
        EXPECT_EQ(ledger.finalizeCount(context), 1u);
    EXPECT_TRUE(weakSet.isEmpty());
    ledger.expectNoViolations();
}

TEST_F(JavaScriptCore_WeakSet, ClearingDeadHandleSkipsItsFinalizer)
{
    DeferGC deferGC(vm());
    auto populations = populationsByWeakSet();
    WeakSet& weakSet = *populations[0].weakSet;
    JSCell* cell = populations[0].cells[0];
    FinalizeLedger& ledger = makeLedger(2 * slotsPerBlock);

    Vector<void*> contexts;
    Vector<WeakImpl*> handles(2 * slotsPerBlock, [&](size_t) {
        void* context = ledger.add(cell);
        contexts.append(context);
        return WeakSet::allocate(cell, &ledger, context);
    });

    weakSet.reap();
    ledger.didReap();
    for (size_t i = 0; i < handles.size(); i += 2)
        WeakSet::deallocate(handles[i]);

    weakSet.sweep();
    for (size_t i = 0; i < handles.size(); ++i) {
        bool wasCleared = !(i % 2);
        EXPECT_EQ(ledger.finalizeCount(contexts[i]), wasCleared ? 0u : 1u);
        if (!wasCleared) {
            EXPECT_EQ(handles[i]->state(), WeakImpl::Finalized);
            WeakSet::deallocate(handles[i]);
        }
    }
    EXPECT_TRUE(weakSet.isEmpty());
    ledger.expectNoViolations();
}

TEST_F(JavaScriptCore_WeakSet, HandlesWithoutOwnerAreFinalized)
{
    DeferGC deferGC(vm());
    auto populations = populationsByWeakSet();
    WeakSet& weakSet = *populations[0].weakSet;

    Vector<WeakImpl*> handles = allocateHandles(populations[0].cells[0], slotsPerBlock + 1);
    weakSet.reap();
    weakSet.sweep();
    for (WeakImpl* handle : handles)
        EXPECT_EQ(handle->state(), WeakImpl::Finalized);
    deallocateHandles(handles);
    EXPECT_TRUE(weakSet.isEmpty());
}

TEST_F(JavaScriptCore_WeakSet, EmptiedBlockLeavesWeakSetWithoutSweep)
{
    DeferGC deferGC(vm());
    auto populations = populationsByWeakSet();
    ASSERT_GE(populations.size(), 2u);
    WeakSet& weakSet = *populations[0].weakSet;
    JSCell* cell = populations[0].cells[0];

    Vector<WeakImpl*> handles = allocateHandles(cell, 2 * slotsPerBlock + 1);
    WeakBlock* first = WeakBlock::blockFor(handles[0]);
    EXPECT_EQ(WeakBlock::blockFor(handles[slotsPerBlock - 1]), first);
    EXPECT_NE(WeakBlock::blockFor(handles[slotsPerBlock]), first);
    EXPECT_EQ(attachedBlocks(weakSet).size(), 3u);
    unsigned blockCount = heap().weakBlockCount();

    Vector<WeakImpl*> firstBlockHandles { handles.span().first(slotsPerBlock) };
    deallocateHandles(firstBlockHandles);
    EXPECT_FALSE(attachedBlocks(weakSet).contains(first));
    EXPECT_EQ(attachedBlocks(weakSet).size(), 2u);
    EXPECT_LE(heap().weakBlockCount(), blockCount);

    // The emptied block went back to the Heap, so another WeakSet can take it without growing the count.
    Vector<WeakImpl*> otherHandles = allocateHandles(populations[1].cells[0], slotsPerBlock);
    EXPECT_LE(heap().weakBlockCount(), blockCount);

    deallocateHandles(Vector<WeakImpl*> { handles.span().subspan(slotsPerBlock) });
    deallocateHandles(otherHandles);
}

TEST_F(JavaScriptCore_WeakSet, OnlyFinalizedBlockIsDetached)
{
    DeferGC deferGC(vm());
    auto populations = populationsByWeakSet();
    ASSERT_GE(populations.size(), 2u);
    WeakSet& weakSet = *populations[0].weakSet;
    JSCell* cell = populations[0].cells[0];

    Vector<WeakImpl*> handles = allocateHandles(cell, slotsPerBlock + 1);
    WeakBlock* first = WeakBlock::blockFor(handles[0]);
    EXPECT_EQ(WeakBlock::blockFor(handles[slotsPerBlock - 1]), first);
    EXPECT_TRUE(attachedBlocks(weakSet).contains(first));
    unsigned blockCount = heap().weakBlockCount();

    weakSet.reap();
    weakSet.sweep();
    EXPECT_FALSE(attachedBlocks(weakSet).contains(first));
    EXPECT_TRUE(first->hasOnlyFinalizedHandles());
    EXPECT_FALSE(first->isEmpty());
    EXPECT_EQ(heap().weakBlockCount(), blockCount);
    for (WeakImpl* handle : handles)
        EXPECT_EQ(handle->state(), WeakImpl::Finalized);

    // A detached block still has Weak<>s pointing into it, so it must not be handed out again.
    Vector<WeakImpl*> otherHandles = allocateHandles(populations[1].cells[0], 2 * slotsPerBlock);
    for (WeakImpl* handle : otherHandles)
        EXPECT_NE(WeakBlock::blockFor(handle), first);
    EXPECT_FALSE(attachedBlocks(*populations[1].weakSet).contains(first));
    unsigned blockCountWithOther = heap().weakBlockCount();

    // Clearing the last handle in a detached block hands it back to the Heap for reuse.
    Vector<WeakImpl*> firstBlockHandles { handles.span().first(slotsPerBlock) };
    deallocateHandles(firstBlockHandles);
    EXPECT_LE(heap().weakBlockCount(), blockCountWithOther);
    Vector<WeakImpl*> moreHandles = allocateHandles(populations[1].cells[0], slotsPerBlock);
    EXPECT_LE(heap().weakBlockCount(), blockCountWithOther);

    WeakSet::deallocate(handles.last());
    deallocateHandles(otherHandles);
    deallocateHandles(moreHandles);
    weakSet.sweep();
    populations[1].weakSet->sweep();
    EXPECT_TRUE(weakSet.isEmpty());
    EXPECT_TRUE(populations[1].weakSet->isEmpty());
}

TEST_F(JavaScriptCore_WeakSet, ChurnDoesNotGrowBlockCount)
{
    DeferGC deferGC(vm());
    auto populations = populationsByWeakSet();
    ASSERT_GE(populations.size(), 2u);

    auto churn = [&] {
        for (unsigned i = 0; i < 2; ++i) {
            WeakSet& weakSet = *populations[i].weakSet;
            Vector<WeakImpl*> handles = allocateHandles(populations[i].cells[0], 3 * slotsPerBlock);
            weakSet.reap();
            weakSet.sweep();
            deallocateHandles(handles);
            weakSet.sweep();
        }
    };

    // The baseline is measured, not assumed: only unbounded growth is a failure.
    churn();
    unsigned baseline = heap().weakBlockCount();

    for (unsigned iteration = 0; iteration < 50; ++iteration)
        churn();

    EXPECT_LE(heap().weakBlockCount(), baseline + 2);
}

// Runs the concurrent tests' checks on one thread, so that a failure in a concurrent test points
// to a race rather than a bad expectation.
TEST_F(JavaScriptCore_WeakSet, RandomChurnStaysConsistent)
{
    DeferGC deferGC(vm());
    auto populations = populationsByWeakSet();
    ASSERT_GE(populations.size(), 2u);
    constexpr unsigned rounds = 20;
    FinalizeLedger& ledger = makeLedger(rounds * operationsPerRound);
    WeakRandom random(42);

    Vector<TrackedHandle> handles;
    Vector<TrackedHandle> otherHandles;
    Vector<TrackedHandles> lists { { populations[0].weakSet, &handles }, { populations[1].weakSet, &otherHandles } };
    for (unsigned round = 0; round < rounds; ++round) {
        for (unsigned i = 0; i < operationsPerRound; ++i) {
            if (random.getUint32(4))
                churnOnce(populations[0], handles, ledger, random);
            else
                churnOnce(populations[1], otherHandles, ledger, random);
        }
        populations[0].weakSet->sweep();
        populations[1].weakSet->sweep();
        expectQuiescentConsistency(heap(), ledger, lists);
        if (HasFailure())
            break;
        populations[0].weakSet->reap();
        populations[1].weakSet->reap();
        ledger.didReap();
    }

    clearTrackedHandles(handles, ledger);
    clearTrackedHandles(otherHandles, ledger);
    populations[0].weakSet->sweep();
    populations[1].weakSet->sweep();
    EXPECT_TRUE(populations[0].weakSet->isEmpty());
    EXPECT_TRUE(populations[1].weakSet->isEmpty());
    ledger.expectNoViolations();
}

// The main thread allocates, clears and releases blocks while a helper thread sweeps the same
// WeakSet. A second WeakSet, only ever touched by the main thread, takes the blocks it releases.
TEST_F(JavaScriptCore_WeakSet, OffThreadSweep)
{
    DeferGC deferGC(vm());
    auto populations = populationsByWeakSet();
    ASSERT_GE(populations.size(), 2u);
    Population& swept = populations[0];
    Population& other = populations[1];
    FinalizeLedger& ledger = makeLedger(offThreadSweepRounds * operationsPerRound);
    WeakRandom random(1);

    Vector<TrackedHandle> sweptHandles;
    Vector<TrackedHandle> otherHandles;
    Vector<TrackedHandles> lists { { swept.weakSet, &sweptHandles }, { other.weakSet, &otherHandles } };
    {
        SweeperThread sweeper(*swept.weakSet);
        for (unsigned round = 0; round < offThreadSweepRounds; ++round) {
            sweeper.resume();
            for (unsigned i = 0; i < operationsPerRound; ++i) {
                if (random.getUint32(4))
                    churnOnce(swept, sweptHandles, ledger, random);
                else
                    churnOnce(other, otherHandles, ledger, random);
                if (!((i + 1) % operationsPerSweepWait)) {
                    sweeper.waitForNextSweep();
                    swept.weakSet->releaseBlocks();
                }
            }
            sweeper.pause();

            swept.weakSet->sweep();
            other.weakSet->sweep();
            expectQuiescentConsistency(heap(), ledger, lists);
            if (HasFailure())
                break;
            swept.weakSet->reap();
            other.weakSet->reap();
            ledger.didReap();
        }
    }

    clearTrackedHandles(sweptHandles, ledger);
    clearTrackedHandles(otherHandles, ledger);
    swept.weakSet->sweep();
    other.weakSet->sweep();
    EXPECT_TRUE(swept.weakSet->isEmpty());
    EXPECT_TRUE(other.weakSet->isEmpty());
    ledger.expectNoViolations();
}

} // namespace TestWebKitAPI
