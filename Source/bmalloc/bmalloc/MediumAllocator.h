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

#ifndef MediumAllocator_h
#define MediumAllocator_h

#include "BAssert.h"
#include "MediumChunk.h"
#include "MediumLine.h"

namespace bmalloc {

// Helper object for allocating medium objects.

class MediumAllocator {
public:
    MediumAllocator();

    bool isNull() { return !m_end; }
    MediumLine* line();

    void* allocate(size_t);
    bool allocate(size_t, void*&);

    unsigned char derefCount();
    void refill(MediumLine*);

private:
    char* m_end;
    size_t m_remaining;
    size_t m_objectCount;
};

inline MediumAllocator::MediumAllocator()
    : m_end()
    , m_remaining()
    , m_objectCount()
{
}

inline MediumLine* MediumAllocator::line()
{
    return MediumLine::get(m_end - 1);
}

inline void* MediumAllocator::allocate(size_t size)
{
    ASSERT(size <= m_remaining);
    ASSERT(size == roundUpToMultipleOf<alignment>(size));
    ASSERT(size >= MediumLine::minimumObjectSize);

    m_remaining -= size;
    void* object = m_end - m_remaining - size;
    ASSERT(isSmallOrMedium(object) && !isSmall(object));

    ++m_objectCount;
    return object;
}

inline bool MediumAllocator::allocate(size_t size, void*& object)
{
    if (size > m_remaining)
        return false;

    object = allocate(size);
    return true;
}

inline unsigned char MediumAllocator::derefCount()
{
    return MediumLine::maxRefCount - m_objectCount;
}

inline void MediumAllocator::refill(MediumLine* line)
{
    line->concurrentRef(MediumLine::maxRefCount);
    m_end = line->end();
    m_remaining = mediumLineSize;
    m_objectCount = 0;
}

} // namespace bmalloc

#endif // MediumAllocator_h
