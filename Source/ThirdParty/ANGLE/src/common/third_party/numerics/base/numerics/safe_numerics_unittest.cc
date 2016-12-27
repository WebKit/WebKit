// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <type_traits>

#include "base/compiler_specific.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(COMPILER_MSVC) && defined(ARCH_CPU_32_BITS)
#include <mmintrin.h>
#endif

using std::numeric_limits;
using base::CheckedNumeric;
using base::checked_cast;
using base::IsValueInRangeForNumericType;
using base::IsValueNegative;
using base::SizeT;
using base::StrictNumeric;
using base::saturated_cast;
using base::strict_cast;
using base::internal::MaxExponent;
using base::internal::RANGE_VALID;
using base::internal::RANGE_INVALID;
using base::internal::RANGE_OVERFLOW;
using base::internal::RANGE_UNDERFLOW;
using base::internal::SignedIntegerForSize;

// These tests deliberately cause arithmetic overflows. If the compiler is
// aggressive enough, it can const fold these overflows. Disable warnings about
// overflows for const expressions.
#if defined(OS_WIN)
#pragma warning(disable : 4756)
#endif

// This is a helper function for finding the maximum value in Src that can be
// wholy represented as the destination floating-point type.
template <typename Dst, typename Src>
Dst GetMaxConvertibleToFloat()
{
    typedef numeric_limits<Dst> DstLimits;
    typedef numeric_limits<Src> SrcLimits;
    static_assert(SrcLimits::is_specialized, "Source must be numeric.");
    static_assert(DstLimits::is_specialized, "Destination must be numeric.");
    CHECK(DstLimits::is_iec559);

    if (SrcLimits::digits <= DstLimits::digits &&
        MaxExponent<Src>::value <= MaxExponent<Dst>::value)
        return SrcLimits::max();
    Src max = SrcLimits::max() / 2 + (SrcLimits::is_integer ? 1 : 0);
    while (max != static_cast<Src>(static_cast<Dst>(max)))
    {
        max /= 2;
    }
    return static_cast<Dst>(max);
}

// Helper macros to wrap displaying the conversion types and line numbers.
#define TEST_EXPECTED_VALIDITY(expected, actual)                                            \
    EXPECT_EQ(expected, CheckedNumeric<Dst>(actual).IsValid())                              \
        << "Result test: Value " << +(actual).ValueUnsafe() << " as " << dst << " on line " \
        << line;

#define TEST_EXPECTED_SUCCESS(actual) TEST_EXPECTED_VALIDITY(true, actual)
#define TEST_EXPECTED_FAILURE(actual) TEST_EXPECTED_VALIDITY(false, actual)

#define TEST_EXPECTED_VALUE(expected, actual)                                                 \
    EXPECT_EQ(static_cast<Dst>(expected), CheckedNumeric<Dst>(actual).ValueUnsafe())          \
        << "Result test: Value " << +((actual).ValueUnsafe()) << " as " << dst << " on line " \
        << line;

// Signed integer arithmetic.
template <typename Dst>
static void TestSpecializedArithmetic(
    const char *dst,
    int line,
    typename std::enable_if<numeric_limits<Dst>::is_integer && numeric_limits<Dst>::is_signed,
                            int>::type = 0)
{
    typedef numeric_limits<Dst> DstLimits;
    TEST_EXPECTED_FAILURE(-CheckedNumeric<Dst>(DstLimits::min()));
    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::min()).Abs());
    TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(-1).Abs());

    TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::max()) + -1);
    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::min()) + -1);
    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(-DstLimits::max()) + -DstLimits::max());

    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::min()) - 1);
    TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::min()) - -1);
    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::max()) - -DstLimits::max());
    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(-DstLimits::max()) - DstLimits::max());

    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::min()) * 2);

    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::min()) / -1);
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(-1) / 2);

    // Modulus is legal only for integers.
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>() % 1);
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) % 1);
    TEST_EXPECTED_VALUE(-1, CheckedNumeric<Dst>(-1) % 2);
    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(-1) % -2);
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(DstLimits::min()) % 2);
    TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(DstLimits::max()) % 2);
    // Test all the different modulus combinations.
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) % CheckedNumeric<Dst>(1));
    TEST_EXPECTED_VALUE(0, 1 % CheckedNumeric<Dst>(1));
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) % 1);
    CheckedNumeric<Dst> checked_dst = 1;
    TEST_EXPECTED_VALUE(0, checked_dst %= 1);
}

// Unsigned integer arithmetic.
template <typename Dst>
static void TestSpecializedArithmetic(
    const char *dst,
    int line,
    typename std::enable_if<numeric_limits<Dst>::is_integer && !numeric_limits<Dst>::is_signed,
                            int>::type = 0)
{
    typedef numeric_limits<Dst> DstLimits;
    TEST_EXPECTED_SUCCESS(-CheckedNumeric<Dst>(DstLimits::min()));
    TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::min()).Abs());
    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::min()) + -1);
    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::min()) - 1);
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(DstLimits::min()) * 2);
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) / 2);
    TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::min()).UnsignedAbs());
    TEST_EXPECTED_SUCCESS(CheckedNumeric<typename SignedIntegerForSize<Dst>::type>(
                              std::numeric_limits<typename SignedIntegerForSize<Dst>::type>::min())
                              .UnsignedAbs());

    // Modulus is legal only for integers.
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>() % 1);
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) % 1);
    TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) % 2);
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(DstLimits::min()) % 2);
    TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(DstLimits::max()) % 2);
    // Test all the different modulus combinations.
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) % CheckedNumeric<Dst>(1));
    TEST_EXPECTED_VALUE(0, 1 % CheckedNumeric<Dst>(1));
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) % 1);
    CheckedNumeric<Dst> checked_dst = 1;
    TEST_EXPECTED_VALUE(0, checked_dst %= 1);
}

// Floating point arithmetic.
template <typename Dst>
void TestSpecializedArithmetic(
    const char *dst,
    int line,
    typename std::enable_if<numeric_limits<Dst>::is_iec559, int>::type = 0)
{
    typedef numeric_limits<Dst> DstLimits;
    TEST_EXPECTED_SUCCESS(-CheckedNumeric<Dst>(DstLimits::min()));

    TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::min()).Abs());
    TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(-1).Abs());

    TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::min()) + -1);
    TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::max()) + 1);
    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(-DstLimits::max()) + -DstLimits::max());

    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::max()) - -DstLimits::max());
    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(-DstLimits::max()) - DstLimits::max());

    TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::min()) * 2);

    TEST_EXPECTED_VALUE(-0.5, CheckedNumeric<Dst>(-1.0) / 2);
    EXPECT_EQ(static_cast<Dst>(1.0), CheckedNumeric<Dst>(1.0).ValueFloating());
}

// Generic arithmetic tests.
template <typename Dst>
static void TestArithmetic(const char *dst, int line)
{
    typedef numeric_limits<Dst> DstLimits;

    EXPECT_EQ(true, CheckedNumeric<Dst>().IsValid());
    EXPECT_EQ(
        false,
        CheckedNumeric<Dst>(CheckedNumeric<Dst>(DstLimits::max()) * DstLimits::max()).IsValid());
    EXPECT_EQ(static_cast<Dst>(0), CheckedNumeric<Dst>().ValueOrDie());
    EXPECT_EQ(static_cast<Dst>(0), CheckedNumeric<Dst>().ValueOrDefault(1));
    EXPECT_EQ(static_cast<Dst>(1),
              CheckedNumeric<Dst>(CheckedNumeric<Dst>(DstLimits::max()) * DstLimits::max())
                  .ValueOrDefault(1));

    // Test the operator combinations.
    TEST_EXPECTED_VALUE(2, CheckedNumeric<Dst>(1) + CheckedNumeric<Dst>(1));
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) - CheckedNumeric<Dst>(1));
    TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) * CheckedNumeric<Dst>(1));
    TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) / CheckedNumeric<Dst>(1));
    TEST_EXPECTED_VALUE(2, 1 + CheckedNumeric<Dst>(1));
    TEST_EXPECTED_VALUE(0, 1 - CheckedNumeric<Dst>(1));
    TEST_EXPECTED_VALUE(1, 1 * CheckedNumeric<Dst>(1));
    TEST_EXPECTED_VALUE(1, 1 / CheckedNumeric<Dst>(1));
    TEST_EXPECTED_VALUE(2, CheckedNumeric<Dst>(1) + 1);
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>(1) - 1);
    TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) * 1);
    TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) / 1);
    CheckedNumeric<Dst> checked_dst = 1;
    TEST_EXPECTED_VALUE(2, checked_dst += 1);
    checked_dst = 1;
    TEST_EXPECTED_VALUE(0, checked_dst -= 1);
    checked_dst = 1;
    TEST_EXPECTED_VALUE(1, checked_dst *= 1);
    checked_dst = 1;
    TEST_EXPECTED_VALUE(1, checked_dst /= 1);

    // Generic negation.
    TEST_EXPECTED_VALUE(0, -CheckedNumeric<Dst>());
    TEST_EXPECTED_VALUE(-1, -CheckedNumeric<Dst>(1));
    TEST_EXPECTED_VALUE(1, -CheckedNumeric<Dst>(-1));
    TEST_EXPECTED_VALUE(static_cast<Dst>(DstLimits::max() * -1),
                        -CheckedNumeric<Dst>(DstLimits::max()));

    // Generic absolute value.
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>().Abs());
    TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1).Abs());
    TEST_EXPECTED_VALUE(DstLimits::max(), CheckedNumeric<Dst>(DstLimits::max()).Abs());

    // Generic addition.
    TEST_EXPECTED_VALUE(1, (CheckedNumeric<Dst>() + 1));
    TEST_EXPECTED_VALUE(2, (CheckedNumeric<Dst>(1) + 1));
    TEST_EXPECTED_VALUE(0, (CheckedNumeric<Dst>(-1) + 1));
    TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::min()) + 1);
    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::max()) + DstLimits::max());

    // Generic subtraction.
    TEST_EXPECTED_VALUE(-1, (CheckedNumeric<Dst>() - 1));
    TEST_EXPECTED_VALUE(0, (CheckedNumeric<Dst>(1) - 1));
    TEST_EXPECTED_VALUE(-2, (CheckedNumeric<Dst>(-1) - 1));
    TEST_EXPECTED_SUCCESS(CheckedNumeric<Dst>(DstLimits::max()) - 1);

    // Generic multiplication.
    TEST_EXPECTED_VALUE(0, (CheckedNumeric<Dst>() * 1));
    TEST_EXPECTED_VALUE(1, (CheckedNumeric<Dst>(1) * 1));
    TEST_EXPECTED_VALUE(-2, (CheckedNumeric<Dst>(-1) * 2));
    TEST_EXPECTED_VALUE(0, (CheckedNumeric<Dst>(0) * 0));
    TEST_EXPECTED_VALUE(0, (CheckedNumeric<Dst>(-1) * 0));
    TEST_EXPECTED_VALUE(0, (CheckedNumeric<Dst>(0) * -1));
    TEST_EXPECTED_FAILURE(CheckedNumeric<Dst>(DstLimits::max()) * DstLimits::max());

    // Generic division.
    TEST_EXPECTED_VALUE(0, CheckedNumeric<Dst>() / 1);
    TEST_EXPECTED_VALUE(1, CheckedNumeric<Dst>(1) / 1);
    TEST_EXPECTED_VALUE(DstLimits::min() / 2, CheckedNumeric<Dst>(DstLimits::min()) / 2);
    TEST_EXPECTED_VALUE(DstLimits::max() / 2, CheckedNumeric<Dst>(DstLimits::max()) / 2);

    TestSpecializedArithmetic<Dst>(dst, line);
}

// Helper macro to wrap displaying the conversion types and line numbers.
#define TEST_ARITHMETIC(Dst) TestArithmetic<Dst>(#Dst, __LINE__)

TEST(SafeNumerics, SignedIntegerMath)
{
    TEST_ARITHMETIC(int8_t);
    TEST_ARITHMETIC(int);
    TEST_ARITHMETIC(intptr_t);
    TEST_ARITHMETIC(intmax_t);
}

TEST(SafeNumerics, UnsignedIntegerMath)
{
    TEST_ARITHMETIC(uint8_t);
    TEST_ARITHMETIC(unsigned int);
    TEST_ARITHMETIC(uintptr_t);
    TEST_ARITHMETIC(uintmax_t);
}

TEST(SafeNumerics, FloatingPointMath)
{
    TEST_ARITHMETIC(float);
    TEST_ARITHMETIC(double);
}

// Enumerates the five different conversions types we need to test.
enum NumericConversionType
{
    SIGN_PRESERVING_VALUE_PRESERVING,
    SIGN_PRESERVING_NARROW,
    SIGN_TO_UNSIGN_WIDEN_OR_EQUAL,
    SIGN_TO_UNSIGN_NARROW,
    UNSIGN_TO_SIGN_NARROW_OR_EQUAL,
};

// Template covering the different conversion tests.
template <typename Dst, typename Src, NumericConversionType conversion>
struct TestNumericConversion
{
};

// EXPECT_EQ wrappers providing specific detail on test failures.
#define TEST_EXPECTED_RANGE(expected, actual)                                                \
    EXPECT_EQ(expected, base::internal::DstRangeRelationToSrcRange<Dst>(actual))             \
        << "Conversion test: " << src << " value " << actual << " to " << dst << " on line " \
        << line;

template <typename Dst, typename Src>
struct TestNumericConversion<Dst, Src, SIGN_PRESERVING_VALUE_PRESERVING>
{
    static void Test(const char *dst, const char *src, int line)
    {
        typedef numeric_limits<Src> SrcLimits;
        typedef numeric_limits<Dst> DstLimits;
        // Integral to floating.
        static_assert(
            (DstLimits::is_iec559 && SrcLimits::is_integer) ||
                // Not floating to integral and...
                (!(DstLimits::is_integer && SrcLimits::is_iec559) &&
                 // Same sign, same numeric, source is narrower or same.
                 ((SrcLimits::is_signed == DstLimits::is_signed && sizeof(Dst) >= sizeof(Src)) ||
                  // Or signed destination and source is smaller
                  (DstLimits::is_signed && sizeof(Dst) > sizeof(Src)))),
            "Comparison must be sign preserving and value preserving");

        const CheckedNumeric<Dst> checked_dst = SrcLimits::max();
        TEST_EXPECTED_SUCCESS(checked_dst);
        if (MaxExponent<Dst>::value > MaxExponent<Src>::value)
        {
            if (MaxExponent<Dst>::value >= MaxExponent<Src>::value * 2 - 1)
            {
                // At least twice larger type.
                TEST_EXPECTED_SUCCESS(SrcLimits::max() * checked_dst);
            }
            else
            {  // Larger, but not at least twice as large.
                TEST_EXPECTED_FAILURE(SrcLimits::max() * checked_dst);
                TEST_EXPECTED_SUCCESS(checked_dst + 1);
            }
        }
        else
        {  // Same width type.
            TEST_EXPECTED_FAILURE(checked_dst + 1);
        }

        TEST_EXPECTED_RANGE(RANGE_VALID, SrcLimits::max());
        TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(1));
        if (SrcLimits::is_iec559)
        {
            TEST_EXPECTED_RANGE(RANGE_VALID, SrcLimits::max() * static_cast<Src>(-1));
            TEST_EXPECTED_RANGE(RANGE_OVERFLOW, SrcLimits::infinity());
            TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, SrcLimits::infinity() * -1);
            TEST_EXPECTED_RANGE(RANGE_INVALID, SrcLimits::quiet_NaN());
        }
        else if (numeric_limits<Src>::is_signed)
        {
            TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(-1));
            TEST_EXPECTED_RANGE(RANGE_VALID, SrcLimits::min());
        }
    }
};

template <typename Dst, typename Src>
struct TestNumericConversion<Dst, Src, SIGN_PRESERVING_NARROW>
{
    static void Test(const char *dst, const char *src, int line)
    {
        typedef numeric_limits<Src> SrcLimits;
        typedef numeric_limits<Dst> DstLimits;
        static_assert(SrcLimits::is_signed == DstLimits::is_signed,
                      "Destination and source sign must be the same");
        static_assert(sizeof(Dst) < sizeof(Src) || (DstLimits::is_integer && SrcLimits::is_iec559),
                      "Destination must be narrower than source");

        const CheckedNumeric<Dst> checked_dst;
        TEST_EXPECTED_FAILURE(checked_dst + SrcLimits::max());
        TEST_EXPECTED_VALUE(1, checked_dst + static_cast<Src>(1));
        TEST_EXPECTED_FAILURE(checked_dst - SrcLimits::max());

        TEST_EXPECTED_RANGE(RANGE_OVERFLOW, SrcLimits::max());
        TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(1));
        if (SrcLimits::is_iec559)
        {
            TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, SrcLimits::max() * -1);
            TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(-1));
            TEST_EXPECTED_RANGE(RANGE_OVERFLOW, SrcLimits::infinity());
            TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, SrcLimits::infinity() * -1);
            TEST_EXPECTED_RANGE(RANGE_INVALID, SrcLimits::quiet_NaN());
            if (DstLimits::is_integer)
            {
                if (SrcLimits::digits < DstLimits::digits)
                {
                    TEST_EXPECTED_RANGE(RANGE_OVERFLOW, static_cast<Src>(DstLimits::max()));
                }
                else
                {
                    TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(DstLimits::max()));
                }
                TEST_EXPECTED_RANGE(RANGE_VALID,
                                    static_cast<Src>(GetMaxConvertibleToFloat<Src, Dst>()));
                TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(DstLimits::min()));
            }
        }
        else if (SrcLimits::is_signed)
        {
            TEST_EXPECTED_VALUE(-1, checked_dst - static_cast<Src>(1));
            TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, SrcLimits::min());
            TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(-1));
        }
        else
        {
            TEST_EXPECTED_FAILURE(checked_dst - static_cast<Src>(1));
            TEST_EXPECTED_RANGE(RANGE_VALID, SrcLimits::min());
        }
    }
};

template <typename Dst, typename Src>
struct TestNumericConversion<Dst, Src, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL>
{
    static void Test(const char *dst, const char *src, int line)
    {
        typedef numeric_limits<Src> SrcLimits;
        typedef numeric_limits<Dst> DstLimits;
        static_assert(sizeof(Dst) >= sizeof(Src),
                      "Destination must be equal or wider than source.");
        static_assert(SrcLimits::is_signed, "Source must be signed");
        static_assert(!DstLimits::is_signed, "Destination must be unsigned");

        const CheckedNumeric<Dst> checked_dst;
        TEST_EXPECTED_VALUE(SrcLimits::max(), checked_dst + SrcLimits::max());
        TEST_EXPECTED_FAILURE(checked_dst + static_cast<Src>(-1));
        TEST_EXPECTED_FAILURE(checked_dst + -SrcLimits::max());

        TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, SrcLimits::min());
        TEST_EXPECTED_RANGE(RANGE_VALID, SrcLimits::max());
        TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(1));
        TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, static_cast<Src>(-1));
    }
};

template <typename Dst, typename Src>
struct TestNumericConversion<Dst, Src, SIGN_TO_UNSIGN_NARROW>
{
    static void Test(const char *dst, const char *src, int line)
    {
        typedef numeric_limits<Src> SrcLimits;
        typedef numeric_limits<Dst> DstLimits;
        static_assert(
            (DstLimits::is_integer && SrcLimits::is_iec559) || (sizeof(Dst) < sizeof(Src)),
            "Destination must be narrower than source.");
        static_assert(SrcLimits::is_signed, "Source must be signed.");
        static_assert(!DstLimits::is_signed, "Destination must be unsigned.");

        const CheckedNumeric<Dst> checked_dst;
        TEST_EXPECTED_VALUE(1, checked_dst + static_cast<Src>(1));
        TEST_EXPECTED_FAILURE(checked_dst + SrcLimits::max());
        TEST_EXPECTED_FAILURE(checked_dst + static_cast<Src>(-1));
        TEST_EXPECTED_FAILURE(checked_dst + -SrcLimits::max());

        TEST_EXPECTED_RANGE(RANGE_OVERFLOW, SrcLimits::max());
        TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(1));
        TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, static_cast<Src>(-1));
        if (SrcLimits::is_iec559)
        {
            TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, SrcLimits::max() * -1);
            TEST_EXPECTED_RANGE(RANGE_OVERFLOW, SrcLimits::infinity());
            TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, SrcLimits::infinity() * -1);
            TEST_EXPECTED_RANGE(RANGE_INVALID, SrcLimits::quiet_NaN());
            if (DstLimits::is_integer)
            {
                if (SrcLimits::digits < DstLimits::digits)
                {
                    TEST_EXPECTED_RANGE(RANGE_OVERFLOW, static_cast<Src>(DstLimits::max()));
                }
                else
                {
                    TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(DstLimits::max()));
                }
                TEST_EXPECTED_RANGE(RANGE_VALID,
                                    static_cast<Src>(GetMaxConvertibleToFloat<Src, Dst>()));
                TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(DstLimits::min()));
            }
        }
        else
        {
            TEST_EXPECTED_RANGE(RANGE_UNDERFLOW, SrcLimits::min());
        }
    }
};

template <typename Dst, typename Src>
struct TestNumericConversion<Dst, Src, UNSIGN_TO_SIGN_NARROW_OR_EQUAL>
{
    static void Test(const char *dst, const char *src, int line)
    {
        typedef numeric_limits<Src> SrcLimits;
        typedef numeric_limits<Dst> DstLimits;
        static_assert(sizeof(Dst) <= sizeof(Src),
                      "Destination must be narrower or equal to source.");
        static_assert(!SrcLimits::is_signed, "Source must be unsigned.");
        static_assert(DstLimits::is_signed, "Destination must be signed.");

        const CheckedNumeric<Dst> checked_dst;
        TEST_EXPECTED_VALUE(1, checked_dst + static_cast<Src>(1));
        TEST_EXPECTED_FAILURE(checked_dst + SrcLimits::max());
        TEST_EXPECTED_VALUE(SrcLimits::min(), checked_dst + SrcLimits::min());

        TEST_EXPECTED_RANGE(RANGE_VALID, SrcLimits::min());
        TEST_EXPECTED_RANGE(RANGE_OVERFLOW, SrcLimits::max());
        TEST_EXPECTED_RANGE(RANGE_VALID, static_cast<Src>(1));
    }
};

// Helper macro to wrap displaying the conversion types and line numbers
#define TEST_NUMERIC_CONVERSION(d, s, t) TestNumericConversion<d, s, t>::Test(#d, #s, __LINE__)

TEST(SafeNumerics, IntMinOperations)
{
    TEST_NUMERIC_CONVERSION(int8_t, int8_t, SIGN_PRESERVING_VALUE_PRESERVING);
    TEST_NUMERIC_CONVERSION(uint8_t, uint8_t, SIGN_PRESERVING_VALUE_PRESERVING);

    TEST_NUMERIC_CONVERSION(int8_t, int, SIGN_PRESERVING_NARROW);
    TEST_NUMERIC_CONVERSION(uint8_t, unsigned int, SIGN_PRESERVING_NARROW);
    TEST_NUMERIC_CONVERSION(int8_t, float, SIGN_PRESERVING_NARROW);

    TEST_NUMERIC_CONVERSION(uint8_t, int8_t, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);

    TEST_NUMERIC_CONVERSION(uint8_t, int, SIGN_TO_UNSIGN_NARROW);
    TEST_NUMERIC_CONVERSION(uint8_t, intmax_t, SIGN_TO_UNSIGN_NARROW);
    TEST_NUMERIC_CONVERSION(uint8_t, float, SIGN_TO_UNSIGN_NARROW);

    TEST_NUMERIC_CONVERSION(int8_t, unsigned int, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
    TEST_NUMERIC_CONVERSION(int8_t, uintmax_t, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
}

TEST(SafeNumerics, IntOperations)
{
    TEST_NUMERIC_CONVERSION(int, int, SIGN_PRESERVING_VALUE_PRESERVING);
    TEST_NUMERIC_CONVERSION(unsigned int, unsigned int, SIGN_PRESERVING_VALUE_PRESERVING);
    TEST_NUMERIC_CONVERSION(int, int8_t, SIGN_PRESERVING_VALUE_PRESERVING);
    TEST_NUMERIC_CONVERSION(unsigned int, uint8_t, SIGN_PRESERVING_VALUE_PRESERVING);
    TEST_NUMERIC_CONVERSION(int, uint8_t, SIGN_PRESERVING_VALUE_PRESERVING);

    TEST_NUMERIC_CONVERSION(int, intmax_t, SIGN_PRESERVING_NARROW);
    TEST_NUMERIC_CONVERSION(unsigned int, uintmax_t, SIGN_PRESERVING_NARROW);
    TEST_NUMERIC_CONVERSION(int, float, SIGN_PRESERVING_NARROW);
    TEST_NUMERIC_CONVERSION(int, double, SIGN_PRESERVING_NARROW);

    TEST_NUMERIC_CONVERSION(unsigned int, int, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);
    TEST_NUMERIC_CONVERSION(unsigned int, int8_t, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);

    TEST_NUMERIC_CONVERSION(unsigned int, intmax_t, SIGN_TO_UNSIGN_NARROW);
    TEST_NUMERIC_CONVERSION(unsigned int, float, SIGN_TO_UNSIGN_NARROW);
    TEST_NUMERIC_CONVERSION(unsigned int, double, SIGN_TO_UNSIGN_NARROW);

    TEST_NUMERIC_CONVERSION(int, unsigned int, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
    TEST_NUMERIC_CONVERSION(int, uintmax_t, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
}

TEST(SafeNumerics, IntMaxOperations)
{
    TEST_NUMERIC_CONVERSION(intmax_t, intmax_t, SIGN_PRESERVING_VALUE_PRESERVING);
    TEST_NUMERIC_CONVERSION(uintmax_t, uintmax_t, SIGN_PRESERVING_VALUE_PRESERVING);
    TEST_NUMERIC_CONVERSION(intmax_t, int, SIGN_PRESERVING_VALUE_PRESERVING);
    TEST_NUMERIC_CONVERSION(uintmax_t, unsigned int, SIGN_PRESERVING_VALUE_PRESERVING);
    TEST_NUMERIC_CONVERSION(intmax_t, unsigned int, SIGN_PRESERVING_VALUE_PRESERVING);
    TEST_NUMERIC_CONVERSION(intmax_t, uint8_t, SIGN_PRESERVING_VALUE_PRESERVING);

    TEST_NUMERIC_CONVERSION(intmax_t, float, SIGN_PRESERVING_NARROW);
    TEST_NUMERIC_CONVERSION(intmax_t, double, SIGN_PRESERVING_NARROW);

    TEST_NUMERIC_CONVERSION(uintmax_t, int, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);
    TEST_NUMERIC_CONVERSION(uintmax_t, int8_t, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);

    TEST_NUMERIC_CONVERSION(uintmax_t, float, SIGN_TO_UNSIGN_NARROW);
    TEST_NUMERIC_CONVERSION(uintmax_t, double, SIGN_TO_UNSIGN_NARROW);

    TEST_NUMERIC_CONVERSION(intmax_t, uintmax_t, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
}

TEST(SafeNumerics, FloatOperations)
{
    TEST_NUMERIC_CONVERSION(float, intmax_t, SIGN_PRESERVING_VALUE_PRESERVING);
    TEST_NUMERIC_CONVERSION(float, uintmax_t, SIGN_PRESERVING_VALUE_PRESERVING);
    TEST_NUMERIC_CONVERSION(float, int, SIGN_PRESERVING_VALUE_PRESERVING);
    TEST_NUMERIC_CONVERSION(float, unsigned int, SIGN_PRESERVING_VALUE_PRESERVING);

    TEST_NUMERIC_CONVERSION(float, double, SIGN_PRESERVING_NARROW);
}

TEST(SafeNumerics, DoubleOperations)
{
    TEST_NUMERIC_CONVERSION(double, intmax_t, SIGN_PRESERVING_VALUE_PRESERVING);
    TEST_NUMERIC_CONVERSION(double, uintmax_t, SIGN_PRESERVING_VALUE_PRESERVING);
    TEST_NUMERIC_CONVERSION(double, int, SIGN_PRESERVING_VALUE_PRESERVING);
    TEST_NUMERIC_CONVERSION(double, unsigned int, SIGN_PRESERVING_VALUE_PRESERVING);
}

TEST(SafeNumerics, SizeTOperations)
{
    TEST_NUMERIC_CONVERSION(size_t, int, SIGN_TO_UNSIGN_WIDEN_OR_EQUAL);
    TEST_NUMERIC_CONVERSION(int, size_t, UNSIGN_TO_SIGN_NARROW_OR_EQUAL);
}

TEST(SafeNumerics, CastTests)
{
// MSVC catches and warns that we're forcing saturation in these tests.
// Since that's intentional, we need to shut this warning off.
#if defined(COMPILER_MSVC)
#pragma warning(disable : 4756)
#endif

    int small_positive      = 1;
    int small_negative      = -1;
    double double_small     = 1.0;
    double double_large     = numeric_limits<double>::max();
    double double_infinity  = numeric_limits<float>::infinity();
    double double_large_int = numeric_limits<int>::max();
    double double_small_int = numeric_limits<int>::min();

    // Just test that the casts compile, since the other tests cover logic.
    EXPECT_EQ(0, checked_cast<int>(static_cast<size_t>(0)));
    EXPECT_EQ(0, strict_cast<int>(static_cast<char>(0)));
    EXPECT_EQ(0, strict_cast<int>(static_cast<unsigned char>(0)));
    EXPECT_EQ(0U, strict_cast<unsigned>(static_cast<unsigned char>(0)));
    EXPECT_EQ(1ULL, static_cast<uint64_t>(StrictNumeric<size_t>(1U)));
    EXPECT_EQ(1ULL, static_cast<uint64_t>(SizeT(1U)));
    EXPECT_EQ(1U, static_cast<size_t>(StrictNumeric<unsigned>(1U)));

    EXPECT_TRUE(CheckedNumeric<uint64_t>(StrictNumeric<unsigned>(1U)).IsValid());
    EXPECT_TRUE(CheckedNumeric<int>(StrictNumeric<unsigned>(1U)).IsValid());
    EXPECT_FALSE(CheckedNumeric<unsigned>(StrictNumeric<int>(-1)).IsValid());

    EXPECT_TRUE(IsValueNegative(-1));
    EXPECT_TRUE(IsValueNegative(numeric_limits<int>::min()));
    EXPECT_FALSE(IsValueNegative(numeric_limits<unsigned>::min()));
    EXPECT_TRUE(IsValueNegative(-numeric_limits<double>::max()));
    EXPECT_FALSE(IsValueNegative(0));
    EXPECT_FALSE(IsValueNegative(1));
    EXPECT_FALSE(IsValueNegative(0u));
    EXPECT_FALSE(IsValueNegative(1u));
    EXPECT_FALSE(IsValueNegative(numeric_limits<int>::max()));
    EXPECT_FALSE(IsValueNegative(numeric_limits<unsigned>::max()));
    EXPECT_FALSE(IsValueNegative(numeric_limits<double>::max()));

    // These casts and coercions will fail to compile:
    // EXPECT_EQ(0, strict_cast<int>(static_cast<size_t>(0)));
    // EXPECT_EQ(0, strict_cast<size_t>(static_cast<int>(0)));
    // EXPECT_EQ(1ULL, StrictNumeric<size_t>(1));
    // EXPECT_EQ(1, StrictNumeric<size_t>(1U));

    // Test various saturation corner cases.
    EXPECT_EQ(saturated_cast<int>(small_negative), static_cast<int>(small_negative));
    EXPECT_EQ(saturated_cast<int>(small_positive), static_cast<int>(small_positive));
    EXPECT_EQ(saturated_cast<unsigned>(small_negative), static_cast<unsigned>(0));
    EXPECT_EQ(saturated_cast<int>(double_small), static_cast<int>(double_small));
    EXPECT_EQ(saturated_cast<int>(double_large), numeric_limits<int>::max());
    EXPECT_EQ(saturated_cast<float>(double_large), double_infinity);
    EXPECT_EQ(saturated_cast<float>(-double_large), -double_infinity);
    EXPECT_EQ(numeric_limits<int>::min(), saturated_cast<int>(double_small_int));
    EXPECT_EQ(numeric_limits<int>::max(), saturated_cast<int>(double_large_int));

    float not_a_number =
        std::numeric_limits<float>::infinity() - std::numeric_limits<float>::infinity();
    EXPECT_TRUE(std::isnan(not_a_number));
    EXPECT_EQ(0, saturated_cast<int>(not_a_number));
}

#if GTEST_HAS_DEATH_TEST

TEST(SafeNumerics, SaturatedCastChecks)
{
    float not_a_number =
        std::numeric_limits<float>::infinity() - std::numeric_limits<float>::infinity();
    EXPECT_TRUE(std::isnan(not_a_number));
    EXPECT_DEATH((saturated_cast<int, base::SaturatedCastNaNBehaviorCheck>(not_a_number)), "");
}

#endif  // GTEST_HAS_DEATH_TEST

TEST(SafeNumerics, IsValueInRangeForNumericType)
{
    EXPECT_TRUE(IsValueInRangeForNumericType<uint32_t>(0));
    EXPECT_TRUE(IsValueInRangeForNumericType<uint32_t>(1));
    EXPECT_TRUE(IsValueInRangeForNumericType<uint32_t>(2));
    EXPECT_FALSE(IsValueInRangeForNumericType<uint32_t>(-1));
    EXPECT_TRUE(IsValueInRangeForNumericType<uint32_t>(0xffffffffu));
    EXPECT_TRUE(IsValueInRangeForNumericType<uint32_t>(UINT64_C(0xffffffff)));
    EXPECT_FALSE(IsValueInRangeForNumericType<uint32_t>(UINT64_C(0x100000000)));
    EXPECT_FALSE(IsValueInRangeForNumericType<uint32_t>(UINT64_C(0x100000001)));
    EXPECT_FALSE(IsValueInRangeForNumericType<uint32_t>(std::numeric_limits<int32_t>::min()));
    EXPECT_FALSE(IsValueInRangeForNumericType<uint32_t>(std::numeric_limits<int64_t>::min()));

    EXPECT_TRUE(IsValueInRangeForNumericType<int32_t>(0));
    EXPECT_TRUE(IsValueInRangeForNumericType<int32_t>(1));
    EXPECT_TRUE(IsValueInRangeForNumericType<int32_t>(2));
    EXPECT_TRUE(IsValueInRangeForNumericType<int32_t>(-1));
    EXPECT_TRUE(IsValueInRangeForNumericType<int32_t>(0x7fffffff));
    EXPECT_TRUE(IsValueInRangeForNumericType<int32_t>(0x7fffffffu));
    EXPECT_FALSE(IsValueInRangeForNumericType<int32_t>(0x80000000u));
    EXPECT_FALSE(IsValueInRangeForNumericType<int32_t>(0xffffffffu));
    EXPECT_FALSE(IsValueInRangeForNumericType<int32_t>(INT64_C(0x80000000)));
    EXPECT_FALSE(IsValueInRangeForNumericType<int32_t>(INT64_C(0xffffffff)));
    EXPECT_FALSE(IsValueInRangeForNumericType<int32_t>(INT64_C(0x100000000)));
    EXPECT_TRUE(IsValueInRangeForNumericType<int32_t>(std::numeric_limits<int32_t>::min()));
    EXPECT_TRUE(IsValueInRangeForNumericType<int32_t>(
        static_cast<int64_t>(std::numeric_limits<int32_t>::min())));
    EXPECT_FALSE(IsValueInRangeForNumericType<int32_t>(
        static_cast<int64_t>(std::numeric_limits<int32_t>::min()) - 1));
    EXPECT_FALSE(IsValueInRangeForNumericType<int32_t>(std::numeric_limits<int64_t>::min()));

    EXPECT_TRUE(IsValueInRangeForNumericType<uint64_t>(0));
    EXPECT_TRUE(IsValueInRangeForNumericType<uint64_t>(1));
    EXPECT_TRUE(IsValueInRangeForNumericType<uint64_t>(2));
    EXPECT_FALSE(IsValueInRangeForNumericType<uint64_t>(-1));
    EXPECT_TRUE(IsValueInRangeForNumericType<uint64_t>(0xffffffffu));
    EXPECT_TRUE(IsValueInRangeForNumericType<uint64_t>(UINT64_C(0xffffffff)));
    EXPECT_TRUE(IsValueInRangeForNumericType<uint64_t>(UINT64_C(0x100000000)));
    EXPECT_TRUE(IsValueInRangeForNumericType<uint64_t>(UINT64_C(0x100000001)));
    EXPECT_FALSE(IsValueInRangeForNumericType<uint64_t>(std::numeric_limits<int32_t>::min()));
    EXPECT_FALSE(IsValueInRangeForNumericType<uint64_t>(INT64_C(-1)));
    EXPECT_FALSE(IsValueInRangeForNumericType<uint64_t>(std::numeric_limits<int64_t>::min()));

    EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(0));
    EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(1));
    EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(2));
    EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(-1));
    EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(0x7fffffff));
    EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(0x7fffffffu));
    EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(0x80000000u));
    EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(0xffffffffu));
    EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(INT64_C(0x80000000)));
    EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(INT64_C(0xffffffff)));
    EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(INT64_C(0x100000000)));
    EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(INT64_C(0x7fffffffffffffff)));
    EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(UINT64_C(0x7fffffffffffffff)));
    EXPECT_FALSE(IsValueInRangeForNumericType<int64_t>(UINT64_C(0x8000000000000000)));
    EXPECT_FALSE(IsValueInRangeForNumericType<int64_t>(UINT64_C(0xffffffffffffffff)));
    EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(std::numeric_limits<int32_t>::min()));
    EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(
        static_cast<int64_t>(std::numeric_limits<int32_t>::min())));
    EXPECT_TRUE(IsValueInRangeForNumericType<int64_t>(std::numeric_limits<int64_t>::min()));
}

TEST(SafeNumerics, CompoundNumericOperations)
{
    CheckedNumeric<int> a = 1;
    CheckedNumeric<int> b = 2;
    CheckedNumeric<int> c = 3;
    CheckedNumeric<int> d = 4;
    a += b;
    EXPECT_EQ(3, a.ValueOrDie());
    a -= c;
    EXPECT_EQ(0, a.ValueOrDie());
    d /= b;
    EXPECT_EQ(2, d.ValueOrDie());
    d *= d;
    EXPECT_EQ(4, d.ValueOrDie());

    CheckedNumeric<int> too_large = std::numeric_limits<int>::max();
    EXPECT_TRUE(too_large.IsValid());
    too_large += d;
    EXPECT_FALSE(too_large.IsValid());
    too_large -= d;
    EXPECT_FALSE(too_large.IsValid());
    too_large /= d;
    EXPECT_FALSE(too_large.IsValid());
}
