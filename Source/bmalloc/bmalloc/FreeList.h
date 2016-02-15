/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#ifndef FreeList_h
#define FreeList_h

#include "LargeObject.h"
#include "Vector.h"

namespace bmalloc {

// Helper object for SegregatedFreeList.

class FreeList {
public:
    FreeList();

    void push(Owner, const LargeObject&);

    LargeObject take(Owner, size_t);
    LargeObject take(Owner, size_t alignment, size_t, size_t unalignedSize);
    
    LargeObject takeGreedy(Owner);

    void removeInvalidAndDuplicateEntries(Owner);
    
private:
    Vector<Range> m_vector;
    size_t m_limit;
};

inline FreeList::FreeList()
    : m_vector()
    , m_limit(freeListSearchDepth)
{
}

inline void FreeList::push(Owner owner, const LargeObject& largeObject)
{
    BASSERT(largeObject.isFree());
    BASSERT(!largeObject.prevCanMerge());
    BASSERT(!largeObject.nextCanMerge());
    if (m_vector.size() == m_limit) {
        removeInvalidAndDuplicateEntries(owner);
        m_limit = std::max(m_vector.size() * freeListGrowFactor, freeListSearchDepth);
    }
    m_vector.push(largeObject.range());
}

} // namespace bmalloc

#endif // FreeList_h
