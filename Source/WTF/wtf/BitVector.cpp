/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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
#include <wtf/BitVector.h>

#include <algorithm>
#include <string.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SIMDHelpers.h>

namespace WTF {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER_AND_EXPORT(BitVector, WTF_INTERNAL);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(BitVector);

void BitVector::setSlow(const BitVector& other)
{
    uintptr_t newBitsOrPointer;
    if (other.isInline() || other.isEmptyOrDeletedValue())
        newBitsOrPointer = other.m_bitsOrPointer;
    else {
        OutOfLineBits* newOutOfLineBits = OutOfLineBits::create(other.size());
        memcpySpan(newOutOfLineBits->byteSpan(), other.byteSpan());
        newBitsOrPointer = std::bit_cast<uintptr_t>(newOutOfLineBits) >> 1;
    }
    if (!isInline() && !isEmptyOrDeletedValue())
        OutOfLineBits::destroy(outOfLineBits());
    m_bitsOrPointer = newBitsOrPointer;
}

void BitVector::resize(size_t numBits)
{
    if (numBits <= maxInlineBits()) {
        if (isInline())
            return;
    
        OutOfLineBits* myOutOfLineBits = outOfLineBits();
        m_bitsOrPointer = makeInlineBits(myOutOfLineBits->wordsSpan().front());
        OutOfLineBits::destroy(myOutOfLineBits);
        return;
    }
    
    resizeOutOfLine(numBits);
}

void BitVector::clearAll()
{
    if (isInline())
        m_bitsOrPointer = makeInlineBits(0);
    else
        zeroSpan(outOfLineBits()->byteSpan());
}

auto BitVector::OutOfLineBits::create(size_t numBits) -> OutOfLineBits*
{
    numBits = (numBits + bitsInPointer() - 1) & ~(static_cast<size_t>(bitsInPointer()) - 1);
    size_t size = sizeof(OutOfLineBits) + sizeof(uintptr_t) * (numBits / bitsInPointer());
    return new (NotNull, BitVectorMalloc::malloc(size)) OutOfLineBits(numBits);
}

void BitVector::OutOfLineBits::destroy(OutOfLineBits* outOfLineBits)
{
    BitVectorMalloc::free(outOfLineBits);
}

void BitVector::shiftRightByMultipleOf64(size_t shiftInBits)
{
    RELEASE_ASSERT(!(shiftInBits % 64));
    static_assert(!(8 % sizeof(void*)), "BitVector::shiftRightByMultipleOf64 assumes that word size is a divisor of 64");
    size_t shiftInWords = shiftInBits / (8 * sizeof(void*));
    size_t numBits = size() + shiftInBits;
    resizeOutOfLine(numBits, shiftInWords);
}

void BitVector::resizeOutOfLine(size_t numBits, size_t shiftInWords)
{
    ASSERT(numBits > maxInlineBits());
    OutOfLineBits* newOutOfLineBits = OutOfLineBits::create(numBits);
    auto newWords = newOutOfLineBits->wordsSpan();
    if (isInline()) {
        zeroSpan(newWords.first(shiftInWords));
        // Make sure that all of the bits are zero in case we do a no-op resize.
        newWords[shiftInWords] = m_bitsOrPointer & ~(static_cast<uintptr_t>(1) << maxInlineBits());
        zeroSpan(newWords.subspan(shiftInWords + 1));
    } else {
        auto oldWords = outOfLineBits()->wordsSpan();
        if (numBits > size()) {
            zeroSpan(newWords.first(shiftInWords));
            memcpySpan(newWords.subspan(shiftInWords), oldWords);
            zeroSpan(newWords.subspan(shiftInWords + oldWords.size()));
        } else
            memcpySpan(newWords, oldWords.first(newOutOfLineBits->numWords()));
        OutOfLineBits::destroy(outOfLineBits());
    }
    m_bitsOrPointer = std::bit_cast<uintptr_t>(newOutOfLineBits) >> 1;
}

void BitVector::mergeSlow(const BitVector& other)
{
    if (other.isInline()) {
        ASSERT(!isInline());
        outOfLineBits()->wordsSpan().front() |= cleanseInlineBits(other.m_bitsOrPointer);
        return;
    }
    
    ensureSize(other.size());
    ASSERT(!isInline());
    ASSERT(!other.isInline());
    
    auto a = outOfLineBits()->wordsSpan();
    auto b = other.outOfLineBits()->wordsSpan();
    for (size_t i = 0; i < a.size(); ++i)
        a[i] |= b[i];
}

void BitVector::filterSlow(const BitVector& other)
{
    if (other.isInline()) {
        ASSERT(!isInline());
        outOfLineBits()->wordsSpan().front() &= cleanseInlineBits(other.m_bitsOrPointer);
        return;
    }
    
    if (isInline()) {
        ASSERT(!other.isInline());
        m_bitsOrPointer &= other.outOfLineBits()->wordsSpan().front();
        m_bitsOrPointer |= (static_cast<uintptr_t>(1) << maxInlineBits());
        ASSERT(isInline());
        return;
    }
    
    auto a = outOfLineBits()->wordsSpan();
    auto b = other.outOfLineBits()->wordsSpan();
    auto commonSize = std::min(a.size(), b.size());
    for (size_t i = 0; i < commonSize; ++i)
        a[i] &= b[i];
    
    if (a.size() > b.size())
        zeroSpan(a.subspan(b.size()));
}

void BitVector::excludeSlow(const BitVector& other)
{
    if (other.isInline()) {
        ASSERT(!isInline());
        outOfLineBits()->wordsSpan().front() &= ~cleanseInlineBits(other.m_bitsOrPointer);
        return;
    }
    
    if (isInline()) {
        ASSERT(!other.isInline());
        m_bitsOrPointer &= ~other.outOfLineBits()->wordsSpan().front();
        m_bitsOrPointer |= (static_cast<uintptr_t>(1) << maxInlineBits());
        ASSERT(isInline());
        return;
    }
    
    auto a = outOfLineBits()->wordsSpan();
    auto b = other.outOfLineBits()->wordsSpan();
    auto commonSize = std::min(a.size(), b.size());
    for (size_t i = 0; i < commonSize; ++i)
        a[i] &= ~b[i];
}

size_t BitVector::bitCountSlow() const
{
    ASSERT(!isInline());
    const OutOfLineBits* bits = outOfLineBits();
    size_t result = 0;
    for (auto word : bits->wordsSpan())
        result += bitCount(word);
    return result;
}

bool BitVector::isEmptySlow() const
{
    ASSERT(!isInline());
    auto vectorMatch = [&](auto input) ALWAYS_INLINE_LAMBDA -> std::optional<uint8_t> {
        if (SIMD::isNonZero(input))
            return 0;
        return std::nullopt;
    };

    auto scalarMatch = [&](auto character) ALWAYS_INLINE_LAMBDA {
        return character;
    };

    using UnitType = std::conditional_t<sizeof(uintptr_t) == sizeof(uint32_t), uint32_t, uint64_t>;
    auto span = spanReinterpretCast<const UnitType>(outOfLineBits()->wordsSpan());
    return SIMD::find(span, vectorMatch, scalarMatch) == std::to_address(span.end());
}

bool BitVector::equalsSlowCase(const BitVector& other) const
{
    bool result = equalsSlowCaseFast(other);
    ASSERT(result == equalsSlowCaseSimple(other));
    return result;
}

bool BitVector::equalsSlowCaseFast(const BitVector& other) const
{
    if (isInline() != other.isInline())
        return equalsSlowCaseSimple(other);
    
    auto myWords = outOfLineBits()->wordsSpan();
    auto otherWords = other.outOfLineBits()->wordsSpan();
    
    size_t myNumWords = myWords.size();
    size_t otherNumWords = otherWords.size();
    
    std::span<const uintptr_t> extraBits;
    if (myNumWords < otherNumWords) {
        extraBits = otherWords.subspan(myNumWords);
        otherWords = otherWords.first(myNumWords);
    } else {
        extraBits = myWords.subspan(otherNumWords);
        myWords = myWords.first(otherNumWords);
    }
    
    if (std::ranges::find_if(extraBits, [](auto word) { return !!word; }) != extraBits.end())
        return false;
    
    return equalSpans(myWords, otherWords);
}

bool BitVector::equalsSlowCaseSimple(const BitVector& other) const
{
    // This is really cheesy, but probably good enough for now.
    for (unsigned i = std::max(size(), other.size()); i--;) {
        if (get(i) != other.get(i))
            return false;
    }
    return true;
}

uintptr_t BitVector::hashSlowCase() const
{
    ASSERT(!isInline());
    uintptr_t result = 0;
    for (auto word : outOfLineBits()->wordsSpan())
        result ^= word;
    return result;
}

void BitVector::dump(PrintStream& out) const
{
    for (size_t i = 0; i < size(); ++i)
        out.print(get(i) ? "1" : "-");
}

} // namespace WTF
