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

#include "Allocator.h"
#include "BAssert.h"
#include "Deallocator.h"
#include "Heap.h"
#include "PerProcess.h"
#include "Sizes.h"
#include <algorithm>

using namespace std;

namespace bmalloc {

Allocator::Allocator(Deallocator& deallocator)
    : m_deallocator(deallocator)
{
    for (unsigned short i = alignment; i <= smallMax; i += alignment)
        m_smallAllocators[smallSizeClassFor(i)].init<SmallLine>(i);

    for (unsigned short i = smallMax + alignment; i <= mediumMax; i += alignment)
        m_mediumAllocators[mediumSizeClassFor(i)].init<MediumLine>(i);
}

Allocator::~Allocator()
{
    scavenge();
}

void Allocator::scavenge()
{
    for (auto& allocator : m_smallAllocators) {
        while (allocator.canAllocate())
            m_deallocator.deallocate(allocator.allocate());
        allocator.clear();
    }

    for (auto& allocator : m_mediumAllocators) {
        while (allocator.canAllocate())
            m_deallocator.deallocate(allocator.allocate());
        allocator.clear();
    }

    std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
    Heap* heap = PerProcess<Heap>::getFastCase();
    
    for (auto& smallLineCache : m_smallLineCaches) {
        while (smallLineCache.size())
            heap->deallocateSmallLine(lock, smallLineCache.pop());
    }
    while (m_mediumLineCache.size())
        heap->deallocateMediumLine(lock, m_mediumLineCache.pop());
}

SmallLine* Allocator::allocateSmallLine(size_t smallSizeClass)
{
    SmallLineCache& smallLineCache = m_smallLineCaches[smallSizeClass];
    if (!smallLineCache.size()) {
        std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
        Heap* heap = PerProcess<Heap>::getFastCase();

        while (smallLineCache.size() != smallLineCache.capacity())
            smallLineCache.push(heap->allocateSmallLine(lock, smallSizeClass));
    }

    return smallLineCache.pop();
}

MediumLine* Allocator::allocateMediumLine()
{
    if (!m_mediumLineCache.size()) {
        std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
        Heap* heap = PerProcess<Heap>::getFastCase();

        while (m_mediumLineCache.size() != m_mediumLineCache.capacity())
            m_mediumLineCache.push(heap->allocateMediumLine(lock));
    }

    return m_mediumLineCache.pop();
}

void* Allocator::allocateLarge(size_t size)
{
    size = roundUpToMultipleOf<largeAlignment>(size);
    std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
    return PerProcess<Heap>::getFastCase()->allocateLarge(lock, size);
}

void* Allocator::allocateXLarge(size_t size)
{
    size = roundUpToMultipleOf<largeAlignment>(size);
    std::lock_guard<StaticMutex> lock(PerProcess<Heap>::mutex());
    return PerProcess<Heap>::getFastCase()->allocateXLarge(lock, size);
}

void* Allocator::allocateMedium(size_t size)
{
    BumpAllocator& allocator = m_mediumAllocators[mediumSizeClassFor(size)];

    if (!allocator.canAllocate())
        allocator.refill(allocateMediumLine());
    return allocator.allocate();
}

void* Allocator::allocateSlowCase(size_t size)
{
IF_DEBUG(
    void* dummy;
    BASSERT(!allocateFastCase(size, dummy));
)
    if (size <= smallMax) {
        size_t smallSizeClass = smallSizeClassFor(size);
        BumpAllocator& allocator = m_smallAllocators[smallSizeClass];
        allocator.refill(allocateSmallLine(smallSizeClass));
        return allocator.allocate();
    }

    if (size <= mediumMax)
        return allocateMedium(size);
    
    if (size <= largeMax)
        return allocateLarge(size);

    return allocateXLarge(size);
}

} // namespace bmalloc
