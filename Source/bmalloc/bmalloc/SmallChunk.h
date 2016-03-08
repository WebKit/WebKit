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

#include "Sizes.h"
#include "SmallLine.h"
#include "SmallPage.h"
#include "VMAllocate.h"

namespace bmalloc {

class SmallChunk {
public:
    SmallChunk(std::lock_guard<StaticMutex>&);

    static SmallChunk* get(void*);

    SmallPage* begin() { return SmallPage::get(SmallLine::get(m_memory)); }
    SmallPage* end() { return m_pages.end(); }
    
    SmallLine* lines() { return m_lines.begin(); }
    SmallPage* pages() { return m_pages.begin(); }
    
private:
    std::array<SmallLine, smallChunkSize / smallLineSize> m_lines;
    std::array<SmallPage, smallChunkSize / vmPageSize> m_pages;
    char m_memory[] __attribute__((aligned(smallLineSize+0)));
};

static_assert(!(vmPageSize % smallLineSize), "vmPageSize must be an even multiple of line size");
static_assert(!(smallChunkSize % smallLineSize), "chunk size must be an even multiple of line size");
static_assert(
    sizeof(SmallChunk) % vmPageSize + smallMax <= vmPageSize,
    "the first page of object memory in a small chunk must be able to allocate smallMax");

inline SmallChunk::SmallChunk(std::lock_guard<StaticMutex>& lock)
{
    // Track the memory used for metadata by allocating imaginary objects.
    for (SmallLine* line = m_lines.begin(); line < SmallLine::get(m_memory); ++line) {
        line->ref(lock, 1);

        SmallPage* page = SmallPage::get(line);
        page->ref(lock);
    }

    for (SmallPage* page = begin(); page != end(); ++page)
        page->setHasFreeLines(lock, true);
}

inline SmallChunk* SmallChunk::get(void* object)
{
    BASSERT(isSmall(object));
    return static_cast<SmallChunk*>(mask(object, smallChunkMask));
}

inline SmallLine* SmallLine::get(void* object)
{
    BASSERT(isSmall(object));
    SmallChunk* chunk = SmallChunk::get(object);
    size_t lineNumber = (reinterpret_cast<char*>(object) - reinterpret_cast<char*>(chunk)) / smallLineSize;
    return &chunk->lines()[lineNumber];
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

inline SmallPage* SmallPage::get(SmallLine* line)
{
    SmallChunk* chunk = SmallChunk::get(line);
    size_t lineNumber = line - chunk->lines();
    size_t pageNumber = lineNumber * smallLineSize / vmPageSize;
    return &chunk->pages()[pageNumber];
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

}; // namespace bmalloc

#endif // Chunk
