/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#include "LargeChunk.h"
#include "FreeList.h"
#include "Vector.h"

namespace bmalloc {

LargeObject FreeList::takeGreedy(Owner owner)
{
    for (size_t i = m_vector.size(); i-- > 0; ) {
        // We don't eagerly remove items when we merge and/or split ranges,
        // so we need to validate each free list entry before using it.
        LargeObject largeObject(LargeObject::DoNotValidate, m_vector[i].begin());
        if (!largeObject.isValidAndFree(owner, m_vector[i].size())) {
            m_vector.pop(i);
            continue;
        }

        m_vector.pop(i);
        return largeObject;
    }

    return LargeObject();
}

LargeObject FreeList::take(Owner owner, size_t size)
{
    LargeObject first;
    size_t end = m_vector.size() > freeListSearchDepth ? m_vector.size() - freeListSearchDepth : 0;
    for (size_t i = m_vector.size(); i-- > end; ) {
        // We don't eagerly remove items when we merge and/or split ranges, so
        // we need to validate each free list entry before using it.
        LargeObject largeObject(LargeObject::DoNotValidate, m_vector[i].begin());
        if (!largeObject.isValidAndFree(owner, m_vector[i].size())) {
            m_vector.pop(i);
            continue;
        }

        if (largeObject.size() < size)
            continue;

        if (!!first && first.begin() < largeObject.begin())
            continue;

        first = largeObject;
    }
    
    return first;
}

LargeObject FreeList::take(Owner owner, size_t alignment, size_t size, size_t unalignedSize)
{
    BASSERT(isPowerOfTwo(alignment));
    size_t alignmentMask = alignment - 1;

    LargeObject first;
    size_t end = m_vector.size() > freeListSearchDepth ? m_vector.size() - freeListSearchDepth : 0;
    for (size_t i = m_vector.size(); i-- > end; ) {
        // We don't eagerly remove items when we merge and/or split ranges, so
        // we need to validate each free list entry before using it.
        LargeObject largeObject(LargeObject::DoNotValidate, m_vector[i].begin());
        if (!largeObject.isValidAndFree(owner, m_vector[i].size())) {
            m_vector.pop(i);
            continue;
        }

        if (largeObject.size() < size)
            continue;

        if (test(largeObject.begin(), alignmentMask) && largeObject.size() < unalignedSize)
            continue;

        if (!!first && first.begin() < largeObject.begin())
            continue;

        first = largeObject;
    }
    
    return first;
}

void FreeList::removeInvalidAndDuplicateEntries(Owner owner)
{
    for (size_t i = m_vector.size(); i-- > 0; ) {
        LargeObject largeObject(LargeObject::DoNotValidate, m_vector[i].begin());
        if (!largeObject.isValidAndFree(owner, m_vector[i].size())) {
            m_vector.pop(i);
            continue;
        }
        
        largeObject.setMarked(false);
    }

    for (size_t i = m_vector.size(); i-- > 0; ) {
        LargeObject largeObject(LargeObject::DoNotValidate, m_vector[i].begin());
        if (largeObject.isMarked()) {
            m_vector.pop(i);
            continue;
        }

        largeObject.setMarked(true);
    }
}


} // namespace bmalloc
