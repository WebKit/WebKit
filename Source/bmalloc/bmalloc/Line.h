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

#ifndef Line_h
#define Line_h

#include "BAssert.h"
#include "Mutex.h"
#include "ObjectType.h"
#include <mutex>

namespace bmalloc {

template<class Traits>
class Line {
public:
    typedef typename Traits::Chunk Chunk;
    static const size_t minimumObjectSize = Traits::minimumObjectSize;
    static const size_t lineSize = Traits::lineSize;
    
    static const unsigned char maxRefCount = std::numeric_limits<unsigned char>::max();
    static_assert(lineSize / minimumObjectSize < maxRefCount, "maximum object count must fit in Line");

    static Line* get(void*);

    void concurrentRef(unsigned char = 1);
    bool deref(unsigned char = 1);
    
    char* begin();
    char* end();

private:
    unsigned char m_refCount;
};

template<class Traits>
inline auto Line<Traits>::get(void* object) -> Line*
{
    ASSERT(isSmallOrMedium(object));
    Chunk* chunk = Chunk::get(object);
    size_t lineNumber = (reinterpret_cast<char*>(object) - reinterpret_cast<char*>(chunk)) / lineSize;
    return &chunk->lines()[lineNumber];
}

template<class Traits>
inline char* Line<Traits>::begin()
{
    Chunk* chunk = Chunk::get(this);
    size_t lineNumber = this - chunk->lines();
    size_t offset = lineNumber * lineSize;
    return &reinterpret_cast<char*>(chunk)[offset];
}

template<class Traits>
inline char* Line<Traits>::end()
{
    return begin() + lineSize;
}

template<class Traits>
inline void Line<Traits>::concurrentRef(unsigned char count)
{
    ASSERT(!m_refCount); // Up-ref from zero can be lock-free because there are no other clients.
    ASSERT(count <= maxRefCount);
    m_refCount = count;
}

template<class Traits>
inline bool Line<Traits>::deref(unsigned char count)
{
    ASSERT(count <= m_refCount);
    m_refCount -= count;
    return !m_refCount;
}

} // namespace bmalloc

#endif // Line_h
