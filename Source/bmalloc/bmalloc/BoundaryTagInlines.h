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

#ifndef BoundaryTagInlines_h
#define BoundaryTagInlines_h

#include "Range.h"
#include "BeginTag.h"
#include "EndTag.h"
#include "Inline.h"
#include "LargeChunk.h"

namespace bmalloc {

static inline void validate(const Range& range)
{
IF_DEBUG(
    BeginTag* beginTag = LargeChunk::beginTag(range.begin());
    EndTag* endTag = LargeChunk::endTag(range.begin(), range.size());

    ASSERT(!beginTag->isEnd());
    if (beginTag->isXLarge())
        return;
)
    ASSERT(range.size() >= largeMin);
    ASSERT(beginTag->size() == range.size());

    ASSERT(beginTag->size() == endTag->size());
    ASSERT(beginTag->isFree() == endTag->isFree());
    ASSERT(beginTag->hasPhysicalPages() == endTag->hasPhysicalPages());
    ASSERT(beginTag->isXLarge() == endTag->isXLarge());
    ASSERT(static_cast<BoundaryTag*>(endTag) == static_cast<BoundaryTag*>(beginTag) || endTag->isEnd());
}

static inline void validatePrev(EndTag* prev, void* object)
{
    size_t prevSize = prev->size();
    void* prevObject = static_cast<char*>(object) - prevSize;
    validate(Range(prevObject, prevSize));
}

static inline void validateNext(BeginTag* next, const Range& range)
{
    if (next->size() == largeMin && !next->isFree()) // Right sentinel tag.
        return;

    void* nextObject = range.end();
    size_t nextSize = next->size();
    validate(Range(nextObject, nextSize));
}

static inline void validate(EndTag* prev, const Range& range, BeginTag* next)
{
    validatePrev(prev, range.begin());
    validate(range);
    validateNext(next, range);
}

inline Range BoundaryTag::init(LargeChunk* chunk)
{
    Range range(chunk->begin(), chunk->end() - chunk->begin());

    BeginTag* beginTag = LargeChunk::beginTag(range.begin());
    beginTag->setSize(range.size());
    beginTag->setFree(true);
    beginTag->setHasPhysicalPages(false);

    EndTag* endTag = LargeChunk::endTag(range.begin(), range.size());
    *endTag = *beginTag;

    // Mark the left and right edges of our chunk as allocated. This naturally
    // prevents merging logic from overflowing beyond our chunk, without requiring
    // special-case checks.
    
    EndTag* leftSentinel = beginTag->prev();
    ASSERT(leftSentinel >= static_cast<void*>(chunk));
    leftSentinel->setSize(largeMin);
    leftSentinel->setFree(false);

    BeginTag* rightSentinel = endTag->next();
    ASSERT(rightSentinel < static_cast<void*>(range.begin()));
    rightSentinel->setSize(largeMin);
    rightSentinel->setFree(false);
    
    return range;
}

inline void BoundaryTag::mergeLargeLeft(EndTag*& prev, BeginTag*& beginTag, Range& range, bool& hasPhysicalPages)
{
    Range left(range.begin() - prev->size(), prev->size());

    hasPhysicalPages &= prev->hasPhysicalPages();

    range = Range(left.begin(), left.size() + range.size());

    prev->clear();
    beginTag->clear();

    beginTag = LargeChunk::beginTag(range.begin());
}

inline void BoundaryTag::mergeLargeRight(EndTag*& endTag, BeginTag*& next, Range& range, bool& hasPhysicalPages)
{
    Range right(range.end(), next->size());

    hasPhysicalPages &= next->hasPhysicalPages();

    range = Range(range.begin(), range.size() + right.size());

    endTag->clear();
    next->clear();

    endTag = LargeChunk::endTag(range.begin(), range.size());
}

INLINE void BoundaryTag::mergeLarge(BeginTag*& beginTag, EndTag*& endTag, Range& range)
{
    EndTag* prev = beginTag->prev();
    BeginTag* next = endTag->next();
    bool hasPhysicalPages = beginTag->hasPhysicalPages();

    validate(prev, range, next);

    if (prev->isFree())
        mergeLargeLeft(prev, beginTag, range, hasPhysicalPages);

    if (next->isFree())
        mergeLargeRight(endTag, next, range, hasPhysicalPages);

    beginTag->setSize(range.size());
    beginTag->setFree(true);
    beginTag->setHasPhysicalPages(hasPhysicalPages);

    if (endTag != static_cast<BoundaryTag*>(beginTag))
        *endTag = *beginTag;

    validate(beginTag->prev(), range, endTag->next());
}

inline Range BoundaryTag::deallocate(void* object)
{
    BeginTag* beginTag = LargeChunk::beginTag(object);
    ASSERT(!beginTag->isFree());
    ASSERT(!beginTag->isXLarge())

    Range range(object, beginTag->size());
    EndTag* endTag = LargeChunk::endTag(range.begin(), range.size());
    mergeLarge(beginTag, endTag, range);
    
    return range;
}

INLINE void BoundaryTag::splitLarge(BeginTag* beginTag, size_t size, EndTag*& endTag, Range& range, Range& leftover)
{
    beginTag->setSize(size);

    EndTag* splitEndTag = LargeChunk::endTag(range.begin(), size);
    if (splitEndTag != static_cast<BoundaryTag*>(beginTag))
        *splitEndTag = *beginTag;

    leftover = Range(range.begin() + size, range.size() - size);
    ASSERT(leftover.size() >= largeMin);
    BeginTag* leftoverBeginTag = LargeChunk::beginTag(leftover.begin());
    *leftoverBeginTag = *beginTag;
    leftoverBeginTag->setSize(leftover.size());

    if (leftoverBeginTag != static_cast<BoundaryTag*>(endTag))
        *endTag = *leftoverBeginTag;

    validate(beginTag->prev(), Range(range.begin(), size), leftoverBeginTag);
    validate(leftoverBeginTag->prev(), leftover, endTag->next());

    range = Range(range.begin(), size);
    endTag = splitEndTag;
}

INLINE void BoundaryTag::allocate(size_t size, Range& range, Range& leftover, bool& hasPhysicalPages)
{
    BeginTag* beginTag = LargeChunk::beginTag(range.begin());
    EndTag* endTag = LargeChunk::endTag(range.begin(), range.size());

    ASSERT(beginTag->isFree());
    validate(beginTag->prev(), range, endTag->next());

    if (range.size() - size > largeMin)
        splitLarge(beginTag, size, endTag, range, leftover);

    hasPhysicalPages = beginTag->hasPhysicalPages();

    beginTag->setHasPhysicalPages(true);
    beginTag->setFree(false);

    endTag->setHasPhysicalPages(true);
    endTag->setFree(false);
}

} // namespace bmalloc

#endif // BoundaryTagInlines_h
