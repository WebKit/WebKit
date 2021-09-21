// Taken from LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See Source/WTF/LICENSE-LLVM.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Portable 128-bit unsigned integer arithmetic for use in impoverished
// C++ implementations lacking __uint128_t.

// Based on:
// https://github.com/llvm/llvm-project/blob/d480f968/flang/include/flang/Common/uint128.h

#pragma once

// Define AVOID_NATIVE_INT128_T to force the use of Int128Impl below instead of
// the C++ compiler's native 128-bit unsigned integer type, if it has one.
#ifndef AVOID_NATIVE_INT128_T
#define AVOID_NATIVE_INT128_T 0
#endif

#include <cstdint>
#include <type_traits>
#include <wtf/LeadingZeroBitCount.h>

namespace WTF {

template <bool IS_SIGNED = false> class Int128Impl {
public:
    constexpr Int128Impl() { }
    // This means of definition provides some portability for
    // "size_t" operands.
    constexpr Int128Impl(unsigned n) : low_ {n} { }
    constexpr Int128Impl(unsigned long n) : low_ {n} { }
    constexpr Int128Impl(unsigned long long n) : low_ {n} { }
    constexpr Int128Impl(int n)
        : low_ {static_cast<std::uint64_t>(n)}
        , high_ {-static_cast<std::uint64_t>(n < 0)}
    { }
    constexpr Int128Impl(long n)
        : low_ {static_cast<std::uint64_t>(n)}
        , high_ {-static_cast<std::uint64_t>(n < 0)}
    { }
    constexpr Int128Impl(long long n)
        : low_ {static_cast<std::uint64_t>(n)}
        , high_ {-static_cast<std::uint64_t>(n < 0)}
    { }
    constexpr Int128Impl(const Int128Impl &) = default;
    constexpr Int128Impl(Int128Impl &&) = default;
    constexpr Int128Impl &operator=(const Int128Impl &) = default;
    constexpr Int128Impl &operator=(Int128Impl &&) = default;

    constexpr Int128Impl operator+() const { return *this; }
    constexpr Int128Impl operator~() const { return {~high_, ~low_}; }
    constexpr Int128Impl operator-() const { return ~*this + 1; }
    constexpr bool operator!() const { return !low_ && !high_; }
    constexpr explicit operator bool() const { return low_ || high_; }
    constexpr explicit operator std::uint64_t() const { return low_; }
    constexpr explicit operator std::int64_t() const { return low_; }
    constexpr explicit operator int() const { return static_cast<int>(low_); }

    constexpr std::uint64_t high() const { return high_; }
    constexpr std::uint64_t low() const { return low_; }

    constexpr Int128Impl operator++(/*prefix*/)
    {
        *this += 1;
        return *this;
    }
    constexpr Int128Impl operator++(int /*postfix*/)
    {
        Int128Impl result {*this};
        *this += 1;
        return result;
    }
    constexpr Int128Impl operator--(/*prefix*/)
    {
        *this -= 1;
        return *this;
    }
    constexpr Int128Impl operator--(int /*postfix*/)
    {
        Int128Impl result {*this};
        *this -= 1;
        return result;
    }

    constexpr Int128Impl operator&(Int128Impl that) const
    {
        return {high_ & that.high_, low_ & that.low_};
    }
    constexpr Int128Impl operator | (Int128Impl that) const
    {
        return {high_ | that.high_, low_ | that.low_};
    }
    constexpr Int128Impl operator^(Int128Impl that) const
    {
        return {high_ ^ that.high_, low_ ^ that.low_};
    }

    constexpr Int128Impl operator<<(Int128Impl that) const
    {
        if (that >= 128)
            return { };
        if (!that)
            return *this;
        std::uint64_t n {that.low_};
        if (n >= 64)
            return {low_ << (n - 64), 0};
        return {(high_ << n) | (low_ >> (64 - n)), low_ << n};
    }
    constexpr Int128Impl operator>>(Int128Impl that) const
    {
        if (that >= 128)
            return { };
        if (!that)
            return *this;
        std::uint64_t n {that.low_};
        if (n >= 64)
            return {0, high_ >> (n - 64)};
        return {high_ >> n, (high_ << (64 - n)) | (low_ >> n)};
    }

    constexpr Int128Impl operator+(Int128Impl that) const
    {
        std::uint64_t lower {(low_ & ~topBit) + (that.low_ & ~topBit)};
        bool carry {((lower >> 63) + (low_ >> 63) + (that.low_ >> 63)) > 1};
        return {high_ + that.high_ + carry, low_ + that.low_};
    }
    constexpr Int128Impl operator-(Int128Impl that) const { return *this + -that; }

    constexpr Int128Impl operator*(Int128Impl that) const
    {
        std::uint64_t mask32 {0xffffffff};
        if (!high_ && !that.high_) {
            std::uint64_t x0 {low_ & mask32}, x1 {low_ >> 32};
            std::uint64_t y0 {that.low_ & mask32}, y1 {that.low_ >> 32};
            Int128Impl x0y0 {x0 * y0}, x0y1 {x0 * y1};
            Int128Impl x1y0 {x1 * y0}, x1y1 {x1 * y1};
            return x0y0 + ((x0y1 + x1y0) << 32) + (x1y1 << 64);
        }

        std::uint64_t x0 {low_ & mask32}, x1 {low_ >> 32}, x2 {high_ & mask32},
            x3 {high_ >> 32};
        std::uint64_t y0 {that.low_ & mask32}, y1 {that.low_ >> 32},
            y2 {that.high_ & mask32}, y3 {that.high_ >> 32};
        Int128Impl x0y0 {x0 * y0}, x0y1 {x0 * y1}, x0y2 {x0 * y2}, x0y3 {x0 * y3};
        Int128Impl x1y0 {x1 * y0}, x1y1 {x1 * y1}, x1y2 {x1 * y2};
        Int128Impl x2y0 {x2 * y0}, x2y1 {x2 * y1};
        Int128Impl x3y0 {x3 * y0};
        return x0y0 + ((x0y1 + x1y0) << 32) + ((x0y2 + x1y1 + x2y0) << 64) +
            ((x0y3 + x1y2 + x2y1 + x3y0) << 96);
    }

    constexpr Int128Impl operator/(Int128Impl that) const
    {
        int j {leadingZeroes()};
        Int128Impl bits {*this};
        bits <<= j;
        Int128Impl numerator { };
        Int128Impl quotient { };
        for (; j < 128; ++j) {
            numerator <<= 1;
            if (bits.high_ & topBit)
                numerator.low_ |= 1;
            bits <<= 1;
            quotient <<= 1;
            if (numerator >= that) {
                ++quotient;
                numerator -= that;
            }
        }
        return quotient;
    }

    constexpr Int128Impl operator%(Int128Impl that) const
    {
        int j {leadingZeroes()};
        Int128Impl bits {*this};
        bits <<= j;
        Int128Impl remainder { };
        for (; j < 128; ++j) {
            remainder <<= 1;
            if (bits.high_ & topBit)
                remainder.low_ |= 1;
            bits <<= 1;
            if (remainder >= that)
                remainder -= that;
        }
        return remainder;
    }

    constexpr bool operator<(Int128Impl that) const
    {
        if (IS_SIGNED && (high_ ^ that.high_) & topBit)
            return !!(high_ & topBit);

        return high_ < that.high_ || (high_ == that.high_ && low_ < that.low_);
    }
    constexpr bool operator<=(Int128Impl that) const { return !(*this > that); }
    constexpr bool operator==(Int128Impl that) const
    {
        return low_ == that.low_ && high_ == that.high_;
    }
    constexpr bool operator!=(Int128Impl that) const { return !(*this == that); }
    constexpr bool operator>=(Int128Impl that) const { return that <= *this; }
    constexpr bool operator>(Int128Impl that) const { return that < *this; }

    constexpr Int128Impl &operator&=(const Int128Impl &that)
    {
        *this = *this & that;
        return *this;
    }
    constexpr Int128Impl &operator|=(const Int128Impl &that)
    {
        *this = *this | that;
        return *this;
    }
    constexpr Int128Impl &operator^=(const Int128Impl &that)
    {
        *this = *this ^ that;
        return *this;
    }
    constexpr Int128Impl &operator<<=(const Int128Impl &that)
    {
        *this = *this << that;
        return *this;
    }
    constexpr Int128Impl &operator>>=(const Int128Impl &that)
    {
        *this = *this >> that;
        return *this;
    }
    constexpr Int128Impl &operator+=(const Int128Impl &that)
    {
        *this = *this + that;
        return *this;
    }
    constexpr Int128Impl &operator-=(const Int128Impl &that)
    {
        *this = *this - that;
        return *this;
    }
    constexpr Int128Impl &operator*=(const Int128Impl &that)
    {
        *this = *this * that;
        return *this;
    }
    constexpr Int128Impl &operator/=(const Int128Impl &that)
    {
        *this = *this / that;
        return *this;
    }
    constexpr Int128Impl &operator%=(const Int128Impl &that)
    {
        *this = *this % that;
        return *this;
    }

private:
    constexpr Int128Impl(std::uint64_t hi, std::uint64_t lo) : low_ {lo}, high_ {hi} { }
    constexpr int leadingZeroes() const
    {
        if (!high_)
            return 64 + leadingZeroBitCount(low_);
        return leadingZeroBitCount(high_);
    }
    static constexpr std::uint64_t topBit {std::uint64_t {1} << 63};
    std::uint64_t low_ {0}, high_ {0};
};

#if !AVOID_NATIVE_INT128_T && HAVE(INT128_T)
using UInt128 = __uint128_t;
using Int128 = __int128_t;
#else
using UInt128 = Int128Impl<false>;
using Int128 = Int128Impl<true>;
#endif

template <int BITS> struct HostUnsignedIntTypeHelper {
    using type = std::conditional_t<(BITS <= 8), std::uint8_t,
        std::conditional_t<(BITS <= 16), std::uint16_t,
        std::conditional_t<(BITS <= 32), std::uint32_t,
        std::conditional_t<(BITS <= 64), std::uint64_t, UInt128>>>>;
};
template <int BITS> struct HostSignedIntTypeHelper {
    using type = std::conditional_t<(BITS <= 8), std::int8_t,
        std::conditional_t<(BITS <= 16), std::int16_t,
        std::conditional_t<(BITS <= 32), std::int32_t,
        std::conditional_t<(BITS <= 64), std::int64_t, Int128>>>>;
};
template <int BITS>
using HostUnsignedIntType = typename HostUnsignedIntTypeHelper<BITS>::type;
template <int BITS>
using HostSignedIntType = typename HostSignedIntTypeHelper<BITS>::type;

} // namespace WTF

using WTF::Int128;
using WTF::UInt128;
