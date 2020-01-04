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
#include "IsoCellSet.h"

#include "BlockDirectoryInlines.h"
#include "MarkedBlockInlines.h"

namespace JSC {

IsoCellSet::IsoCellSet(IsoSubspace& subspace)
    : m_subspace(subspace)
{
    size_t size = subspace.m_directory.m_blocks.size();
    m_blocksWithBits.resize(size);
    m_bits.grow(size);
    subspace.m_cellSets.append(this);
}

IsoCellSet::~IsoCellSet()
{
    if (isOnList())
        PackedRawSentinelNode<IsoCellSet>::remove();
}

Ref<SharedTask<MarkedBlock::Handle*()>> IsoCellSet::parallelNotEmptyMarkedBlockSource()
{
    class Task : public SharedTask<MarkedBlock::Handle*()> {
    public:
        Task(IsoCellSet& set)
            : m_set(set)
            , m_directory(set.m_subspace.m_directory)
        {
        }
        
        MarkedBlock::Handle* run() override
        {
            if (m_done)
                return nullptr;
            auto locker = holdLock(m_lock);
            auto bits = m_directory.m_bits.markingNotEmpty() & m_set.m_blocksWithBits;
            m_index = bits.findBit(m_index, true);
            if (m_index >= m_directory.m_blocks.size()) {
                m_done = true;
                return nullptr;
            }
            return m_directory.m_blocks[m_index++];
        }
        
    private:
        IsoCellSet& m_set;
        BlockDirectory& m_directory;
        size_t m_index { 0 };
        Lock m_lock;
        bool m_done { false };
    };
    
    return adoptRef(*new Task(*this));
}

NEVER_INLINE Bitmap<MarkedBlock::atomsPerBlock>* IsoCellSet::addSlow(unsigned blockIndex)
{
    auto locker = holdLock(m_subspace.m_directory.m_bitvectorLock);
    auto& bitsPtrRef = m_bits[blockIndex];
    auto* bits = bitsPtrRef.get();
    if (!bits) {
        bitsPtrRef = makeUnique<Bitmap<MarkedBlock::atomsPerBlock>>();
        bits = bitsPtrRef.get();
        WTF::storeStoreFence();
        m_blocksWithBits[blockIndex] = true;
    }
    return bits;
}

void IsoCellSet::didResizeBits(unsigned newSize)
{
    m_blocksWithBits.resize(newSize);
    m_bits.grow(newSize);
}

void IsoCellSet::didRemoveBlock(unsigned blockIndex)
{
    {
        auto locker = holdLock(m_subspace.m_directory.m_bitvectorLock);
        m_blocksWithBits[blockIndex] = false;
    }
    m_bits[blockIndex] = nullptr;
}

void IsoCellSet::sweepToFreeList(MarkedBlock::Handle* block)
{
    RELEASE_ASSERT(!block->isAllocated());
    
    if (!m_blocksWithBits[block->index()])
        return;
    
    WTF::loadLoadFence();
    
    if (!m_bits[block->index()]) {
        dataLog("FATAL: for block index ", block->index(), ":\n");
        dataLog("Blocks with bits says: ", !!m_blocksWithBits[block->index()], "\n");
        dataLog("Bits says: ", RawPointer(m_bits[block->index()].get()), "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    if (block->block().hasAnyNewlyAllocated()) {
        // The newlyAllocated() bits are a superset of the marks() bits.
        m_bits[block->index()]->concurrentFilter(block->block().newlyAllocated());
        return;
    }

    if (block->isEmpty() || block->areMarksStaleForSweep()) {
        {
            // Holding the bitvector lock happens to be enough because that's what we also hold in
            // other places where we manipulate this bitvector.
            auto locker = holdLock(m_subspace.m_directory.m_bitvectorLock);
            m_blocksWithBits[block->index()] = false;
        }
        m_bits[block->index()] = nullptr;
        return;
    }
    
    m_bits[block->index()]->concurrentFilter(block->block().marks());
}

} // namespace JSC

