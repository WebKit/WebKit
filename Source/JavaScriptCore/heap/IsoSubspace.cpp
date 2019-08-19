/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "IsoSubspace.h"

#include "AllocatorInlines.h"
#include "BlockDirectoryInlines.h"
#include "IsoAlignedMemoryAllocator.h"
#include "IsoSubspaceInlines.h"
#include "LocalAllocatorInlines.h"

namespace JSC {

IsoSubspace::IsoSubspace(CString name, Heap& heap, HeapCellType* heapCellType, size_t size)
    : Subspace(name, heap)
    , m_size(size)
    , m_directory(&heap, WTF::roundUpToMultipleOf<MarkedBlock::atomSize>(size))
    , m_localAllocator(&m_directory)
    , m_isoAlignedMemoryAllocator(makeUnique<IsoAlignedMemoryAllocator>())
{
    initialize(heapCellType, m_isoAlignedMemoryAllocator.get());

    auto locker = holdLock(m_space.directoryLock());
    m_directory.setSubspace(this);
    m_space.addBlockDirectory(locker, &m_directory);
    m_alignedMemoryAllocator->registerDirectory(&m_directory);
    m_firstDirectory = &m_directory;
}

IsoSubspace::~IsoSubspace()
{
}

Allocator IsoSubspace::allocatorFor(size_t size, AllocatorForMode mode)
{
    return allocatorForNonVirtual(size, mode);
}

void* IsoSubspace::allocate(VM& vm, size_t size, GCDeferralContext* deferralContext, AllocationFailureMode failureMode)
{
    return allocateNonVirtual(vm, size, deferralContext, failureMode);
}

void IsoSubspace::didResizeBits(size_t blockIndex)
{
    m_cellSets.forEach(
        [&] (IsoCellSet* set) {
            set->didResizeBits(blockIndex);
        });
}

void IsoSubspace::didRemoveBlock(size_t blockIndex)
{
    m_cellSets.forEach(
        [&] (IsoCellSet* set) {
            set->didRemoveBlock(blockIndex);
        });
}

void IsoSubspace::didBeginSweepingToFreeList(MarkedBlock::Handle* block)
{
    m_cellSets.forEach(
        [&] (IsoCellSet* set) {
            set->sweepToFreeList(block);
        });
}

} // namespace JSC

