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

#ifndef GCHandle_h
#define GCHandle_h

#include "AlignedMemoryAllocator.h"

namespace JSC {

class Heap;
class JSCell;
class WeakGCHandle;
class WeakGCHandlePool;

class WeakGCHandle {
    friend class WeakGCHandlePool;

public:
    // Because JSCell objects are aligned, we can use the lower two bits as
    // status flags. The least significant bit is set when the handle is not a
    // pointer, i.e. when it's used as a offset for the free list in
    // WeakGCHandlePool. The second least significant bit is set when the object
    // the pointer corresponds to has been deleted by a garbage collection

    bool isValidPtr() { return !(m_ptr & 3); }
    bool isPtr() { return !(m_ptr & 1); }
    bool isNext() { return (m_ptr & 3) == 1; }

    void invalidate()
    {
        ASSERT(isValidPtr());
        m_ptr |= 2;
    }

    JSCell* get()
    {
        ASSERT(isPtr());
        return reinterpret_cast<JSCell*>(m_ptr & ~3);
    }

    void set(JSCell* p)
    {
        m_ptr = reinterpret_cast<uintptr_t>(p);
        ASSERT(isPtr());
    }

    WeakGCHandlePool* pool();

private:
    uintptr_t getNextInFreeList()
    {
        ASSERT(isNext());
        return m_ptr >> 2;
    }

    void setNextInFreeList(uintptr_t n)
    {
        m_ptr = (n << 2) | 1;
        ASSERT(isNext());
    }

    uintptr_t m_ptr;
};

class WeakGCHandlePool {
public:
    static const size_t poolSize = 32 * 1024; // 32k
    static const size_t poolMask = ~(poolSize - 1);
    static const size_t numPoolEntries = (poolSize - sizeof(Heap*) - 3 * sizeof(unsigned)) / sizeof(WeakGCHandle);

    typedef AlignedMemoryAllocator<WeakGCHandlePool::poolSize> Allocator;

    WeakGCHandlePool();

    WeakGCHandle* allocate(JSCell* cell);
    void free(WeakGCHandle*);

    bool isFull()
    {
        ASSERT(m_entriesSize < WeakGCHandlePool::numPoolEntries);
        return m_entriesSize == WeakGCHandlePool::numPoolEntries - 1;
    }

    void update();

    void* operator new(size_t, AlignedMemory<WeakGCHandlePool::poolSize>&);

private:
    Heap* m_heap;
    unsigned m_entriesSize;
    unsigned m_initialAlloc;

    WeakGCHandle m_entries[WeakGCHandlePool::numPoolEntries];
};

}
#endif
