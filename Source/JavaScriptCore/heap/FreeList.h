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

#pragma once

#include "MarkedBlock.h"
#include <wtf/Noncopyable.h>
#include <wtf/PrintStream.h>
#include <wtf/StdIntExtras.h>

namespace JSC {

class HeapCell;

#if CPU(X86_64)
#define ENABLE_BITMAP_FREELIST 1
#else
#define ENABLE_BITMAP_FREELIST 0
#endif

#if !ENABLE(BITMAP_FREELIST)
struct FreeCell {
    static uintptr_t scramble(FreeCell* cell, uintptr_t secret)
    {
        return bitwise_cast<uintptr_t>(cell) ^ secret;
    }
    
    static FreeCell* descramble(uintptr_t cell, uintptr_t secret)
    {
        return bitwise_cast<FreeCell*>(cell ^ secret);
    }
    
    void setNext(FreeCell* next, uintptr_t secret)
    {
        scrambledNext = scramble(next, secret);
    }
    
    FreeCell* next(uintptr_t secret) const
    {
        return descramble(scrambledNext, secret);
    }
    
    static ptrdiff_t offsetOfScrambledNext() { return OBJECT_OFFSETOF(FreeCell, scrambledNext); }

    uint64_t preservedBitsForCrashAnalysis;
    uintptr_t scrambledNext;
};
#endif

class FreeList {
public:
    FreeList(unsigned cellSize);
    ~FreeList();
    
    void clear();
    
    JS_EXPORT_PRIVATE void initializeBump(char* payloadEnd, unsigned remaining);
    
    bool allocationWillSucceed() const { return !allocationWillFail(); }
    
    template<typename Func>
    HeapCell* allocate(const Func& slowPath);
    
    bool contains(HeapCell*, MarkedBlock::Handle* currentBlock) const;
    
    template<typename Func>
    void forEach(const Func&) const;
    
    unsigned originalSize() const { return m_originalSize; }

#if ENABLE(BITMAP_FREELIST)
    using Atom = MarkedBlock::Atom;
    using AtomsBitmap = MarkedBlock::AtomsBitmap;

    static constexpr size_t atomsPerRow = AtomsBitmap::bitsInWord;
    static constexpr size_t atomsRowBytes = atomsPerRow * sizeof(Atom);
    static constexpr unsigned atomSizeShift = WTF::log2Constexpr(sizeof(Atom));
    static_assert((static_cast<size_t>(1) << atomSizeShift) == sizeof(Atom));

    JS_EXPORT_PRIVATE void initializeAtomsBitmap(MarkedBlock::Handle*, AtomsBitmap& freeAtoms, unsigned bytes);

    bool bitmapIsEmpty() const
    {
        // Remember, we don't actually clear the bits in m_bitmap as we allocate
        // the atoms. Instead, m_currentRowBitmap and m_currentRowIndex tells us
        // if there atoms still available for allocation. See comment blob below
        // at the declaration of m_currentRowIndex for more details.
        return !m_currentRowBitmap && !m_currentRowIndex;
    }
    bool allocationWillFail() const { return bitmapIsEmpty() && !m_remaining; }

    static ptrdiff_t offsetOfCurrentRowBitmap() { return OBJECT_OFFSETOF(FreeList, m_currentRowBitmap); }

    // We're deliberately returning the address of 1 word before m_bitmap so that
    // we can schedule instructions better i.e. to do a load before decrementing the
    // row index.
    static ptrdiff_t offsetOfBitmapRows() { return OBJECT_OFFSETOF(FreeList, m_bitmap) - sizeof(AtomsBitmap::Word); }

    static ptrdiff_t offsetOfCurrentRowIndex() { return OBJECT_OFFSETOF(FreeList, m_currentRowIndex); }
    static ptrdiff_t offsetOfCurrentMarkedBlockRowAddress() { return OBJECT_OFFSETOF(FreeList, m_currentMarkedBlockRowAddress); }
#else
    JS_EXPORT_PRIVATE void initializeList(FreeCell* head, uintptr_t secret, unsigned bytes);

    bool allocationWillFail() const { return !head() && !m_remaining; }

    static ptrdiff_t offsetOfScrambledHead() { return OBJECT_OFFSETOF(FreeList, m_scrambledHead); }
    static ptrdiff_t offsetOfSecret() { return OBJECT_OFFSETOF(FreeList, m_secret); }
#endif

    static ptrdiff_t offsetOfPayloadEnd() { return OBJECT_OFFSETOF(FreeList, m_payloadEnd); }
    static ptrdiff_t offsetOfRemaining() { return OBJECT_OFFSETOF(FreeList, m_remaining); }
    static ptrdiff_t offsetOfCellSize() { return OBJECT_OFFSETOF(FreeList, m_cellSize); }
    
    JS_EXPORT_PRIVATE void dump(PrintStream&) const;

    unsigned cellSize() const { return m_cellSize; }
    
private:

#if ENABLE(BITMAP_FREELIST)
    AtomsBitmap& atomsBitmap() { return m_bitmap; }
    AtomsBitmap::Word* bitmapRows() const
    {
        // See comment about offsetOfBitmapRows().
        return bitwise_cast<AtomsBitmap::Word*>(&m_bitmap) - 1;
    }

    // This allocation algorithm thinks of the MarkedBlock as consisting of rows
    // of atoms, where the number of atoms in a row equals the number of bits in
    // a AtomsBitmap::Word. On 64-bit CPUs, this would be 64.
    //
    // We will start allocating from the last (highest numbered) row down to the
    // first (row 0). As we allocate, we will only update m_currentRowIndex and
    // m_currentRowBitmap. m_bitmap will not be updated. This is so in oder to
    // reduce the number of instructions executed during an allocation.
    //
    // When m_currentRowIndex points to N, the AtomsBitmap::Word for row N in
    // m_bitmap will have been copied into m_currentRowBitmap. This is the row
    // that we will be allocating from until the row is exhausted.
    //
    // This is how we know whether an atom is available for allocation or not:
    // 1. Atoms in any rows above m_currentRowIndex are guaranteed to be
    //    allocated already (because we allocate downwards), and hence, are not
    //    available.
    // 2. For row m_currentRowIndex, m_currentRowBitmap is the source of truth
    //    on which atoms in the row are available for allocation.
    // 3. For rows below m_currentRowIndex, m_bitmap is the source of truth on
    //    which atoms are available for allocation.
    //
    // When m_currentRowIndex reaches 0, the info in m_bitmap is completely
    // obsoleted, and m_currentRowBitmap holds the availability info for row 0.
    // When both m_currentRowIndex and m_currentRowBitmap are 0, then we have
    // completely exhausted the block and no more atoms are available for
    // allocation.

    AtomsBitmap::Word m_currentRowBitmap { 0 };
    unsigned m_currentRowIndex { 0 };
    unsigned m_originalSize { 0 };

#else
    FreeCell* head() const { return FreeCell::descramble(m_scrambledHead, m_secret); }

    uintptr_t m_scrambledHead { 0 };
    uintptr_t m_secret { 0 };
#endif

    union {
        char* m_payloadEnd { nullptr };
#if ENABLE(BITMAP_FREELIST)
        Atom* m_currentMarkedBlockRowAddress;
#endif
    };
    unsigned m_remaining { 0 };
    unsigned m_cellSize { 0 };

#if ENABLE(BITMAP_FREELIST)
    AtomsBitmap m_bitmap;
#else
    unsigned m_originalSize { 0 };
#endif

#if ASSERT_ENABLED
    MarkedBlock::Handle* m_markedBlock { nullptr };
#endif

    friend class MarkedBlock;
};

} // namespace JSC

