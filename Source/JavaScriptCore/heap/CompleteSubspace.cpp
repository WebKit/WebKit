/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#include "AlignedMemoryAllocator.h"
#include "AllocatorInlines.h"
#include "JSCellInlines.h"
#include "LocalAllocatorInlines.h"
#include "MarkedSpaceInlines.h"
#include "SubspaceInlines.h"

namespace JSC {

CompleteSubspace::CompleteSubspace(CString name, Heap& heap, HeapCellType* heapCellType, AlignedMemoryAllocator* alignedMemoryAllocator)
    : Subspace(name, heap)
{
    initialize(heapCellType, alignedMemoryAllocator);
}

CompleteSubspace::~CompleteSubspace()
{
}

Allocator CompleteSubspace::allocatorFor(size_t size, AllocatorForMode mode)
{
    return allocatorForNonVirtual(size, mode);
}

void* CompleteSubspace::allocate(VM& vm, size_t size, GCDeferralContext* deferralContext, AllocationFailureMode failureMode)
{
    return allocateNonVirtual(vm, size, deferralContext, failureMode);
}

Allocator CompleteSubspace::allocatorForSlow(size_t size)
{
    size_t index = MarkedSpace::sizeClassToIndex(size);
    size_t sizeClass = MarkedSpace::s_sizeClassForSizeStep[index];
    if (!sizeClass)
        return Allocator();
    
    // This is written in such a way that it's OK for the JIT threads to end up here if they want
    // to generate code that uses some allocator that hadn't been used yet. Note that a possibly-
    // just-as-good solution would be to return null if we're in the JIT since the JIT treats null
    // allocator as "please always take the slow path". But, that could lead to performance
    // surprises and the algorithm here is pretty easy. Only this code has to hold the lock, to
    // prevent simultaneously BlockDirectory creations from multiple threads. This code ensures
    // that any "forEachAllocator" traversals will only see this allocator after it's initialized
    // enough: it will have 
    Locker locker { m_space.directoryLock() };
    if (Allocator allocator = m_allocatorForSizeStep[index])
        return allocator;

    if (false)
        dataLog("Creating BlockDirectory/LocalAllocator for ", m_name, ", ", attributes(), ", ", sizeClass, ".\n");
    
    std::unique_ptr<BlockDirectory> uniqueDirectory = makeUnique<BlockDirectory>(sizeClass);
    BlockDirectory* directory = uniqueDirectory.get();
    m_directories.append(WTFMove(uniqueDirectory));
    
    directory->setSubspace(this);
    m_space.addBlockDirectory(locker, directory);
    
    std::unique_ptr<LocalAllocator> uniqueLocalAllocator =
        makeUnique<LocalAllocator>(directory);
    LocalAllocator* localAllocator = uniqueLocalAllocator.get();
    m_localAllocators.append(WTFMove(uniqueLocalAllocator));
    
    Allocator allocator(localAllocator);
    
    index = MarkedSpace::sizeClassToIndex(sizeClass);
    for (;;) {
        if (MarkedSpace::s_sizeClassForSizeStep[index] != sizeClass)
            break;

        m_allocatorForSizeStep[index] = allocator;
        
        if (!index--)
            break;
    }
    
    directory->setNextDirectoryInSubspace(m_firstDirectory);
    m_alignedMemoryAllocator->registerDirectory(m_space.heap(), directory);
    WTF::storeStoreFence();
    m_firstDirectory = directory;
    return allocator;
}

void* CompleteSubspace::allocateSlow(VM& vm, size_t size, GCDeferralContext* deferralContext, AllocationFailureMode failureMode)
{
    void* result = tryAllocateSlow(vm, size, deferralContext);
    if (failureMode == AllocationFailureMode::Assert)
        RELEASE_ASSERT(result);
    return result;
}

void* CompleteSubspace::tryAllocateSlow(VM& vm, size_t size, GCDeferralContext* deferralContext)
{
    if constexpr (validateDFGDoesGC)
        vm.heap.verifyCanGC();

    sanitizeStackForVM(vm);
    
    if (Allocator allocator = allocatorFor(size, AllocatorForMode::EnsureAllocator))
        return allocator.allocate(vm.heap, deferralContext, AllocationFailureMode::ReturnNull);
    
    if (size <= Options::preciseAllocationCutoff()
        && size <= MarkedSpace::largeCutoff) {
        dataLog("FATAL: attampting to allocate small object using large allocation.\n");
        dataLog("Requested allocation size: ", size, "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    vm.heap.collectIfNecessaryOrDefer(deferralContext);
    
    size = WTF::roundUpToMultipleOf<MarkedSpace::sizeStep>(size);
    PreciseAllocation* allocation = PreciseAllocation::tryCreate(vm.heap, size, this, m_space.m_preciseAllocations.size());
    if (!allocation)
        return nullptr;
    
    m_space.m_preciseAllocations.append(allocation);
    if (auto* set = m_space.preciseAllocationSet())
        set->add(allocation->cell());
    ASSERT(allocation->indexInSpace() == m_space.m_preciseAllocations.size() - 1);
    vm.heap.didAllocate(size);
    m_space.m_capacity += size;
    
    m_preciseAllocations.append(allocation);
        
    return allocation->cell();
}

void* CompleteSubspace::reallocatePreciseAllocationNonVirtual(VM& vm, HeapCell* oldCell, size_t size, GCDeferralContext* deferralContext, AllocationFailureMode failureMode)
{
    if constexpr (validateDFGDoesGC)
        vm.heap.verifyCanGC();

    // The following conditions are met in Butterfly for example.
    ASSERT(oldCell->isPreciseAllocation());

    PreciseAllocation* oldAllocation = &oldCell->preciseAllocation();
    ASSERT(oldAllocation->cellSize() <= size);
    ASSERT(oldAllocation->weakSet().isTriviallyDestructible());
    ASSERT(oldAllocation->attributes().destruction == DoesNotNeedDestruction);
    ASSERT(oldAllocation->attributes().cellKind == HeapCell::Auxiliary);
    ASSERT(size > MarkedSpace::largeCutoff);

    sanitizeStackForVM(vm);

    if (size <= Options::preciseAllocationCutoff()
        && size <= MarkedSpace::largeCutoff) {
        dataLog("FATAL: attampting to allocate small object using large allocation.\n");
        dataLog("Requested allocation size: ", size, "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }

    vm.heap.collectIfNecessaryOrDefer(deferralContext);

    size = WTF::roundUpToMultipleOf<MarkedSpace::sizeStep>(size);
    size_t difference = size - oldAllocation->cellSize();
    unsigned oldIndexInSpace = oldAllocation->indexInSpace();
    if (oldAllocation->isOnList())
        oldAllocation->remove();

    PreciseAllocation* allocation = oldAllocation->tryReallocate(size, this);
    if (!allocation) {
        RELEASE_ASSERT(failureMode != AllocationFailureMode::Assert);
        m_preciseAllocations.append(oldAllocation);
        return nullptr;
    }
    ASSERT(oldIndexInSpace == allocation->indexInSpace());

    // If reallocation changes the address, we should update HashSet.
    if (oldAllocation != allocation) {
        if (auto* set = m_space.preciseAllocationSet()) {
            set->remove(oldAllocation->cell());
            set->add(allocation->cell());
        }
    }

    m_space.m_preciseAllocations[oldIndexInSpace] = allocation;
    vm.heap.didAllocate(difference);
    m_space.m_capacity += difference;

    m_preciseAllocations.append(allocation);

    return allocation->cell();
}

} // namespace JSC

