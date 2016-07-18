/*
 *  Copyright (C) 2010, 2016 Apple Inc. All rights reserved.
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
#ifndef Bitmap_h
#define Bitmap_h

#include <array>
#include <wtf/Atomics.h>
#include <wtf/StdLibExtras.h>
#include <stdint.h>
#include <string.h>

namespace WTF {

enum BitmapAtomicMode {
    // This makes concurrentTestAndSet behave just like testAndSet.
    BitmapNotAtomic,

    // This makes concurrentTestAndSet use compareAndSwap, so that it's
    // atomic even when used concurrently.
    BitmapAtomic
};

template<size_t bitmapSize, BitmapAtomicMode atomicMode = BitmapNotAtomic, typename WordType = uint32_t>
class Bitmap {
    WTF_MAKE_FAST_ALLOCATED;
    
    static_assert(sizeof(WordType) <= sizeof(unsigned), "WordType must not be bigger than unsigned");
public:
    Bitmap();

    static constexpr size_t size()
    {
        return bitmapSize;
    }

    bool get(size_t) const;
    void set(size_t);
    void set(size_t, bool);
    bool testAndSet(size_t);
    bool testAndClear(size_t);
    bool concurrentTestAndSet(size_t);
    bool concurrentTestAndClear(size_t);
    size_t nextPossiblyUnset(size_t) const;
    void clear(size_t);
    void clearAll();
    int64_t findRunOfZeros(size_t) const;
    size_t count(size_t = 0) const;
    size_t isEmpty() const;
    size_t isFull() const;
    
    void merge(const Bitmap&);
    void filter(const Bitmap&);
    void exclude(const Bitmap&);
    
    template<typename Func>
    void forEachSetBit(const Func&) const;
    
    bool operator==(const Bitmap&) const;
    bool operator!=(const Bitmap&) const;
    
    unsigned hash() const;

private:
    static const unsigned wordSize = sizeof(WordType) * 8;
    static const unsigned words = (bitmapSize + wordSize - 1) / wordSize;

    // the literal '1' is of type signed int.  We want to use an unsigned
    // version of the correct size when doing the calculations because if
    // WordType is larger than int, '1 << 31' will first be sign extended
    // and then casted to unsigned, meaning that set(31) when WordType is
    // a 64 bit unsigned int would give 0xffff8000
    static const WordType one = 1;

    std::array<WordType, words> bits;
};

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline Bitmap<bitmapSize, atomicMode, WordType>::Bitmap()
{
    clearAll();
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline bool Bitmap<bitmapSize, atomicMode, WordType>::get(size_t n) const
{
    return !!(bits[n / wordSize] & (one << (n % wordSize)));
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline void Bitmap<bitmapSize, atomicMode, WordType>::set(size_t n)
{
    bits[n / wordSize] |= (one << (n % wordSize));
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline void Bitmap<bitmapSize, atomicMode, WordType>::set(size_t n, bool value)
{
    if (value)
        set(n);
    else
        clear(n);
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline bool Bitmap<bitmapSize, atomicMode, WordType>::testAndSet(size_t n)
{
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
    bool result = bits[index] & mask;
    bits[index] |= mask;
    return result;
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline bool Bitmap<bitmapSize, atomicMode, WordType>::testAndClear(size_t n)
{
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
    bool result = bits[index] & mask;
    bits[index] &= ~mask;
    return result;
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline bool Bitmap<bitmapSize, atomicMode, WordType>::concurrentTestAndSet(size_t n)
{
    if (atomicMode == BitmapNotAtomic)
        return testAndSet(n);

    ASSERT(atomicMode == BitmapAtomic);
    
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
    WordType* wordPtr = bits.data() + index;
    WordType oldValue;
    do {
        oldValue = *wordPtr;
        if (oldValue & mask)
            return true;
    } while (!weakCompareAndSwap(wordPtr, oldValue, static_cast<WordType>(oldValue | mask)));
    return false;
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline bool Bitmap<bitmapSize, atomicMode, WordType>::concurrentTestAndClear(size_t n)
{
    if (atomicMode == BitmapNotAtomic)
        return testAndClear(n);

    ASSERT(atomicMode == BitmapAtomic);
    
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
    WordType* wordPtr = bits.data() + index;
    WordType oldValue;
    do {
        oldValue = *wordPtr;
        if (!(oldValue & mask))
            return false;
    } while (!weakCompareAndSwap(wordPtr, oldValue, static_cast<WordType>(oldValue & ~mask)));
    return true;
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline void Bitmap<bitmapSize, atomicMode, WordType>::clear(size_t n)
{
    bits[n / wordSize] &= ~(one << (n % wordSize));
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline void Bitmap<bitmapSize, atomicMode, WordType>::clearAll()
{
    memset(bits.data(), 0, sizeof(bits));
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline size_t Bitmap<bitmapSize, atomicMode, WordType>::nextPossiblyUnset(size_t start) const
{
    if (!~bits[start / wordSize])
        return ((start / wordSize) + 1) * wordSize;
    return start + 1;
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline int64_t Bitmap<bitmapSize, atomicMode, WordType>::findRunOfZeros(size_t runLength) const
{
    if (!runLength) 
        runLength = 1; 
     
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

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline size_t Bitmap<bitmapSize, atomicMode, WordType>::count(size_t start) const
{
    size_t result = 0;
    for ( ; (start % wordSize); ++start) {
        if (get(start))
            ++result;
    }
    for (size_t i = start / wordSize; i < words; ++i)
        result += WTF::bitCount(static_cast<unsigned>(bits[i]));
    return result;
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline size_t Bitmap<bitmapSize, atomicMode, WordType>::isEmpty() const
{
    for (size_t i = 0; i < words; ++i)
        if (bits[i])
            return false;
    return true;
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline size_t Bitmap<bitmapSize, atomicMode, WordType>::isFull() const
{
    for (size_t i = 0; i < words; ++i)
        if (~bits[i])
            return false;
    return true;
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline void Bitmap<bitmapSize, atomicMode, WordType>::merge(const Bitmap& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] |= other.bits[i];
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline void Bitmap<bitmapSize, atomicMode, WordType>::filter(const Bitmap& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] &= other.bits[i];
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline void Bitmap<bitmapSize, atomicMode, WordType>::exclude(const Bitmap& other)
{
    for (size_t i = 0; i < words; ++i)
        bits[i] &= ~other.bits[i];
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
template<typename Func>
inline void Bitmap<bitmapSize, atomicMode, WordType>::forEachSetBit(const Func& func) const
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

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline bool Bitmap<bitmapSize, atomicMode, WordType>::operator==(const Bitmap& other) const
{
    for (size_t i = 0; i < words; ++i) {
        if (bits[i] != other.bits[i])
            return false;
    }
    return true;
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline bool Bitmap<bitmapSize, atomicMode, WordType>::operator!=(const Bitmap& other) const
{
    return !(*this == other);
}

template<size_t bitmapSize, BitmapAtomicMode atomicMode, typename WordType>
inline unsigned Bitmap<bitmapSize, atomicMode, WordType>::hash() const
{
    unsigned result = 0;
    for (size_t i = 0; i < words; ++i)
        result ^= IntHash<WordType>::hash(bits[i]);
    return result;
}

} // namespace WTF

using WTF::Bitmap;

#endif
