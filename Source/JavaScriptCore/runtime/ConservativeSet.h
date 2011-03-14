/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef ConservativeSet_h
#define ConservativeSet_h

#include <wtf/OSAllocator.h>
#include <wtf/Vector.h>

namespace JSC {

class JSCell;
class Heap;

// May contain duplicates.

class ConservativeSet {
public:
    ConservativeSet(Heap*);
    ~ConservativeSet();

    void add(void* begin, void* end);
    
    size_t size();
    JSCell** set();

private:
    static const size_t inlineCapacity = 128;
    static const size_t nonInlineCapacity = 8192 / sizeof(JSCell*);
    
    void grow();

    Heap* m_heap;
    JSCell** m_set;
    size_t m_size;
    size_t m_capacity;
    JSCell* m_inlineSet[inlineCapacity];
};

inline ConservativeSet::ConservativeSet(Heap* heap)
    : m_heap(heap)
    , m_set(m_inlineSet)
    , m_size(0)
    , m_capacity(inlineCapacity)
{
}

inline ConservativeSet::~ConservativeSet()
{
    if (m_set != m_inlineSet)
        OSAllocator::decommitAndRelease(m_set, m_capacity * sizeof(JSCell*));
}

inline size_t ConservativeSet::size()
{
    return m_size;
}

inline JSCell** ConservativeSet::set()
{
    return m_set;
}

} // namespace JSC

#endif // ConservativeSet_h
