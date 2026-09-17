/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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
#include <JavaScriptCore/WeakImpl.h>
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
class WeakBlock : public DoublyLinkedListNode<WeakBlock> {
public:
    friend class WTF::DoublyLinkedListNode<WeakBlock>;
    static constexpr size_t blockSize = 1024; // 1/16 of MarkedBlock size

    struct FreeCell {
        FreeCell* next;
    };

    // A block is on exactly one list, and which one follows from this.
    enum class Ownership : uint8_t {
        Attached, // a WeakSet's m_blocks
        Detached, // Heap::m_detachedWeakBlocks: nothing live points out, some Weak<> still point in
        Pooled, // Heap::m_pooledWeakBlocks: no allocated slots, waiting to be handed to a WeakSet
    };

    static WeakBlock* create(Heap&, CellContainer);
    static void destroy(Heap&, WeakBlock*);

    // A walk that runs finalizers holds exactly one block pointer across the callout. That block
    // must stay linked even if a finalizer empties it; every other block is free to go. Nothing
    // hands a held block back on its own, so the walk owes it a WeakSet::tryReleaseBlock once the
    // scope closes, or an emptied block stays linked to its WeakSet forever.
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

    static WeakImpl* asWeakImpl(FreeCell*);
    static WeakBlock* blockFor(WeakImpl*);

    JSC::Heap& heap() const { return m_heap; }

    bool isEmpty() const { return !m_allocatedCount; }
    bool hasLiveHandles() const { return m_liveCount; }
    bool hasOnlyFinalizedHandles() const { return m_allocatedCount && !m_liveCount && !m_deadCount; }
    bool hasFreeCell() const { return !!m_freeList; }

    void sweep();

    JS_EXPORT_PRIVATE void visit(AbstractSlotVisitor&);
    JS_EXPORT_PRIVATE void visit(SlotVisitor&);

    void reap();

    void lastChanceToFinalize();

    static constexpr size_t weakImplCount();

private:
    friend class Heap;
    friend class WeakImpl;
    friend class WeakSet;

    JS_EXPORT_PRIVATE void didBecomeEmpty();

    void reattach(CellContainer);
    void setDetached();
    void setPooled();

    template<typename Visitor> void visitImpl(Visitor&);

    static FreeCell* asFreeCell(WeakImpl*);
    static WeakBlock* blockContaining(const void*);

    template<typename ContainerType, typename Visitor>
    void specializedVisit(ContainerType&, Visitor&);

    WeakBlock(Heap&, CellContainer);
    void finalize(WeakImpl*);
    WeakImpl* weakImpls();

    FreeCell* takeFreeCell();
    void pushFreeCell(WeakImpl*);
    void deallocate(WeakImpl*, WeakImpl::State previousState);

    void assertFreeListIsConsistent();

    CellContainer m_container;
    WeakBlock* m_prev;
    WeakBlock* m_next;
    FreeCell* m_freeList { nullptr };
    JSC::Heap& m_heap;
    // Slots in each WeakImpl state, so that emptiness, pending finalization and "nothing here
    // refers to a cell any more" are all answerable without scanning the block.
    uint16_t m_allocatedCount { 0 };
    uint16_t m_liveCount { 0 };
    uint16_t m_deadCount { 0 };
    Ownership m_ownership { Ownership::Attached };
    bool m_isIterationTarget { false };
};

inline WeakImpl* WeakBlock::asWeakImpl(FreeCell* freeCell)
{
    return reinterpret_cast_ptr<WeakImpl*>(freeCell);
}

inline WeakBlock::FreeCell* WeakBlock::asFreeCell(WeakImpl* weakImpl)
{
    return reinterpret_cast_ptr<FreeCell*>(weakImpl);
}

inline WeakBlock* WeakBlock::blockContaining(const void* address)
{
    return std::bit_cast<WeakBlock*>(roundDownToMultipleOf<blockSize>(std::bit_cast<uintptr_t>(address)));
}

inline WeakImpl* WeakBlock::weakImpls()
{
    return reinterpret_cast_ptr<WeakImpl*>(this) + ((sizeof(WeakBlock) + sizeof(WeakImpl) - 1) / sizeof(WeakImpl));
}

inline constexpr size_t WeakBlock::weakImplCount()
{
    return (blockSize / sizeof(WeakImpl)) - ((sizeof(WeakBlock) + sizeof(WeakImpl) - 1) / sizeof(WeakImpl));
}
static_assert(WeakBlock::weakImplCount() <= std::numeric_limits<uint16_t>::max());

// A member added to WeakBlock costs a WeakImpl slot in every block in the VM.
static_assert(sizeof(WeakBlock) <= 2 * sizeof(WeakImpl), "The payload must not lose a slot.");

inline WeakBlock* WeakBlock::blockFor(WeakImpl* weakImpl)
{
    WeakBlock* block = blockContaining(weakImpl);
    ASSERT(weakImpl >= block->weakImpls() && weakImpl < block->weakImpls() + weakImplCount());
    return block;
}

inline WeakBlock::FreeCell* WeakBlock::takeFreeCell()
{
    FreeCell* freeCell = m_freeList;
    m_freeList = freeCell->next;
    ASSERT(m_allocatedCount < weakImplCount());
    ++m_allocatedCount;
    // The caller constructs a WeakImpl in this slot, and a WeakImpl is born Live.
    ++m_liveCount;
    return freeCell;
}

inline void WeakBlock::pushFreeCell(WeakImpl* weakImpl)
{
    ASSERT(weakImpl->state() == WeakImpl::Deallocated);
    ASSERT(blockFor(weakImpl) == this);
    FreeCell* freeCell = asFreeCell(weakImpl);
    freeCell->next = m_freeList;
    m_freeList = freeCell;
}

inline void WeakBlock::deallocate(WeakImpl* weakImpl, WeakImpl::State previousState)
{
    pushFreeCell(weakImpl);

    if (previousState == WeakImpl::Live) {
        ASSERT(m_liveCount);
        --m_liveCount;
    } else if (previousState == WeakImpl::Dead) {
        ASSERT(m_deadCount);
        --m_deadCount;
    }

    RELEASE_ASSERT(m_allocatedCount);
    if (!--m_allocatedCount) [[unlikely]]
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
    m_ownership = Ownership::Pooled;
}

inline void WeakBlock::assertFreeListIsConsistent()
{
#if ASSERT_ENABLED
    size_t count = 0;
    for (FreeCell* freeCell = m_freeList; freeCell; freeCell = freeCell->next) {
        ASSERT(blockContaining(freeCell) == this);
        ASSERT(asWeakImpl(freeCell)->state() == WeakImpl::Deallocated);
        ASSERT(++count <= weakImplCount());
    }
    ASSERT(count + m_allocatedCount == weakImplCount());
    ASSERT(m_liveCount + m_deadCount <= m_allocatedCount);
#endif
}

// The free list threads through m_jsValue, so clearing must set the Deallocated tag in
// m_weakHandleOwner first: that tag is what every scanner filters on before reading the value.
SUPPRESS_NODELETE inline void WeakImpl::clear()
{
    State previousState = state();
    ASSERT(previousState != Deallocated);
    m_weakHandleOwner = std::bit_cast<WeakHandleOwner*>(static_cast<uintptr_t>(Deallocated));

    WeakBlock::blockFor(this)->deallocate(this, previousState);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
