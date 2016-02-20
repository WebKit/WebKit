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
    static SmallChunk* get(void*);

    SmallPage* begin() { return SmallPage::get(SmallLine::get(m_memory)); }
    SmallPage* end() { return &m_pages[pageCount]; }
    
    SmallLine* lines() { return m_lines; }
    SmallPage* pages() { return m_pages; }

private:
    static_assert(!(vmPageSize % smallLineSize), "vmPageSize must be an even multiple of line size");
    static_assert(!(smallChunkSize % smallLineSize), "chunk size must be an even multiple of line size");

    static const size_t lineCount = smallChunkSize / smallLineSize;
    static const size_t pageCount = smallChunkSize / vmPageSize;

    SmallLine m_lines[lineCount];
    SmallPage m_pages[pageCount];

    // Align to vmPageSize to avoid sharing physical pages with metadata.
    // Otherwise, we'll confuse the scavenger into trying to scavenge metadata.
    // FIXME: Below #ifdef workaround fix should be removed after all linux based ports bump
    // own gcc version. See https://bugs.webkit.org/show_bug.cgi?id=140162#c87
#if BPLATFORM(IOS)
    char m_memory[] __attribute__((aligned(16384)));
    static_assert(vmPageSize == 16384, "vmPageSize and alignment must be same");
#else
    char m_memory[] __attribute__((aligned(4096)));
    static_assert(vmPageSize == 4096, "vmPageSize and alignment must be same");
#endif
};

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
