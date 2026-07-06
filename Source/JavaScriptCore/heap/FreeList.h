/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/JSExportMacros.h>
#include <JavaScriptCore/MarkedBlock.h>
#include <wtf/Compiler.h>
#include <wtf/MathExtras.h>
#include <wtf/PrintStream.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class HeapCell;

class FreeList {
public:
    FreeList(unsigned cellSize);
    ~FreeList();

    void NODELETE clear();

    JS_EXPORT_PRIVATE void initialize(MarkedBlock::Handle*, const WTF::BitSet<MarkedBlock::atomsPerBlock>& live, unsigned startIndex, unsigned bytes);
    JS_EXPORT_PRIVATE void initializeEmpty(char* intervalStart, char* intervalEnd);

    bool allocationWillSucceed()
    {
        if (m_intervalStart < m_intervalEnd)
            return true;
        // findNextInterval may only be called once the current interval is exhausted.
        if (m_block && findNextInterval(m_cellSize))
            return true;
        return false;
    }
    bool allocationWillFail() { return !allocationWillSucceed(); }

    template<typename Func>
    HeapCell* allocateWithCellSize(const Func& slowPath, size_t cellSize);

    // Empties the FreeList, invoking func on each remaining free cell along the way.
    template<typename Func>
    void consumeRemaining(const Func&);

    unsigned originalSize() const { return m_originalSize; }

    static constexpr ptrdiff_t offsetOfIntervalStart() { return OBJECT_OFFSETOF(FreeList, m_intervalStart); }
    static constexpr ptrdiff_t offsetOfIntervalEnd() { return OBJECT_OFFSETOF(FreeList, m_intervalEnd); }
    static constexpr ptrdiff_t offsetOfOriginalSize() { return OBJECT_OFFSETOF(FreeList, m_originalSize); }
    static constexpr ptrdiff_t offsetOfCellSize() { return OBJECT_OFFSETOF(FreeList, m_cellSize); }

    JS_EXPORT_PRIVATE void dump(PrintStream&) const;

    unsigned cellSize() const { return m_cellSize; }

private:
    ALWAYS_INLINE bool findNextInterval(unsigned cellSize)
    {
        ASSERT(m_block);
        unsigned atomsPerCell = cellSize / MarkedBlock::atomSize;
        size_t startAtom = m_live.findBitInStride(m_startIndex, false, atomsPerCell, m_strideMask);
        if (startAtom >= MarkedBlock::atomsPerBlock) {
            m_startIndex = MarkedBlock::atomsPerBlock;
            return false;
        }
        size_t endAtom = m_live.findBitInStride(startAtom, true, atomsPerCell, m_strideMask);
        ASSERT(endAtom <= MarkedBlock::atomsPerBlock);
        m_intervalStart = std::bit_cast<char*>(m_block->atomAt(startAtom));
        m_intervalEnd = std::bit_cast<char*>(m_block->atomAt(endAtom));
        m_startIndex = endAtom;
        return true;
    }

    // m_intervalStart and m_intervalEnd store an interval to bump allocate from (bumping by m_cellSize)
    char* m_intervalStart { nullptr };
    char* m_intervalEnd { nullptr };

    unsigned m_cellSize { 0 };

    unsigned m_originalSize { 0 };

    // When allocating from a block that wasn't empty, we also store m_block, m_live, and m_startIndex.
    // Otherwise, we set m_block to null to quickly indicate that the block was empty.
    // So, we should check m_block before accessing m_live or m_startIndex.
    MarkedBlock::Handle* m_block { nullptr };
    WTF::BitSet<MarkedBlock::atomsPerBlock> m_live;
    // m_strideMask caches the bit mask to avoid reconstructing frequently
    typename WTF::BitSet<MarkedBlock::atomsPerBlock>::WordType m_strideMask { 0 };
    // m_startIndex remembers where we are in m_live
    unsigned m_startIndex { MarkedBlock::atomsPerBlock };
};

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
