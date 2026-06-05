/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "FreeList.h"
#include <wtf/Assertions.h>

namespace JSC {

FreeList::FreeList(unsigned cellSize)
    : m_cellSize(cellSize)
{
    ASSERT(!(cellSize % MarkedBlock::atomSize));
    // We only use every Nth bit in m_live.
    // Precompute a bitmask filter so that allocation is fast.

    // Wait, can't we just do N**k - 1?

    // todo: should really do this in BlockDirectory
    unsigned numberOfUnallocatableAtoms = MarkedBlock::numberOfPayloadAtoms % atomsPerCell();
    unsigned startAtom = MarkedBlock::firstPayloadRegionAtom + numberOfUnallocatableAtoms;

    WTF::BitSet<MarkedBlock::atomsPerBlock> bitmask;
    unsigned stride = atomsPerCell();
    for (uint64_t i = startAtom; i < MarkedBlock::atomsPerBlock; i += stride) {
        // bitmask |= 1 << (63 - i); // wrong because bitset is little endian
        // bitmask |= one << i;
        bitmask.set(i);
        // WTFLogAlways("afryer_bitmask_wip %llu : %llx\n", i, bitmask);
    }
    m_bitmask = bitmask;
    // WTFLogAlways("afryer_bitmask %u : %llx\n", atomsPerCell(), m_bitmask);
}

FreeList::~FreeList() = default;

void FreeList::clear()
{
    m_nextCachedInterval = nullptr;
    m_intervalEnd = nullptr; // No more cells in current interval
    m_startIndex = MarkedBlock::atomsPerBlock; // No more intervals to find. Note that we are lazy and don't clear m_live
    m_originalSize = 0;
}

void FreeList::initialize(MarkedBlock::Handle* block, const WTF::BitSet<MarkedBlock::atomsPerBlock>& live, unsigned startIndex, unsigned bytes)
{
    m_nextCachedInterval = nullptr;
    // WTFLogAlways("afryer_initalize\n");
    m_block = block;
    m_live = live;
    m_startIndex = startIndex;
    m_originalSize = bytes;
    // WTFLogAlways("afryer_initialize %u\n", countIntervals());
    findNextInterval();
}

void FreeList::initializeEmpty(MarkedBlock::Handle* block, char* intervalStart, char* intervalEnd)
{
    m_nextCachedInterval = nullptr;
    // WTFLogAlways("afryer_initalizeEmpty\n");
    m_block = block;
    m_originalSize = intervalEnd - intervalStart;
    m_intervalStart = intervalStart;
    m_intervalEnd = intervalEnd;
    m_startIndex = MarkedBlock::atomsPerBlock; // No more intervals to find. Note that we are lazy and don't clear m_live
    // WTFLogAlways("afryer_initializeEmpty %u\n", (unsigned)1);
    // m_live.clearAll(); // just for now; I think this might be important for stopAllocating
}

void FreeList::dump(PrintStream& out) const
{
    out.print("{intervalStart = ", RawPointer(m_intervalStart), ", intervalEnd = ", RawPointer(m_intervalEnd), ", cellSize = ", m_cellSize, ", block = ", m_block, ", bitmask = ", m_bitmask, ", startIndex = ", m_startIndex, ", live = ", m_live, ", originalSize = ", m_originalSize, "}");
}

} // namespace JSC

