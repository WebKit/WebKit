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

#ifndef WeakBlock_h
#define WeakBlock_h

#include "HeapBlock.h"
#include "WeakHandleOwner.h"
#include "WeakImpl.h"
#include <wtf/DoublyLinkedList.h>
#include <wtf/PageAllocation.h>
#include <wtf/StdLibExtras.h>

namespace JSC {

class HeapRootVisitor;
class JSValue;
class WeakHandleOwner;

class WeakBlock : public DoublyLinkedListNode<WeakBlock> {
public:
    friend class WTF::DoublyLinkedListNode<WeakBlock>;
    static const size_t blockSize = 4 * KB;

    struct FreeCell {
        FreeCell* next;
    };

    struct SweepResult {
        SweepResult();
        bool isNull() const;

        bool blockIsFree;
        FreeCell* freeList;
    };

    static WeakBlock* create();
    static void destroy(WeakBlock*);

    static WeakImpl* asWeakImpl(FreeCell*);

    void sweep();
    const SweepResult& sweepResult();
    SweepResult takeSweepResult();

    void visitLiveWeakImpls(HeapRootVisitor&);
    void visitDeadWeakImpls(HeapRootVisitor&);

    void finalizeAll();

private:
    static FreeCell* asFreeCell(WeakImpl*);

    WeakBlock(PageAllocation&);
    WeakImpl* firstWeakImpl();
    void finalize(WeakImpl*);
    WeakImpl* weakImpls();
    size_t weakImplCount();
    void addToFreeList(FreeCell**, WeakImpl*);

    PageAllocation m_allocation;
    WeakBlock* m_prev;
    WeakBlock* m_next;
    SweepResult m_sweepResult;
};

inline WeakBlock::SweepResult::SweepResult()
    : blockIsFree(true)
    , freeList(0)
{
    ASSERT(isNull());
}

inline bool WeakBlock::SweepResult::isNull() const
{
    return blockIsFree && !freeList; // This state is impossible, so we can use it to mean null.
}

inline WeakImpl* WeakBlock::asWeakImpl(FreeCell* freeCell)
{
    return reinterpret_cast<WeakImpl*>(freeCell);
}

inline WeakBlock::SweepResult WeakBlock::takeSweepResult()
{
    SweepResult tmp;
    std::swap(tmp, m_sweepResult);
    ASSERT(m_sweepResult.isNull());
    return tmp;
}

inline const WeakBlock::SweepResult& WeakBlock::sweepResult()
{
    return m_sweepResult;
}

inline WeakBlock::FreeCell* WeakBlock::asFreeCell(WeakImpl* weakImpl)
{
    return reinterpret_cast<FreeCell*>(weakImpl);
}

inline void WeakBlock::finalize(WeakImpl* weakImpl)
{
    ASSERT(weakImpl->state() == WeakImpl::Dead);
    weakImpl->setState(WeakImpl::Finalized);
    WeakHandleOwner* weakHandleOwner = weakImpl->weakHandleOwner();
    if (!weakHandleOwner)
        return;
    weakHandleOwner->finalize(Handle<Unknown>::wrapSlot(&const_cast<JSValue&>(weakImpl->jsValue())), weakImpl->context());
}

inline WeakImpl* WeakBlock::weakImpls()
{
    return reinterpret_cast<WeakImpl*>(this) + ((sizeof(WeakBlock) + sizeof(WeakImpl) - 1) / sizeof(WeakImpl));
}

inline size_t WeakBlock::weakImplCount()
{
    return (blockSize / sizeof(WeakImpl)) - ((sizeof(WeakBlock) + sizeof(WeakImpl) - 1) / sizeof(WeakImpl));
}

inline void WeakBlock::addToFreeList(FreeCell** freeList, WeakImpl* weakImpl)
{
    ASSERT(weakImpl->state() == WeakImpl::Deallocated);
    FreeCell* freeCell = asFreeCell(weakImpl);
    ASSERT(!*freeList || ((char*)*freeList > (char*)this && (char*)*freeList < (char*)this + blockSize));
    ASSERT((char*)freeCell > (char*)this && (char*)freeCell < (char*)this + blockSize);
    freeCell->next = *freeList;
    *freeList = freeCell;
}

} // namespace JSC

#endif // WeakBlock_h
