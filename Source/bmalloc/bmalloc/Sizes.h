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

#ifndef Sizes_h
#define Sizes_h

#include "Algorithm.h"
#include "BPlatform.h"
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <chrono>

namespace bmalloc {

// Repository for malloc sizing constants and calculations.

namespace Sizes {
    static const size_t kB = 1024;
    static const size_t MB = kB * kB;

    static const size_t alignment = 8;
    static const size_t alignmentMask = alignment - 1ul;

#if BPLATFORM(IOS)
    static const size_t vmPageSize = 16 * kB;
#else
    static const size_t vmPageSize = 4 * kB;
#endif
    static const size_t vmPageMask = ~(vmPageSize - 1);
    
    static const size_t superChunkSize = 2 * MB;
    static const size_t superChunkMask = ~(superChunkSize - 1);

    static const size_t smallChunkSize = superChunkSize / 2;
    static const size_t smallChunkOffset = superChunkSize / 2;
    static const size_t smallChunkMask = ~(smallChunkSize - 1ul);

    static const size_t smallMax = 1024;
    static const size_t smallLineSize = 256;
    static const size_t smallLineCount = vmPageSize / smallLineSize;

    static const size_t largeChunkSize = superChunkSize / 2;
    static const size_t largeChunkOffset = 0;
    static const size_t largeChunkMask = ~(largeChunkSize - 1ul);

    static const size_t largeAlignment = 64;
    static const size_t largeMin = smallMax;
    static const size_t largeChunkMetadataSize = 4 * kB; // sizeof(LargeChunk)
    static const size_t largeMax = largeChunkSize - largeChunkMetadataSize;

    static const size_t xLargeAlignment = vmPageSize;
    static const size_t xLargeMax = std::numeric_limits<size_t>::max() - xLargeAlignment; // Make sure that rounding up to xLargeAlignment does not overflow.

    static const size_t freeListSearchDepth = 16;
    static const size_t freeListGrowFactor = 2;

    static const uintptr_t typeMask = (superChunkSize - 1) & ~((superChunkSize / 2) - 1); // 2 taggable chunks
    static const uintptr_t largeMask = typeMask & (superChunkSize + largeChunkOffset);
    static const uintptr_t smallMask = typeMask & (superChunkSize + smallChunkOffset);

    static const size_t deallocatorLogCapacity = 256;
    static const size_t bumpRangeCacheCapacity = 3;
    
    static const std::chrono::milliseconds scavengeSleepDuration = std::chrono::milliseconds(512);

    inline size_t sizeClass(size_t size)
    {
        static const size_t sizeClassMask = (smallMax / alignment) - 1;
        return mask((size - 1) / alignment, sizeClassMask);
    }

    inline size_t objectSize(size_t sizeClass)
    {
        return (sizeClass + 1) * alignment;
    }
};

using namespace Sizes;

} // namespace bmalloc

#endif // Sizes_h
