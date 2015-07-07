/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef RecursiveAllocationScope_h
#define RecursiveAllocationScope_h

#include "Heap.h"
#include "VM.h"

namespace JSC {

class RecursiveAllocationScope {
public:
    RecursiveAllocationScope(Heap& heap)
        : m_heap(heap)
#ifndef NDEBUG
        , m_savedObjectClass(heap.vm()->m_initializingObjectClass)
#endif
    {
#ifndef NDEBUG
        m_heap.vm()->m_initializingObjectClass = nullptr;
#endif
        m_heap.m_deferralDepth++; // Make sure that we don't GC.
    }
    
    ~RecursiveAllocationScope()
    {
        m_heap.m_deferralDepth--; // Decrement deferal manually so we don't GC when we do so since we are already GCing!.
#ifndef NDEBUG
        m_heap.vm()->m_initializingObjectClass = m_savedObjectClass;
#endif
    }

private:
    Heap& m_heap;
#ifndef NDEBUG
    const ClassInfo* m_savedObjectClass;
#endif
};

}

#endif // RecursiveAllocationScope_h
