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
#include "BlockDirectory.h"
#include <wtf/Assertions.h>

namespace JSC {

FreeList::FreeList(unsigned cellSize)
    : m_cellSize(cellSize)
    , m_strideMask(WTF::BitSet<MarkedBlock::atomsPerBlock>::strideMask(cellSize / MarkedBlock::atomSize))
{
    ASSERT(!(cellSize % MarkedBlock::atomSize));
}

FreeList::~FreeList() = default;

void FreeList::clear()
{
    m_intervalStart = nullptr;
    m_intervalEnd = nullptr;
    m_startIndex = MarkedBlock::atomsPerBlock;
    m_originalSize = 0;
}

void FreeList::initialize(MarkedBlock::Handle* block, const WTF::BitSet<MarkedBlock::atomsPerBlock>& live, unsigned startIndex, unsigned bytes)
{
    ASSERT(block);
    ASSERT(block->directory()->cellSize() == m_cellSize);
    // m_intervalStart and m_intervalEnd are set by findNextInterval
    m_originalSize = bytes;
    m_block = block;
    m_live = live;
    m_startIndex = startIndex;
    findNextInterval(m_cellSize);
}

void FreeList::initializeEmpty(char* intervalStart, char* intervalEnd)
{
    m_intervalStart = intervalStart;
    m_intervalEnd = intervalEnd;
    m_originalSize = intervalEnd - intervalStart;
    m_block = nullptr;
    // m_live and m_startIndex should not be read if m_block is null
}

void FreeList::dump(PrintStream& out) const
{
    out.print("{intervalStart = ", RawPointer(m_intervalStart), ", intervalEnd = ", RawPointer(m_intervalEnd), ", originalSize = ", m_originalSize, "}");
}

} // namespace JSC
