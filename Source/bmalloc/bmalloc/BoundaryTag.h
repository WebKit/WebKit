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

#ifndef BoundaryTag_h
#define BoundaryTag_h

#include "BAssert.h"
#include "Range.h"
#include "Sizes.h"
#include "VMState.h"
#include <cstring>

namespace bmalloc {

class BeginTag;
class EndTag;
class LargeChunk;
class Range;

class BoundaryTag {
public:
    static Range init(LargeChunk*);
    static unsigned compactBegin(void*);

    bool isFree() { return m_isFree; }
    void setFree(bool isFree) { m_isFree = isFree; }
    
    bool isEnd() { return m_isEnd; }
    void setEnd(bool isEnd) { m_isEnd = isEnd; }

    VMState vmState() { return VMState(m_vmState); }
    void setVMState(VMState vmState) { m_vmState = static_cast<unsigned>(vmState); }

    bool isMarked() { return m_isMarked; }
    void setMarked(bool isMarked) { m_isMarked = isMarked; }

    bool isNull() { return !m_size; }
    void clear() { std::memset(this, 0, sizeof(*this)); }
    
    size_t size() { return m_size; }
    unsigned compactBegin() { return m_compactBegin; }

    void setRange(const Range&);
    
    bool isSentinel() { return !m_compactBegin; }
    void initSentinel();
    
    EndTag* prev();
    BeginTag* next();

private:
    static const size_t flagBits = 5;
    static const size_t compactBeginBits = 4;
    static const size_t sizeBits = bitCount<unsigned>() - flagBits - compactBeginBits;

    static_assert(
        (1 << compactBeginBits) - 1 >= (largeMin - 1) / largeAlignment,
        "compactBegin must be encodable in a BoundaryTag.");

    static_assert(
        (1 << sizeBits) - 1 >= largeObjectMax,
        "largeObjectMax must be encodable in a BoundaryTag.");

    bool m_isFree: 1;
    bool m_isEnd: 1;
    unsigned m_vmState: 2;
    bool m_isMarked: 1;
    unsigned m_compactBegin: compactBeginBits;
    unsigned m_size: sizeBits;
};

inline unsigned BoundaryTag::compactBegin(void* object)
{
    return static_cast<unsigned>(
        reinterpret_cast<uintptr_t>(mask(object, largeMin - 1)) / largeAlignment);
}

inline void BoundaryTag::setRange(const Range& range)
{
    m_compactBegin = compactBegin(range.begin());
    BASSERT(this->compactBegin() == compactBegin(range.begin()));

    m_size = static_cast<unsigned>(range.size());
    BASSERT(this->size() == range.size());
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

inline void BoundaryTag::initSentinel()
{
    setRange(Range(nullptr, largeMin));
    setFree(false);
    setVMState(VMState::Virtual);
}

} // namespace bmalloc

#endif // BoundaryTag_h
