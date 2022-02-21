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

#pragma once

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
static constexpr size_t kB = 1024;
static constexpr size_t MB = kB * kB;
static constexpr size_t GB = kB * kB * kB;

static constexpr size_t alignment = 8;
static constexpr size_t alignmentMask = alignment - 1ul;

static constexpr size_t chunkSize = 1 * MB;
static constexpr size_t chunkMask = ~(chunkSize - 1ul);

static constexpr size_t smallLineSize = 256;
static constexpr size_t smallPageSize = 4 * kB;
static constexpr size_t smallPageLineCount = smallPageSize / smallLineSize;

static constexpr size_t maskSizeClassMax = 512;
static constexpr size_t smallMax = 32 * kB;

static constexpr size_t pageSizeMax = smallMax * 2;
static constexpr size_t pageClassCount = pageSizeMax / smallPageSize;

static constexpr size_t pageSizeWasteFactor = 8;
static constexpr size_t logWasteFactor = 8;

static constexpr size_t largeAlignment = smallMax / pageSizeWasteFactor;
static constexpr size_t largeAlignmentMask = largeAlignment - 1;

static constexpr size_t deallocatorLogCapacity = 512;
static constexpr size_t bumpRangeCacheCapacity = 3;

static constexpr size_t scavengerBytesPerMemoryPressureCheck = 16 * MB;
static constexpr double memoryPressureThreshold = 0.75;

static constexpr size_t maskSizeClassCount = maskSizeClassMax / alignment;

constexpr size_t maskSizeClass(size_t size)
{
    // We mask to accommodate zero.
    return mask((size - 1) / alignment, maskSizeClassCount - 1);
}

constexpr size_t maskObjectSize(size_t maskSizeClass)
{
    return (maskSizeClass + 1) * alignment;
}

static constexpr size_t logAlignmentMin = maskSizeClassMax / logWasteFactor;

static constexpr size_t logSizeClassCount = (log2(smallMax) - log2(maskSizeClassMax)) * logWasteFactor;

constexpr size_t logSizeClass(size_t size)
{
    size_t base = log2(size - 1) - log2(maskSizeClassMax);
    size_t offset = (size - 1 - (maskSizeClassMax << base));
    return base * logWasteFactor + offset / (logAlignmentMin << base);
}

constexpr size_t logObjectSize(size_t logSizeClass)
{
    size_t base = logSizeClass / logWasteFactor;
    size_t offset = logSizeClass % logWasteFactor;
    return (maskSizeClassMax << base) + (offset + 1) * (logAlignmentMin << base);
}

static constexpr size_t sizeClassCount = maskSizeClassCount + logSizeClassCount;

constexpr size_t sizeClass(size_t size)
{
    if (size <= maskSizeClassMax)
        return maskSizeClass(size);
    return maskSizeClassCount + logSizeClass(size);
}

constexpr size_t objectSize(size_t sizeClass)
{
    if (sizeClass < maskSizeClassCount)
        return maskObjectSize(sizeClass);
    return logObjectSize(sizeClass - maskSizeClassCount);
}

constexpr size_t pageSize(size_t pageClass)
{
    return (pageClass + 1) * smallPageSize;
}

constexpr size_t smallLineCount(size_t vmPageSize)
{
    return vmPageSize / smallLineSize;
}

} // namespace Sizes

using namespace Sizes;

} // namespace bmalloc
