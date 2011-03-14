/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "ConservativeSet.h"

#include "Heap.h"

namespace JSC {

inline bool isPointerAligned(void* p)
{
    return !((intptr_t)(p) & (sizeof(char*) - 1));
}

void ConservativeSet::grow()
{
    size_t newCapacity = m_capacity == inlineCapacity ? nonInlineCapacity : m_capacity * 2;
    JSCell** newSet = static_cast<JSCell**>(OSAllocator::reserveAndCommit(newCapacity * sizeof(JSCell*)));
    memcpy(newSet, m_set, m_size * sizeof(JSCell*));
    if (m_set != m_inlineSet)
        OSAllocator::decommitAndRelease(m_set, m_capacity * sizeof(JSCell*));
    m_capacity = newCapacity;
    m_set = newSet;
}

void ConservativeSet::add(void* begin, void* end)
{
    ASSERT(begin <= end);
    ASSERT((static_cast<char*>(end) - static_cast<char*>(begin)) < 0x1000000);
    ASSERT(isPointerAligned(begin));
    ASSERT(isPointerAligned(end));

    for (char** it = static_cast<char**>(begin); it != static_cast<char**>(end); ++it) {
        if (!m_heap->contains(*it))
            continue;

        if (m_size == m_capacity)
            grow();

        m_set[m_size++] = reinterpret_cast<JSCell*>(*it);
    }
}

} // namespace JSC
