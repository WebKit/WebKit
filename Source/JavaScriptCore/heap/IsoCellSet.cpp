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
#include "IsoCellSet.h"

#include "MarkedAllocatorInlines.h"
#include "MarkedBlockInlines.h"

namespace JSC {

IsoCellSet::IsoCellSet(IsoSubspace& subspace)
    : m_subspace(subspace)
{
    size_t size = subspace.m_allocator.m_blocks.size();
    m_blocksWithBits.resize(size);
    m_bits.grow(size);
    subspace.m_cellSets.append(this);
}

IsoCellSet::~IsoCellSet()
{
    if (isOnList())
        BasicRawSentinelNode<IsoCellSet>::remove();
}

NEVER_INLINE Bitmap<MarkedBlock::atomsPerBlock>* IsoCellSet::addSlow(size_t blockIndex)
{
    auto locker = holdLock(m_subspace.m_allocator.m_bitvectorLock);
    auto& bitsPtrRef = m_bits[blockIndex];
    auto* bits = bitsPtrRef.get();
    if (!bits) {
        bitsPtrRef = std::make_unique<Bitmap<MarkedBlock::atomsPerBlock>>();
        bits = bitsPtrRef.get();
        WTF::storeStoreFence();
        m_blocksWithBits[blockIndex] = true;
    }
    return bits;
}

void IsoCellSet::didResizeBits(size_t newSize)
{
    m_blocksWithBits.resize(newSize);
    m_bits.grow(newSize);
}

void IsoCellSet::didRemoveBlock(size_t blockIndex)
{
    {
        auto locker = holdLock(m_subspace.m_allocator.m_bitvectorLock);
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
    
    if (block->hasAnyNewlyAllocated()) {
        m_bits[block->index()]->concurrentFilter(block->newlyAllocated());
        return;
    }

    if (block->isEmpty() || block->areMarksStale()) {
        {
            // Holding the bitvector lock happens to be enough because that's what we also hold in
            // other places where we manipulate this bitvector.
            auto locker = holdLock(m_subspace.m_allocator.m_bitvectorLock);
            m_blocksWithBits[block->index()] = false;
        }
        m_bits[block->index()] = nullptr;
        return;
    }
    
    m_bits[block->index()]->concurrentFilter(block->block().marks());
}

} // namespace JSC

