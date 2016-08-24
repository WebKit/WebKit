/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "Heap.h"
#include "HeapRootVisitor.h"
#include "JSCInlines.h"
#include "JSObject.h"
#include "WeakHandleOwner.h"

namespace JSC {

WeakBlock* WeakBlock::create(Heap& heap, MarkedBlock& markedBlock)
{
    heap.didAllocateBlock(WeakBlock::blockSize);
    return new (NotNull, fastMalloc(blockSize)) WeakBlock(markedBlock);
}

void WeakBlock::destroy(Heap& heap, WeakBlock* block)
{
    block->~WeakBlock();
    fastFree(block);
    heap.didFreeBlock(WeakBlock::blockSize);
}

WeakBlock::WeakBlock(MarkedBlock& markedBlock)
    : DoublyLinkedListNode<WeakBlock>()
    , m_markedBlock(&markedBlock)
{
    for (size_t i = 0; i < weakImplCount(); ++i) {
        WeakImpl* weakImpl = &weakImpls()[i];
        new (NotNull, weakImpl) WeakImpl;
        addToFreeList(&m_sweepResult.freeList, weakImpl);
    }

    ASSERT(isEmpty());
}

void WeakBlock::lastChanceToFinalize()
{
    for (size_t i = 0; i < weakImplCount(); ++i) {
        WeakImpl* weakImpl = &weakImpls()[i];
        if (weakImpl->state() >= WeakImpl::Finalized)
            continue;
        weakImpl->setState(WeakImpl::Dead);
        finalize(weakImpl);
    }
}

void WeakBlock::sweep()
{
    // If a block is completely empty, a sweep won't have any effect.
    if (isEmpty())
        return;

    SweepResult sweepResult;
    for (size_t i = 0; i < weakImplCount(); ++i) {
        WeakImpl* weakImpl = &weakImpls()[i];
        if (weakImpl->state() == WeakImpl::Dead)
            finalize(weakImpl);
        if (weakImpl->state() == WeakImpl::Deallocated)
            addToFreeList(&sweepResult.freeList, weakImpl);
        else {
            sweepResult.blockIsFree = false;
            if (weakImpl->state() == WeakImpl::Live)
                sweepResult.blockIsLogicallyEmpty = false;
        }
    }

    m_sweepResult = sweepResult;
    ASSERT(!m_sweepResult.isNull());
}

void WeakBlock::visit(HeapRootVisitor& heapRootVisitor)
{
    // If a block is completely empty, a visit won't have any effect.
    if (isEmpty())
        return;

    // If this WeakBlock doesn't belong to a MarkedBlock, we won't even be here.
    ASSERT(m_markedBlock);

    // We only visit after marking.
    ASSERT(m_markedBlock->isMarkedOrRetired());

    SlotVisitor& visitor = heapRootVisitor.visitor();

    for (size_t i = 0; i < weakImplCount(); ++i) {
        WeakImpl* weakImpl = &weakImpls()[i];
        if (weakImpl->state() != WeakImpl::Live)
            continue;

        WeakHandleOwner* weakHandleOwner = weakImpl->weakHandleOwner();
        if (!weakHandleOwner)
            continue;

        const JSValue& jsValue = weakImpl->jsValue();
        if (m_markedBlock->isMarkedOrNewlyAllocated(jsValue.asCell()))
            continue;

        if (!weakHandleOwner->isReachableFromOpaqueRoots(Handle<Unknown>::wrapSlot(&const_cast<JSValue&>(jsValue)), weakImpl->context(), visitor))
            continue;

        heapRootVisitor.visit(&const_cast<JSValue&>(jsValue));
    }
}

void WeakBlock::reap()
{
    // If a block is completely empty, a reaping won't have any effect.
    if (isEmpty())
        return;

    // If this WeakBlock doesn't belong to a MarkedBlock, we won't even be here.
    ASSERT(m_markedBlock);

    // We only reap after marking.
    ASSERT(m_markedBlock->isMarkedOrRetired());

    for (size_t i = 0; i < weakImplCount(); ++i) {
        WeakImpl* weakImpl = &weakImpls()[i];
        if (weakImpl->state() > WeakImpl::Dead)
            continue;

        if (m_markedBlock->isMarkedOrNewlyAllocated(weakImpl->jsValue().asCell())) {
            ASSERT(weakImpl->state() == WeakImpl::Live);
            continue;
        }

        weakImpl->setState(WeakImpl::Dead);
    }
}

} // namespace JSC
