/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef DFGAllocator_h
#define DFGAllocator_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "DFGCommon.h"
#include <wtf/PageAllocationAligned.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace DFG {

// Custom pool allocator for exactly one type (type T). It has fast (O(1), only a few
// instructions) allocator, and a similarly fast free(). Recycling works if either of
// the following is true:
// - T has a trivial destructor. In that case you don't have to ever call free() on
//   anything. You can just call freeAll() instead.
// - You call free() on all T's that you allocated, and never use freeAll().

// forcedCellSize: set this to 0 if you're happy with the default alignment, or set it
// to a non-zero value if you want to forcibly align the allocations to a certain
// value. When you do this, Allocator will ASSERT that forcedCellAlignment >= sizeof(T).

template<typename T, unsigned forcedCellAlignment>
class Allocator {
public:
    Allocator();
    ~Allocator();
    
    void* allocate(); // Use placement new to allocate, and avoid using this method.
    void free(T*); // Call this method to delete; never use 'delete' directly.
    
    void freeAll(); // Only call this if T has a trivial destructor.
    void reset(); // Like freeAll(), but also returns all memory to the OS.
    
    unsigned indexOf(const T*);
    
    static Allocator* allocatorOf(const T*);
    
private:
    void* bumpAllocate();
    void* freeListAllocate();
    void* allocateSlow();
    
    static size_t cellSize()
    {
        if (!forcedCellAlignment)
            return sizeof(T);
        
        ASSERT(forcedCellAlignment >= sizeof(T));
        return forcedCellAlignment;
    }
    
    struct Region {
        static size_t size() { return 64 * KB; }
        static size_t headerSize() { return std::max(sizeof(Region), cellSize()); }
        static unsigned numberOfThingsPerRegion() { return (size() - headerSize()) / cellSize(); }
        static size_t payloadSize() { return numberOfThingsPerRegion() * cellSize(); }
        char* payloadBegin() { return bitwise_cast<char*>(this) + headerSize(); }
        char* payloadEnd() { return payloadBegin() + payloadSize(); }
        bool isInThisRegion(const T* pointer) { return static_cast<size_t>(bitwise_cast<char*>(pointer) - payloadBegin()) < payloadSize(); }
        static Region* regionFor(const T* pointer) { return bitwise_cast<Region*>(bitwise_cast<uintptr_t>(pointer) & ~(size() - 1)); }
        
        PageAllocationAligned m_allocation;
        Allocator* m_allocator;
        Region* m_next;
    };
    
    void freeRegionsStartingAt(Allocator::Region*);
    void startBumpingIn(Allocator::Region*);
    
    Region* m_regionHead;
    void** m_freeListHead;
    char* m_bumpEnd;
    size_t m_bumpRemaining;
};

template<typename T, unsigned forcedCellAlignment>
inline Allocator<T, forcedCellAlignment>::Allocator()
    : m_regionHead(0)
    , m_freeListHead(0)
    , m_bumpRemaining(0)
{
}

template<typename T, unsigned forcedCellAlignment>
inline Allocator<T, forcedCellAlignment>::~Allocator()
{
    reset();
}

template<typename T, unsigned forcedCellAlignment>
ALWAYS_INLINE void* Allocator<T, forcedCellAlignment>::allocate()
{
    void* result = bumpAllocate();
    if (LIKELY(!!result))
        return result;
    return freeListAllocate();
}

template<typename T, unsigned forcedCellAlignment>
void Allocator<T, forcedCellAlignment>::free(T* object)
{
    object->~T();
    
    void** cell = bitwise_cast<void**>(object);
    *cell = m_freeListHead;
    m_freeListHead = cell;
}

template<typename T, unsigned forcedCellAlignment>
void Allocator<T, forcedCellAlignment>::freeAll()
{
    if (!m_regionHead) {
        ASSERT(!m_bumpRemaining);
        ASSERT(!m_freeListHead);
        return;
    }
    
    // Since the caller is opting out of calling the destructor for any allocated thing,
    // we have two choices, plus a continuum between: we can either just delete all regions
    // (i.e. call reset()), or we can make all regions available for reuse. We do something
    // that optimizes for (a) speed of freeAll(), (b) the assumption that if the user calls
    // freeAll() then they will probably be calling allocate() in the near future. Namely,
    // we free all but one region, and make the remaining region a bump allocation region.
    
    freeRegionsStartingAt(m_regionHead->m_next);
    
    m_regionHead->m_next = 0;
    m_freeListHead = 0;
    startBumpingIn(m_regionHead);
}

template<typename T, unsigned forcedCellAlignment>
void Allocator<T, forcedCellAlignment>::reset()
{
    freeRegionsStartingAt(m_regionHead);
    
    m_regionHead = 0;
    m_freeListHead = 0;
    m_bumpRemaining = 0;
}

template<typename T, unsigned forcedCellAlignment>
unsigned Allocator<T, forcedCellAlignment>::indexOf(const T* object)
{
    unsigned baseIndex = 0;
    for (Region* region = m_regionHead; region; region = region->m_next) {
        if (region->isInThisRegion(object))
            return baseIndex + (bitwise_cast<char*>(object) - region->payloadBegin()) / cellSize();
        baseIndex += Region::numberOfThingsPerRegion();
    }
    CRASH();
    return 0;
}

template<typename T, unsigned forcedCellAlignment>
Allocator<T, forcedCellAlignment>* Allocator<T, forcedCellAlignment>::allocatorOf(const T* object)
{
    return Region::regionFor(object)->m_allocator;
}

template<typename T, unsigned forcedCellAlignment>
ALWAYS_INLINE void* Allocator<T, forcedCellAlignment>::bumpAllocate()
{
    if (size_t remaining = m_bumpRemaining) {
        remaining -= cellSize();
        m_bumpRemaining = remaining;
        return m_bumpEnd - (remaining + cellSize());
    }
    return 0;
}

template<typename T, unsigned forcedCellAlignment>
void* Allocator<T, forcedCellAlignment>::freeListAllocate()
{
    void** result = m_freeListHead;
    if (UNLIKELY(!result))
        return allocateSlow();
    m_freeListHead = bitwise_cast<void**>(*result);
    return result;
}

template<typename T, unsigned forcedCellAlignment>
void* Allocator<T, forcedCellAlignment>::allocateSlow()
{
    ASSERT(!m_freeListHead);
    ASSERT(!m_bumpRemaining);
    
    if (logCompilationChanges())
        dataLog("Allocating another allocator region.\n");
    
    PageAllocationAligned allocation = PageAllocationAligned::allocate(Region::size(), Region::size(), OSAllocator::JSGCHeapPages);
    if (!static_cast<bool>(allocation))
        CRASH();
    Region* region = static_cast<Region*>(allocation.base());
    region->m_allocation = allocation;
    region->m_allocator = this;
    startBumpingIn(region);
    region->m_next = m_regionHead;
    m_regionHead = region;
    
    void* result = bumpAllocate();
    ASSERT(result);
    return result;
}

template<typename T, unsigned forcedCellAlignment>
void Allocator<T, forcedCellAlignment>::freeRegionsStartingAt(typename Allocator<T, forcedCellAlignment>::Region* region)
{
    while (region) {
        Region* nextRegion = region->m_next;
        region->m_allocation.deallocate();
        region = nextRegion;
    }
}

template<typename T, unsigned forcedCellAlignment>
void Allocator<T, forcedCellAlignment>::startBumpingIn(typename Allocator<T, forcedCellAlignment>::Region* region)
{
    m_bumpEnd = region->payloadEnd();
    m_bumpRemaining = Region::payloadSize();
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGAllocator_h

