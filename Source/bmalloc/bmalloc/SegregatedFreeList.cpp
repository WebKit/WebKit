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

#include "BeginTag.h"
#include "LargeChunk.h"
#include "SegregatedFreeList.h"
#include "Vector.h"

namespace bmalloc {

SegregatedFreeList::SegregatedFreeList()
{
    ASSERT(&select(largeMax) - m_lists.begin() == m_lists.size() - 1);
}

void SegregatedFreeList::insert(const Range& range)
{
IF_DEBUG(
    BeginTag* beginTag = LargeChunk::beginTag(range.begin());
    ASSERT(beginTag->isInFreeList(range.size()));
)

    auto& list = select(range.size());
    list.push(range);
}

Range SegregatedFreeList::takeGreedy(size_t minimum)
{
    for (size_t i = m_lists.size(); i-- > 0; ) {
        Range range = takeGreedy(m_lists[i], minimum);
        if (!range)
            continue;

        return range;
    }
    return Range();
}

Range SegregatedFreeList::takeGreedy(List& list, size_t minimum)
{
    for (size_t i = list.size(); i-- > 0; ) {
        Range range = list[i];

        // We don't eagerly remove items when we merge and/or split ranges,
        // so we need to validate each free list entry before using it.
        BeginTag* beginTag = LargeChunk::beginTag(range.begin());
        if (!beginTag->isInFreeList(range.size())) {
            list.pop(i);
            continue;
        }

        if (range.size() < minimum)
            continue;

        list.pop(i);
        return range;
    }

    return Range();
}

Range SegregatedFreeList::take(size_t minimum)
{
    for (auto* list = &select(minimum); list != m_lists.end(); ++list) {
        Range range = take(*list, minimum);
        if (!range)
            continue;

        return range;
    }
    return Range();
}

INLINE auto SegregatedFreeList::select(size_t size) -> List&
{
    size_t alignCount = (size - largeMin) / largeAlignment;
    size_t result = 0;
    while (alignCount) {
        ++result;
        alignCount >>= 1;
    }
    return m_lists[result];
}

INLINE Range SegregatedFreeList::take(List& list, size_t minimum)
{
    Range first;
    size_t end = list.size() > segregatedFreeListSearchDepth ? list.size() - segregatedFreeListSearchDepth : 0;
    for (size_t i = list.size(); i-- > end; ) {
        Range range = list[i];

        // We don't eagerly remove items when we merge and/or split ranges, so
        // we need to validate each free list entry before using it.
        BeginTag* beginTag = LargeChunk::beginTag(range.begin());
        if (!beginTag->isInFreeList(range.size())) {
            list.pop(i);
            continue;
        }

        if (range.size() < minimum)
            continue;

        if (!!first && first < range)
            continue;

        first = range;
    }
    
    return first;
}

} // namespace bmalloc
