//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// bitset_utils:
//   Bitset-related helper classes, such as a fast iterator to scan for set bits.
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

template <size_t N, typename BitsT>
class BitSetT final
{
  public:
    class Reference final
    {
      public:
        ~Reference() {}
        Reference &operator=(bool x)
        {
            mParent->set(mBit, x);
            return *this;
        }
        explicit operator bool() const { return mParent->test(mBit); }

      private:
        friend class BitSetT;

        Reference(BitSetT *parent, std::size_t bit) : mParent(parent), mBit(bit) {}

        BitSetT *mParent;
        std::size_t mBit;
    };

    class Iterator final
    {
      public:
        Iterator(const BitSetT &bits);
        Iterator &operator++();

        bool operator==(const Iterator &other) const;
        bool operator!=(const Iterator &other) const;
        std::size_t operator*() const;

      private:
        std::size_t getNextBit();

        BitSetT mBitsCopy;
        std::size_t mCurrentBit;
    };

    BitSetT();
    BitSetT(BitsT value);
    ~BitSetT();

    BitSetT(const BitSetT &other);
    BitSetT &operator=(const BitSetT &other);

    bool operator==(const BitSetT &other) const;
    bool operator!=(const BitSetT &other) const;

    constexpr bool operator[](std::size_t pos) const;
    Reference operator[](std::size_t pos) { return Reference(this, pos); }

    bool test(std::size_t pos) const;

    bool all() const;
    bool any() const;
    bool none() const;
    std::size_t count() const;

    constexpr std::size_t size() const { return N; }

    BitSetT &operator&=(const BitSetT &other);
    BitSetT &operator|=(const BitSetT &other);
    BitSetT &operator^=(const BitSetT &other);
    BitSetT operator~() const;

    BitSetT operator<<(std::size_t pos) const;
    BitSetT &operator<<=(std::size_t pos);
    BitSetT operator>>(std::size_t pos) const;
    BitSetT &operator>>=(std::size_t pos);

    BitSetT &set();
    BitSetT &set(std::size_t pos, bool value = true);

    BitSetT &reset();
    BitSetT &reset(std::size_t pos);

    BitSetT &flip();
    BitSetT &flip(std::size_t pos);

    unsigned long to_ulong() const { return static_cast<unsigned long>(mBits); }
    BitsT bits() const { return mBits; }

    Iterator begin() const { return Iterator(*this); }
    Iterator end() const { return Iterator(BitSetT()); }

  private:
    constexpr static BitsT Bit(std::size_t x) { return (static_cast<BitsT>(1) << x); }
    constexpr static BitsT Mask(std::size_t x) { return ((Bit(x - 1) - 1) << 1) + 1; }

    BitsT mBits;
};

template <size_t N>
class IterableBitSet : public std::bitset<N>
{
  public:
    IterableBitSet() {}
    IterableBitSet(const std::bitset<N> &implicitBitSet) : std::bitset<N>(implicitBitSet) {}

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

        static constexpr size_t BitsPerWord = sizeof(uint32_t) * 8;
        std::bitset<N> mBits;
        unsigned long mCurrentBit;
        unsigned long mOffset;
    };

    Iterator begin() const { return Iterator(*this); }
    Iterator end() const { return Iterator(std::bitset<N>(0)); }
};

template <size_t N>
IterableBitSet<N>::Iterator::Iterator(const std::bitset<N> &bitset)
    : mBits(bitset), mCurrentBit(0), mOffset(0)
{
    if (mBits.any())
    {
        mCurrentBit = getNextBit();
    }
    else
    {
        mOffset = static_cast<unsigned long>(rx::roundUp(N, BitsPerWord));
    }
}

template <size_t N>
typename IterableBitSet<N>::Iterator &IterableBitSet<N>::Iterator::operator++()
{
    ASSERT(mBits.any());
    mBits.set(mCurrentBit - mOffset, 0);
    mCurrentBit = getNextBit();
    return *this;
}

template <size_t N>
bool IterableBitSet<N>::Iterator::operator==(const Iterator &other) const
{
    return mOffset == other.mOffset && mBits == other.mBits;
}

template <size_t N>
bool IterableBitSet<N>::Iterator::operator!=(const Iterator &other) const
{
    return !(*this == other);
}

template <size_t N>
unsigned long IterableBitSet<N>::Iterator::getNextBit()
{
    // TODO(jmadill): Use 64-bit scan when possible.
    static constexpr std::bitset<N> wordMask(std::numeric_limits<uint32_t>::max());

    while (mOffset < N)
    {
        uint32_t wordBits = static_cast<uint32_t>((mBits & wordMask).to_ulong());
        if (wordBits != 0)
        {
            return gl::ScanForward(wordBits) + mOffset;
        }

        mBits >>= BitsPerWord;
        mOffset += BitsPerWord;
    }
    return 0;
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT>::BitSetT() : mBits(0)
{
    static_assert(N > 0, "Bitset type cannot support zero bits.");
    static_assert(N <= sizeof(BitsT) * 8, "Bitset type cannot support a size this large.");
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT>::BitSetT(BitsT value) : mBits(value & Mask(N))
{
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT>::~BitSetT()
{
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT>::BitSetT(const BitSetT &other) : mBits(other.mBits)
{
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT> &BitSetT<N, BitsT>::operator=(const BitSetT &other)
{
    mBits = other.mBits;
    return *this;
}

template <size_t N, typename BitsT>
bool BitSetT<N, BitsT>::operator==(const BitSetT &other) const
{
    return mBits == other.mBits;
}

template <size_t N, typename BitsT>
bool BitSetT<N, BitsT>::operator!=(const BitSetT &other) const
{
    return mBits != other.mBits;
}

template <size_t N, typename BitsT>
constexpr bool BitSetT<N, BitsT>::operator[](std::size_t pos) const
{
    return test(pos);
}

template <size_t N, typename BitsT>
bool BitSetT<N, BitsT>::test(std::size_t pos) const
{
    return (mBits & Bit(pos)) != 0;
}

template <size_t N, typename BitsT>
bool BitSetT<N, BitsT>::all() const
{
    ASSERT(mBits == (mBits & Mask(N)));
    return mBits == Mask(N);
}

template <size_t N, typename BitsT>
bool BitSetT<N, BitsT>::any() const
{
    ASSERT(mBits == (mBits & Mask(N)));
    return (mBits != 0);
}

template <size_t N, typename BitsT>
bool BitSetT<N, BitsT>::none() const
{
    ASSERT(mBits == (mBits & Mask(N)));
    return (mBits == 0);
}

template <size_t N, typename BitsT>
std::size_t BitSetT<N, BitsT>::count() const
{
    return gl::BitCount(mBits);
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT> &BitSetT<N, BitsT>::operator&=(const BitSetT &other)
{
    mBits &= other.mBits;
    return *this;
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT> &BitSetT<N, BitsT>::operator|=(const BitSetT &other)
{
    mBits |= other.mBits;
    return *this;
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT> &BitSetT<N, BitsT>::operator^=(const BitSetT &other)
{
    mBits = (mBits ^ other.mBits) & Mask(N);
    return *this;
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT> BitSetT<N, BitsT>::operator~() const
{
    return BitSetT<N, BitsT>(~mBits & Mask(N));
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT> BitSetT<N, BitsT>::operator<<(std::size_t pos) const
{
    return BitSetT<N, BitsT>((mBits << pos) & Mask(N));
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT> &BitSetT<N, BitsT>::operator<<=(std::size_t pos)
{
    mBits = (mBits << pos & Mask(N));
    return *this;
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT> BitSetT<N, BitsT>::operator>>(std::size_t pos) const
{
    return BitSetT<N, BitsT>(mBits >> pos);
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT> &BitSetT<N, BitsT>::operator>>=(std::size_t pos)
{
    mBits = ((mBits >> pos) & Mask(N));
    return *this;
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT> &BitSetT<N, BitsT>::set()
{
    mBits = Mask(N);
    return *this;
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT> &BitSetT<N, BitsT>::set(std::size_t pos, bool value)
{
    if (value)
    {
        mBits |= Bit(pos);
    }
    else
    {
        reset(pos);
    }
    return *this;
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT> &BitSetT<N, BitsT>::reset()
{
    mBits = 0;
    return *this;
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT> &BitSetT<N, BitsT>::reset(std::size_t pos)
{
    mBits &= ~Bit(pos);
    return *this;
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT> &BitSetT<N, BitsT>::flip()
{
    mBits ^= Mask(N);
    return *this;
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT> &BitSetT<N, BitsT>::flip(std::size_t pos)
{
    mBits ^= Bit(pos);
    return *this;
}

template <size_t N, typename BitsT>
BitSetT<N, BitsT>::Iterator::Iterator(const BitSetT &bits) : mBitsCopy(bits), mCurrentBit(0)
{
    if (bits.any())
    {
        mCurrentBit = getNextBit();
    }
}

template <size_t N, typename BitsT>
typename BitSetT<N, BitsT>::Iterator &BitSetT<N, BitsT>::Iterator::operator++()
{
    ASSERT(mBitsCopy.any());
    mBitsCopy.reset(mCurrentBit);
    mCurrentBit = getNextBit();
    return *this;
}

template <size_t N, typename BitsT>
bool BitSetT<N, BitsT>::Iterator::operator==(const Iterator &other) const
{
    return mBitsCopy == other.mBitsCopy;
}

template <size_t N, typename BitsT>
bool BitSetT<N, BitsT>::Iterator::operator!=(const Iterator &other) const
{
    return !(*this == other);
}

template <size_t N, typename BitsT>
std::size_t BitSetT<N, BitsT>::Iterator::operator*() const
{
    return mCurrentBit;
}

template <size_t N, typename BitsT>
std::size_t BitSetT<N, BitsT>::Iterator::getNextBit()
{
    if (mBitsCopy.none())
    {
        return 0;
    }

    return gl::ScanForward(mBitsCopy.mBits);
}

template <size_t N>
using BitSet32 = BitSetT<N, uint32_t>;

// ScanForward for 64-bits requires a 64-bit implementation.
#if defined(ANGLE_IS_64_BIT_CPU)
template <size_t N>
using BitSet64 = BitSetT<N, uint64_t>;
#endif  // defined(ANGLE_IS_64_BIT_CPU)

namespace priv
{

template <size_t N, typename T>
using EnableIfBitsFit = typename std::enable_if<N <= sizeof(T) * 8>::type;

template <size_t N, typename Enable = void>
struct GetBitSet
{
    using Type = IterableBitSet<N>;
};

// Prefer 64-bit bitsets on 64-bit CPUs. They seem faster than 32-bit.
#if defined(ANGLE_IS_64_BIT_CPU)
template <size_t N>
struct GetBitSet<N, EnableIfBitsFit<N, uint64_t>>
{
    using Type = BitSet64<N>;
};
#else
template <size_t N>
struct GetBitSet<N, EnableIfBitsFit<N, uint32_t>>
{
    using Type = BitSet32<N>;
};
#endif  // defined(ANGLE_IS_64_BIT_CPU)

}  // namespace priv

template <size_t N>
using BitSet = typename priv::GetBitSet<N>::Type;

}  // angle

template <size_t N, typename BitsT>
inline angle::BitSetT<N, BitsT> operator&(const angle::BitSetT<N, BitsT> &lhs,
                                          const angle::BitSetT<N, BitsT> &rhs)
{
    return angle::BitSetT<N, BitsT>(lhs.bits() & rhs.bits());
}

template <size_t N, typename BitsT>
inline angle::BitSetT<N, BitsT> operator|(const angle::BitSetT<N, BitsT> &lhs,
                                          const angle::BitSetT<N, BitsT> &rhs)
{
    return angle::BitSetT<N, BitsT>(lhs.bits() | rhs.bits());
}

template <size_t N, typename BitsT>
inline angle::BitSetT<N, BitsT> operator^(const angle::BitSetT<N, BitsT> &lhs,
                                          const angle::BitSetT<N, BitsT> &rhs)
{
    return angle::BitSetT<N, BitsT>(lhs.bits() ^ rhs.bits());
}

#endif  // COMMON_BITSETITERATOR_H_
