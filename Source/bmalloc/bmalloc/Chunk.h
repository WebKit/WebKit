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

#ifndef Chunk_h
#define Chunk_h

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

class Chunk {
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

    static const size_t boundaryTagCount = chunkSize / largeMin;
    static_assert(boundaryTagCount > 2, "Chunk must have space for two sentinel boundary tags");

public:
    static Chunk* get(void*);

    static BeginTag* beginTag(void*);
    static EndTag* endTag(void*, size_t);

    Chunk(std::lock_guard<StaticMutex>&, ObjectType);

    size_t offset(void*);

    char* object(size_t offset);
    SmallPage* page(size_t offset);
    SmallLine* line(size_t offset);

    char* bytes() { return reinterpret_cast<char*>(this); }
    SmallLine* lines() { return m_lines.begin(); }
    SmallPage* pages() { return m_pages.begin(); }
    std::array<BoundaryTag, boundaryTagCount>& boundaryTags() { return m_boundaryTags; }

    ObjectType objectType() { return m_objectType; }

private:
    union {
        // The first few bytes of metadata cover the metadata region, so they're
        // not used. We can steal them to store m_objectType.
        ObjectType m_objectType;
        std::array<SmallLine, chunkSize / smallLineSize> m_lines;
    };

    union {
        // A chunk is either small or large for its lifetime, so we can union
        // small and large metadata, and then use one or the other at runtime.
        std::array<SmallPage, chunkSize / smallPageSize> m_pages;
        std::array<BoundaryTag, boundaryTagCount> m_boundaryTags;
    };
};

static_assert(sizeof(Chunk) + largeMax <= chunkSize, "largeMax is too big");

static_assert(sizeof(Chunk) / smallLineSize > sizeof(ObjectType),
    "Chunk::m_objectType overlaps with metadata");

inline Chunk::Chunk(std::lock_guard<StaticMutex>&, ObjectType objectType)
    : m_objectType(objectType)
{
}

inline Chunk* Chunk::get(void* object)
{
    return static_cast<Chunk*>(mask(object, chunkMask));
}

inline BeginTag* Chunk::beginTag(void* object)
{
    Chunk* chunk = get(object);
    size_t boundaryTagNumber = (static_cast<char*>(object) - reinterpret_cast<char*>(chunk)) / largeMin - 1; // - 1 to offset from the right sentinel.
    return static_cast<BeginTag*>(&chunk->m_boundaryTags[boundaryTagNumber]);
}

inline EndTag* Chunk::endTag(void* object, size_t size)
{
    Chunk* chunk = get(object);
    char* end = static_cast<char*>(object) + size;

    // We subtract largeMin before computing the end pointer's boundary tag. An
    // object's size need not be an even multiple of largeMin. Subtracting
    // largeMin rounds down to the last boundary tag prior to our neighbor.

    size_t boundaryTagNumber = (end - largeMin - reinterpret_cast<char*>(chunk)) / largeMin - 1; // - 1 to offset from the right sentinel.
    return static_cast<EndTag*>(&chunk->m_boundaryTags[boundaryTagNumber]);
}

inline size_t Chunk::offset(void* object)
{
    BASSERT(object >= this);
    BASSERT(object < bytes() + chunkSize);
    return static_cast<char*>(object) - bytes();
}

inline char* Chunk::object(size_t offset)
{
    return bytes() + offset;
}

inline SmallPage* Chunk::page(size_t offset)
{
    size_t pageNumber = offset / smallPageSize;
    SmallPage* page = &m_pages[pageNumber];
    return page - page->slide();
}

inline SmallLine* Chunk::line(size_t offset)
{
    size_t lineNumber = offset / smallLineSize;
    return &m_lines[lineNumber];
}

inline char* SmallLine::begin()
{
    Chunk* chunk = Chunk::get(this);
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
    BASSERT(!m_slide);
    Chunk* chunk = Chunk::get(this);
    size_t pageNumber = this - chunk->pages();
    size_t lineNumber = pageNumber * smallPageLineCount;
    return &chunk->lines()[lineNumber];
}

inline Object::Object(void* object)
    : m_chunk(Chunk::get(object))
    , m_offset(m_chunk->offset(object))
{
}

inline Object::Object(Chunk* chunk, void* object)
    : m_chunk(chunk)
    , m_offset(m_chunk->offset(object))
{
    BASSERT(chunk == Chunk::get(object));
}

inline char* Object::begin()
{
    return m_chunk->object(m_offset);
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

#endif // Chunk
