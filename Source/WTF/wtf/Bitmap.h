/*
 *  Copyright (C) 2010-2020 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include <array>
#include <wtf/Atomics.h>
#include <wtf/HashFunctions.h>
#include <wtf/MathExtras.h>
#include <wtf/PrintStream.h>
#include <wtf/StdIntExtras.h>
#include <string.h>
#include <type_traits>

namespace WTF {

template<size_t size>
using BitmapWordType = std::conditional_t<(size <= 32 && sizeof(UCPURegister) > sizeof(uint32_t)), uint32_t, UCPURegister>;

template<size_t bitmapSize, typename PassedWordType = BitmapWordType<bitmapSize>>
class Bitmap final {
    WTF_MAKE_FAST_ALLOCATED;
    
public:
    using WordType = PassedWordType;

    static_assert(sizeof(WordType) <= sizeof(UCPURegister), "WordType must not be bigger than the CPU atomic word size");
    constexpr Bitmap();

    static constexpr size_t size()
    {
        return bitmapSize;
    }

    bool get(size_t, Dependency = Dependency()) const;
    void set(size_t);
    void set(size_t, bool);
    bool testAndSet(size_t);
    bool testAndClear(size_t);
    bool concurrentTestAndSet(size_t, Dependency = Dependency());
    bool concurrentTestAndClear(size_t, Dependency = Dependency());
    size_t nextPossiblyUnset(size_t) const;
    void clear(size_t);
    void clearAll();
    void invert();
    int64_t findRunOfZeros(size_t runLength) const;
    size_t count(size_t start = 0) const;
    bool isEmpty() const;
    bool isFull() const;
    
    void merge(const Bitmap&);
    void filter(const Bitmap&);
    void exclude(const Bitmap&);

    void concurrentFilter(const Bitmap&);
    
    bool subsumes(const Bitmap&) const;
    
    template<typename Func>
    void forEachSetBit(const Func&) const;
    
    size_t findBit(size_t startIndex, bool value) const;
    
    class iterator {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        iterator()
            : m_bitmap(nullptr)
            , m_index(0)
        {
        }
        
        iterator(const Bitmap& bitmap, size_t index)
            : m_bitmap(&bitmap)
            , m_index(index)
        {
        }
        
        size_t operator*() const { return m_index; }
        
        iterator& operator++()
        {
            m_index = m_bitmap->findBit(m_index + 1, true);
            return *this;
        }
        
        bool operator==(const iterator& other) const
        {
            return m_index == other.m_index;
        }
        
        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }

    private:
        const Bitmap* m_bitmap;
        size_t m_index;
    };
    
    // Use this to iterate over set bits.
    iterator begin() const { return iterator(*this, findBit(0, true)); }
    iterator end() const { return iterator(*this, bitmapSize); }
    
    void mergeAndClear(Bitmap&);
    void setAndClear(Bitmap&);

    void setEachNthBit(size_t n, size_t start = 0, size_t end = bitmapSize);

    bool operator==(const Bitmap&) const;
    bool operator!=(const Bitmap&) const;

    void operator|=(const Bitmap&);
    void operator&=(const Bitmap&);
    void operator^=(const Bitmap&);

    unsigned hash() const;

    void dump(PrintStream& out) const;

    WordType* storage() { return bits.data(); }
    const WordType* storage() const { return bits.data(); }

private:
    static constexpr unsigned wordSize = sizeof(WordType) * 8;
    static constexpr unsigned words = (bitmapSize + wordSize - 1) / wordSize;

    // the literal '1' is of type signed int.  We want to use an unsigned
    // version of the correct size when doing the calculations because if
    // WordType is larger than int, '1 << 31' will first be sign extended
    // and then casted to unsigned, meaning that set(31) when WordType is
    // a 64 bit unsigned int would give 0xffff8000
    static constexpr WordType one = 1;

    std::array<WordType, words> bits;
};

template<size_t bitmapSize, typename WordType>
constexpr Bitmap<bitmapSize, WordType>::Bitmap()
{
    clearAll();
}

template<size_t bitmapSize, typename WordType>
inline bool Bitmap<bitmapSize, WordType>::get(size_t n, Dependency dependency) const
{
    return !!(dependency.consume(this)->bits[n / wordSize] & (one << (n % wordSize)));
}

template<size_t bitmapSize, typename WordType>
inline void Bitmap<bitmapSize, WordType>::set(size_t n)
{
    bits[n / wordSize] |= (one << (n % wordSize));
}

template<size_t bitmapSize, typename WordType>
inline void Bitmap<bitmapSize, WordType>::set(size_t n, bool value)
{
    if (value)
        set(n);
    else
        clear(n);
}

template<size_t bitmapSize, typename WordType>
inline bool Bitmap<bitmapSize, WordType>::testAndSet(size_t n)
{
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
    bool result = bits[index] & mask;
    bits[index] |= mask;
    return result;
}

template<size_t bitmapSize, typename WordType>
inline bool Bitmap<bitmapSize, WordType>::testAndClear(size_t n)
{
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
    bool result = bits[index] & mask;
    bits[index] &= ~mask;
    return result;
}

template<size_t bitmapSize, typename WordType>
ALWAYS_INLINE bool Bitmap<bitmapSize, WordType>::concurrentTestAndSet(size_t n, Dependency dependency)
{
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
    WordType* data = dependency.consume(bits.data()) + index;
    return !bitwise_cast<Atomic<WordType>*>(data)->transactionRelaxed(
        [&] (WordType& value) -> bool {
            if (value & mask)
                return false;
            
            value |= mask;
            return true;
        });
}

template<size_t bitmapSize, typename WordType>
ALWAYS_INLINE bool Bitmap<bitmapSize, WordType>::concurrentTestAndClear(size_t n, Dependency dependency)
{
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
    WordType* data = dependency.consume(bits.data()) + index;
    return !bitwise_cast<Atomic<WordType>*>(data)->transactionRelaxed(
        [&] (WordType& value) -> bool {
            if (!(value & mask))
                return false;
            
            value &= ~mask;
            return true;
        });
}

template<size_t bitmapSize, typename WordType>
inline void Bitmap<bitmapSize, WordType>::clear(size_t n)
{
    bits[n / wordSize] &= ~(one << (n % wordSize));
}

template<size_t bitmapSize, typename WordType>
inline void Bitmap<bitmapSize, WordType>::clearAll()
{
    memset(bits.data(), 0, sizeof(bits));
}

template<size_t bitmapSize, typename WordType>
inline void Bitmap<bitmapSize, WordType>::invert()
{
    for (size_t i = 0; i < words; ++i)
        bits[i] = ~bits[i];
    if constexpr (!!(bitmapSize % wordSize)) {
        constexpr size_t remainingBits = bitmapSize % wordSize;
        constexpr WordType mask = (static_cast<WordType>(1) << remainingBits) - 1;
        bits[words - 1] &= mask;
    }
}

template<size_t bitmapSize, typename WordType>
inline size_t Bitmap<bitmapSize, WordType>::nextPossiblyUnset(size_t start) const
{
    if (!~bits[start / wordSize])
        return ((start / wordSize) + 1) * wordSize;
    return start + 1;
}

template<size_t bitmapSize, typename WordType>
inline int64_t Bitmap<bitmapSize, WordType>::findRunOfZeros(size_t runLength) const
{
    if (!runLength) 
        runLength = 1; 
     
    if (runLength > bitmapSize)
        return -1;

    for (size_t i = 0; i <= (bitmapSize - runLength) ; i++) {
        bool found = true; 
        for (size_t j = i; j <= (i + runLength - 1) ; j++) { 
            if (get(j)) {
                found = false; 
                break;
            }
        }
        if (found)  
            return i; 
    }
    return -1;
}

template<size_t bitmapSize, typename WordType>
inline size_t Bitmap<bitmapSize, WordType>::count(size_t start) const
{
    size_t result = 0;
    for ( ; (start % wordSize); ++start) {
        if (get(start))
            ++result;
    }
    for (size_t i = start / wordSize; i < words; ++i)
        result += WTF::bitCount(bits[i]);
    return result;
}

template<size_t bitmapSize, typename WordType>
inline bool Bitmap<bitmapSize, WordType>::isEmpty() const
{
    for (size_t i = 0; i < words; ++i)
        if (bits[i])
            return false;
    return true;
}

template<size_t bitmapSize, typename WordType>
inline bool Bitmap<bitmapSize, WordType>::isFull() const
{
    for (size_t i = 0; i < words; ++i)
        if (~bits[i]) {
            if constexpr (!!(bitmapSize % wordSize)) {
                if (i == words - 1) {
                    constexpr size_t remainingBits = bitmapSize % wordSize;
                    constexpr WordType mask = (static_cast<WordType>(1) << remainingBits) - 1;
                    if ((bits[i] & mask) == mask)
                        return true;
                }
            }
            return false;
        }
    return true;
}

template<size_t bitmapSize, typename WordType>
inline void Bitmap<bitmapSize, WordType>::merge(const Bitmap& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] |= other.bits[i];
}

template<size_t bitmapSize, typename WordType>
inline void Bitmap<bitmapSize, WordType>::filter(const Bitmap& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] &= other.bits[i];
}

template<size_t bitmapSize, typename WordType>
inline void Bitmap<bitmapSize, WordType>::exclude(const Bitmap& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] &= ~other.bits[i];
}

template<size_t bitmapSize, typename WordType>
inline void Bitmap<bitmapSize, WordType>::concurrentFilter(const Bitmap& other)
{
    for (size_t i = 0; i < words; ++i) {
        for (;;) {
            WordType otherBits = other.bits[i];
            if (!otherBits) {
                bits[i] = 0;
                break;
            }
            WordType oldBits = bits[i];
            WordType filteredBits = oldBits & otherBits;
            if (oldBits == filteredBits)
                break;
            if (atomicCompareExchangeWeakRelaxed(&bits[i], oldBits, filteredBits))
                break;
        }
    }
}

template<size_t bitmapSize, typename WordType>
inline bool Bitmap<bitmapSize, WordType>::subsumes(const Bitmap& other) const
{
    for (size_t i = 0; i < words; ++i) {
        WordType myBits = bits[i];
        WordType otherBits = other.bits[i];
        if ((myBits | otherBits) != myBits)
            return false;
    }
    return true;
}

template<size_t bitmapSize, typename WordType>
template<typename Func>
inline void Bitmap<bitmapSize, WordType>::forEachSetBit(const Func& func) const
{
    for (size_t i = 0; i < words; ++i) {
        WordType word = bits[i];
        if (!word)
            continue;
        size_t base = i * wordSize;
        for (size_t j = 0; j < wordSize; ++j) {
            if (word & 1)
                func(base + j);
            word >>= 1;
        }
    }
}

template<size_t bitmapSize, typename WordType>
inline size_t Bitmap<bitmapSize, WordType>::findBit(size_t startIndex, bool value) const
{
    WordType skipValue = -(static_cast<WordType>(value) ^ 1);
    size_t wordIndex = startIndex / wordSize;
    size_t startIndexInWord = startIndex - wordIndex * wordSize;
    
    while (wordIndex < words) {
        WordType word = bits[wordIndex];
        if (word != skipValue) {
            size_t index = startIndexInWord;
            if (findBitInWord(word, index, wordSize, value))
                return wordIndex * wordSize + index;
        }
        
        wordIndex++;
        startIndexInWord = 0;
    }
    
    return bitmapSize;
}

template<size_t bitmapSize, typename WordType>
inline void Bitmap<bitmapSize, WordType>::mergeAndClear(Bitmap& other)
{
    for (size_t i = 0; i < words; ++i) {
        bits[i] |= other.bits[i];
        other.bits[i] = 0;
    }
}

template<size_t bitmapSize, typename WordType>
inline void Bitmap<bitmapSize, WordType>::setAndClear(Bitmap& other)
{
    for (size_t i = 0; i < words; ++i) {
        bits[i] = other.bits[i];
        other.bits[i] = 0;
    }
}

template<size_t bitmapSize, typename WordType>
inline void Bitmap<bitmapSize, WordType>::setEachNthBit(size_t n, size_t start, size_t end)
{
    ASSERT(start <= end);
    ASSERT(end <= bitmapSize);

    size_t wordIndex = start / wordSize;
    size_t endWordIndex = end / wordSize;
    size_t index = start - wordIndex * wordSize;
    while (wordIndex < endWordIndex) {
        while (index < wordSize) {
            bits[wordIndex] |= (one << index);
            index += n;
        }
        index -= wordSize;
        wordIndex++;
    }

    size_t endIndex = end - endWordIndex * wordSize;
    while (index < endIndex) {
        bits[wordIndex] |= (one << index);
        index += n;
    }

    if constexpr (!!(bitmapSize % wordSize)) {
        constexpr size_t remainingBits = bitmapSize % wordSize;
        constexpr WordType mask = (static_cast<WordType>(1) << remainingBits) - 1;
        bits[words - 1] &= mask;
    }
}

template<size_t bitmapSize, typename WordType>
inline bool Bitmap<bitmapSize, WordType>::operator==(const Bitmap& other) const
{
    for (size_t i = 0; i < words; ++i) {
        if (bits[i] != other.bits[i])
            return false;
    }
    return true;
}

template<size_t bitmapSize, typename WordType>
inline bool Bitmap<bitmapSize, WordType>::operator!=(const Bitmap& other) const
{
    return !(*this == other);
}

template<size_t bitmapSize, typename WordType>
inline void Bitmap<bitmapSize, WordType>::operator|=(const Bitmap& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] |= other.bits[i];
}

template<size_t bitmapSize, typename WordType>
inline void Bitmap<bitmapSize, WordType>::operator&=(const Bitmap& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] &= other.bits[i];
}

template<size_t bitmapSize, typename WordType>
inline void Bitmap<bitmapSize, WordType>::operator^=(const Bitmap& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] ^= other.bits[i];
}

template<size_t bitmapSize, typename WordType>
inline unsigned Bitmap<bitmapSize, WordType>::hash() const
{
    unsigned result = 0;
    for (size_t i = 0; i < words; ++i)
        result ^= IntHash<WordType>::hash(bits[i]);
    return result;
}

template<size_t bitmapSize, typename WordType>
inline void Bitmap<bitmapSize, WordType>::dump(PrintStream& out) const
{
    for (size_t i = 0; i < size(); ++i)
        out.print(get(i) ? "1" : "-");
}

} // namespace WTF

using WTF::Bitmap;
