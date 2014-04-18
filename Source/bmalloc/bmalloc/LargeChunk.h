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

namespace bmalloc {

class LargeChunk {
public:
    static LargeChunk* create();
    static LargeChunk* get(void*);

    static BeginTag* beginTag(void*);
    static EndTag* endTag(void*, size_t);

    char* begin() { return m_memory; }
    char* end() { return reinterpret_cast<char*>(this) + largeChunkSize; }

private:
     // Round up to ensure 2 dummy boundary tags -- for the left and right sentinels.
     static const size_t boundaryTagCount = max(2 * largeMin / sizeof(BoundaryTag), largeChunkSize / largeMin); 

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

    BoundaryTag m_boundaryTags[boundaryTagCount];
    alignas(largeAlignment) char m_memory[];
};

inline LargeChunk* LargeChunk::create()
{
    size_t vmSize = bmalloc::vmSize(largeChunkSize);
    std::pair<void*, Range> result = vmAllocate(vmSize, superChunkSize, largeChunkOffset);
    return new (result.first) LargeChunk;
}

inline LargeChunk* LargeChunk::get(void* object)
{
    BASSERT(isLarge(object));
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
    BASSERT(isLarge(object));

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
