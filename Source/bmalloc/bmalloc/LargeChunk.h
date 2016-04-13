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
#include "Object.h"
#include "ObjectType.h"
#include "Sizes.h"
#include "SmallLine.h"
#include "SmallPage.h"
#include "VMAllocate.h"
#include <array>

namespace bmalloc {

class LargeChunk {
public:
    static LargeChunk* get(void*);

    static BeginTag* beginTag(void*);
    static EndTag* endTag(void*, size_t);

    LargeChunk(std::lock_guard<StaticMutex>&);

    size_t offset(void*);

    void* object(size_t offset);
    SmallPage* page(size_t offset);
    SmallLine* line(size_t offset);

    SmallPage* pageBegin() { return Object(m_memory).page(); }
    SmallPage* pageEnd() { return m_pages.end(); }
    
    SmallLine* lines() { return m_lines.begin(); }
    SmallPage* pages() { return m_pages.begin(); }

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

    std::array<SmallLine, largeChunkSize / smallLineSize> m_lines;
    std::array<SmallPage, largeChunkSize / vmPageSize> m_pages;
    std::array<BoundaryTag, boundaryTagCount> m_boundaryTags;
    char m_memory[] __attribute__((aligned(2 * smallMax + 0)));
};

static_assert(sizeof(LargeChunk) + largeMax <= largeChunkSize, "largeMax is too big");
static_assert(
    sizeof(LargeChunk) % vmPageSize + 2 * smallMax <= vmPageSize,
    "the first page of object memory in a small chunk must be able to allocate smallMax");

inline LargeChunk::LargeChunk(std::lock_guard<StaticMutex>& lock)
{
    Range range(begin(), end() - begin());
    BASSERT(range.size() <= largeObjectMax);

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

    // Track the memory used for metadata by allocating imaginary objects.
    for (char* it = reinterpret_cast<char*>(this); it < m_memory; it += smallLineSize) {
        Object object(it);
        object.line()->ref(lock);
        object.page()->ref(lock);
    }
}

inline LargeChunk* LargeChunk::get(void* object)
{
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
    LargeChunk* chunk = get(object);
    char* end = static_cast<char*>(object) + size;

    // We subtract largeMin before computing the end pointer's boundary tag. An
    // object's size need not be an even multiple of largeMin. Subtracting
    // largeMin rounds down to the last boundary tag prior to our neighbor.

    size_t boundaryTagNumber = (end - largeMin - reinterpret_cast<char*>(chunk)) / largeMin - 1; // - 1 to offset from the right sentinel.
    return static_cast<EndTag*>(&chunk->m_boundaryTags[boundaryTagNumber]);
}

inline size_t LargeChunk::offset(void* object)
{
    BASSERT(object >= this);
    BASSERT(object < reinterpret_cast<char*>(this) + largeChunkSize);
    return static_cast<char*>(object) - reinterpret_cast<char*>(this);
}

inline void* LargeChunk::object(size_t offset)
{
    return reinterpret_cast<char*>(this) + offset;
}

inline SmallPage* LargeChunk::page(size_t offset)
{
    size_t pageNumber = offset / vmPageSize;
    return &m_pages[pageNumber];
}

inline SmallLine* LargeChunk::line(size_t offset)
{
    size_t lineNumber = offset / smallLineSize;
    return &m_lines[lineNumber];
}

inline char* SmallLine::begin()
{
    LargeChunk* chunk = LargeChunk::get(this);
    size_t lineNumber = this - chunk->lines();
    size_t offset = lineNumber * smallLineSize;
    return &reinterpret_cast<char*>(chunk)[offset];
}

inline char* SmallLine::end()
{
    return begin() + smallLineSize;
}

inline SmallLine* SmallPage::begin()
{
    LargeChunk* chunk = LargeChunk::get(this);
    size_t pageNumber = this - chunk->pages();
    size_t lineNumber = pageNumber * smallLineCount;
    return &chunk->lines()[lineNumber];
}

inline SmallLine* SmallPage::end()
{
    return begin() + smallLineCount;
}

inline Object::Object(void* object)
    : m_chunk(LargeChunk::get(object))
    , m_offset(m_chunk->offset(object))
{
}

inline Object::Object(LargeChunk* chunk, void* object)
    : m_chunk(chunk)
    , m_offset(m_chunk->offset(object))
{
    BASSERT(chunk == LargeChunk::get(object));
}

inline void* Object::begin()
{
    return m_chunk->object(m_offset);
}

inline void* Object::pageBegin()
{
    return m_chunk->object(roundDownToMultipleOf(vmPageSize, m_offset));
}

inline SmallLine* Object::line()
{
    return m_chunk->line(m_offset);
}

inline SmallPage* Object::page()
{
    return m_chunk->page(m_offset);
}

}; // namespace bmalloc

#endif // LargeChunk
