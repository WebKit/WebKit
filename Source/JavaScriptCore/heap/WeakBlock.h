/*
 * Copyright (C) 2012-2021, 2026 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/CellContainer.h>
#include <JavaScriptCore/WeakHandleOwner.h>
#include <JavaScriptCore/WeakImpl.h>
#include <wtf/Atomics.h>
#include <wtf/DebugHeap.h>
#include <wtf/DoublyLinkedList.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class AbstractSlotVisitor;
class Heap;
class SlotVisitor;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(WeakBlock);

// Holds the WeakImpl slots for the Weak<> handles pointing at one CellContainer's cells. The block
// is blockSize-aligned, so masking a slot address yields its block, which is what lets a Weak<>
// return its slot in constant time without knowing anything about its WeakSet.
//
// Only the mutator allocates slots and unlinks blocks. It's valid to sweep/finalize on a background
// thread as long as that finalizer doesn't clear other WeakImpls.
class WeakBlock : public DoublyLinkedListNode<WeakBlock> {
public:
    friend class WTF::DoublyLinkedListNode<WeakBlock>;
    static constexpr size_t blockSize = 1024; // 1/16 of MarkedBlock size

    // A block is on exactly one list, and which one follows from this.
    enum class Ownership : uint8_t {
        Attached, // a WeakSet's m_blocks
        Detached, // Heap::m_detachedWeakBlocks: nothing live points out, some Weak<> still point in
        Pooled, // Heap::m_pooledWeakBlocks: no allocated slots, waiting to be handed to a WeakSet
    };

    static WeakBlock* create(Heap&, CellContainer);
    static void destroy(Heap&, WeakBlock*);

    // A sweep running finalizers holds exactly one block pointer across the callout. That block
    // must stay linked even if a finalizer empties it. The iterator should report any blocks it
    // empties to the Heap.
    class IterationScope {
    public:
        IterationScope(WeakBlock& block)
            : m_block(block)
        {
            ASSERT(!m_block.m_isIterationTarget);
            m_block.m_isIterationTarget = true;
        }

        ~IterationScope()
        {
            m_block.m_isIterationTarget = false;
        }

    private:
        WeakBlock& m_block;
    };

    static WeakBlock* blockFor(WeakImpl*);

    JSC::Heap& heap() const { return m_heap; }

    bool isEmpty() const { return m_freeList.load<MemoryOrder::Relaxed>() == allFreeMask(); }
    bool hasLiveHandles() const { return m_liveCount; }
    bool hasOnlyFinalizedHandles() const { return !isEmpty() && !m_liveCount && !m_deadCountUpperBound; }
    bool hasFreeCell() const { return !!m_freeList.load<MemoryOrder::Relaxed>(); }

    void sweep();

    JS_EXPORT_PRIVATE void visit(AbstractSlotVisitor&);
    JS_EXPORT_PRIVATE void visit(SlotVisitor&);

    void reap();

    void reapAllLiveHandles();

    static constexpr size_t weakImplCount() { return weakAtomsPerBlock - firstPayloadRegionWeakAtom(); }
    static constexpr size_t weakAtomsPerBlock = blockSize / sizeof(WeakImpl);

private:
    friend class Heap;
    friend class WeakImpl;
    friend class WeakSet;

    JS_EXPORT_PRIVATE void didBecomeEmpty();

    void reattach(CellContainer);
    void setDetached();
    void setPooled();

    template<typename Visitor> void visitImpl(Visitor&);

    static WeakBlock* blockContaining(const void*);

    template<typename ContainerType, typename Visitor>
    void specializedVisit(ContainerType&, Visitor&);

    WeakBlock(Heap&, CellContainer);
    WeakImpl* weakAtoms() { return reinterpret_cast_ptr<WeakImpl*>(this); }
    WeakImpl* weakImpls() { return weakAtoms() + firstPayloadRegionWeakAtom(); }

    static constexpr size_t firstPayloadRegionWeakAtom() { return (sizeof(WeakBlock) + sizeof(WeakImpl) - 1) / sizeof(WeakImpl); }
    static constexpr uint64_t allFreeMask();
    static constexpr uint64_t bitFor(size_t weakAtom) { ASSERT(weakAtom < weakAtomsPerBlock); return 1ull << weakAtom; }
    size_t weakAtomNumber(WeakImpl* weakImpl) { return weakImpl - weakAtoms(); }
    WeakImpl* takeFreeCell();
    bool addFreeCell(WeakImpl*);
    void deallocate(WeakImpl*, WeakImpl::State previousState);

    void assertFreeListIsConsistent();

    static_assert(weakAtomsPerBlock <= std::numeric_limits<uint64_t>::digits);
    // Bit i of this freelist is set when the ith weakAtom is free.
    Atomic<uint64_t> m_freeList;
    CellContainer m_container;
    WeakBlock* m_prev;
    WeakBlock* m_next;
    JSC::Heap& m_heap;
    // clear() of a Dead slot does not decrement m_deadCountUpperBound.
    uint16_t m_liveCount { 0 };
    uint16_t m_deadCountUpperBound { 0 };
    Ownership m_ownership { Ownership::Attached };
    bool m_isIterationTarget { false };
};

inline WeakBlock* WeakBlock::blockContaining(const void* address)
{
    return std::bit_cast<WeakBlock*>(roundDownToMultipleOf<blockSize>(std::bit_cast<uintptr_t>(address)));
}

inline constexpr uint64_t WeakBlock::allFreeMask()
{
    constexpr uint64_t weakAtoms = std::numeric_limits<uint64_t>::max() >> (std::numeric_limits<uint64_t>::digits - weakAtomsPerBlock);
    constexpr uint64_t header = bitFor(firstPayloadRegionWeakAtom()) - 1;
    constexpr uint64_t mask = weakAtoms & ~header;
    static_assert(std::popcount(mask) == weakImplCount());
    return mask;
}

// A member added to WeakBlock costs a WeakImpl slot in every block in the VM.
static_assert(sizeof(WeakBlock) <= 2 * sizeof(WeakImpl), "The payload must not lose a slot.");

inline WeakBlock* WeakBlock::blockFor(WeakImpl* weakImpl)
{
    WeakBlock* block = blockContaining(weakImpl);
    ASSERT(weakImpl >= block->weakImpls() && weakImpl < block->weakImpls() + weakImplCount());
    return block;
}

inline WeakImpl* WeakBlock::takeFreeCell()
{
    uint64_t freeList = m_freeList.load<MemoryOrder::Relaxed>();
    ASSERT(freeList);
    size_t weakAtom = ctz(freeList);
    // Only the mutator clears bits, so the bit chosen from the relaxed load is still set.
    uint64_t oldFreeList = m_freeList.exchangeAnd<MemoryOrder::Acquire>(~bitFor(weakAtom));
    ASSERT_UNUSED(oldFreeList, oldFreeList & bitFor(weakAtom));
    // The caller constructs a WeakImpl in this slot, and a WeakImpl is born Live.
    ++m_liveCount;
    return weakAtoms() + weakAtom;
}

inline bool WeakBlock::addFreeCell(WeakImpl* weakImpl)
{
    ASSERT(weakImpl->state() == WeakImpl::Deallocated);
    ASSERT(blockFor(weakImpl) == this);
    uint64_t bit = bitFor(weakAtomNumber(weakImpl));
    uint64_t oldFreeList = m_freeList.exchangeOr<MemoryOrder::Release>(bit);
    RELEASE_ASSERT(!(oldFreeList & bit));
    return (oldFreeList | bit) == allFreeMask();
}

inline void WeakBlock::deallocate(WeakImpl* weakImpl, WeakImpl::State previousState)
{
    if (previousState == WeakImpl::Live) {
        ASSERT(m_liveCount);
        --m_liveCount;
    }

    if (addFreeCell(weakImpl)) [[unlikely]]
        didBecomeEmpty();
}

inline void WeakBlock::reattach(CellContainer container)
{
    ASSERT(m_ownership == Ownership::Pooled);
    ASSERT(isEmpty());
    assertFreeListIsConsistent();
    m_container = container;
    m_ownership = Ownership::Attached;
}

inline void WeakBlock::setDetached()
{
    ASSERT(m_ownership == Ownership::Attached);
    ASSERT(!m_isIterationTarget);
    m_container = CellContainer();
    m_ownership = Ownership::Detached;
}

inline void WeakBlock::setPooled()
{
    ASSERT(isEmpty());
    ASSERT(!m_isIterationTarget);
    m_container = CellContainer();
    m_liveCount = 0;
    m_deadCountUpperBound = 0;
    m_ownership = Ownership::Pooled;
}

inline void WeakBlock::assertFreeListIsConsistent()
{
#if ASSERT_ENABLED
    uint64_t freeList = m_freeList.load();
    ASSERT(!(freeList & ~allFreeMask()));
    size_t liveCount = 0;
    size_t deadCount = 0;
    for (size_t i = 0; i < weakImplCount(); ++i) {
        WeakImpl::State state = weakImpls()[i].state();
        ASSERT(!!(freeList & bitFor(i + firstPayloadRegionWeakAtom())) == (state == WeakImpl::Deallocated));
        liveCount += state == WeakImpl::Live;
        deadCount += state == WeakImpl::Dead;
    }
    ASSERT(liveCount == m_liveCount);
    ASSERT(deadCount <= m_deadCountUpperBound);
#endif
}

// Only a Dead slot can race with trySetFinalized(). A next occupant cannot overwrite this slot
// before trySetFinalized() has read it, because trySetFinalized() reads before its Release and
// addFreeCell()'s Release / takeFreeCell()'s Acquire keep this state read before the slot is reused.
SUPPRESS_NODELETE inline void WeakImpl::clear()
{
    State previousState = state();
    ASSERT(previousState != Deallocated);
    // FIXME: It might be faster to just always exchange here, at least on ARM64.
    if (previousState == Dead)
        previousState = stateOf(m_weakHandleOwner.exchange<MemoryOrder::Relaxed>(Deallocated));
    else
        m_weakHandleOwner.store<MemoryOrder::Relaxed>(Deallocated);

    WeakBlock::blockFor(this)->deallocate(this, previousState);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
