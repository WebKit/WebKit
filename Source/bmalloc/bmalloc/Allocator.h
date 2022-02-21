/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#include "BExport.h"
#include "BumpAllocator.h"
#include "FailureAction.h"
#include <array>

#if !BUSE(LIBPAS)

namespace bmalloc {

class Deallocator;
class Heap;

// Per-cache object allocator.

class Allocator {
public:
    Allocator(Heap&, Deallocator&);
    ~Allocator();

    void* tryAllocate(size_t size) { return allocateImpl(size, FailureAction::ReturnNull); }
    void* allocate(size_t size) { return allocateImpl(size, FailureAction::Crash); }
    void* tryAllocate(size_t alignment, size_t size) { return allocateImpl(alignment, size, FailureAction::ReturnNull); }
    void* allocate(size_t alignment, size_t size) { return allocateImpl(alignment, size, FailureAction::Crash); }
    void* tryReallocate(void* object, size_t newSize) { return reallocateImpl(object, newSize, FailureAction::ReturnNull); }
    void* reallocate(void* object, size_t newSize) { return reallocateImpl(object, newSize, FailureAction::Crash); }

    void scavenge();

private:
    void* allocateImpl(size_t, FailureAction);
    BEXPORT void* allocateImpl(size_t alignment, size_t, FailureAction);
    BEXPORT void* reallocateImpl(void*, size_t, FailureAction);

    bool allocateFastCase(size_t, void*&);
    BEXPORT void* allocateSlowCase(size_t, FailureAction);

    void* allocateLogSizeClass(size_t, FailureAction);
    void* allocateLarge(size_t, FailureAction);
    
    inline void refillAllocator(BumpAllocator&, size_t sizeClass, FailureAction);
    void refillAllocatorSlowCase(BumpAllocator&, size_t sizeClass, FailureAction);
    
    std::array<BumpAllocator, sizeClassCount> m_bumpAllocators;
    std::array<BumpRangeCache, sizeClassCount> m_bumpRangeCaches;

    Heap& m_heap;
    Deallocator& m_deallocator;
};

inline bool Allocator::allocateFastCase(size_t size, void*& object)
{
    if (size > maskSizeClassMax)
        return false;

    BumpAllocator& allocator = m_bumpAllocators[maskSizeClass(size)];
    if (!allocator.canAllocate())
        return false;

    object = allocator.allocate();
    return true;
}

inline void* Allocator::allocateImpl(size_t size, FailureAction action)
{
    void* object;
    if (!allocateFastCase(size, object))
        return allocateSlowCase(size, action);
    return object;
}

} // namespace bmalloc

#endif
