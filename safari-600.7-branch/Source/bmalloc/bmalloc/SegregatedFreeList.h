/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef SegregatedFreeList_h
#define SegregatedFreeList_h

#include "Range.h"
#include "Vector.h"
#include <array>

namespace bmalloc {

class SegregatedFreeList {
public:
    SegregatedFreeList();

    void insert(const Range&);

    // Returns a reasonable fit for the provided size, or Range() if no fit
    // is found. May return Range() spuriously if searching takes too long.
    // Incrementally removes stale items from the free list while searching.
    // Does not eagerly remove the returned range from the free list.
    Range take(size_t);

    // Returns an unreasonable fit for the provided size, or Range() if no fit
    // is found. Never returns Range() spuriously.
    // Incrementally removes stale items from the free list while searching.
    // Eagerly removes the returned range from the free list.
    Range takeGreedy(size_t);
    
private:
    typedef Vector<Range> List;

    List& select(size_t);

    Range take(List&, size_t);
    Range takeGreedy(List&, size_t);

    std::array<List, 19> m_lists;
};

} // namespace bmalloc

#endif // SegregatedFreeList_h
