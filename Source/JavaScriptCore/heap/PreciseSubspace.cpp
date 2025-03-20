/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "PreciseSubspace.h"

#include "AllocatingScope.h"
#include "IsoAlignedMemoryAllocator.h"
#include "IsoCellSetInlines.h"
#include "JSCellInlines.h"
#include "MarkedSpaceInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PreciseSubspace);

PreciseSubspace::PreciseSubspace(CString name, JSC::Heap& heap, const HeapCellType& heapCellType, AlignedMemoryAllocator* allocator)
    : Subspace(SubspaceKind::PreciseSubspace, name, heap)
{
    initialize(heapCellType, allocator);
}

PreciseSubspace::~PreciseSubspace() = default;

void PreciseSubspace::didResizeBits(unsigned)
{
    UNREACHABLE_FOR_PLATFORM();
}

void PreciseSubspace::didRemoveBlock(unsigned)
{
    UNREACHABLE_FOR_PLATFORM();
}

void PreciseSubspace::didBeginSweepingToFreeList(MarkedBlock::Handle*)
{
    UNREACHABLE_FOR_PLATFORM();
}

void* PreciseSubspace::tryAllocate(size_t size)
{
    PreciseAllocation* allocation = PreciseAllocation::tryCreate(m_space.heap(), size, this, 0);
    if (!allocation)
        return nullptr;

    m_preciseAllocations.append(allocation);
    m_space.registerPreciseAllocation(allocation, /* isNewAllocation */ true);
    return allocation->cell();
}

void* PreciseSubspace::allocate(VM& vm, size_t cellSize, GCDeferralContext* deferralContext, AllocationFailureMode failureMode)
{
    ASSERT(vm.currentThreadIsHoldingAPILock());
    ASSERT(!vm.heap.objectSpace().isIterating());
    UNUSED_PARAM(failureMode);
    ASSERT_WITH_MESSAGE(failureMode == AllocationFailureMode::ReturnNull, "PreciseSubspace is meant for large objects so it should always be able to fail.");

    AllocatingScope helpingHeap(vm.heap);

    vm.heap.collectIfNecessaryOrDefer(deferralContext);

    return tryAllocate(cellSize);
}

} // namespace JSC

