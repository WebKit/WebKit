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

#ifndef WeakSet_h
#define WeakSet_h

#include "WeakBlock.h"

namespace JSC {

class Heap;
class WeakImpl;

class WeakSet {
public:
    WeakSet(Heap*);
    void finalizeAll();
    ~WeakSet();

    WeakImpl* allocate(JSValue, WeakHandleOwner* = 0, void* context = 0);
    static void deallocate(WeakImpl*);

    void visitLiveWeakImpls(HeapRootVisitor&);
    void visitDeadWeakImpls(HeapRootVisitor&);

    void sweep();
    void resetAllocator();

    void shrink();

private:
    JS_EXPORT_PRIVATE WeakBlock::FreeCell* findAllocator();
    WeakBlock::FreeCell* tryFindAllocator();
    WeakBlock::FreeCell* addAllocator();
    void removeAllocator(WeakBlock*);

    WeakBlock::FreeCell* m_allocator;
    WeakBlock* m_nextAllocator;
    DoublyLinkedList<WeakBlock> m_blocks;
    Heap* m_heap;
};

inline WeakSet::WeakSet(Heap* heap)
    : m_allocator(0)
    , m_nextAllocator(0)
    , m_heap(heap)
{
}

inline WeakImpl* WeakSet::allocate(JSValue jsValue, WeakHandleOwner* weakHandleOwner, void* context)
{
    WeakBlock::FreeCell* allocator = m_allocator;
    if (UNLIKELY(!allocator))
        allocator = findAllocator();
    m_allocator = allocator->next;

    WeakImpl* weakImpl = WeakBlock::asWeakImpl(allocator);
    return new (NotNull, weakImpl) WeakImpl(jsValue, weakHandleOwner, context);
}

inline void WeakSet::deallocate(WeakImpl* weakImpl)
{
    weakImpl->setState(WeakImpl::Deallocated);
}

} // namespace JSC

#endif // WeakSet_h
