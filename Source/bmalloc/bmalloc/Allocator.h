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

#ifndef Allocator_h
#define Allocator_h

#include "FixedVector.h"
#include "MediumAllocator.h"
#include "Sizes.h"
#include "SmallAllocator.h"
#include "XSmallAllocator.h"
#include <array>

namespace bmalloc {

class Deallocator;

// Per-cache object allocator.

class Allocator {
public:
    Allocator(Deallocator&);
    ~Allocator();

    void* allocate(size_t);
    bool allocateFastCase(size_t, void*&);
    void* allocateSlowCase(size_t);
    
    void scavenge();

private:
    XSmallAllocator& xSmallAllocatorFor(size_t);
    SmallAllocator& smallAllocatorFor(size_t);
    
    void* allocateFastCase(SmallAllocator&);

    void* allocateMedium(size_t);
    void* allocateLarge(size_t);
    void* allocateXLarge(size_t);
    
    void log(XSmallAllocator&);
    void log(SmallAllocator&);
    void log(MediumAllocator&);

    void processXSmallAllocatorLog();
    void processSmallAllocatorLog();
    void processMediumAllocatorLog();

    Deallocator& m_deallocator;

    std::array<XSmallAllocator, xSmallMax / alignment> m_xSmallAllocators;
    std::array<SmallAllocator, smallMax / alignment> m_smallAllocators;
    MediumAllocator m_mediumAllocator;

    FixedVector<std::pair<XSmallLine*, unsigned char>, xSmallAllocatorLogCapacity> m_xSmallAllocatorLog;
    FixedVector<std::pair<SmallLine*, unsigned char>, smallAllocatorLogCapacity> m_smallAllocatorLog;
    FixedVector<std::pair<MediumLine*, unsigned char>, mediumAllocatorLogCapacity> m_mediumAllocatorLog;
};

inline XSmallAllocator& Allocator::xSmallAllocatorFor(size_t size)
{
    size_t index = mask((size - 1ul) / alignment, m_xSmallAllocators.size() - 1);
    return m_xSmallAllocators[index];
}

inline SmallAllocator& Allocator::smallAllocatorFor(size_t size)
{
    size_t index = mask((size - 1ul) / alignment, m_smallAllocators.size() - 1);
    return m_smallAllocators[index];
}

inline bool Allocator::allocateFastCase(size_t size, void*& object)
{
    if (size <= xSmallMax) {
        XSmallAllocator& allocator = xSmallAllocatorFor(size);
        if (!allocator.canAllocate())
            return false;
        
        object = allocator.allocate();
        return true;
    }
    
    if (size <= smallMax) {
        SmallAllocator& allocator = smallAllocatorFor(size);
        if (!allocator.canAllocate())
            return false;

        object = allocator.allocate();
        return true;
    }
    
    return false;
}

inline void* Allocator::allocate(size_t size)
{
    void* object;
    if (!allocateFastCase(size, object))
        return allocateSlowCase(size);
    return object;
}

} // namespace bmalloc

#endif // Allocator_h
