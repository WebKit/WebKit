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

#ifndef BoundaryTag_h
#define BoundaryTag_h

#include "BAssert.h"
#include "Sizes.h"

namespace bmalloc {

class BeginTag;
class EndTag;
class LargeChunk;
class Range;

class BoundaryTag {
public:
    static Range init(LargeChunk*);
    static Range deallocate(void*);
    static void allocate(size_t, Range&, Range& leftover, bool& hasPhysicalPages);

    bool isXLarge() { return m_size == xLargeMarker; }
    void setXLarge() { m_size = xLargeMarker; }

    bool isFree() { return m_isFree; }
    void setFree(bool isFree) { m_isFree = isFree; }
    
    bool isEnd() { return m_isEnd; }
    void setEnd(bool isEnd) { m_isEnd = isEnd; }

    bool hasPhysicalPages() { return m_hasPhysicalPages; }
    void setHasPhysicalPages(bool hasPhysicalPages) { m_hasPhysicalPages = hasPhysicalPages; }

    bool isNull() { return !m_size; }
    void clear() { memset(this, 0, sizeof(*this)); }
    
    size_t size() { return m_size; }
    void setSize(size_t);
    
    EndTag* prev();
    BeginTag* next();

private:
    static const size_t flagBits = 3;
    static const size_t sizeBits = bitCount<unsigned>() - flagBits;
    static const size_t xLargeMarker = 1; // This size is unused because our minimum object size is greater than it.

    static_assert(largeMin > xLargeMarker, "largeMin must provide enough umbrella to fit xLargeMarker.");
    static_assert((1 << sizeBits) - 1 >= largeMax, "largeMax must be encodable in a BoundaryTag.");

    static void splitLarge(BeginTag*, size_t size, EndTag*& endTag, Range&, Range& leftover);
    static void mergeLargeLeft(EndTag*& prev, BeginTag*& beginTag, Range&, bool& hasPhysicalPages);
    static void mergeLargeRight(EndTag*&, BeginTag*& next, Range&, bool& hasPhysicalPages);
    static void mergeLarge(BeginTag*&, EndTag*&, Range&);

    bool m_isFree: 1;
    bool m_isEnd: 1;
    bool m_hasPhysicalPages: 1;
    unsigned m_size: sizeBits;
};

inline void BoundaryTag::setSize(size_t size)
{
    m_size = static_cast<unsigned>(size);
    ASSERT(this->size() == size);
    ASSERT(!isXLarge());
}

inline EndTag* BoundaryTag::prev()
{
    BoundaryTag* prev = this - 1;
    return reinterpret_cast<EndTag*>(prev);
}

inline BeginTag* BoundaryTag::next()
{
    BoundaryTag* next = this + 1;
    return reinterpret_cast<BeginTag*>(next);
}

} // namespace bmalloc

#endif // BoundaryTag_h
