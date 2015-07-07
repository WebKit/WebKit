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

#ifndef XLargeChunk_h
#define XLargeChunk_h

#include "Sizes.h"

namespace bmalloc {

class XLargeChunk {
public:
    static XLargeChunk* create(size_t);
    static void destroy(XLargeChunk*);
    
    static XLargeChunk* get(void* object) { return reinterpret_cast<XLargeChunk*>(LargeChunk::get(object)); }

    char* begin() { return m_largeChunk.begin(); }
    size_t& size();

private:
    XLargeChunk(const Range&, size_t);
    Range& range();
    
    LargeChunk m_largeChunk;
};

inline XLargeChunk::XLargeChunk(const Range& range, size_t size)
{
    this->range() = range;
    this->size() = size;
}

inline XLargeChunk* XLargeChunk::create(size_t size)
{
    size_t vmSize = bmalloc::vmSize(sizeof(XLargeChunk) + size);
    std::pair<void*, Range> result = vmAllocate(vmSize, superChunkSize, largeChunkOffset);
    return new (result.first) XLargeChunk(result.second, size);
}

inline void XLargeChunk::destroy(XLargeChunk* chunk)
{
    const Range& range = chunk->range();
    vmDeallocate(range.begin(), range.size());
}

inline Range& XLargeChunk::range()
{
    // Since we hold only one object, we only use our first BoundaryTag. So, we
    // can stuff our range into the remaining metadata.
    Range& result = *reinterpret_cast<Range*>(roundUpToMultipleOf<alignment>(LargeChunk::beginTag(begin()) + 1));
    BASSERT(static_cast<void*>(&result) < static_cast<void*>(begin()));
    return result;
}

inline size_t& XLargeChunk::size()
{
    // Since we hold only one object, we only use our first BoundaryTag. So, we
    // can stuff our size into the remaining metadata.
    size_t& result = *reinterpret_cast<size_t*>(roundUpToMultipleOf<alignment>(&range() + 1));
    BASSERT(static_cast<void*>(&result) < static_cast<void*>(begin()));
    return result;
}

}; // namespace bmalloc

#endif // XLargeChunk
