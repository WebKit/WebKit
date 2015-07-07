/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef BitVector_h
#define BitVector_h

#include <stdio.h>
#include <wtf/Assertions.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/PrintStream.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

// This is a space-efficient, resizeable bitvector class. In the common case it
// occupies one word, but if necessary, it will inflate this one word to point
// to a single chunk of out-of-line allocated storage to store an arbitrary number
// of bits.
//
// - The bitvector remembers the bound of how many bits can be stored, but this
//   may be slightly greater (by as much as some platform-specific constant)
//   than the last argument passed to ensureSize().
//
// - The bitvector can resize itself automatically (set, clear, get) or can be used
//   in a manual mode, which is faster (quickSet, quickClear, quickGet, ensureSize).
//
// - Accesses ASSERT that you are within bounds.
//
// - Bits are automatically initialized to zero.
//
// On the other hand, this BitVector class may not be the fastest around, since
// it does conditionals on every get/set/clear. But it is great if you need to
// juggle a lot of variable-length BitVectors and you're worried about wasting
// space.

class BitVector {
public: 
    BitVector()
        : m_bitsOrPointer(makeInlineBits(0))
    {
    }
    
    explicit BitVector(size_t numBits)
        : m_bitsOrPointer(makeInlineBits(0))
    {
        ensureSize(numBits);
    }
    
    BitVector(const BitVector& other)
        : m_bitsOrPointer(makeInlineBits(0))
    {
        (*this) = other;
    }

    
    ~BitVector()
    {
        if (isInline())
            return;
        OutOfLineBits::destroy(outOfLineBits());
    }
    
    BitVector& operator=(const BitVector& other)
    {
        if (isInline() && other.isInline())
            m_bitsOrPointer = other.m_bitsOrPointer;
        else
            setSlow(other);
        return *this;
    }

    size_t size() const
    {
        if (isInline())
            return maxInlineBits();
        return outOfLineBits()->numBits();
    }

    void ensureSize(size_t numBits)
    {
        if (numBits <= size())
            return;
        resizeOutOfLine(numBits);
    }
    
    // Like ensureSize(), but supports reducing the size of the bitvector.
    WTF_EXPORT_PRIVATE void resize(size_t numBits);
    
    WTF_EXPORT_PRIVATE void clearAll();

    bool quickGet(size_t bit) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(bit < size());
        return !!(bits()[bit / bitsInPointer()] & (static_cast<uintptr_t>(1) << (bit & (bitsInPointer() - 1))));
    }
    
    void quickSet(size_t bit)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(bit < size());
        bits()[bit / bitsInPointer()] |= (static_cast<uintptr_t>(1) << (bit & (bitsInPointer() - 1)));
    }
    
    void quickClear(size_t bit)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(bit < size());
        bits()[bit / bitsInPointer()] &= ~(static_cast<uintptr_t>(1) << (bit & (bitsInPointer() - 1)));
    }
    
    void quickSet(size_t bit, bool value)
    {
        if (value)
            quickSet(bit);
        else
            quickClear(bit);
    }
    
    bool get(size_t bit) const
    {
        if (bit >= size())
            return false;
        return quickGet(bit);
    }
    
    void set(size_t bit)
    {
        ensureSize(bit + 1);
        quickSet(bit);
    }

    void ensureSizeAndSet(size_t bit, size_t size)
    {
        ensureSize(size);
        quickSet(bit);
    }

    void clear(size_t bit)
    {
        if (bit >= size())
            return;
        quickClear(bit);
    }
    
    void set(size_t bit, bool value)
    {
        if (value)
            set(bit);
        else
            clear(bit);
    }
    
    void merge(const BitVector& other)
    {
        if (!isInline() || !other.isInline()) {
            mergeSlow(other);
            return;
        }
        m_bitsOrPointer |= other.m_bitsOrPointer;
        ASSERT(isInline());
    }
    
    void filter(const BitVector& other)
    {
        if (!isInline() || !other.isInline()) {
            filterSlow(other);
            return;
        }
        m_bitsOrPointer &= other.m_bitsOrPointer;
        ASSERT(isInline());
    }
    
    void exclude(const BitVector& other)
    {
        if (!isInline() || !other.isInline()) {
            excludeSlow(other);
            return;
        }
        m_bitsOrPointer &= ~other.m_bitsOrPointer;
        m_bitsOrPointer |= (static_cast<uintptr_t>(1) << maxInlineBits());
        ASSERT(isInline());
    }
    
    size_t bitCount() const
    {
        if (isInline())
            return bitCount(cleanseInlineBits(m_bitsOrPointer));
        return bitCountSlow();
    }
    
    WTF_EXPORT_PRIVATE void dump(PrintStream& out) const;
    
    enum EmptyValueTag { EmptyValue };
    enum DeletedValueTag { DeletedValue };
    
    BitVector(EmptyValueTag)
        : m_bitsOrPointer(0)
    {
    }
    
    BitVector(DeletedValueTag)
        : m_bitsOrPointer(1)
    {
    }
    
    bool isEmptyValue() const { return !m_bitsOrPointer; }
    bool isDeletedValue() const { return m_bitsOrPointer == 1; }
    
    bool isEmptyOrDeletedValue() const { return m_bitsOrPointer <= 1; }
    
    bool operator==(const BitVector& other) const
    {
        if (isInline() && other.isInline())
            return m_bitsOrPointer == other.m_bitsOrPointer;
        return equalsSlowCase(other);
    }
    
    unsigned hash() const
    {
        // This is a very simple hash. Just xor together the words that hold the various
        // bits and then compute the hash. This makes it very easy to deal with bitvectors
        // that have a lot of trailing zero's.
        uintptr_t value;
        if (isInline())
            value = cleanseInlineBits(m_bitsOrPointer);
        else
            value = hashSlowCase();
        return IntHash<uintptr_t>::hash(value);
    }
    
private:
    static unsigned bitsInPointer()
    {
        return sizeof(void*) << 3;
    }

    static unsigned maxInlineBits()
    {
        return bitsInPointer() - 1;
    }

    static size_t byteCount(size_t bitCount)
    {
        return (bitCount + 7) >> 3;
    }

    static uintptr_t makeInlineBits(uintptr_t bits)
    {
        ASSERT(!(bits & (static_cast<uintptr_t>(1) << maxInlineBits())));
        return bits | (static_cast<uintptr_t>(1) << maxInlineBits());
    }
    
    static uintptr_t cleanseInlineBits(uintptr_t bits)
    {
        return bits & ~(static_cast<uintptr_t>(1) << maxInlineBits());
    }
    
    static size_t bitCount(uintptr_t bits)
    {
        if (sizeof(uintptr_t) == 4)
            return WTF::bitCount(static_cast<unsigned>(bits));
        return WTF::bitCount(static_cast<uint64_t>(bits));
    }
    
    class OutOfLineBits {
    public:
        size_t numBits() const { return m_numBits; }
        size_t numWords() const { return (m_numBits + bitsInPointer() - 1) / bitsInPointer(); }
        uintptr_t* bits() { return bitwise_cast<uintptr_t*>(this + 1); }
        const uintptr_t* bits() const { return bitwise_cast<const uintptr_t*>(this + 1); }
        
        static WTF_EXPORT_PRIVATE OutOfLineBits* create(size_t numBits);
        
        static WTF_EXPORT_PRIVATE void destroy(OutOfLineBits*);

    private:
        OutOfLineBits(size_t numBits)
            : m_numBits(numBits)
        {
        }
        
        size_t m_numBits;
    };
    
    bool isInline() const { return m_bitsOrPointer >> maxInlineBits(); }
    
    const OutOfLineBits* outOfLineBits() const { return bitwise_cast<const OutOfLineBits*>(m_bitsOrPointer << 1); }
    OutOfLineBits* outOfLineBits() { return bitwise_cast<OutOfLineBits*>(m_bitsOrPointer << 1); }
    
    WTF_EXPORT_PRIVATE void resizeOutOfLine(size_t numBits);
    WTF_EXPORT_PRIVATE void setSlow(const BitVector& other);
    
    WTF_EXPORT_PRIVATE void mergeSlow(const BitVector& other);
    WTF_EXPORT_PRIVATE void filterSlow(const BitVector& other);
    WTF_EXPORT_PRIVATE void excludeSlow(const BitVector& other);
    
    WTF_EXPORT_PRIVATE size_t bitCountSlow() const;
    
    WTF_EXPORT_PRIVATE bool equalsSlowCase(const BitVector& other) const;
    WTF_EXPORT_PRIVATE uintptr_t hashSlowCase() const;
    
    uintptr_t* bits()
    {
        if (isInline())
            return &m_bitsOrPointer;
        return outOfLineBits()->bits();
    }
    
    const uintptr_t* bits() const
    {
        if (isInline())
            return &m_bitsOrPointer;
        return outOfLineBits()->bits();
    }
    
    uintptr_t m_bitsOrPointer;
};

struct BitVectorHash {
    static unsigned hash(const BitVector& vector) { return vector.hash(); }
    static bool equal(const BitVector& a, const BitVector& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

template<typename T> struct DefaultHash;
template<> struct DefaultHash<BitVector> {
    typedef BitVectorHash Hash;
};

template<typename T> struct HashTraits;
template<> struct HashTraits<BitVector> : public CustomHashTraits<BitVector> { };

} // namespace WTF

using WTF::BitVector;

#endif // BitVector_h
