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

#ifndef BumpAllocator_h
#define BumpAllocator_h

#include "BAssert.h"
#include "ObjectType.h"

namespace bmalloc {

// Helper object for allocating small and medium objects.

class BumpAllocator {
public:
    BumpAllocator();
    template<typename Line> void init(size_t);
    
    bool isNull() { return !m_ptr; }
    template<typename Line> Line* line();

    bool canAllocate() { return !!m_remaining; }
    void* allocate();

    template<typename Line> void refill(Line*);
    void clear();

private:
    void validate(void*);

    char* m_ptr;
    unsigned short m_size;
    unsigned char m_remaining;
    unsigned char m_maxObjectCount;
};

inline BumpAllocator::BumpAllocator()
    : m_ptr()
    , m_size()
    , m_remaining()
    , m_maxObjectCount()
{
}

template<typename Line>
inline void BumpAllocator::init(size_t size)
{
    BASSERT(size >= Line::minimumObjectSize);

    m_ptr = nullptr;
    m_size = size;
    m_remaining = 0;
    m_maxObjectCount = Line::lineSize / size;
}

template<typename Line>
inline Line* BumpAllocator::line()
{
    return Line::get(canAllocate() ? m_ptr : m_ptr - 1);
}

inline void BumpAllocator::validate(void* ptr)
{
    UNUSED(ptr);
    if (m_size <= smallMax) {
        BASSERT(isSmall(ptr));
        return;
    }
    
    BASSERT(m_size <= mediumMax);
    BASSERT(isMedium(ptr));
}

inline void* BumpAllocator::allocate()
{
    BASSERT(m_remaining);

    --m_remaining;
    char* result = m_ptr;
    m_ptr += m_size;
    validate(result);
    return result;
}

template<typename Line>
inline void BumpAllocator::refill(Line* line)
{
    BASSERT(!canAllocate());
    line->concurrentRef(m_maxObjectCount);
    m_ptr = line->begin();
    m_remaining = m_maxObjectCount;
}

inline void BumpAllocator::clear()
{
    m_ptr = nullptr;
    m_remaining = 0;
}

} // namespace bmalloc

#endif // BumpAllocator_h
