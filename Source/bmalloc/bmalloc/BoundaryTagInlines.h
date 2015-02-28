/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

inline Range BoundaryTag::init(LargeChunk* chunk)
{
    Range range(chunk->begin(), chunk->end() - chunk->begin());

    BeginTag* beginTag = LargeChunk::beginTag(range.begin());
    beginTag->setRange(range);
    beginTag->setFree(true);
    beginTag->setHasPhysicalPages(false);

    EndTag* endTag = LargeChunk::endTag(range.begin(), range.size());
    endTag->init(beginTag);

    // Mark the left and right edges of our chunk as allocated. This naturally
    // prevents merging logic from overflowing beyond our chunk, without requiring
    // special-case checks.
    
    EndTag* leftSentinel = beginTag->prev();
    BASSERT(leftSentinel >= static_cast<void*>(chunk));
    leftSentinel->initSentinel();

    BeginTag* rightSentinel = endTag->next();
    BASSERT(rightSentinel < static_cast<void*>(range.begin()));
    rightSentinel->initSentinel();
    
    return range;
}

} // namespace bmalloc

#endif // BoundaryTagInlines_h
