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
    , m_smallAllocators()
    , m_mediumAllocator()
    , m_smallAllocatorLog()
    , m_mediumAllocatorLog()
{
    unsigned short size = alignment;
    for (auto& allocator : m_smallAllocators) {
        allocator = SmallAllocator(size);
        size += alignment;
    }
}

Allocator::~Allocator()
{
    scavenge();
}
    
void Allocator::scavenge()
{
    for (auto& allocator : m_smallAllocators)
        log(allocator);
    processSmallAllocatorLog();

    log(m_mediumAllocator);
    processMediumAllocatorLog();
}

void Allocator::log(SmallAllocator& allocator)
{
    if (m_smallAllocatorLog.size() == m_smallAllocatorLog.capacity())
        processSmallAllocatorLog();
    
    if (allocator.isNull())
        return;

    m_smallAllocatorLog.push(std::make_pair(allocator.line(), allocator.derefCount()));
}

void Allocator::processSmallAllocatorLog()
{
    std::lock_guard<Mutex> lock(PerProcess<Heap>::mutex());

    for (auto& logEntry : m_smallAllocatorLog) {
        if (!logEntry.first->deref(lock, logEntry.second))
            continue;
        m_deallocator.deallocateSmallLine(lock, logEntry.first);
    }
    m_smallAllocatorLog.clear();
}

void Allocator::log(MediumAllocator& allocator)
{
    if (m_mediumAllocatorLog.size() == m_mediumAllocatorLog.capacity())
        processMediumAllocatorLog();

    if (allocator.isNull())
        return;

    m_mediumAllocatorLog.push(std::make_pair(allocator.line(), allocator.derefCount()));
}

void Allocator::processMediumAllocatorLog()
{
    std::lock_guard<Mutex> lock(PerProcess<Heap>::mutex());

    for (auto& logEntry : m_mediumAllocatorLog) {
        if (!logEntry.first->deref(lock, logEntry.second))
            continue;
        m_deallocator.deallocateMediumLine(lock, logEntry.first);
    }
    m_mediumAllocatorLog.clear();
}

void* Allocator::allocateLarge(size_t size)
{
    size = roundUpToMultipleOf<largeAlignment>(size);
    std::lock_guard<Mutex> lock(PerProcess<Heap>::mutex());
    return PerProcess<Heap>::getFastCase()->allocateLarge(lock, size);
}

void* Allocator::allocateXLarge(size_t size)
{
    size = roundUpToMultipleOf<largeAlignment>(size);
    std::lock_guard<Mutex> lock(PerProcess<Heap>::mutex());
    return PerProcess<Heap>::getFastCase()->allocateXLarge(lock, size);
}

void* Allocator::allocateMedium(size_t size)
{
    MediumAllocator& allocator = m_mediumAllocator;
    size = roundUpToMultipleOf<alignment>(size);

    void* object;
    if (allocator.allocate(size, object))
        return object;

    log(allocator);
    allocator.refill(m_deallocator.allocateMediumLine());
    return allocator.allocate(size);
}

void* Allocator::allocateSlowCase(size_t size)
{
IF_DEBUG(
    void* dummy;
    BASSERT(!allocateFastCase(size, dummy));
)
    if (size <= smallMax) {
        SmallAllocator& allocator = smallAllocatorFor(size);
        log(allocator);
        allocator.refill(m_deallocator.allocateSmallLine());
        return allocator.allocate();
    }

    if (size <= mediumMax)
        return allocateMedium(size);
    
    if (size <= largeMax)
        return allocateLarge(size);

    return allocateXLarge(size);
}

} // namespace bmalloc
