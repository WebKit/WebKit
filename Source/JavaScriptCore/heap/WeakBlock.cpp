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
    ASSERT(!block->m_isIterationTarget);
    block->~WeakBlock();
    WeakBlockMalloc::free(block);
    --heap.m_weakBlockCount;
    heap.didFreeBlock(WeakBlock::blockSize);
}

WeakBlock::WeakBlock(JSC::Heap& heap, CellContainer container)
    : DoublyLinkedListNode<WeakBlock>()
    , m_container(container)
    , m_heap(heap)
{
    for (size_t i = 0; i < weakImplCount(); ++i) {
        WeakImpl* weakImpl = &weakImpls()[i];
        new (NotNull, weakImpl) WeakImpl;
        pushFreeCell(weakImpl);
    }

    ASSERT(isEmpty());
    assertFreeListIsConsistent();
}

void WeakBlock::didBecomeEmpty()
{
    ASSERT(isEmpty());

    switch (m_ownership) {
    case Ownership::Attached:
        // A walk that runs finalizers is holding this block; WeakSet::sweep collects it afterwards.
        if (m_isIterationTarget)
            return;
        m_container.weakSet().didBecomeEmpty(this);
        break;
    case Ownership::Detached:
        // A detached block is on no WeakSet and is never swept, reaped or visited, and every slot in
        // it is already finalized, so only this call can change it. That makes returning it to the
        // Heap safe at any reentrancy depth, including from within a finalizer.
        ASSERT(!m_container);
        m_heap.releaseDetachedWeakBlock(this);
        break;
    case Ownership::Pooled:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void WeakBlock::lastChanceToFinalize()
{
    for (size_t i = 0; i < weakImplCount(); ++i) {
        WeakImpl* weakImpl = &weakImpls()[i];
        WeakImpl::State state = weakImpl->state();
        if (state >= WeakImpl::Finalized)
            continue;
        if (state == WeakImpl::Live) {
            weakImpl->setState(WeakImpl::Dead);
            --m_liveCount;
            ++m_deadCount;
        }
        finalize(weakImpl);
    }
}

void WeakBlock::sweep()
{
    for (size_t i = 0; m_deadCount && i < weakImplCount(); ++i) {
        WeakImpl* weakImpl = &weakImpls()[i];
        // finalize() calls out to a WeakHandleOwner, which may clear handles in this block or any
        // other. Clearing only pushes onto a free list, so the rest of this scan stays valid.
        if (weakImpl->state() == WeakImpl::Dead)
            finalize(weakImpl);
    }

    ASSERT(!m_deadCount);
    assertFreeListIsConsistent();
}

template<typename ContainerType, typename Visitor>
void WeakBlock::specializedVisit(ContainerType& container, Visitor& visitor)
{
    size_t count = weakImplCount();
    HeapAnalyzer* heapAnalyzer = visitor.vm().activeHeapAnalyzer();
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
    // specializedVisit only ever acts on a live handle.
    if (!m_liveCount)
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

void WeakBlock::reap()
{
    if (!m_liveCount)
        return;

    // If this WeakBlock doesn't belong to a CellContainer, we won't even be here.
    ASSERT(m_container);

    HeapVersion markingVersion = m_heap.objectSpace().markingVersion();

    for (size_t i = 0; i < weakImplCount(); ++i) {
        WeakImpl* weakImpl = &weakImpls()[i];
        if (weakImpl->state() != WeakImpl::Live)
            continue;

        if (m_container.isMarked(markingVersion, weakImpl->jsValue().asCell()))
            continue;

        weakImpl->setState(WeakImpl::Dead);
        --m_liveCount;
        ++m_deadCount;
    }
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
