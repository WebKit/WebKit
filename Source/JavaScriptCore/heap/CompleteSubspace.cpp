/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Subspace.h"

#include "JSCInlines.h"
#include "MarkedAllocatorInlines.h"
#include "MarkedBlockInlines.h"
#include "PreventCollectionScope.h"
#include "SubspaceInlines.h"

namespace JSC {

CompleteSubspace::CompleteSubspace(CString name, Heap& heap, HeapCellType* heapCellType, AlignedMemoryAllocator* alignedMemoryAllocator)
    : Subspace(name, heap)
{
    initialize(heapCellType, alignedMemoryAllocator);
    for (size_t i = MarkedSpace::numSizeClasses; i--;)
        m_allocatorForSizeStep[i] = nullptr;
}

CompleteSubspace::~CompleteSubspace()
{
}

MarkedAllocator* CompleteSubspace::allocatorFor(size_t size, AllocatorForMode mode)
{
    return allocatorForNonVirtual(size, mode);
}

void* CompleteSubspace::allocate(size_t size, GCDeferralContext* deferralContext, AllocationFailureMode failureMode)
{
    return allocateNonVirtual(size, deferralContext, failureMode);
}

void* CompleteSubspace::allocateNonVirtual(size_t size, GCDeferralContext* deferralContext, AllocationFailureMode failureMode)
{
    void *result;
    if (MarkedAllocator* allocator = allocatorForNonVirtual(size, AllocatorForMode::AllocatorIfExists))
        result = allocator->allocate(deferralContext, failureMode);
    else
        result = allocateSlow(size, deferralContext, failureMode);
    return result;
}

MarkedAllocator* CompleteSubspace::allocatorForSlow(size_t size)
{
    size_t index = MarkedSpace::sizeClassToIndex(size);
    size_t sizeClass = MarkedSpace::s_sizeClassForSizeStep[index];
    if (!sizeClass)
        return nullptr;
    
    // This is written in such a way that it's OK for the JIT threads to end up here if they want
    // to generate code that uses some allocator that hadn't been used yet. Note that a possibly-
    // just-as-good solution would be to return null if we're in the JIT since the JIT treats null
    // allocator as "please always take the slow path". But, that could lead to performance
    // surprises and the algorithm here is pretty easy. Only this code has to hold the lock, to
    // prevent simultaneously MarkedAllocator creations from multiple threads. This code ensures
    // that any "forEachAllocator" traversals will only see this allocator after it's initialized
    // enough: it will have 
    auto locker = holdLock(m_space.allocatorLock());
    if (MarkedAllocator* allocator = m_allocatorForSizeStep[index])
        return allocator;

    if (false)
        dataLog("Creating marked allocator for ", m_name, ", ", m_attributes, ", ", sizeClass, ".\n");
    std::unique_ptr<MarkedAllocator> uniqueAllocator =
        std::make_unique<MarkedAllocator>(m_space.heap(), sizeClass);
    MarkedAllocator* allocator = uniqueAllocator.get();
    m_allocators.append(WTFMove(uniqueAllocator));
    allocator->setSubspace(this);
    m_space.addMarkedAllocator(locker, allocator);
    index = MarkedSpace::sizeClassToIndex(sizeClass);
    for (;;) {
        if (MarkedSpace::s_sizeClassForSizeStep[index] != sizeClass)
            break;

        m_allocatorForSizeStep[index] = allocator;
        
        if (!index--)
            break;
    }
    allocator->setNextAllocatorInSubspace(m_firstAllocator);
    m_alignedMemoryAllocator->registerAllocator(allocator);
    WTF::storeStoreFence();
    m_firstAllocator = allocator;
    return allocator;
}

void* CompleteSubspace::allocateSlow(size_t size, GCDeferralContext* deferralContext, AllocationFailureMode failureMode)
{
    void* result = tryAllocateSlow(size, deferralContext);
    if (failureMode == AllocationFailureMode::Assert)
        RELEASE_ASSERT(result);
    return result;
}

void* CompleteSubspace::tryAllocateSlow(size_t size, GCDeferralContext* deferralContext)
{
    sanitizeStackForVM(m_space.heap()->vm());
    
    if (MarkedAllocator* allocator = allocatorFor(size, AllocatorForMode::EnsureAllocator))
        return allocator->allocate(deferralContext, AllocationFailureMode::ReturnNull);
    
    if (size <= Options::largeAllocationCutoff()
        && size <= MarkedSpace::largeCutoff) {
        dataLog("FATAL: attampting to allocate small object using large allocation.\n");
        dataLog("Requested allocation size: ", size, "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    m_space.heap()->collectIfNecessaryOrDefer(deferralContext);
    
    size = WTF::roundUpToMultipleOf<MarkedSpace::sizeStep>(size);
    LargeAllocation* allocation = LargeAllocation::tryCreate(*m_space.m_heap, size, this);
    if (!allocation)
        return nullptr;
    
    m_space.m_largeAllocations.append(allocation);
    m_space.m_heap->didAllocate(size);
    m_space.m_capacity += size;
    
    m_largeAllocations.append(allocation);
        
    return allocation->cell();
}

} // namespace JSC

