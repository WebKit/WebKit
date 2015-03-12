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

#include "Allocator.h"
#include "BAssert.h"
#include "Deallocator.h"
#include "Heap.h"
#include "LargeChunk.h"
#include "LargeObject.h"
#include "PerProcess.h"
#include "Sizes.h"
#include <algorithm>
#include <cstdlib>

using namespace std;

namespace bmalloc {

Allocator::Allocator(Heap* heap, Deallocator& deallocator)
    : m_isBmallocEnabled(heap->environment().isBmallocEnabled())
    , m_deallocator(deallocator)
{
    for (unsigned short size = alignment; size <= mediumMax; size += alignment)
        m_bumpAllocators[sizeClass(size)].init(size);
}

Allocator::~Allocator()
{
    scavenge();
}

void* Allocator::tryAllocate(size_t size)
{
    if (!m_isBmallocEnabled)
        return malloc(size);

    if (size <= largeMax)
        return allocate(size);

    std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
    return PerProcess<Heap>::get()->tryAllocateXLarge(lock, superChunkSize, roundUpToMultipleOf<xLargeAlignment>(size));
}

void* Allocator::allocate(size_t alignment, size_t size)
{
    BASSERT(isPowerOfTwo(alignment));

    if (!m_isBmallocEnabled) {
        void* result = nullptr;
        if (posix_memalign(&result, alignment, size))
            return nullptr;
        return result;
    }
    
    if (size <= smallMax && alignment <= smallLineSize) {
        size_t alignmentMask = alignment - 1;
        while (void* p = allocate(size)) {
            if (!test(p, alignmentMask))
                return p;
            m_deallocator.deallocate(p);
        }
    }

    if (size <= mediumMax && alignment <= mediumLineSize) {
        size = std::max(size, smallMax + Sizes::alignment);
        size_t alignmentMask = alignment - 1;
        while (void* p = allocate(size)) {
            if (!test(p, alignmentMask))
                return p;
            m_deallocator.deallocate(p);
        }
    }

    size = std::max(largeMin, roundUpToMultipleOf<largeAlignment>(size));
    alignment = roundUpToMultipleOf<largeAlignment>(alignment);
    size_t unalignedSize = largeMin + alignment + size;
    if (unalignedSize <= largeMax && alignment <= largeChunkSize / 2) {
        std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
        return PerProcess<Heap>::getFastCase()->allocateLarge(lock, alignment, size, unalignedSize);
    }

    size = roundUpToMultipleOf<xLargeAlignment>(size);
    alignment = std::max(superChunkSize, alignment);
    std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
    return PerProcess<Heap>::getFastCase()->allocateXLarge(lock, alignment, size);
}

void* Allocator::reallocate(void* object, size_t newSize)
{
    if (!m_isBmallocEnabled)
        return realloc(object, newSize);

    void* result = allocate(newSize);
    if (!object)
        return result;

    size_t oldSize = 0;
    switch (objectType(object)) {
    case Small: {
        SmallPage* page = SmallPage::get(SmallLine::get(object));
        oldSize = objectSize(page->sizeClass());
        break;
    }
    case Medium: {
        MediumPage* page = MediumPage::get(MediumLine::get(object));
        oldSize = objectSize(page->sizeClass());
        break;
    }
    case Large: {
        std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
        LargeObject largeObject(object);
        oldSize = largeObject.size();
        break;
    }
    case XLarge: {
        std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
        Range range = PerProcess<Heap>::getFastCase()->findXLarge(lock, object);
        RELEASE_BASSERT(range);
        oldSize = range.size();
        break;
    }
    }

    size_t copySize = std::min(oldSize, newSize);
    memcpy(result, object, copySize);
    m_deallocator.deallocate(object);
    return result;
}

void Allocator::scavenge()
{
    for (unsigned short i = alignment; i <= mediumMax; i += alignment) {
        BumpAllocator& allocator = m_bumpAllocators[sizeClass(i)];
        BumpRangeCache& bumpRangeCache = m_bumpRangeCaches[sizeClass(i)];

        while (allocator.canAllocate())
            m_deallocator.deallocate(allocator.allocate());

        while (bumpRangeCache.size()) {
            allocator.refill(bumpRangeCache.pop());
            while (allocator.canAllocate())
                m_deallocator.deallocate(allocator.allocate());
        }

        allocator.clear();
    }
}

NO_INLINE BumpRange Allocator::allocateBumpRangeSlowCase(size_t sizeClass)
{
    BumpRangeCache& bumpRangeCache = m_bumpRangeCaches[sizeClass];

    std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
    if (sizeClass <= bmalloc::sizeClass(smallMax))
        PerProcess<Heap>::getFastCase()->refillSmallBumpRangeCache(lock, sizeClass, bumpRangeCache);
    else
        PerProcess<Heap>::getFastCase()->refillMediumBumpRangeCache(lock, sizeClass, bumpRangeCache);

    return bumpRangeCache.pop();
}

INLINE BumpRange Allocator::allocateBumpRange(size_t sizeClass)
{
    BumpRangeCache& bumpRangeCache = m_bumpRangeCaches[sizeClass];
    if (!bumpRangeCache.size())
        return allocateBumpRangeSlowCase(sizeClass);
    return bumpRangeCache.pop();
}

NO_INLINE void* Allocator::allocateLarge(size_t size)
{
    size = roundUpToMultipleOf<largeAlignment>(size);
    std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
    return PerProcess<Heap>::getFastCase()->allocateLarge(lock, size);
}

NO_INLINE void* Allocator::allocateXLarge(size_t size)
{
    size = roundUpToMultipleOf<xLargeAlignment>(size);
    std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
    return PerProcess<Heap>::getFastCase()->allocateXLarge(lock, size);
}

void* Allocator::allocateSlowCase(size_t size)
{
    if (!m_isBmallocEnabled)
        return malloc(size);

    if (size <= mediumMax) {
        size_t sizeClass = bmalloc::sizeClass(size);
        BumpAllocator& allocator = m_bumpAllocators[sizeClass];
        allocator.refill(allocateBumpRange(sizeClass));
        return allocator.allocate();
    }

    if (size <= largeMax)
        return allocateLarge(size);

    return allocateXLarge(size);
}

} // namespace bmalloc
