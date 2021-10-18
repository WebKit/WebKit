// Taken from LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See Source/WTF/LICENSE-LLVM.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Based on:
// https://github.com/llvm/llvm-project/blob/d480f968/flang/unittests/Evaluate/uint128.cpp

#include "config.h"
#include <wtf/Int128.h>

#include <cinttypes>

namespace TestWebKitAPI {

using NonNativeInt128 = WTF::Int128Impl<true>;
using NonNativeUInt128 = WTF::Int128Impl<false>;

static void TestUnary(std::uint64_t x)
{
    NonNativeUInt128 n { x };
    EXPECT_EQ(x, static_cast<std::uint64_t>(n));
    EXPECT_EQ(~x, static_cast<std::uint64_t>(~n));
    EXPECT_EQ(-x, static_cast<std::uint64_t>(-n));
    EXPECT_EQ(!x, static_cast<std::uint64_t>(!n));
    EXPECT_TRUE(n == n);
    EXPECT_TRUE(n + n == n * static_cast<NonNativeUInt128>(2));
    EXPECT_TRUE(n - n == static_cast<NonNativeUInt128>(0));
    EXPECT_TRUE(n + n == n << static_cast<NonNativeUInt128>(1));
    EXPECT_TRUE(n + n == n << static_cast<NonNativeUInt128>(1));
    EXPECT_TRUE((n + n) - n == n);
    EXPECT_TRUE(((n + n) >> static_cast<NonNativeUInt128>(1)) == n);
    if (x) {
        EXPECT_TRUE(static_cast<NonNativeUInt128>(0) / n == static_cast<NonNativeUInt128>(0));
        EXPECT_TRUE(static_cast<NonNativeUInt128>(n - 1) / n == static_cast<NonNativeUInt128>(0));
        EXPECT_TRUE(static_cast<NonNativeUInt128>(n) / n == static_cast<NonNativeUInt128>(1));
        EXPECT_TRUE(static_cast<NonNativeUInt128>(n + n - 1) / n == static_cast<NonNativeUInt128>(1));
        EXPECT_TRUE(static_cast<NonNativeUInt128>(n + n) / n == static_cast<NonNativeUInt128>(2));
    }
}

static void TestBinary(std::uint64_t x, std::uint64_t y)
{
    NonNativeUInt128 m { x }, n { y };
    EXPECT_EQ(x, static_cast<std::uint64_t>(m));
    EXPECT_EQ(y, static_cast<std::uint64_t>(n));
    EXPECT_EQ(x & y, static_cast<std::uint64_t>(m & n));
    EXPECT_EQ(x | y, static_cast<std::uint64_t>(m | n));
    EXPECT_EQ(x ^ y, static_cast<std::uint64_t>(m ^ n));
    EXPECT_EQ(x + y, static_cast<std::uint64_t>(m + n));
    EXPECT_EQ(x - y, static_cast<std::uint64_t>(m - n));
    EXPECT_EQ(x * y, static_cast<std::uint64_t>(m * n));
    if (n)
        EXPECT_EQ(x / y, static_cast<std::uint64_t>(m / n));
}

TEST(WTF_Int128, Basic)
{
    for (std::uint64_t j {0}; j < 64; ++j) {
        TestUnary(j);
        TestUnary(~j);
        TestUnary(std::uint64_t(1) << j);
        for (std::uint64_t k {0}; k < 64; ++k)
            TestBinary(j, k);
    }
}

#if HAVE(INT128_T)
static __uint128_t ToNative(NonNativeUInt128 n)
{
    return static_cast<__uint128_t>(static_cast<std::uint64_t>(n >> 64)) << 64 |
        static_cast<std::uint64_t>(n);
}

static NonNativeUInt128 FromNative(__uint128_t n)
{
    return NonNativeUInt128 { static_cast<std::uint64_t>(n >> 64)} << 64 | NonNativeUInt128 { static_cast<std::uint64_t>(n) };
}

static void TestVsNative(__uint128_t x, __uint128_t y)
{
    NonNativeUInt128 m { FromNative(x) }, n { FromNative(y) };
    EXPECT_TRUE(ToNative(m) == x);
    EXPECT_TRUE(ToNative(n) == y);
    EXPECT_TRUE(ToNative(~m) == ~x);
    EXPECT_TRUE(ToNative(-m) == -x);
    EXPECT_TRUE(ToNative(!m) == !x);
    EXPECT_TRUE(ToNative(m < n) == (x < y));
    EXPECT_TRUE(ToNative(m <= n) == (x <= y));
    EXPECT_TRUE(ToNative(m == n) == (x == y));
    EXPECT_TRUE(ToNative(m != n) == (x != y));
    EXPECT_TRUE(ToNative(m >= n) == (x >= y));
    EXPECT_TRUE(ToNative(m > n) == (x > y));
    EXPECT_TRUE(ToNative(m & n) == (x & y));
    EXPECT_TRUE(ToNative(m | n) == (x | y));
    EXPECT_TRUE(ToNative(m ^ n) == (x ^ y));
    if (y < 128) {
        EXPECT_TRUE(ToNative(m << n) == (x << y));
        EXPECT_TRUE(ToNative(m >> n) == (x >> y));
    }
    EXPECT_TRUE(ToNative(m + n) == (x + y));
    EXPECT_TRUE(ToNative(m - n) == (x - y));
    EXPECT_TRUE(ToNative(m * n) == (x * y));
    if (y > 0) {
        EXPECT_TRUE(ToNative(m / n) == (x / y));
        EXPECT_TRUE(ToNative(m % n) == (x % y));
        EXPECT_TRUE(ToNative(m - n * (m / n)) == (x % y));
    }
}

TEST(WTF_Int128, VsNative)
{
    GTEST_LOG_(INFO) << "Environment has native __uint128_t";

    for (int j {0}; j < 128; ++j) {
        for (int k {0}; k < 128; ++k) {
            __uint128_t m { 1 }, n { 1 };
            m <<= j;
            n <<= k;
            TestVsNative(m, n);
            TestVsNative(~m, n);
            TestVsNative(m, ~n);
            TestVsNative(~m, ~n);
            TestVsNative(m ^ n, n);
            TestVsNative(m, m ^ n);
            TestVsNative(m ^ ~n, n);
            TestVsNative(m, ~m ^ n);
            TestVsNative(m ^ ~n, m ^ n);
            TestVsNative(m ^ n, ~m ^ n);
            TestVsNative(m ^ ~n, ~m ^ n);
            TestBinary(m, 10000000000000000); // important case for decimal conversion
            TestBinary(~m, 10000000000000000);
        }
    }
}

#else // !HAVE(INT128_T)

TEST(WTF_Int128, VsNative)
{
    GTEST_LOG_(INFO) << "Environment lacks native __uint128_t";
}

#endif // !HAVE(INT128_T)

} // namespace TestWebKitAPI
