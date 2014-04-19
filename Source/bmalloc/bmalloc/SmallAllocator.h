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

#ifndef SmallAllocator_h
#define SmallAllocator_h

#include "BAssert.h"
#include "SmallChunk.h"
#include "SmallLine.h"

namespace bmalloc {

// Helper object for allocating small objects.

class SmallAllocator {
public:
    SmallAllocator();
    SmallAllocator(size_t);
    
    bool isNull() { return !m_ptr; }
    SmallLine* line();

    bool canAllocate() { return !!m_remaining; }
    void* allocate();

    unsigned short objectCount();
    unsigned char derefCount();
    void refill(SmallLine*);

private:
    char* m_ptr;
    unsigned short m_size;
    unsigned char m_remaining;
    unsigned char m_maxObjectCount;
};

inline SmallAllocator::SmallAllocator()
    : m_ptr()
    , m_size()
    , m_remaining()
    , m_maxObjectCount()
{
}

inline SmallAllocator::SmallAllocator(size_t size)
    : m_ptr()
    , m_size(size)
    , m_remaining()
    , m_maxObjectCount(smallLineSize / size)
{
}

inline SmallLine* SmallAllocator::line()
{
    return SmallLine::get(canAllocate() ? m_ptr : m_ptr - 1);
}

inline void* SmallAllocator::allocate()
{
    BASSERT(m_remaining);
    BASSERT(m_size >= SmallLine::minimumObjectSize);

    --m_remaining;
    char* result = m_ptr;
    m_ptr += m_size;
    BASSERT(isSmall(result));
    return result;
}

inline unsigned short SmallAllocator::objectCount()
{
    return m_maxObjectCount - m_remaining;
}

inline unsigned char SmallAllocator::derefCount()
{
    return SmallLine::maxRefCount - objectCount();
}

inline void SmallAllocator::refill(SmallLine* line)
{
    BASSERT(!canAllocate());
    line->concurrentRef(SmallLine::maxRefCount);
    m_ptr = line->begin();
    m_remaining = m_maxObjectCount;
}

} // namespace bmalloc

#endif // SmallAllocator_h
