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

#include <wtf/Noncopyable.h>
#include <wtf/PrintStream.h>

namespace JSC {

class HeapCell;

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

class FreeList {
public:
    FreeList(unsigned cellSize);
    ~FreeList();
    
    void clear();
    
    JS_EXPORT_PRIVATE void initializeList(FreeCell* head, uintptr_t secret, unsigned bytes);
    JS_EXPORT_PRIVATE void initializeBump(char* payloadEnd, unsigned remaining);
    
    bool allocationWillFail() const { return !head() && !m_remaining; }
    bool allocationWillSucceed() const { return !allocationWillFail(); }
    
    template<typename Func>
    HeapCell* allocate(const Func& slowPath);
    
    bool contains(HeapCell*) const;
    
    template<typename Func>
    void forEach(const Func&) const;
    
    unsigned originalSize() const { return m_originalSize; }

    static ptrdiff_t offsetOfScrambledHead() { return OBJECT_OFFSETOF(FreeList, m_scrambledHead); }
    static ptrdiff_t offsetOfSecret() { return OBJECT_OFFSETOF(FreeList, m_secret); }
    static ptrdiff_t offsetOfPayloadEnd() { return OBJECT_OFFSETOF(FreeList, m_payloadEnd); }
    static ptrdiff_t offsetOfRemaining() { return OBJECT_OFFSETOF(FreeList, m_remaining); }
    static ptrdiff_t offsetOfOriginalSize() { return OBJECT_OFFSETOF(FreeList, m_originalSize); }
    static ptrdiff_t offsetOfCellSize() { return OBJECT_OFFSETOF(FreeList, m_cellSize); }
    
    JS_EXPORT_PRIVATE void dump(PrintStream&) const;

    unsigned cellSize() const { return m_cellSize; }
    
private:
    FreeCell* head() const { return FreeCell::descramble(m_scrambledHead, m_secret); }
    
    uintptr_t m_scrambledHead { 0 };
    uintptr_t m_secret { 0 };
    char* m_payloadEnd { nullptr };
    unsigned m_remaining { 0 };
    unsigned m_originalSize { 0 };
    unsigned m_cellSize { 0 };
};

} // namespace JSC

