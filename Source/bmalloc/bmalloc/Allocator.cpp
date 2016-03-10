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
    for (unsigned short size = alignment; size <= smallMax; size += alignment)
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

    if (size <= xLargeMax) {
        std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
        return PerProcess<Heap>::getFastCase()->tryAllocateXLarge(lock, alignment, size);
    }

    return nullptr;
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

    if (size <= largeMax && alignment <= largeMax) {
        size = std::max(largeMin, roundUpToMultipleOf<largeAlignment>(size));
        alignment = roundUpToMultipleOf<largeAlignment>(alignment);
        size_t unalignedSize = largeMin + alignment - largeAlignment + size;
        if (unalignedSize <= largeMax && alignment <= largeChunkSize / 2) {
            std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
            return PerProcess<Heap>::getFastCase()->allocateLarge(lock, alignment, size, unalignedSize);
        }
    }

    if (size <= xLargeMax && alignment <= xLargeMax) {
        std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
        return PerProcess<Heap>::getFastCase()->allocateXLarge(lock, alignment, size);
    }

    BCRASH();
    return nullptr;
}

void* Allocator::reallocate(void* object, size_t newSize)
{
    if (!m_isBmallocEnabled)
        return realloc(object, newSize);

    size_t oldSize = 0;
    switch (objectType(object)) {
    case Small: {
        SmallRun* run = SmallRun::get(SmallLine::get(object));
        oldSize = objectSize(run->sizeClass());
        break;
    }
    case Large: {
        std::unique_lock<StaticMutex> lock(PerProcess<Heap>::mutex());
        LargeObject largeObject(object);
        oldSize = largeObject.size();

        if (newSize < oldSize && newSize > smallMax) {
            newSize = roundUpToMultipleOf<largeAlignment>(newSize);
            if (oldSize - newSize >= largeMin) {
                std::pair<LargeObject, LargeObject> split = largeObject.split(newSize);
                
                lock.unlock();
                m_deallocator.deallocate(split.second.begin());
                lock.lock();
            }
            return object;
        }
        break;
    }
    case XLarge: {
        BASSERT(objectType(nullptr) == XLarge);
        if (!object)
            break;

        std::unique_lock<StaticMutex> lock(PerProcess<Heap>::mutex());
        oldSize = PerProcess<Heap>::getFastCase()->xLargeSize(lock, object);

        if (newSize == oldSize)
            return object;

        if (newSize < oldSize && newSize > largeMax) {
            PerProcess<Heap>::getFastCase()->shrinkXLarge(lock, Range(object, oldSize), newSize);
            return object;
        }
        break;
    }
    }

    void* result = allocate(newSize);
    size_t copySize = std::min(oldSize, newSize);
    memcpy(result, object, copySize);
    m_deallocator.deallocate(object);
    return result;
}

void Allocator::scavenge()
{
    for (unsigned short i = alignment; i <= smallMax; i += alignment) {
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

NO_INLINE void Allocator::refillAllocatorSlowCase(BumpAllocator& allocator, size_t sizeClass)
{
    BumpRangeCache& bumpRangeCache = m_bumpRangeCaches[sizeClass];

    std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
    PerProcess<Heap>::getFastCase()->allocateSmallBumpRanges(lock, sizeClass, allocator, bumpRangeCache);
}

INLINE void Allocator::refillAllocator(BumpAllocator& allocator, size_t sizeClass)
{
    BumpRangeCache& bumpRangeCache = m_bumpRangeCaches[sizeClass];
    if (!bumpRangeCache.size())
        return refillAllocatorSlowCase(allocator, sizeClass);
    return allocator.refill(bumpRangeCache.pop());
}

NO_INLINE void* Allocator::allocateLarge(size_t size)
{
    size = roundUpToMultipleOf<largeAlignment>(size);
    std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
    return PerProcess<Heap>::getFastCase()->allocateLarge(lock, size);
}

NO_INLINE void* Allocator::allocateXLarge(size_t size)
{
    std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
    return PerProcess<Heap>::getFastCase()->allocateXLarge(lock, size);
}

void* Allocator::allocateSlowCase(size_t size)
{
    if (!m_isBmallocEnabled)
        return malloc(size);

    if (size <= smallMax) {
        size_t sizeClass = bmalloc::sizeClass(size);
        BumpAllocator& allocator = m_bumpAllocators[sizeClass];
        refillAllocator(allocator, sizeClass);
        return allocator.allocate();
    }

    if (size <= largeMax)
        return allocateLarge(size);

    if (size <= xLargeMax)
        return allocateXLarge(size);

    BCRASH();
    return nullptr;
}

} // namespace bmalloc
