/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/Atomics.h>
#include <wtf/BitVector.h>
#include <wtf/HashTraits.h>
#include <wtf/PrintStream.h>
#include <wtf/StdIntExtras.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

class FixedBitVector final {
    WTF_MAKE_FAST_ALLOCATED;
    using WordType = decltype(BitVector::m_bitsOrPointer);

public:
    FixedBitVector() = default;

    FixedBitVector(size_t size)
        : m_bitVector(size)
    {
    }

    bool concurrentTestAndSet(size_t bitIndex, Dependency = Dependency());
    bool concurrentTestAndClear(size_t bitIndex, Dependency = Dependency());

    bool testAndSet(size_t bitIndex);
    bool testAndClear(size_t bitIndex);
    bool test(size_t bitIndex);

    // Note that BitVector will be in inline mode with fixed size when
    // the BitVector is constructed with size less or equal to `maxInlineBits`.
    size_t size() const { return m_bitVector.size(); }

    bool isEmpty() const { return m_bitVector.isEmpty(); }

    size_t findBit(size_t startIndex, bool value) const;

    friend bool operator==(const FixedBitVector&, const FixedBitVector&) = default;

    unsigned hash() const;

    void dump(PrintStream& out) const;

    BitVector::iterator begin() const { return m_bitVector.begin(); }
    BitVector::iterator end() const { return m_bitVector.end(); }

private:
    static constexpr unsigned wordSize = sizeof(WordType) * 8;
    static constexpr WordType one = 1;

    BitVector m_bitVector;
};

ALWAYS_INLINE bool FixedBitVector::concurrentTestAndSet(size_t bitIndex, Dependency dependency)
{
    if (UNLIKELY(bitIndex >= size()))
        return false;

    WordType mask = one << (bitIndex % wordSize);
    size_t wordIndex = bitIndex / wordSize;
    WordType* data = dependency.consume(m_bitVector.bits()) + wordIndex;
    return !bitwise_cast<Atomic<WordType>*>(data)->transactionRelaxed(
        [&](WordType& value) -> bool {
            if (value & mask)
                return false;

            value |= mask;
            return true;
        });
}

ALWAYS_INLINE bool FixedBitVector::concurrentTestAndClear(size_t bitIndex, Dependency dependency)
{
    if (UNLIKELY(bitIndex >= size()))
        return false;

    WordType mask = one << (bitIndex % wordSize);
    size_t wordIndex = bitIndex / wordSize;
    WordType* data = dependency.consume(m_bitVector.bits()) + wordIndex;
    return bitwise_cast<Atomic<WordType>*>(data)->transactionRelaxed(
        [&](WordType& value) -> bool {
            if (!(value & mask))
                return false;

            value &= ~mask;
            return true;
        });
}

ALWAYS_INLINE bool FixedBitVector::testAndSet(size_t bitIndex)
{
    if (UNLIKELY(bitIndex >= size()))
        return false;

    WordType mask = one << (bitIndex % wordSize);
    size_t wordIndex = bitIndex / wordSize;
    WordType* bits = m_bitVector.bits();
    bool previousValue = bits[wordIndex] & mask;
    bits[wordIndex] |= mask;
    return previousValue;
}

ALWAYS_INLINE bool FixedBitVector::testAndClear(size_t bitIndex)
{
    if (UNLIKELY(bitIndex >= size()))
        return false;

    WordType mask = one << (bitIndex % wordSize);
    size_t wordIndex = bitIndex / wordSize;
    WordType* bits = m_bitVector.bits();
    bool previousValue = bits[wordIndex] & mask;
    bits[wordIndex] &= ~mask;
    return previousValue;
}

ALWAYS_INLINE bool FixedBitVector::test(size_t bitIndex)
{
    if (UNLIKELY(bitIndex >= size()))
        return false;

    WordType mask = one << (bitIndex % wordSize);
    size_t wordIndex = bitIndex / wordSize;
    return m_bitVector.bits()[wordIndex] & mask;
}

ALWAYS_INLINE size_t FixedBitVector::findBit(size_t startIndex, bool value) const
{
    return m_bitVector.findBit(startIndex, value);
}

ALWAYS_INLINE unsigned FixedBitVector::hash() const
{
    return m_bitVector.hash();
}

ALWAYS_INLINE void FixedBitVector::dump(PrintStream& out) const
{
    m_bitVector.dump(out);
}

struct FixedBitVectorHash {
    static unsigned hash(const FixedBitVector& vector) { return vector.hash(); }
    static bool equal(const FixedBitVector& a, const FixedBitVector& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

template<typename T> struct DefaultHash;
template<> struct DefaultHash<FixedBitVector> : FixedBitVectorHash {
};

template<> struct HashTraits<FixedBitVector> : public CustomHashTraits<FixedBitVector> {
};

} // namespace WTF

using WTF::FixedBitVector;
