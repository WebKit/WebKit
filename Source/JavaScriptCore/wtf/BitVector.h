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

#include <algorithm>
#include <string.h>
#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

// This is a space-efficient, resizeable bitvector class. In the common case it
// occupies one word, but if necessary, it will inflate this one word to point
// to a single chunk of out-of-line allocated storage to store an arbitrary number
// of bits.
//
// - The bitvector needs to be resized manually (just call ensureSize()).
//
// - The bitvector remembers the bound of how many bits can be stored, but this
//   may be slightly greater (by as much as some platform-specific constant)
//   than the last argument passed to ensureSize().
//
// - Accesses ASSERT that you are within bounds.
//
// - Bits are not automatically initialized to zero.
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
        uintptr_t newBitsOrPointer;
        if (other.isInline())
            newBitsOrPointer = other.m_bitsOrPointer;
        else {
            OutOfLineBits* newOutOfLineBits = OutOfLineBits::create(other.size());
            memcpy(newOutOfLineBits->bits(), other.bits(), byteCount(other.size()));
            newBitsOrPointer = reinterpret_cast<uintptr_t>(newOutOfLineBits);
        }
        if (!isInline())
            OutOfLineBits::destroy(outOfLineBits());
        m_bitsOrPointer = newBitsOrPointer;
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
    void resize(size_t numBits)
    {
        if (isInline())
            return;
        
        if (numBits <= maxInlineBits()) {
            OutOfLineBits* myOutOfLineBits = outOfLineBits();
            m_bitsOrPointer = makeInlineBits(*myOutOfLineBits->bits());
            OutOfLineBits::destroy(myOutOfLineBits);
            return;
        }
        
        resizeOutOfLine(numBits);
    }
    
    void clearAll()
    {
        if (isInline())
            m_bitsOrPointer = makeInlineBits(0);
        else
            memset(outOfLineBits()->bits(), 0, byteCount(size()));
    }

    bool get(size_t bit) const
    {
        ASSERT(bit < size());
        return !!(bits()[bit >> bitsInPointer()] & (static_cast<uintptr_t>(1) << (bit & (bitsInPointer() - 1))));
    }
    
    void set(size_t bit)
    {
        ASSERT(bit < size());
        bits()[bit >> bitsInPointer()] |= (static_cast<uintptr_t>(1) << (bit & (bitsInPointer() - 1)));
    }
    
    void clear(size_t bit)
    {
        ASSERT(bit < size());
        bits()[bit >> bitsInPointer()] &= ~(static_cast<uintptr_t>(1) << (bit & (bitsInPointer() - 1)));
    }
    
    void set(size_t bit, bool value)
    {
        if (value)
            set(bit);
        else
            clear(bit);
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
    
    // This function relies on bitCount being a multiple of bitsInPointer()
    static size_t byteCount(size_t bitCount)
    {
        ASSERT(!(bitCount & (bitsInPointer() - 1)));
        return bitCount >> 3;
    }
    
    static uintptr_t makeInlineBits(uintptr_t bits)
    {
        ASSERT(!(bits & (static_cast<uintptr_t>(1) << maxInlineBits())));
        return bits | (static_cast<uintptr_t>(1) << maxInlineBits());
    }
    
    class OutOfLineBits {
    public:
        size_t numBits() const { return m_numBits; }
        size_t numWords() const { return (m_numBits + bitsInPointer() - 1) >> bitsInPointer(); }
        uintptr_t* bits() { return reinterpret_cast<uintptr_t*>(this + 1); }
        const uintptr_t* bits() const { return reinterpret_cast<const uintptr_t*>(this + 1); }
        
        static OutOfLineBits* create(size_t numBits)
        {
            numBits = (numBits + bitsInPointer() - 1) & ~bitsInPointer();
            return new (fastMalloc(sizeof(OutOfLineBits) + (numBits >> bitsInPointer()))) OutOfLineBits(numBits);
        }
        
        static void destroy(OutOfLineBits* outOfLineBits)
        {
            fastFree(outOfLineBits);
        }

    private:
        OutOfLineBits(size_t numBits)
            : m_numBits(numBits)
        {
        }
        
        size_t m_numBits;
    };
    
    bool isInline() const { return m_bitsOrPointer >> maxInlineBits(); }
    
    const OutOfLineBits* outOfLineBits() const { return reinterpret_cast<const OutOfLineBits*>(m_bitsOrPointer); }
    OutOfLineBits* outOfLineBits() { return reinterpret_cast<OutOfLineBits*>(m_bitsOrPointer); }
    
    void resizeOutOfLine(size_t numBits)
    {
        ASSERT(numBits > maxInlineBits());
        OutOfLineBits* newOutOfLineBits = OutOfLineBits::create(numBits);
        memcpy(newOutOfLineBits->bits(), bits(), byteCount(std::min(size(), numBits)));
        if (!isInline())
            OutOfLineBits::destroy(outOfLineBits());
        m_bitsOrPointer = reinterpret_cast<uintptr_t>(newOutOfLineBits);
    }
    
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

} // namespace WTF

using WTF::BitVector;

#endif // BitVector_h
