/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#include "FreeList.h"
#include <wtf/MathExtras.h>

namespace JSC {

template<typename Func>
ALWAYS_INLINE HeapCell* FreeList::allocate(const Func& slowPath)
{
    unsigned remaining = m_remaining;
    if (remaining) {
        unsigned cellSize = m_cellSize;
        remaining -= cellSize;
        m_remaining = remaining;
        return bitwise_cast<HeapCell*>(m_payloadEnd - remaining - cellSize);
    }

#if ENABLE(BITMAP_FREELIST)
    AtomsBitmap::Word rowBitmap = m_currentRowBitmap;
    do {
        if (rowBitmap) {
            constexpr AtomsBitmap::Word one = 1;
            unsigned atomIndexInRow = ctz(rowBitmap);
            auto* cell = bitwise_cast<HeapCell*>(&m_currentMarkedBlockRowAddress[atomIndexInRow]);
            rowBitmap &= ~(one << atomIndexInRow);
            m_currentRowBitmap = rowBitmap;
            return cell;
        }

        unsigned rowIndex = m_currentRowIndex;
        auto* rowAddress = m_currentMarkedBlockRowAddress;
        while (rowIndex) {
            // We load before decrementing rowIndex because bitmapRows() points
            // to 1 word before m_bitmap. See comments about offsetOfBitmapRows()
            // for why we do this.
            rowBitmap = bitmapRows()[rowIndex--];
            rowAddress -= atomsPerRow;
            if (rowBitmap)
                break;
        }
        m_currentMarkedBlockRowAddress = rowAddress;
        m_currentRowIndex = rowIndex;
    } while (rowBitmap);

    m_currentRowBitmap = rowBitmap;
    ASSERT(bitmapIsEmpty());
    return slowPath();

#else // !ENABLE(BITMAP_FREELIST)
    FreeCell* result = head();
    if (UNLIKELY(!result))
        return slowPath();
    
    m_scrambledHead = result->scrambledNext;
    return bitwise_cast<HeapCell*>(result);
#endif // !ENABLE(BITMAP_FREELIST)
}

template<typename Func>
void FreeList::forEach(const Func& func) const
{
    if (m_remaining) {
        for (unsigned remaining = m_remaining; remaining; remaining -= m_cellSize)
            func(bitwise_cast<HeapCell*>(m_payloadEnd - remaining));
    } else {
#if ENABLE(BITMAP_FREELIST)
        if (bitmapIsEmpty())
            return;

        AtomsBitmap::Word rowBitmap = m_currentRowBitmap;
        unsigned rowIndex = m_currentRowIndex;
        Atom* currentMarkedBlockRowAddress = m_currentMarkedBlockRowAddress;
        do {
            while (rowBitmap) {
                constexpr AtomsBitmap::Word one = 1;
                unsigned atomIndexInRow = ctz(rowBitmap);
                auto* cell = bitwise_cast<HeapCell*>(&currentMarkedBlockRowAddress[atomIndexInRow]);
                rowBitmap &= ~(one << atomIndexInRow);
                func(cell);
            }

            while (rowIndex) {
                // We load before decrementing rowIndex because bitmapRows() points
                // to 1 word before m_bitmap. See comments about offsetOfBitmapRows()
                // for why we do this.
                rowBitmap = bitmapRows()[rowIndex--];
                currentMarkedBlockRowAddress -= atomsPerRow;
                if (rowBitmap)
                    break;
            }
        } while (rowBitmap);
#else
        for (FreeCell* cell = head(); cell;) {
            // We can use this to overwrite free objects before destroying the free list. So, we need
            // to get next before proceeding further.
            FreeCell* next = cell->next(m_secret);
            func(bitwise_cast<HeapCell*>(cell));
            cell = next;
        }
#endif
    }
}

} // namespace JSC
