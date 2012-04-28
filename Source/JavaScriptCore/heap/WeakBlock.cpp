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
#include "JSObject.h"
#include "ScopeChain.h"
#include "Structure.h"

namespace JSC {

WeakBlock* WeakBlock::create()
{
    PageAllocation allocation = PageAllocation::allocate(blockSize, OSAllocator::JSGCHeapPages);
    if (!static_cast<bool>(allocation))
        CRASH();
    return new (NotNull, allocation.base()) WeakBlock(allocation);
}

void WeakBlock::destroy(WeakBlock* block)
{
    block->m_allocation.deallocate();
}

WeakBlock::WeakBlock(PageAllocation& allocation)
    : m_allocation(allocation)
{
    for (size_t i = 0; i < weakImplCount(); ++i) {
        WeakImpl* weakImpl = &weakImpls()[i];
        new (NotNull, weakImpl) WeakImpl;
        addToFreeList(&m_sweepResult.freeList, weakImpl);
    }

    ASSERT(!m_sweepResult.isNull() && m_sweepResult.blockIsFree);
}

void WeakBlock::finalizeAll()
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
    if (!m_sweepResult.isNull())
        return;

    SweepResult sweepResult;
    for (size_t i = 0; i < weakImplCount(); ++i) {
        WeakImpl* weakImpl = &weakImpls()[i];
        if (weakImpl->state() == WeakImpl::Dead)
            finalize(weakImpl);
        if (weakImpl->state() == WeakImpl::Deallocated)
            addToFreeList(&sweepResult.freeList, weakImpl);
        else
            sweepResult.blockIsFree = false;
    }

    m_sweepResult = sweepResult;
    ASSERT(!m_sweepResult.isNull());
}

void WeakBlock::visitLiveWeakImpls(HeapRootVisitor& heapRootVisitor)
{
    // If a block is completely empty, a visit won't have any effect.
    if (!m_sweepResult.isNull() && m_sweepResult.blockIsFree)
        return;

    SlotVisitor& visitor = heapRootVisitor.visitor();

    for (size_t i = 0; i < weakImplCount(); ++i) {
        WeakImpl* weakImpl = &weakImpls()[i];
        if (weakImpl->state() != WeakImpl::Live)
            continue;

        const JSValue& jsValue = weakImpl->jsValue();
        if (Heap::isMarked(jsValue.asCell()))
            continue;

        WeakHandleOwner* weakHandleOwner = weakImpl->weakHandleOwner();
        if (!weakHandleOwner)
            continue;

        if (!weakHandleOwner->isReachableFromOpaqueRoots(Handle<Unknown>::wrapSlot(&const_cast<JSValue&>(jsValue)), weakImpl->context(), visitor))
            continue;

        heapRootVisitor.visit(&const_cast<JSValue&>(jsValue));
    }
}

void WeakBlock::visitDeadWeakImpls(HeapRootVisitor&)
{
    // If a block is completely empty, a visit won't have any effect.
    if (!m_sweepResult.isNull() && m_sweepResult.blockIsFree)
        return;

    for (size_t i = 0; i < weakImplCount(); ++i) {
        WeakImpl* weakImpl = &weakImpls()[i];
        if (weakImpl->state() > WeakImpl::Dead)
            continue;

        if (Heap::isMarked(weakImpl->jsValue().asCell()))
            continue;

        weakImpl->setState(WeakImpl::Dead);
    }
}

} // namespace JSC
