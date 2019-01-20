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
#include "Subspace.h"

#include "AlignedMemoryAllocator.h"
#include "BlockDirectoryInlines.h"
#include "HeapCellType.h"
#include "JSCInlines.h"
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
    m_heapCellType = heapCellType;
    m_alignedMemoryAllocator = alignedMemoryAllocator;
    m_directoryForEmptyAllocation = m_alignedMemoryAllocator->firstDirectory();

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
    forEachDirectory(
        [&] (BlockDirectory& directory) {
            directory.prepareForAllocation();
        });

    m_directoryForEmptyAllocation = m_alignedMemoryAllocator->firstDirectory();
}

MarkedBlock::Handle* Subspace::findEmptyBlockToSteal()
{
    for (; m_directoryForEmptyAllocation; m_directoryForEmptyAllocation = m_directoryForEmptyAllocation->nextDirectoryInAlignedMemoryAllocator()) {
        if (MarkedBlock::Handle* block = m_directoryForEmptyAllocation->findEmptyBlockToSteal())
            return block;
    }
    return nullptr;
}

Ref<SharedTask<BlockDirectory*()>> Subspace::parallelDirectorySource()
{
    class Task : public SharedTask<BlockDirectory*()> {
    public:
        Task(BlockDirectory* directory)
            : m_directory(directory)
        {
        }
        
        BlockDirectory* run() override
        {
            auto locker = holdLock(m_lock);
            BlockDirectory* result = m_directory;
            if (result)
                m_directory = result->nextDirectoryInSubspace();
            return result;
        }
        
    private:
        BlockDirectory* m_directory;
        Lock m_lock;
    };
    
    return adoptRef(*new Task(m_firstDirectory));
}

Ref<SharedTask<MarkedBlock::Handle*()>> Subspace::parallelNotEmptyMarkedBlockSource()
{
    return createParallelSourceAdapter<BlockDirectory*, MarkedBlock::Handle*>(
        parallelDirectorySource(),
        [] (BlockDirectory* directory) -> RefPtr<SharedTask<MarkedBlock::Handle*()>> {
            if (!directory)
                return nullptr;
            return directory->parallelNotEmptyBlockSource();
        });
}

void Subspace::sweep()
{
    forEachDirectory(
        [&] (BlockDirectory& directory) {
            directory.sweep();
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

