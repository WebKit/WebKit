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

#ifndef SmallChunk_h
#define SmallChunk_h

#include "Object.h"
#include "Sizes.h"
#include "SmallLine.h"
#include "SmallPage.h"
#include "VMAllocate.h"

namespace bmalloc {

class SmallChunk {
public:
    SmallChunk(std::lock_guard<StaticMutex>&);

    static SmallChunk* get(void*);
    size_t offset(void*);

    void* object(size_t offset);
    SmallPage* page(size_t offset);
    SmallLine* line(size_t offset);

    SmallPage* begin() { return Object(m_memory).page(); }
    SmallPage* end() { return m_pages.end(); }
    
    SmallLine* lines() { return m_lines.begin(); }
    SmallPage* pages() { return m_pages.begin(); }
    
private:
    std::array<SmallLine, smallChunkSize / smallLineSize> m_lines;
    std::array<SmallPage, smallChunkSize / vmPageSize> m_pages;
    char m_memory[] __attribute__((aligned(2 * smallMax + 0)));
};

static_assert(!(vmPageSize % smallLineSize), "vmPageSize must be an even multiple of line size");
static_assert(!(smallChunkSize % smallLineSize), "chunk size must be an even multiple of line size");
static_assert(
    sizeof(SmallChunk) % vmPageSize + 2 * smallMax <= vmPageSize,
    "the first page of object memory in a small chunk must be able to allocate smallMax");

inline SmallChunk::SmallChunk(std::lock_guard<StaticMutex>& lock)
{
    // Track the memory used for metadata by allocating imaginary objects.
    for (char* it = reinterpret_cast<char*>(this); it < m_memory; it += smallLineSize) {
        Object object(it);
        object.line()->ref(lock);
        object.page()->ref(lock);
    }
}

inline SmallChunk* SmallChunk::get(void* object)
{
    BASSERT(isSmall(object));
    return static_cast<SmallChunk*>(mask(object, smallChunkMask));
}

inline size_t SmallChunk::offset(void* object)
{
    BASSERT(object >= this);
    BASSERT(object < reinterpret_cast<char*>(this) + smallChunkSize);
    return static_cast<char*>(object) - reinterpret_cast<char*>(this);
}

inline void* SmallChunk::object(size_t offset)
{
    return reinterpret_cast<char*>(this) + offset;
}

inline SmallPage* SmallChunk::page(size_t offset)
{
    size_t pageNumber = offset / vmPageSize;
    return &m_pages[pageNumber];
}

inline SmallLine* SmallChunk::line(size_t offset)
{
    size_t lineNumber = offset / smallLineSize;
    return &m_lines[lineNumber];
}

inline char* SmallLine::begin()
{
    SmallChunk* chunk = SmallChunk::get(this);
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
    SmallChunk* chunk = SmallChunk::get(this);
    size_t pageNumber = this - chunk->pages();
    size_t lineNumber = pageNumber * smallLineCount;
    return &chunk->lines()[lineNumber];
}

inline SmallLine* SmallPage::end()
{
    return begin() + smallLineCount;
}

inline Object::Object(void* object)
    : m_chunk(SmallChunk::get(object))
    , m_offset(m_chunk->offset(object))
{
}

inline Object::Object(SmallChunk* chunk, void* object)
    : m_chunk(chunk)
    , m_offset(m_chunk->offset(object))
{
    BASSERT(chunk == SmallChunk::get(object));
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
