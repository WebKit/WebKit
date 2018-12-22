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

#pragma once

#include "BlockDirectoryInlines.h"
#include "JSCast.h"
#include "MarkedBlock.h"
#include "MarkedSpace.h"
#include "Subspace.h"

namespace JSC {

template<typename Func>
void Subspace::forEachDirectory(const Func& func)
{
    for (BlockDirectory* directory = m_firstDirectory; directory; directory = directory->nextDirectoryInSubspace())
        func(*directory);
}

template<typename Func>
void Subspace::forEachMarkedBlock(const Func& func)
{
    forEachDirectory(
        [&] (BlockDirectory& directory) {
            directory.forEachBlock(func);
        });
}

template<typename Func>
void Subspace::forEachNotEmptyMarkedBlock(const Func& func)
{
    forEachDirectory(
        [&] (BlockDirectory& directory) {
            directory.forEachNotEmptyBlock(func);
        });
}

template<typename Func>
void Subspace::forEachLargeAllocation(const Func& func)
{
    for (LargeAllocation* allocation = m_largeAllocations.begin(); allocation != m_largeAllocations.end(); allocation = allocation->next())
        func(allocation);
}

template<typename Func>
void Subspace::forEachMarkedCell(const Func& func)
{
    forEachNotEmptyMarkedBlock(
        [&] (MarkedBlock::Handle* handle) {
            handle->forEachMarkedCell(
                [&] (size_t, HeapCell* cell, HeapCell::Kind kind) -> IterationStatus {
                    func(cell, kind);
                    return IterationStatus::Continue;
                });
        });
    forEachLargeAllocation(
        [&] (LargeAllocation* allocation) {
            if (allocation->isMarked())
                func(allocation->cell(), m_attributes.cellKind);
        });
}

template<typename Func>
Ref<SharedTask<void(SlotVisitor&)>> Subspace::forEachMarkedCellInParallel(const Func& func)
{
    class Task : public SharedTask<void(SlotVisitor&)> {
    public:
        Task(Subspace& subspace, const Func& func)
            : m_subspace(subspace)
            , m_blockSource(subspace.parallelNotEmptyMarkedBlockSource())
            , m_func(func)
        {
        }
        
        void run(SlotVisitor& visitor) override
        {
            while (MarkedBlock::Handle* handle = m_blockSource->run()) {
                handle->forEachMarkedCell(
                    [&] (size_t, HeapCell* cell, HeapCell::Kind kind) -> IterationStatus {
                        m_func(visitor, cell, kind);
                        return IterationStatus::Continue;
                    });
            }
            
            {
                auto locker = holdLock(m_lock);
                if (!m_needToVisitLargeAllocations)
                    return;
                m_needToVisitLargeAllocations = false;
            }
            
            m_subspace.forEachLargeAllocation(
                [&] (LargeAllocation* allocation) {
                    if (allocation->isMarked())
                        m_func(visitor, allocation->cell(), m_subspace.m_attributes.cellKind);
                });
        }
        
    private:
        Subspace& m_subspace;
        Ref<SharedTask<MarkedBlock::Handle*()>> m_blockSource;
        Func m_func;
        Lock m_lock;
        bool m_needToVisitLargeAllocations { true };
    };
    
    return adoptRef(*new Task(*this, func));
}

template<typename Func>
void Subspace::forEachLiveCell(const Func& func)
{
    forEachMarkedBlock(
        [&] (MarkedBlock::Handle* handle) {
            handle->forEachLiveCell(
                [&] (size_t, HeapCell* cell, HeapCell::Kind kind) -> IterationStatus {
                    func(cell, kind);
                    return IterationStatus::Continue;
                });
        });
    forEachLargeAllocation(
        [&] (LargeAllocation* allocation) {
            if (allocation->isLive())
                func(allocation->cell(), m_attributes.cellKind);
        });
}

} // namespace JSC

