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

#include "AlignedMemoryAllocator.h"
#include "HeapCellType.h"
#include "JSCInlines.h"
#include "MarkedAllocatorInlines.h"
#include "MarkedBlockInlines.h"
#include "ParallelSourceAdapter.h"
#include "PreventCollectionScope.h"
#include "SubspaceInlines.h"

namespace JSC {

Subspace::Subspace(CString name, Heap& heap)
    : m_space(heap.objectSpace())
    , m_name(name)
{
}

void Subspace::initialize(HeapCellType* heapCellType, AlignedMemoryAllocator* alignedMemoryAllocator)
{
    m_attributes = heapCellType->attributes();
    m_heapCellType = heapCellType;
    m_alignedMemoryAllocator = alignedMemoryAllocator;
    m_allocatorForEmptyAllocation = m_alignedMemoryAllocator->firstAllocator();

    Heap& heap = *m_space.heap();
    PreventCollectionScope preventCollectionScope(heap);
    heap.objectSpace().m_subspaces.append(this);
    m_alignedMemoryAllocator->registerSubspace(this);
}

Subspace::~Subspace()
{
}

void Subspace::finishSweep(MarkedBlock::Handle& block, FreeList* freeList)
{
    m_heapCellType->finishSweep(block, freeList);
}

void Subspace::destroy(VM& vm, JSCell* cell)
{
    m_heapCellType->destroy(vm, cell);
}

void Subspace::prepareForAllocation()
{
    forEachAllocator(
        [&] (MarkedAllocator& allocator) {
            allocator.prepareForAllocation();
        });

    m_allocatorForEmptyAllocation = m_alignedMemoryAllocator->firstAllocator();
}

MarkedBlock::Handle* Subspace::findEmptyBlockToSteal()
{
    for (; m_allocatorForEmptyAllocation; m_allocatorForEmptyAllocation = m_allocatorForEmptyAllocation->nextAllocatorInAlignedMemoryAllocator()) {
        if (MarkedBlock::Handle* block = m_allocatorForEmptyAllocation->findEmptyBlockToSteal())
            return block;
    }
    return nullptr;
}

RefPtr<SharedTask<MarkedAllocator*()>> Subspace::parallelAllocatorSource()
{
    class Task : public SharedTask<MarkedAllocator*()> {
    public:
        Task(MarkedAllocator* allocator)
            : m_allocator(allocator)
        {
        }
        
        MarkedAllocator* run() override
        {
            auto locker = holdLock(m_lock);
            MarkedAllocator* result = m_allocator;
            if (result)
                m_allocator = result->nextAllocatorInSubspace();
            return result;
        }
        
    private:
        MarkedAllocator* m_allocator;
        Lock m_lock;
    };
    
    return adoptRef(new Task(m_firstAllocator));
}

RefPtr<SharedTask<MarkedBlock::Handle*()>> Subspace::parallelNotEmptyMarkedBlockSource()
{
    return createParallelSourceAdapter<MarkedAllocator*, MarkedBlock::Handle*>(
        parallelAllocatorSource(),
        [] (MarkedAllocator* allocator) -> RefPtr<SharedTask<MarkedBlock::Handle*()>> {
            if (!allocator)
                return nullptr;
            return allocator->parallelNotEmptyBlockSource();
        });
}

void Subspace::sweep()
{
    forEachAllocator(
        [&] (MarkedAllocator& allocator) {
            allocator.sweep();
        });
}

void Subspace::didResizeBits(size_t)
{
}

void Subspace::didRemoveBlock(size_t)
{
}

void Subspace::didBeginSweepingToFreeList(MarkedBlock::Handle*)
{
}

} // namespace JSC

