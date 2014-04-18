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

#include "Cache.h"
#include "Heap.h"
#include "LargeChunk.h"
#include "PerProcess.h"
#include "XLargeChunk.h"
#include "Sizes.h"

namespace bmalloc {
namespace api {

inline void* malloc(size_t size)
{
    return Cache::allocate(size);
}

inline void free(void* object)
{
    return Cache::deallocate(object);
}

inline void* realloc(void* object, size_t newSize)
{
    void* result = Cache::allocate(newSize);
    if (!object)
        return result;

    size_t oldSize = 0;
    switch (objectType(object)) {
    case XSmall: {
        // We don't have an exact size, but we can calculate a maximum.
        void* end = roundUpToMultipleOf<xSmallLineSize>(static_cast<char*>(object) + 1);
        oldSize = static_cast<char*>(end) - static_cast<char*>(object);
        break;
    }
    case Small: {
        // We don't have an exact size, but we can calculate a maximum.
        void* end = roundUpToMultipleOf<smallLineSize>(static_cast<char*>(object) + 1);
        oldSize = static_cast<char*>(end) - static_cast<char*>(object);
        break;
    }
    case Medium: {
        // We don't have an exact size, but we can calculate a maximum.
        void* end = roundUpToMultipleOf<mediumLineSize>(static_cast<char*>(object) + 1);
        oldSize = static_cast<char*>(end) - static_cast<char*>(object);
        break;
    }
    case Large: {
        BeginTag* beginTag = LargeChunk::beginTag(object);
        oldSize = beginTag->size();
        break;
    }
    case XLarge: {
        XLargeChunk* chunk = XLargeChunk::get(object);
        oldSize = chunk->size();
        break;
    }
    }

    size_t copySize = std::min(oldSize, newSize);
    memcpy(result, object, copySize);
    Cache::deallocate(object);
    return result;
}
    
inline void scavenge()
{
    PerThread<Cache>::get()->scavenge();
    
    std::unique_lock<Mutex> lock(PerProcess<Heap>::mutex());
    PerProcess<Heap>::get()->scavenge(lock, std::chrono::milliseconds(0));
}

} // namespace api
} // namespace bmalloc
