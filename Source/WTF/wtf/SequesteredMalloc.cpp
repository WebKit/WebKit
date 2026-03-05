/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/SequesteredMalloc.h>

#include <wtf/SequesteredAllocator.h>

#if USE(PROTECTED_JIT)

namespace WTF {

#if ASSERT_ENABLED
namespace {
// We do not use std::numeric_limits<size_t>::max() here due to the edge case in VC++.
// https://bugs.webkit.org/show_bug.cgi?id=173720
static size_t maxSingleSequesteredArenaAllocationSize = SIZE_MAX;
};

void sequesteredArenaSetMaxSingleAllocationSize(size_t size)
{
    maxSingleSequesteredArenaAllocationSize = size;
}

#if ASSERT_ENABLED
#define ASSERT_IS_WITHIN_LIMIT(size) do { \
        size_t size__ = (size); \
        ASSERT_WITH_MESSAGE((size__) <= maxSingleSequesteredArenaAllocationSize, "Requested size (%zu) exceeds max single allocation size set for testing (%zu)", (size__), maxSingleSequesteredArenaAllocationSize); \
    } while (false)
#else
#define ASSERT_IS_WITHIN_LIMIT(size)
#endif // ASSERT_ENABLED

#define FAIL_IF_EXCEEDS_LIMIT(size) do { \
        if ((size) > maxSingleSequesteredArenaAllocationSize) [[unlikely]] \
            return nullptr; \
    } while (false)
#else // !ASSERT_ENABLED

#define ASSERT_IS_WITHIN_LIMIT(size)
#define FAIL_IF_EXCEEDS_LIMIT(size)

#endif // !ASSERT_ENABLED

bool isSequesteredArenaMallocEnabled()
{
#if USE(PROTECTED_JIT)
    return true;
#else
    return false;
#endif
}

void* sequesteredArenaMalloc(size_t size)
{
    ASSERT_IS_WITHIN_LIMIT(size);
    return SequesteredArenaAllocator::getCurrentAllocator()->malloc(size);
}

void* sequesteredArenaZeroedMalloc(size_t size)
{
    ASSERT_IS_WITHIN_LIMIT(size);
    return SequesteredArenaAllocator::getCurrentAllocator()->zeroedMalloc(size);
}
void* sequesteredArenaCalloc(size_t numElements, size_t elementSize)
{
    ASSERT_IS_WITHIN_LIMIT(numElements * elementSize);
    Checked<size_t> checkedSize = elementSize;
    checkedSize *= numElements;
    void* result = sequesteredArenaZeroedMalloc(checkedSize);
    if (!result)
        CRASH();
    return result;
}
void* sequesteredArenaRealloc(void* object, size_t newSize)
{
    ASSERT_IS_WITHIN_LIMIT(newSize);
    return SequesteredArenaAllocator::getCurrentAllocator()->realloc(object, newSize);
}

void sequesteredArenaFree(void* object)
{
    SequesteredArenaAllocator::getCurrentAllocator()->free(object);
}

TryMallocReturnValue trySequesteredArenaMalloc(size_t size)
{
    FAIL_IF_EXCEEDS_LIMIT(size);
    return SequesteredArenaAllocator::getCurrentAllocator()->tryMalloc(size);
}
TryMallocReturnValue trySequesteredArenaZeroedMalloc(size_t size)
{
    FAIL_IF_EXCEEDS_LIMIT(size);
    return SequesteredArenaAllocator::getCurrentAllocator()->tryZeroedMalloc(size);
}
TryMallocReturnValue trySequesteredArenaCalloc(size_t numElements, size_t elementSize)
{
    FAIL_IF_EXCEEDS_LIMIT(numElements * elementSize);
    CheckedSize checkedSize = elementSize;
    checkedSize *= numElements;
    if (checkedSize.hasOverflowed())
        return nullptr;
    return trySequesteredArenaZeroedMalloc(checkedSize);
}
TryMallocReturnValue trySequesteredArenaRealloc(void* object, size_t newSize)
{
    FAIL_IF_EXCEEDS_LIMIT(newSize);
    return SequesteredArenaAllocator::getCurrentAllocator()->tryRealloc(object, newSize);
}

void* sequesteredArenaAlignedMalloc(size_t alignment, size_t size)
{
    ASSERT_IS_WITHIN_LIMIT(size);
    return SequesteredArenaAllocator::getCurrentAllocator()->alignedMalloc(alignment, size);
}
void* trySequesteredArenaAlignedMalloc(size_t alignment, size_t size)
{
    FAIL_IF_EXCEEDS_LIMIT(size);
    return SequesteredArenaAllocator::getCurrentAllocator()->tryAlignedMalloc(alignment, size);
}
void sequesteredArenaAlignedFree(void* p)
{
    return SequesteredArenaAllocator::getCurrentAllocator()->free(p);
}

SequesteredArenaMallocStatistics sequesteredArenaMallocStatistics()
{
    return { };
}

void sequesteredArenaMallocDumpMallocStats()
{
}

void* sequesteredImmortalMalloc(size_t size)
{
    return SequesteredImmortalHeap::instance().immortalMalloc(size);
}

void* sequesteredImmortalAlignedMalloc(size_t alignment, size_t size)
{
    return SequesteredImmortalHeap::instance().immortalAlignedMalloc(alignment, size);
}

}

#endif // USE(PROTECTED_JIT)
