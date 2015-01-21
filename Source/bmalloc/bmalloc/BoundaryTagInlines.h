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
    UNUSED(range);
IF_DEBUG(
    BeginTag* beginTag = LargeChunk::beginTag(range.begin());
    EndTag* endTag = LargeChunk::endTag(range.begin(), range.size());

    BASSERT(!beginTag->isEnd());
    BASSERT(range.size() >= largeMin);
    BASSERT(beginTag->size() == range.size());

    BASSERT(beginTag->size() == endTag->size());
    BASSERT(beginTag->isFree() == endTag->isFree());
    BASSERT(beginTag->hasPhysicalPages() == endTag->hasPhysicalPages());
    BASSERT(static_cast<BoundaryTag*>(endTag) == static_cast<BoundaryTag*>(beginTag) || endTag->isEnd());
);
}

static inline void validatePrev(EndTag* prev, void* object)
{
    size_t prevSize = prev->size();
    void* prevObject = static_cast<char*>(object) - prevSize;
    validate(Range(prevObject, prevSize));
}

static inline void validateNext(BeginTag* next, const Range& range)
{
    if (next->size() == largeMin && !next->compactBegin() && !next->isFree()) // Right sentinel tag.
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
    beginTag->setRange(range);
    beginTag->setFree(true);
    beginTag->setHasPhysicalPages(false);

    EndTag* endTag = LargeChunk::endTag(range.begin(), range.size());
    *endTag = *beginTag;

    // Mark the left and right edges of our chunk as allocated. This naturally
    // prevents merging logic from overflowing beyond our chunk, without requiring
    // special-case checks.
    
    EndTag* leftSentinel = beginTag->prev();
    BASSERT(leftSentinel >= static_cast<void*>(chunk));
    leftSentinel->setRange(Range(nullptr, largeMin));
    leftSentinel->setFree(false);

    BeginTag* rightSentinel = endTag->next();
    BASSERT(rightSentinel < static_cast<void*>(range.begin()));
    rightSentinel->setRange(Range(nullptr, largeMin));
    rightSentinel->setFree(false);
    
    return range;
}

inline Range BoundaryTag::mergeLeft(const Range& range, BeginTag*& beginTag, EndTag* prev, bool& hasPhysicalPages)
{
    Range left(range.begin() - prev->size(), prev->size());
    Range merged(left.begin(), left.size() + range.size());

    hasPhysicalPages &= prev->hasPhysicalPages();

    prev->clear();
    beginTag->clear();

    beginTag = LargeChunk::beginTag(merged.begin());
    return merged;
}

inline Range BoundaryTag::mergeRight(const Range& range, EndTag*& endTag, BeginTag* next, bool& hasPhysicalPages)
{
    Range right(range.end(), next->size());
    Range merged(range.begin(), range.size() + right.size());

    hasPhysicalPages &= next->hasPhysicalPages();

    endTag->clear();
    next->clear();

    endTag = LargeChunk::endTag(merged.begin(), merged.size());
    return merged;
}

INLINE Range BoundaryTag::merge(const Range& range, BeginTag*& beginTag, EndTag*& endTag)
{
    EndTag* prev = beginTag->prev();
    BeginTag* next = endTag->next();
    bool hasPhysicalPages = beginTag->hasPhysicalPages();

    validate(prev, range, next);
    
    Range merged = range;

    if (prev->isFree())
        merged = mergeLeft(merged, beginTag, prev, hasPhysicalPages);

    if (next->isFree())
        merged = mergeRight(merged, endTag, next, hasPhysicalPages);

    beginTag->setRange(merged);
    beginTag->setFree(true);
    beginTag->setHasPhysicalPages(hasPhysicalPages);

    if (endTag != static_cast<BoundaryTag*>(beginTag))
        *endTag = *beginTag;

    validate(beginTag->prev(), merged, endTag->next());
    return merged;
}

inline Range BoundaryTag::deallocate(void* object)
{
    BeginTag* beginTag = LargeChunk::beginTag(object);
    BASSERT(!beginTag->isFree());

    Range range(object, beginTag->size());
    EndTag* endTag = LargeChunk::endTag(range.begin(), range.size());
    return merge(range, beginTag, endTag);
}

INLINE void BoundaryTag::split(const Range& range, size_t size, BeginTag* beginTag, EndTag*& endTag, Range& leftover)
{
    leftover = Range(range.begin() + size, range.size() - size);
    Range split(range.begin(), size);

    beginTag->setRange(split);

    EndTag* splitEndTag = LargeChunk::endTag(split.begin(), size);
    if (splitEndTag != static_cast<BoundaryTag*>(beginTag))
        *splitEndTag = *beginTag;

    BASSERT(leftover.size() >= largeMin);
    BeginTag* leftoverBeginTag = LargeChunk::beginTag(leftover.begin());
    *leftoverBeginTag = *beginTag;
    leftoverBeginTag->setRange(leftover);

    if (leftoverBeginTag != static_cast<BoundaryTag*>(endTag))
        *endTag = *leftoverBeginTag;

    validate(beginTag->prev(), split, leftoverBeginTag);
    validate(leftoverBeginTag->prev(), leftover, endTag->next());

    endTag = splitEndTag;
}

INLINE void BoundaryTag::allocate(const Range& range, size_t size, Range& leftover, bool& hasPhysicalPages)
{
    BeginTag* beginTag = LargeChunk::beginTag(range.begin());
    EndTag* endTag = LargeChunk::endTag(range.begin(), range.size());

    BASSERT(beginTag->isFree());
    validate(beginTag->prev(), range, endTag->next());

    if (range.size() - size > largeMin)
        split(range, size, beginTag, endTag, leftover);

    hasPhysicalPages = beginTag->hasPhysicalPages();

    beginTag->setHasPhysicalPages(true);
    beginTag->setFree(false);

    endTag->setHasPhysicalPages(true);
    endTag->setFree(false);
}

} // namespace bmalloc

#endif // BoundaryTagInlines_h
