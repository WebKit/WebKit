/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "GCHandle.h"

namespace JSC {

WeakGCHandlePool* WeakGCHandle::pool()
{
    uintptr_t pool = (reinterpret_cast<uintptr_t>(this) & WeakGCHandlePool::poolMask);
    return reinterpret_cast<WeakGCHandlePool*>(pool);
}

WeakGCHandlePool::WeakGCHandlePool()
{
    ASSERT(sizeof(WeakGCHandlePool) <= WeakGCHandlePool::poolSize);
    m_entriesSize = 0;
    m_initialAlloc = 1;
    m_entries[0].setNextInFreeList(0);
}

WeakGCHandle* WeakGCHandlePool::allocate(JSCell* cell)
{
    ASSERT(cell);
    ASSERT(m_entries[0].isNext());
    unsigned freeList = m_entries[0].getNextInFreeList();
    ASSERT(freeList < WeakGCHandlePool::numPoolEntries);
    ASSERT(m_entriesSize < WeakGCHandlePool::numPoolEntries);

    if (m_entriesSize == WeakGCHandlePool::numPoolEntries - 1)
        return 0;

    if (freeList) {
        unsigned i = freeList;
        freeList = m_entries[i].getNextInFreeList();
        m_entries[i].set(cell);
        m_entries[0].setNextInFreeList(freeList);
        ++m_entriesSize;
        return &m_entries[i];
    }

    ASSERT(m_initialAlloc < WeakGCHandlePool::numPoolEntries);

    unsigned i = m_initialAlloc;
    ++m_initialAlloc;
    m_entries[i].set(cell);
    ++m_entriesSize;
    return &m_entries[i];

}

void WeakGCHandlePool::free(WeakGCHandle* handle)
{
    ASSERT(handle->pool() == this);
    ASSERT(m_entries[0].isNext());
    unsigned freeList = m_entries[0].getNextInFreeList();
    ASSERT(freeList < WeakGCHandlePool::numPoolEntries);
    handle->setNextInFreeList(freeList);
    m_entries[0].setNextInFreeList(handle - m_entries);
    --m_entriesSize;
}

void* WeakGCHandlePool::operator new(size_t, AlignedMemory<WeakGCHandlePool::poolSize>& allocation)
{
    return allocation.base();
}

}
