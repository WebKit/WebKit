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

#include "ObjectType.h"
#include "Sizes.h"
#include "VMAllocate.h"

namespace bmalloc {

template<class Traits>
class Chunk {
public:
    typedef typename Traits::PageType Page;
    typedef typename Traits::LineType Line;

    static const size_t lineSize = Traits::lineSize;
    static const size_t chunkSize = Traits::chunkSize;
    static const size_t chunkOffset = Traits::chunkOffset;
    static const uintptr_t chunkMask = Traits::chunkMask;

    static Chunk* get(void*);

    Page* begin() { return Page::get(Line::get(m_memory)); }
    Page* end() { return &m_pages[pageCount]; }
    
    Line* lines() { return m_lines; }
    Page* pages() { return m_pages; }

private:
    static_assert(!(vmPageSize % lineSize), "vmPageSize must be an even multiple of line size");
    static_assert(!(chunkSize % lineSize), "chunk size must be an even multiple of line size");

    static const size_t lineCount = chunkSize / lineSize;
    static const size_t pageCount = chunkSize / vmPageSize;

    Line m_lines[lineCount];
    Page m_pages[pageCount];

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

template<class Traits>
inline auto Chunk<Traits>::get(void* object) -> Chunk*
{
    BASSERT(isSmall(object));
    return static_cast<Chunk*>(mask(object, chunkMask));
}

}; // namespace bmalloc

#endif // Chunk
