/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

namespace JSC {

FreeList::FreeList(unsigned cellSize)
    : m_cellSize(cellSize)
{
}

FreeList::~FreeList()
{
}

void FreeList::clear()
{
#if ENABLE(BITMAP_FREELIST)
    m_currentRowBitmap = 0;
    m_currentRowIndex = 0;
#else
    m_scrambledHead = 0;
    m_secret = 0;
#endif
    m_payloadEnd = nullptr;
    m_remaining = 0;
    m_originalSize = 0;
}

#if ENABLE(BITMAP_FREELIST)

void FreeList::initializeAtomsBitmap(MarkedBlock::Handle* block, AtomsBitmap& freeAtoms, unsigned bytes)
{
#if ASSERT_ENABLED
    m_markedBlock = block;
#endif
    ASSERT_UNUSED(freeAtoms, &freeAtoms == &m_bitmap);
    // m_bitmap has already been filled in by MarkedBlock::Handle::specializedSweep().

    m_currentRowBitmap = 0;
    size_t rowIndex = AtomsBitmap::numberOfWords;
    while (rowIndex--) {
        auto rowBitmap = m_bitmap.wordAt(rowIndex);
        if (rowBitmap) {
            m_currentRowBitmap = rowBitmap;
            break;
        }
    }
    ASSERT(m_currentRowBitmap || m_bitmap.isEmpty());
    m_currentRowIndex = m_currentRowBitmap ? rowIndex : 0;

    size_t firstAtomInRow = m_currentRowIndex * atomsPerRow;
    m_currentMarkedBlockRowAddress = bitwise_cast<Atom*>(block->atomAt(firstAtomInRow));
    m_originalSize = bytes;
}

#else
// Linked List implementation.

void FreeList::initializeList(FreeCell* head, uintptr_t secret, unsigned bytes)
{
    // It's *slightly* more optimal to use a scrambled head. It saves a register on the fast path.
    m_scrambledHead = FreeCell::scramble(head, secret);
    m_secret = secret;
    m_payloadEnd = nullptr;
    m_remaining = 0;
    m_originalSize = bytes;
}

#endif // ENABLE(BITMAP_FREELIST)

void FreeList::initializeBump(char* payloadEnd, unsigned remaining)
{
#if ENABLE(BITMAP_FREELIST)
    m_currentRowBitmap = 0;
    m_currentRowIndex = 0;
#else
    m_scrambledHead = 0;
    m_secret = 0;
#endif
    m_payloadEnd = payloadEnd;
    m_remaining = remaining;
    m_originalSize = remaining;
}

bool FreeList::contains(HeapCell* target, MarkedBlock::Handle* currentBlock) const
{
    if (m_remaining) {
        const void* start = (m_payloadEnd - m_remaining);
        const void* end = m_payloadEnd;
        return (start <= target) && (target < end);
    }

#if ENABLE(BITMAP_FREELIST)
    if (bitmapIsEmpty())
        return false;

    // currentBlock may be null if the allocator has been reset (and therefore,
    // the FreeList cleared. Hence, we should only check this assertion after
    // we check if the FreeList bitmap is empty above.
    ASSERT(m_markedBlock == currentBlock);
    if (!currentBlock->contains(target))
        return false;

    unsigned atomNumber = currentBlock->block().atomNumber(target);
    unsigned rowIndex = atomNumber / atomsPerRow;
    if (rowIndex > m_currentRowIndex)
        return false;
    if (rowIndex == m_currentRowIndex) {
        constexpr AtomsBitmap::Word one = 1;
        unsigned firstAtomInRow = rowIndex * atomsPerRow;
        unsigned atomIndexInRow = atomNumber - firstAtomInRow;
        return m_currentRowBitmap & (one << atomIndexInRow);
    }
    return m_bitmap.get(atomNumber);

#else
    UNUSED_PARAM(currentBlock);
    FreeCell* candidate = head();
    while (candidate) {
        if (bitwise_cast<HeapCell*>(candidate) == target)
            return true;
        candidate = candidate->next(m_secret);
    }
    return false;
#endif
}

void FreeList::dump(PrintStream& out) const
{
#if ENABLE(BITMAP_FREELIST)
    if (m_remaining)
        out.print("{payloadEnd = ", RawPointer(m_payloadEnd), ", remaining = ", m_remaining, ", originalSize = ", m_originalSize, "}");
    else
        out.print("{currentRowBitmap = ", m_currentRowBitmap, ", currentRowIndex = ", m_currentRowIndex, ", originalSize = ", m_originalSize, "}");
#else
    out.print("{head = ", RawPointer(head()), ", secret = ", m_secret, ", payloadEnd = ", RawPointer(m_payloadEnd), ", remaining = ", m_remaining, ", originalSize = ", m_originalSize, "}");
#endif
}

} // namespace JSC

