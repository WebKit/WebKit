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

#ifndef LargeChunk_h
#define LargeChunk_h

#include "BeginTag.h"
#include "EndTag.h"
#include "ObjectType.h"
#include "Sizes.h"
#include "VMAllocate.h"
#include <array>

namespace bmalloc {

class LargeChunk {
public:
    LargeChunk();
    static LargeChunk* get(void*);

    static BeginTag* beginTag(void*);
    static EndTag* endTag(void*, size_t);

    char* begin() { return m_memory; }
    char* end() { return reinterpret_cast<char*>(this) + largeChunkSize; }

private:
    static const size_t boundaryTagCount = largeChunkSize / largeMin;
    static_assert(boundaryTagCount > 2, "LargeChunk must have space for two sentinel boundary tags");

    // Our metadata layout includes a left and right edge sentinel.
    // Metadata takes up enough space to leave at least the first two
    // boundary tag slots unused.
    //
    //      So, boundary tag space looks like this:
    //
    //          [OOXXXXX...]
    //
    //      And BoundaryTag::get subtracts one, producing:
    //
    //          [OXXXXX...O].
    //
    // We use the X's for boundary tags and the O's for edge sentinels.

    std::array<BoundaryTag, boundaryTagCount> m_boundaryTags;
    char m_memory[] __attribute__((aligned(largeAlignment+0)));
};

static_assert(largeChunkMetadataSize == sizeof(LargeChunk), "Our largeChunkMetadataSize math in Sizes.h is wrong");
static_assert(largeChunkMetadataSize + largeObjectMax == largeChunkSize, "largeObjectMax is too small or too big");

inline LargeChunk::LargeChunk()
{
    Range range(begin(), end() - begin());
    BASSERT(range.size() == largeObjectMax);

    BeginTag* beginTag = LargeChunk::beginTag(range.begin());
    beginTag->setRange(range);
    beginTag->setFree(true);
    beginTag->setVMState(VMState::Virtual);

    EndTag* endTag = LargeChunk::endTag(range.begin(), range.size());
    endTag->init(beginTag);

    // Mark the left and right edges of our range as allocated. This naturally
    // prevents merging logic from overflowing left (into metadata) or right
    // (beyond our chunk), without requiring special-case checks.

    EndTag* leftSentinel = beginTag->prev();
    BASSERT(leftSentinel >= m_boundaryTags.begin());
    BASSERT(leftSentinel < m_boundaryTags.end());
    leftSentinel->initSentinel();

    BeginTag* rightSentinel = endTag->next();
    BASSERT(rightSentinel >= m_boundaryTags.begin());
    BASSERT(rightSentinel < m_boundaryTags.end());
    rightSentinel->initSentinel();
}

inline LargeChunk* LargeChunk::get(void* object)
{
    BASSERT(!isSmall(object));
    return static_cast<LargeChunk*>(mask(object, largeChunkMask));
}

inline BeginTag* LargeChunk::beginTag(void* object)
{
    LargeChunk* chunk = get(object);
    size_t boundaryTagNumber = (static_cast<char*>(object) - reinterpret_cast<char*>(chunk)) / largeMin - 1; // - 1 to offset from the right sentinel.
    return static_cast<BeginTag*>(&chunk->m_boundaryTags[boundaryTagNumber]);
}

inline EndTag* LargeChunk::endTag(void* object, size_t size)
{
    BASSERT(!isSmall(object));

    LargeChunk* chunk = get(object);
    char* end = static_cast<char*>(object) + size;

    // We subtract largeMin before computing the end pointer's boundary tag. An
    // object's size need not be an even multiple of largeMin. Subtracting
    // largeMin rounds down to the last boundary tag prior to our neighbor.

    size_t boundaryTagNumber = (end - largeMin - reinterpret_cast<char*>(chunk)) / largeMin - 1; // - 1 to offset from the right sentinel.
    return static_cast<EndTag*>(&chunk->m_boundaryTags[boundaryTagNumber]);
}

}; // namespace bmalloc

#endif // LargeChunk
