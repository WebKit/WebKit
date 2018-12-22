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

#pragma once

#include "AtomIndices.h"
#include "IsoCellSet.h"
#include "MarkedBlockInlines.h"

namespace JSC {

inline bool IsoCellSet::add(HeapCell* cell)
{
    AtomIndices atomIndices(cell);
    auto& bitsPtrRef = m_bits[atomIndices.blockIndex];
    auto* bits = bitsPtrRef.get();
    if (UNLIKELY(!bits))
        bits = addSlow(atomIndices.blockIndex);
    return !bits->concurrentTestAndSet(atomIndices.atomNumber);
}

inline bool IsoCellSet::remove(HeapCell* cell)
{
    AtomIndices atomIndices(cell);
    auto& bitsPtrRef = m_bits[atomIndices.blockIndex];
    auto* bits = bitsPtrRef.get();
    if (!bits)
        return false;
    return bits->concurrentTestAndClear(atomIndices.atomNumber);
}

inline bool IsoCellSet::contains(HeapCell* cell) const
{
    AtomIndices atomIndices(cell);
    auto* bits = m_bits[atomIndices.blockIndex].get();
    if (bits)
        return bits->get(atomIndices.atomNumber);
    return false;
}

template<typename Func>
void IsoCellSet::forEachMarkedCell(const Func& func)
{
    BlockDirectory& directory = m_subspace.m_directory;
    (directory.m_markingNotEmpty & m_blocksWithBits).forEachSetBit(
        [&] (size_t blockIndex) {
            MarkedBlock::Handle* block = directory.m_blocks[blockIndex];

            auto* bits = m_bits[blockIndex].get();
            block->forEachMarkedCell(
                [&] (size_t atomNumber, HeapCell* cell, HeapCell::Kind kind) -> IterationStatus {
                    if (bits->get(atomNumber))
                        func(cell, kind);
                    return IterationStatus::Continue;
                });
        });
}

template<typename Func>
Ref<SharedTask<void(SlotVisitor&)>> IsoCellSet::forEachMarkedCellInParallel(const Func& func)
{
    class Task : public SharedTask<void(SlotVisitor&)> {
    public:
        Task(IsoCellSet& set, const Func& func)
            : m_set(set)
            , m_blockSource(set.parallelNotEmptyMarkedBlockSource())
            , m_func(func)
        {
        }
        
        void run(SlotVisitor& visitor) override
        {
            while (MarkedBlock::Handle* handle = m_blockSource->run()) {
                size_t blockIndex = handle->index();
                auto* bits = m_set.m_bits[blockIndex].get();
                handle->forEachMarkedCell(
                    [&] (size_t atomNumber, HeapCell* cell, HeapCell::Kind kind) -> IterationStatus {
                        if (bits->get(atomNumber))
                            m_func(visitor, cell, kind);
                        return IterationStatus::Continue;
                    });
            }
        }
        
    private:
        IsoCellSet& m_set;
        Ref<SharedTask<MarkedBlock::Handle*()>> m_blockSource;
        Func m_func;
        Lock m_lock;
    };
    
    return adoptRef(*new Task(*this, func));
}

template<typename Func>
void IsoCellSet::forEachLiveCell(const Func& func)
{
    BlockDirectory& directory = m_subspace.m_directory;
    m_blocksWithBits.forEachSetBit(
        [&] (size_t blockIndex) {
            MarkedBlock::Handle* block = directory.m_blocks[blockIndex];

            // FIXME: We could optimize this by checking our bits before querying isLive.
            // OOPS! (need bug URL)
            auto* bits = m_bits[blockIndex].get();
            block->forEachLiveCell(
                [&] (size_t atomNumber, HeapCell* cell, HeapCell::Kind kind) -> IterationStatus {
                    if (bits->get(atomNumber))
                        func(cell, kind);
                    return IterationStatus::Continue;
                });
        });
}

} // namespace JSC

