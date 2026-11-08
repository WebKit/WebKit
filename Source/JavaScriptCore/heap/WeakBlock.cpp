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

#include "config.h"
#include "WeakBlock.h"

#include "CellContainerInlines.h"
#include "Heap.h"
#include "HeapAnalyzer.h"
#include "JSCInlines.h"
#include "WeakHandleOwner.h"
#include <wtf/FastMalloc.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(WeakBlock);

WeakBlock* WeakBlock::create(JSC::Heap& heap, CellContainer container)
{
    void* memory = WeakBlockMalloc::alignedMalloc(blockSize, blockSize);
    // blockFor() masks a slot address back to its block, so an unaligned block would silently
    // corrupt an unrelated one.
    RELEASE_ASSERT(blockContaining(memory) == memory);
    heap.didAllocateBlock(WeakBlock::blockSize);
    ++heap.m_weakBlockCount;
    return new (NotNull, memory) WeakBlock(heap, container);
}

void WeakBlock::destroy(JSC::Heap& heap, WeakBlock* block)
{
    RELEASE_ASSERT(!block->next() && !block->prev());
    ASSERT(!block->isIterationTarget());
    block->~WeakBlock();
    WeakBlockMalloc::free(block);
    --heap.m_weakBlockCount;
    heap.didFreeBlock(WeakBlock::blockSize);
}

WeakBlock::WeakBlock(JSC::Heap& heap, CellContainer container)
    : DoublyLinkedListNode<WeakBlock>()
    , m_freeList(allWeakAtomsMask())
    , m_container(container)
    , m_heap(heap)
{
    for (size_t i = 0; i < weakImplCount(); ++i)
        new (NotNull, &weakImpls()[i]) WeakImpl;

    ASSERT(isEmpty());
    // A newborn block is not yet reachable by any other thread.
    [&]() WTF_IGNORES_THREAD_SAFETY_ANALYSIS {
        ASSERT(ownership() == Ownership::Attached);
        constexpr bool blockIsVisibleToOtherThreads = false;
        assertAtomStatesAreConsistent(blockIsVisibleToOtherThreads);
    }();
}

#if ASSERT_ENABLED
void WeakBlock::assertAtomStatesAreConsistent(bool blockIsVisibleToOtherThreads)
{
    if (blockIsVisibleToOtherThreads)
        m_heap.assertIsExclusiveAccessHolderOrConductorWithWorldStopped();

    uint64_t freeList = m_freeList.load();
    uint64_t reapedAtoms = m_reapedAtoms.load();
    ASSERT(!(freeList & ~allWeakAtomsMask()));
    ASSERT(!(reapedAtoms & ~allWeakAtomsMask() & ~ownershipMask));
    bool hasDeadAtom = false;
    for (size_t weakAtom = firstPayloadRegionWeakAtom(); weakAtom < weakAtomsPerBlock; ++weakAtom) {
        WeakImpl::State state = weakAtoms()[weakAtom].state();
        ASSERT(!!(freeList & bitFor(weakAtom)) == (state == WeakImpl::Deallocated));
        ASSERT(!!(reapedAtoms & bitFor(weakAtom)) == (state == WeakImpl::Dead || state == WeakImpl::Finalized));
        hasDeadAtom |= state == WeakImpl::Dead;
    }

    uint64_t liveAtoms = ~freeList & ~reapedAtoms & allWeakAtomsMask();
    switch (ownership()) {
    case Ownership::Attached:
        ASSERT(m_container);
        break;
    case Ownership::Detached:
        // Nothing reaps or sweeps a detached block, so every handle left in it must be finalized.
        ASSERT(!m_container);
        ASSERT(!liveAtoms);
        ASSERT(!hasDeadAtom);
        break;
    case Ownership::Pooled:
        ASSERT(!m_container);
        ASSERT(freeList == allWeakAtomsMask());
        break;
    }
}
#endif

void WeakBlock::didBecomeEmpty()
{
    Locker locker { weakBlockLock() };
    didBecomeEmpty(locker, IsConcurrent::No);
}

void WeakBlock::didBecomeEmpty(Locker<Lock>& locker, IsConcurrent isConcurrent)
{
    locker.assertIsHolding(weakBlockLock());
    switch (ownership()) {
    case Ownership::Attached: {
        // May have been allocated from since it became empty; WeakSet rechecks.
        WeakSet& weakSet = m_container.weakSet();
        weakSet.assertIsHoldingWeakBlockLock();
        weakSet.tryReleaseBlock(locker, this, isConcurrent);
        break;
    }
    case Ownership::Detached:
        // A detached block is on no WeakSet and is never swept, reaped or visited, and every slot in
        // it is already finalized, so only this call can change it. That makes returning it to the
        // Heap safe at any reentrancy depth, including from within a finalizer.
        ASSERT(isEmpty());
        ASSERT(!m_container);
        locker.assertIsHolding(m_heap.weakBlockLock());
        m_heap.releaseDetachedWeakBlock(locker, this, isConcurrent);
        break;
    case Ownership::Pooled:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void WeakImpl::clearConcurrently()
{
    WeakBlock* block = WeakBlock::blockFor(this);
    ASSERT(!block->heap().worldIsStopped());
    // Held across the free so that the mutator cannot release the block once this empties it.
    Locker locker { block->weakBlockLock() };
    if (block->addFreeCell(this, setDeallocated()))
        block->didBecomeEmpty(locker, IsConcurrent::Yes);
}

void WeakBlock::reapAllLiveHandles(Locker<Lock>& locker)
{
    locker.assertIsHolding(weakBlockLock());
    ASSERT(!isIterationTarget());
    uint64_t newlyDeadAtoms = 0;
    forEachAtom(liveAtoms(), [&](size_t weakAtom) {
        WeakImpl* weakImpl = &weakAtoms()[weakAtom];
        if (weakImpl->state() != WeakImpl::Live)
            return;
        weakImpl->setDead();
        newlyDeadAtoms |= bitFor(weakAtom);
    });
    if (newlyDeadAtoms)
        m_reapedAtoms.exchangeOr<MemoryOrder::Relaxed>(newlyDeadAtoms);
}

void WeakBlock::sweep()
{
    ASSERT(isIterationTarget());
    forEachAtom(reapedAtoms(), [&](size_t weakAtom) {
        WeakImpl* weakImpl = &weakAtoms()[weakAtom];
        uintptr_t stateWord = weakImpl->stateWord();
        // We can be racing with the mutator or a finalizer so the atom bits can be stale. The state is the source of truth.
        if (WeakImpl::stateOf(stateWord) != WeakImpl::Dead)
            return;

        auto snapshot = weakImpl->trySetFinalized(stateWord);
        if (!snapshot)
            return;

        if (!snapshot.weakHandleOwner)
            return;

        if (snapshot.weakHandleOwner->finalize(Handle<Unknown>::wrapSlot(&snapshot.jsValue), snapshot.context) == WeakHandleOwner::FinalizeResult::DeallocateImpl) {
            ASSERT(weakImpl->state() == WeakImpl::Finalized);
            weakImpl->clear();
        }
    });
}

template<typename ContainerType, typename Visitor>
void WeakBlock::specializedVisit(ContainerType& container, Visitor& visitor)
{
    size_t count = weakImplCount();
    HeapAnalyzer* heapAnalyzer = visitor.isAnalyzingHeap() ? container.vm().activeHeapAnalyzer() : nullptr;
    for (size_t i = 0; i < count; ++i) {
        WeakImpl* weakImpl = &weakImpls()[i];
        if (weakImpl->state() != WeakImpl::Live)
            continue;

        WeakHandleOwner* weakHandleOwner = weakImpl->weakHandleOwner();
        if (!weakHandleOwner)
            continue;

        JSValue jsValue = weakImpl->jsValue();
        if (visitor.isMarked(container, jsValue.asCell()))
            continue;

        ASCIILiteral reason = ""_s;
        ASCIILiteral* reasonPtr = nullptr;
        if (heapAnalyzer) [[unlikely]]
            reasonPtr = &reason;

        typename Visitor::ReferrerContext context(visitor, Visitor::OpaqueRoot);

        if (!weakHandleOwner->isReachableFromOpaqueRoots(Handle<Unknown>::wrapSlot(&const_cast<JSValue&>(jsValue)), weakImpl->context(), visitor, reasonPtr))
            continue;

        visitor.appendUnbarriered(jsValue);

        if (heapAnalyzer) [[unlikely]] {
            if (jsValue.isCell())
                heapAnalyzer->setOpaqueRootReachabilityReasonForCell(jsValue.asCell(), *reasonPtr);
        }
    }
}

template<typename Visitor>
ALWAYS_INLINE void WeakBlock::visitImpl(Visitor& visitor)
{
    assertWorldIsStopped();
    // specializedVisit only ever acts on a live handle.
    if (!hasLiveHandles())
        return;

    // If this WeakBlock doesn't belong to a CellContainer, we won't even be here.
    ASSERT(m_container);

    if (m_container.isPreciseAllocation())
        specializedVisit(m_container.preciseAllocation(), visitor);
    else
        specializedVisit(m_container.markedBlock(), visitor);
}

void WeakBlock::visit(AbstractSlotVisitor& visitor) { visitImpl(visitor); }
void WeakBlock::visit(SlotVisitor& visitor) { visitImpl(visitor); }

void WeakBlock::reap(Locker<Lock>& locker)
{
    locker.assertIsHolding(weakBlockLock());
    ASSERT(!isIterationTarget());
    assertAtomStatesAreConsistent();
    uint64_t liveAtoms = this->liveAtoms();
    if (!liveAtoms)
        return;

    // If this WeakBlock doesn't belong to a CellContainer, we won't even be here.
    ASSERT(m_container);

    HeapVersion markingVersion = m_heap.objectSpace().markingVersion();

    uint64_t newlyDeadAtoms = 0;
    forEachAtom(liveAtoms, [&](size_t weakAtom) {
        WeakImpl* weakImpl = &weakAtoms()[weakAtom];
        if (weakImpl->state() != WeakImpl::Live)
            return;
        if (m_container.isMarked(markingVersion, weakImpl->jsValue().asCell()))
            return;

        weakImpl->setDead();
        newlyDeadAtoms |= bitFor(weakAtom);
    });
    if (newlyDeadAtoms)
        m_reapedAtoms.exchangeOr<MemoryOrder::Relaxed>(newlyDeadAtoms);
    assertAtomStatesAreConsistent();
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
