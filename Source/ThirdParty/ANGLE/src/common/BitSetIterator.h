//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BitSetIterator:
//   A helper class to quickly bitscan bitsets for set bits.
//

#ifndef COMMON_BITSETITERATOR_H_
#define COMMON_BITSETITERATOR_H_

#include <stdint.h>

#include <bitset>

#include "common/angleutils.h"
#include "common/debug.h"
#include "common/mathutil.h"
#include "common/platform.h"

namespace angle
{
template <size_t N>
class BitSetIterator final
{
  public:
    BitSetIterator(const std::bitset<N> &bitset);
    BitSetIterator(const BitSetIterator &other);
    BitSetIterator &operator=(const BitSetIterator &other);

    class Iterator final
    {
      public:
        Iterator(const std::bitset<N> &bits);
        Iterator &operator++();

        bool operator==(const Iterator &other) const;
        bool operator!=(const Iterator &other) const;
        unsigned long operator*() const { return mCurrentBit; }

      private:
        unsigned long getNextBit();

        static const size_t BitsPerWord = sizeof(unsigned long) * 8;
        std::bitset<N> mBits;
        unsigned long mCurrentBit;
        unsigned long mOffset;
    };

    Iterator begin() const { return Iterator(mBits); }
    Iterator end() const { return Iterator(std::bitset<N>(0)); }

  private:
    const std::bitset<N> mBits;
};

template <size_t N>
BitSetIterator<N>::BitSetIterator(const std::bitset<N> &bitset)
    : mBits(bitset)
{
}

template <size_t N>
BitSetIterator<N>::BitSetIterator(const BitSetIterator &other)
    : mBits(other.mBits)
{
}

template <size_t N>
BitSetIterator<N> &BitSetIterator<N>::operator=(const BitSetIterator &other)
{
    mBits = other.mBits;
    return *this;
}

template <size_t N>
BitSetIterator<N>::Iterator::Iterator(const std::bitset<N> &bits)
    : mBits(bits), mCurrentBit(0), mOffset(0)
{
    if (bits.any())
    {
        mCurrentBit = getNextBit();
    }
    else
    {
        mOffset = static_cast<unsigned long>(rx::roundUp(N, BitsPerWord));
    }
}

template <size_t N>
typename BitSetIterator<N>::Iterator &BitSetIterator<N>::Iterator::operator++()
{
    ASSERT(mBits.any());
    mBits.set(mCurrentBit - mOffset, 0);
    mCurrentBit = getNextBit();
    return *this;
}

inline unsigned long ScanForward(unsigned long bits)
{
    ASSERT(bits != 0);
#if defined(ANGLE_PLATFORM_WINDOWS)
    unsigned long firstBitIndex = 0ul;
    unsigned char ret = _BitScanForward(&firstBitIndex, bits);
    ASSERT(ret != 0);
    UNUSED_ASSERTION_VARIABLE(ret);
    return firstBitIndex;
#elif defined(ANGLE_PLATFORM_POSIX)
    return static_cast<unsigned long>(__builtin_ctzl(bits));
#else
#error Please implement bit-scan-forward for your platform!
#endif
}

template <size_t N>
bool BitSetIterator<N>::Iterator::operator==(const Iterator &other) const
{
    return mOffset == other.mOffset && mBits == other.mBits;
}

template <size_t N>
bool BitSetIterator<N>::Iterator::operator!=(const Iterator &other) const
{
    return !(*this == other);
}

template <size_t N>
unsigned long BitSetIterator<N>::Iterator::getNextBit()
{
    static std::bitset<N> wordMask(std::numeric_limits<unsigned long>::max());

    while (mOffset < N)
    {
        unsigned long wordBits = (mBits & wordMask).to_ulong();
        if (wordBits != 0ul)
        {
            return ScanForward(wordBits) + mOffset;
        }

        mBits >>= BitsPerWord;
        mOffset += BitsPerWord;
    }
    return 0;
}

// Helper to avoid needing to specify the template parameter size
template <size_t N>
BitSetIterator<N> IterateBitSet(const std::bitset<N> &bitset)
{
    return BitSetIterator<N>(bitset);
}

}  // angle

#endif  // COMMON_BITSETITERATOR_H_
