/*
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <wtf/MathExtras.h>

namespace TestWebKitAPI {

TEST(WTF, Lrint)
{
    EXPECT_EQ(lrint(-7.5), -8);
    EXPECT_EQ(lrint(-8.5), -8);
    EXPECT_EQ(lrint(-0.5), 0);
    EXPECT_EQ(lrint(0.5), 0);
    EXPECT_EQ(lrint(-0.5), 0);
    EXPECT_EQ(lrint(1.3), 1);
    EXPECT_EQ(lrint(1.7), 2);
    EXPECT_EQ(lrint(0), 0);
    EXPECT_EQ(lrint(-0), 0);
    if (sizeof(long int) == 8) {
        // Largest double number with 0.5 precision and one halfway rounding case below.
        EXPECT_EQ(lrint(pow(2.0, 52) - 0.5), pow(2.0, 52));
        EXPECT_EQ(lrint(pow(2.0, 52) - 1.5), pow(2.0, 52) - 2);
        // Smallest double number with 0.5 precision and one halfway rounding case above.
        EXPECT_EQ(lrint(-pow(2.0, 52) + 0.5), -pow(2.0, 52));
        EXPECT_EQ(lrint(-pow(2.0, 52) + 1.5), -pow(2.0, 52) + 2);
    }
}

TEST(WTF, clampToIntLong)
{
    if (sizeof(long) == sizeof(int))
        return;

    long maxInt = std::numeric_limits<int>::max();
    long minInt = std::numeric_limits<int>::min();
    long overflowInt = maxInt + 1;
    long underflowInt = minInt - 1;

    EXPECT_GT(overflowInt, maxInt);
    EXPECT_LT(underflowInt, minInt);

    EXPECT_EQ(clampTo<int>(maxInt), maxInt);
    EXPECT_EQ(clampTo<int>(minInt), minInt);

    EXPECT_EQ(clampTo<int>(overflowInt), maxInt);
    EXPECT_EQ(clampTo<int>(underflowInt), minInt);
}

TEST(WTF, clampToIntLongLong)
{
    long long maxInt = std::numeric_limits<int>::max();
    long long minInt = std::numeric_limits<int>::min();
    long long overflowInt = maxInt + 1;
    long long underflowInt = minInt - 1;

    EXPECT_GT(overflowInt, maxInt);
    EXPECT_LT(underflowInt, minInt);

    EXPECT_EQ(clampTo<int>(maxInt), maxInt);
    EXPECT_EQ(clampTo<int>(minInt), minInt);

    EXPECT_EQ(clampTo<int>(overflowInt), maxInt);
    EXPECT_EQ(clampTo<int>(underflowInt), minInt);
}

TEST(WTF, clampToIntegerFloat)
{
    // This test is inaccurate as floats will round the min / max integer
    // due to the narrow mantissa. However it will properly checks within
    // (close to the extreme) and outside the integer range.
    float maxInt = std::numeric_limits<int>::max();
    float minInt = std::numeric_limits<int>::min();
    float overflowInt = maxInt * 1.1;
    float underflowInt = minInt * 1.1;

    EXPECT_GT(overflowInt, maxInt);
    EXPECT_LT(underflowInt, minInt);

    // If maxInt == 2^31 - 1 (ie on I32 architecture), the closest float used to represent it is 2^31.
    EXPECT_NEAR(clampToInteger(maxInt), maxInt, 1);
    EXPECT_EQ(clampToInteger(minInt), minInt);

    EXPECT_NEAR(clampToInteger(overflowInt), maxInt, 1);
    EXPECT_EQ(clampToInteger(underflowInt), minInt);
}

TEST(WTF, clampToIntegerDouble)
{
    double maxInt = std::numeric_limits<int>::max();
    double minInt = std::numeric_limits<int>::min();
    double overflowInt = maxInt + 1;
    double underflowInt = minInt - 1;

    EXPECT_GT(overflowInt, maxInt);
    EXPECT_LT(underflowInt, minInt);

    EXPECT_EQ(clampToInteger(maxInt), maxInt);
    EXPECT_EQ(clampToInteger(minInt), minInt);

    EXPECT_EQ(clampToInteger(overflowInt), maxInt);
    EXPECT_EQ(clampToInteger(underflowInt), minInt);
}

TEST(WTF, clampToFloat)
{
    double maxFloat = std::numeric_limits<float>::max();
    double minFloat = -maxFloat;
    double overflowFloat = maxFloat * 1.1;
    double underflowFloat = minFloat * 1.1;

    EXPECT_GT(overflowFloat, maxFloat);
    EXPECT_LT(underflowFloat, minFloat);

    EXPECT_EQ(clampToFloat(maxFloat), maxFloat);
    EXPECT_EQ(clampToFloat(minFloat), minFloat);

    EXPECT_EQ(clampToFloat(overflowFloat), maxFloat);
    EXPECT_EQ(clampToFloat(underflowFloat), minFloat);

    EXPECT_EQ(clampToFloat(std::numeric_limits<float>::infinity()), maxFloat);
    EXPECT_EQ(clampToFloat(-std::numeric_limits<float>::infinity()), minFloat);
}

TEST(WTF, clampToUnsignedLong)
{
    if (sizeof(unsigned long) == sizeof(unsigned))
        return;

    unsigned long maxUnsigned = std::numeric_limits<unsigned>::max();
    unsigned long overflowUnsigned = maxUnsigned + 1;

    EXPECT_GT(overflowUnsigned, maxUnsigned);

    EXPECT_EQ(clampTo<unsigned>(maxUnsigned), maxUnsigned);

    EXPECT_EQ(clampTo<unsigned>(overflowUnsigned), maxUnsigned);
    EXPECT_EQ(clampTo<unsigned>(-1), 0u);
}

TEST(WTF, clampToUnsignedLongLong)
{
    unsigned long long maxUnsigned = std::numeric_limits<unsigned>::max();
    unsigned long long overflowUnsigned = maxUnsigned + 1;

    EXPECT_GT(overflowUnsigned, maxUnsigned);

    EXPECT_EQ(clampTo<unsigned>(maxUnsigned), maxUnsigned);

    EXPECT_EQ(clampTo<unsigned>(overflowUnsigned), maxUnsigned);
    EXPECT_EQ(clampTo<unsigned>(-1), 0u);
}

#if !COMPILER(MSVC)
template<typename TargetType, typename SourceType>
static void testClampFloatingPointToFloatingPoint()
{
    // No clamping.
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(0)), static_cast<TargetType>(0));
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(1)), static_cast<TargetType>(1));
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(1.5)), static_cast<TargetType>(1.5));
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(-1)), static_cast<TargetType>(-1));
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(-1.5)), static_cast<TargetType>(-1.5));

    // Explicit boundaries, clamped or not.
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(-42), static_cast<SourceType>(-42.5)), static_cast<TargetType>(-42));
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(-43), static_cast<SourceType>(-42.5)), static_cast<TargetType>(static_cast<SourceType>(-42.5)));
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(42), static_cast<SourceType>(41), static_cast<SourceType>(42.5)), static_cast<TargetType>(42));
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(43), static_cast<SourceType>(41), static_cast<SourceType>(42.5)), static_cast<TargetType>(static_cast<SourceType>(42.5)));

    // Integer bounds.
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<int32_t>::max()) + 1), static_cast<TargetType>(std::numeric_limits<int32_t>::max()) + 1);
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<int64_t>::max())), static_cast<TargetType>(std::numeric_limits<int64_t>::max()));
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<int32_t>::max()) + 1), static_cast<TargetType>(std::numeric_limits<int32_t>::max()) + 1);

    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<int32_t>::min())), static_cast<TargetType>(std::numeric_limits<int32_t>::min()));
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<int64_t>::min())), static_cast<TargetType>(std::numeric_limits<int64_t>::min()));

    if (std::is_same<TargetType, double>::value && std::is_same<SourceType, float>::value) {
        // If the source is float and target is double, the input of those cases has lost bits in float.
        // In that case, we also round the expectation to float.
        EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<int32_t>::max())), static_cast<TargetType>(static_cast<SourceType>(std::numeric_limits<int32_t>::max())));
        EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<int32_t>::min()) - 1), static_cast<TargetType>(static_cast<SourceType>(std::numeric_limits<int32_t>::min()) - 1));
        EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<int32_t>::min()) - 1), static_cast<TargetType>(static_cast<SourceType>(std::numeric_limits<int32_t>::min()) - 1));
    } else {
        EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<int32_t>::max())), static_cast<TargetType>(std::numeric_limits<int32_t>::max()));
        EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<int32_t>::min()) - 1), static_cast<TargetType>(std::numeric_limits<int32_t>::min()) - 1);
        EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<int32_t>::min()) - 1), static_cast<TargetType>(std::numeric_limits<int32_t>::min()) - 1);
    }

    // At the limit.
    EXPECT_EQ(clampTo<TargetType>(std::numeric_limits<TargetType>::max()), std::numeric_limits<TargetType>::max());
    EXPECT_EQ(clampTo<TargetType>(-std::numeric_limits<TargetType>::max()), -std::numeric_limits<TargetType>::max());

    // At Epsilon from the limit.
    TargetType epsilon = std::numeric_limits<TargetType>::epsilon();
    EXPECT_EQ(clampTo<TargetType>(std::numeric_limits<TargetType>::max() - epsilon), std::numeric_limits<TargetType>::max() - epsilon);
    EXPECT_EQ(clampTo<TargetType>(-std::numeric_limits<TargetType>::max() + epsilon), -std::numeric_limits<TargetType>::max() + epsilon);

    // Infinity.
    EXPECT_EQ(clampTo<TargetType>(std::numeric_limits<SourceType>::infinity()), std::numeric_limits<TargetType>::max());
    EXPECT_EQ(clampTo<TargetType>(-std::numeric_limits<SourceType>::infinity()), -std::numeric_limits<TargetType>::max());
}

TEST(WTF, clampFloatingPointToFloatingPoint)
{
    testClampFloatingPointToFloatingPoint<float, float>();
    testClampFloatingPointToFloatingPoint<double, double>();

    testClampFloatingPointToFloatingPoint<double, float>();
    testClampFloatingPointToFloatingPoint<float, double>();

    // Large double into smaller float.
    EXPECT_EQ(clampTo<float>(static_cast<double>(std::numeric_limits<float>::max())), std::numeric_limits<float>::max());
    EXPECT_EQ(clampTo<float>(-static_cast<double>(std::numeric_limits<float>::max())), -std::numeric_limits<float>::max());
    EXPECT_EQ(clampTo<float>(static_cast<double>(std::numeric_limits<float>::max()) + 1), std::numeric_limits<float>::max());
    EXPECT_EQ(clampTo<float>(-static_cast<double>(std::numeric_limits<float>::max()) - 1), -std::numeric_limits<float>::max());
    EXPECT_EQ(clampTo<float>(std::numeric_limits<double>::max()), std::numeric_limits<float>::max());
    EXPECT_EQ(clampTo<float>(-std::numeric_limits<double>::max()), -std::numeric_limits<float>::max());

    float floatEspilon = std::numeric_limits<float>::epsilon();
    double doubleEspilon = std::numeric_limits<double>::epsilon();
    EXPECT_EQ(clampTo<float>(static_cast<double>(std::numeric_limits<float>::max()) + doubleEspilon), std::numeric_limits<float>::max());
    EXPECT_EQ(clampTo<float>(static_cast<double>(std::numeric_limits<float>::max()) - doubleEspilon), std::numeric_limits<float>::max());
    EXPECT_EQ(clampTo<float>(static_cast<double>(std::numeric_limits<float>::max()) + floatEspilon), std::numeric_limits<float>::max());
    EXPECT_EQ(clampTo<float>(static_cast<double>(std::numeric_limits<float>::max()) - floatEspilon), std::numeric_limits<float>::max() - floatEspilon);
}
#endif // !COMPILER(MSVC)

template<typename FloatingPointType>
static void testClampFloatingPointToInteger()
{
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(0)), 0);
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(1)), 1);
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(-1)), -1);
    if (std::is_same<FloatingPointType, double>::value)
        EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<int32_t>::max()) - 1.f), std::numeric_limits<int32_t>::max() - 1);
    else
        EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<int32_t>::max()) - 1.f), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<int32_t>::max())), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<int32_t>::max()) + 1.f), std::numeric_limits<int32_t>::max());

    if (std::is_same<FloatingPointType, double>::value)
        EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<int32_t>::min()) + 1.f), std::numeric_limits<int32_t>::min() + 1);
    else
        EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<int32_t>::min()) + 1.f), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<int32_t>::min())), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<int32_t>::min()) - 1.f), std::numeric_limits<int32_t>::min());

    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<uint32_t>::max())), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<uint32_t>::max()) + 1.f), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<uint32_t>::min())), 0.f);

    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<int64_t>::max())), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<int64_t>::max()) + 1.f), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<int64_t>::min())), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<int64_t>::min()) - 1.f), std::numeric_limits<int32_t>::min());

    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<uint64_t>::max())), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<uint64_t>::max()) + 1.f), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<uint64_t>::min())), 0.f);

    FloatingPointType epsilon = std::numeric_limits<FloatingPointType>::epsilon();
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<int32_t>::max()) - epsilon), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<int32_t>::max()) + epsilon), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<int32_t>::min()) - epsilon), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<int32_t>::min()) + epsilon), std::numeric_limits<int32_t>::min());

    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(std::numeric_limits<FloatingPointType>::infinity())), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(clampTo<int32_t>(static_cast<FloatingPointType>(-std::numeric_limits<FloatingPointType>::infinity())), std::numeric_limits<int32_t>::min());
}

TEST(WTF, clampFloatToInt)
{
    testClampFloatingPointToInteger<float>();
    testClampFloatingPointToInteger<double>();

    // 2**24 = 16777216, the largest integer representable exactly as float.
    EXPECT_EQ(clampTo<int32_t>(static_cast<float>(16777215)), 16777215);
    EXPECT_EQ(clampTo<int32_t>(static_cast<float>(16777216)), 16777216);
    EXPECT_EQ(clampTo<int32_t>(static_cast<float>(16777217)), 16777216);
    EXPECT_EQ(clampTo<int32_t>(static_cast<double>(16777216)), 16777216);
    EXPECT_EQ(clampTo<int32_t>(static_cast<double>(16777217)), 16777217);

    EXPECT_EQ(clampTo<int16_t>(static_cast<float>(16777215)), std::numeric_limits<int16_t>::max());
    EXPECT_EQ(clampTo<int16_t>(static_cast<float>(16777216)), std::numeric_limits<int16_t>::max());
    EXPECT_EQ(clampTo<int16_t>(static_cast<float>(16777217)), std::numeric_limits<int16_t>::max());

    // 2**53 = 9007199254740992, the largest integer representable exactly as double.
    EXPECT_EQ(clampTo<uint64_t>(static_cast<double>(9007199254740991)), static_cast<uint64_t>(9007199254740991));
    EXPECT_EQ(clampTo<uint64_t>(static_cast<double>(9007199254740992)), static_cast<uint64_t>(9007199254740992));
    EXPECT_EQ(clampTo<uint64_t>(static_cast<double>(9007199254740993)), static_cast<uint64_t>(9007199254740992));

    EXPECT_EQ(clampTo<int32_t>(static_cast<double>(9007199254740991)), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(clampTo<int32_t>(static_cast<double>(9007199254740992)), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(clampTo<int32_t>(static_cast<double>(9007199254740993)), std::numeric_limits<int32_t>::max());

    // Test the double at the edge of max/min and max-1/min+1.
    double intMax = static_cast<double>(std::numeric_limits<int32_t>::max());
    EXPECT_EQ(clampTo<int32_t>(intMax), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(clampTo<int32_t>(std::nextafter(intMax, 0)), std::numeric_limits<int32_t>::max() - 1);
    EXPECT_EQ(clampTo<int32_t>(std::nextafter(intMax, std::numeric_limits<double>::max())), std::numeric_limits<int32_t>::max());

    EXPECT_EQ(clampTo<int32_t>(std::nextafter(intMax - 1., 0)), std::numeric_limits<int32_t>::max() - 2);
    EXPECT_EQ(clampTo<int32_t>(intMax - 1), std::numeric_limits<int32_t>::max() - 1);
    EXPECT_EQ(clampTo<int32_t>(std::nextafter(intMax - 1., std::numeric_limits<double>::max())), std::numeric_limits<int32_t>::max() - 1);

    double intMin = static_cast<double>(std::numeric_limits<int32_t>::min());
    EXPECT_EQ(clampTo<int32_t>(intMin), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(clampTo<int32_t>(std::nextafter(intMin, 0)), std::numeric_limits<int32_t>::min() + 1);
    EXPECT_EQ(clampTo<int32_t>(std::nextafter(intMin, -std::numeric_limits<double>::max())), std::numeric_limits<int32_t>::min());

    EXPECT_EQ(clampTo<int32_t>(std::nextafter(intMin + 1, 0)), std::numeric_limits<int32_t>::min() + 2);
    EXPECT_EQ(clampTo<int32_t>(intMin + 1), std::numeric_limits<int32_t>::min() + 1);
    EXPECT_EQ(clampTo<int32_t>(std::nextafter(intMin + 1, -std::numeric_limits<double>::max())), std::numeric_limits<int32_t>::min() + 1);
}

template<typename TargetType, typename SourceType>
static void testClampSameSignIntegers()
{
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(0)), static_cast<TargetType>(0));
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(1)), static_cast<TargetType>(1));
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(-1)), std::numeric_limits<TargetType>::is_signed ? static_cast<TargetType>(-1) : std::numeric_limits<TargetType>::max());

    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<TargetType>::min())), std::numeric_limits<TargetType>::min());
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<TargetType>::max())), std::numeric_limits<TargetType>::max());

    EXPECT_EQ(clampTo<TargetType>(std::numeric_limits<SourceType>::min()), std::numeric_limits<TargetType>::min());
    EXPECT_EQ(clampTo<TargetType>(std::numeric_limits<SourceType>::max()), std::numeric_limits<TargetType>::max());
}

TEST(WTF, clampSameSignIntegers)
{
    testClampSameSignIntegers<char, char>();
    testClampSameSignIntegers<unsigned char, unsigned char>();
    testClampSameSignIntegers<char, int32_t>();
    testClampSameSignIntegers<unsigned char, uint32_t>();
    testClampSameSignIntegers<char, int64_t>();
    testClampSameSignIntegers<unsigned char, uint64_t>();

    testClampSameSignIntegers<int32_t, int32_t>();
    testClampSameSignIntegers<uint32_t, uint32_t>();
    testClampSameSignIntegers<int32_t, int64_t>();
    testClampSameSignIntegers<uint32_t, uint64_t>();
    testClampSameSignIntegers<int16_t, int64_t>();
    testClampSameSignIntegers<uint16_t, uint64_t>();
}

template<typename TargetType, typename SourceType>
static void testClampUnsignedToSigned()
{
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(0)), static_cast<TargetType>(0));
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(1)), static_cast<TargetType>(1));

    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<TargetType>::max()) - 1), std::numeric_limits<TargetType>::max() - 1);
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<TargetType>::max())), std::numeric_limits<TargetType>::max());
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<TargetType>::max()) + 1), std::numeric_limits<TargetType>::max());
    EXPECT_EQ(clampTo<TargetType>(std::numeric_limits<SourceType>::max()), std::numeric_limits<TargetType>::max());
    EXPECT_EQ(clampTo<TargetType>(std::numeric_limits<SourceType>::max() - 1), std::numeric_limits<TargetType>::max());
}

TEST(WTF, clampUnsignedToSigned)
{
    testClampUnsignedToSigned<char, unsigned char>();
    testClampUnsignedToSigned<char, uint32_t>();
    testClampUnsignedToSigned<int32_t, uint32_t>();
    testClampUnsignedToSigned<int64_t, uint64_t>();
    testClampUnsignedToSigned<int32_t, uint64_t>();
    testClampUnsignedToSigned<int16_t, uint32_t>();
    testClampUnsignedToSigned<int16_t, uint64_t>();
}

template<typename TargetType, typename SourceType>
static void testClampSignedToUnsigned()
{
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(0)), static_cast<TargetType>(0));
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(1)), static_cast<TargetType>(1));
    EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(-1)), static_cast<TargetType>(0));

    if (sizeof(TargetType) < sizeof(SourceType)) {
        EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<TargetType>::min())), static_cast<TargetType>(0));
        EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<TargetType>::max()) - 1), std::numeric_limits<TargetType>::max() - 1);
        EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<TargetType>::max())), std::numeric_limits<TargetType>::max());
        EXPECT_EQ(clampTo<TargetType>(static_cast<SourceType>(std::numeric_limits<TargetType>::max()) + 1), std::numeric_limits<TargetType>::max());
    }

    EXPECT_EQ(clampTo<TargetType>(std::numeric_limits<SourceType>::min()), static_cast<TargetType>(0));
    if (sizeof(TargetType) < sizeof(SourceType))
        EXPECT_EQ(clampTo<TargetType>(std::numeric_limits<SourceType>::max()), std::numeric_limits<TargetType>::max());
    else
        EXPECT_EQ(clampTo<TargetType>(std::numeric_limits<SourceType>::max()), static_cast<TargetType>(std::numeric_limits<SourceType>::max()));
}

TEST(WTF, clampSignedToUnsigned)
{
    testClampSignedToUnsigned<unsigned char, char>();
    testClampSignedToUnsigned<unsigned char, int32_t>();
    testClampSignedToUnsigned<uint32_t, int32_t>();
    testClampSignedToUnsigned<uint64_t, int64_t>();
    testClampSignedToUnsigned<uint32_t, int64_t>();
    testClampSignedToUnsigned<uint16_t, int32_t>();
    testClampSignedToUnsigned<uint16_t, int64_t>();
}

TEST(WTF, roundUpToPowerOfTwo)
{
    EXPECT_EQ(WTF::roundUpToPowerOfTwo(UINT32_MAX), 0U);
    EXPECT_EQ(WTF::roundUpToPowerOfTwo(1U << 31), (1U << 31));
    EXPECT_EQ(WTF::roundUpToPowerOfTwo((1U << 31) + 1), 0U);
}

TEST(WTF, clz)
{
    EXPECT_EQ(WTF::clz<int32_t>(1), 31U);
    EXPECT_EQ(WTF::clz<int32_t>(42), 26U);
    EXPECT_EQ(WTF::clz<uint32_t>(static_cast<uint32_t>(-1)), 0U);
    EXPECT_EQ(WTF::clz<uint32_t>(static_cast<uint32_t>(std::numeric_limits<int32_t>::min()) >> 1), 1U);
    EXPECT_EQ(WTF::clz<uint32_t>(0), 32U);

    EXPECT_EQ(WTF::clz<int8_t>(42), 2U);
    EXPECT_EQ(WTF::clz<int8_t>(3), 6U);
    EXPECT_EQ(WTF::clz<uint8_t>(static_cast<uint8_t>(-1)), 0U);
    EXPECT_EQ(WTF::clz<uint8_t>(0), 8U);

    EXPECT_EQ(WTF::clz<int64_t>(-1), 0U);
    EXPECT_EQ(WTF::clz<int64_t>(1), 63U);
    EXPECT_EQ(WTF::clz<int64_t>(3), 62U);
    EXPECT_EQ(WTF::clz<uint64_t>(42), 58U);
    EXPECT_EQ(WTF::clz<uint64_t>(0), 64U);
}

TEST(WTF, ctz)
{
    EXPECT_EQ(WTF::ctz<int32_t>(1), 0U);
    EXPECT_EQ(WTF::ctz<int32_t>(42), 1U);
    EXPECT_EQ(WTF::ctz<uint32_t>(static_cast<uint32_t>(-1)), 0U);
    EXPECT_EQ(WTF::ctz<uint32_t>(static_cast<uint32_t>(std::numeric_limits<int32_t>::min()) >> 1), 30U);
    EXPECT_EQ(WTF::ctz<uint32_t>(0), 32U);

    EXPECT_EQ(WTF::ctz<int8_t>(42), 1U);
    EXPECT_EQ(WTF::ctz<int8_t>(3), 0U);
    EXPECT_EQ(WTF::ctz<uint8_t>(static_cast<uint8_t>(-1)), 0U);
    EXPECT_EQ(WTF::ctz<uint8_t>(0), 8U);

    EXPECT_EQ(WTF::ctz<int64_t>(static_cast<uint32_t>(-1)), 0U);
    EXPECT_EQ(WTF::ctz<int64_t>(1), 0U);
    EXPECT_EQ(WTF::ctz<int64_t>(3), 0U);
    EXPECT_EQ(WTF::ctz<uint64_t>(42), 1U);
    EXPECT_EQ(WTF::ctz<uint64_t>(0), 64U);
}

TEST(WTF, getLSBSet)
{
    EXPECT_EQ(WTF::getLSBSet<int32_t>(1), 0U);
    EXPECT_EQ(WTF::getLSBSet<int32_t>(42), 1U);
    EXPECT_EQ(WTF::getLSBSet<uint32_t>(static_cast<uint32_t>(-1)), 0U);
    EXPECT_EQ(WTF::getLSBSet<uint32_t>(static_cast<uint32_t>(std::numeric_limits<int32_t>::min()) >> 1), 30U);

    EXPECT_EQ(WTF::getLSBSet<int8_t>(42), 1U);
    EXPECT_EQ(WTF::getLSBSet<int8_t>(3), 0U);
    EXPECT_EQ(WTF::getLSBSet<uint8_t>(static_cast<uint8_t>(-1)), 0U);

    EXPECT_EQ(WTF::getLSBSet<int64_t>(-1), 0U);
    EXPECT_EQ(WTF::getLSBSet<int64_t>(1), 0U);
    EXPECT_EQ(WTF::getLSBSet<int64_t>(3), 0U);
    EXPECT_EQ(WTF::getLSBSet<uint64_t>(42), 1U);
}

TEST(WTF, getMSBSet)
{
    EXPECT_EQ(WTF::getMSBSet<int32_t>(1), 0U);
    EXPECT_EQ(WTF::getMSBSet<int32_t>(42), 5U);
    EXPECT_EQ(WTF::getMSBSet<uint32_t>(static_cast<uint32_t>(-1)), 31U);
    EXPECT_EQ(WTF::getMSBSet<uint32_t>(static_cast<uint32_t>(std::numeric_limits<int32_t>::min()) >> 1), 30U);

    EXPECT_EQ(WTF::getMSBSet<int8_t>(42), 5U);
    EXPECT_EQ(WTF::getMSBSet<int8_t>(3), 1U);
    EXPECT_EQ(WTF::getMSBSet<uint8_t>(static_cast<uint8_t>(-1)), 7U);

    EXPECT_EQ(WTF::getMSBSet<int64_t>(-1), 63U);
    EXPECT_EQ(WTF::getMSBSet<int64_t>(1), 0U);
    EXPECT_EQ(WTF::getMSBSet<int64_t>(3), 1U);
    EXPECT_EQ(WTF::getMSBSet<uint64_t>(42), 5U);
}

TEST(WTF, clzConstexpr)
{
    EXPECT_EQ(WTF::clzConstexpr<int32_t>(1), 31U);
    EXPECT_EQ(WTF::clzConstexpr<int32_t>(42), 26U);
    EXPECT_EQ(WTF::clzConstexpr<uint32_t>(static_cast<uint32_t>(-1)), 0U);
    EXPECT_EQ(WTF::clzConstexpr<uint32_t>(static_cast<uint32_t>(std::numeric_limits<int32_t>::min()) >> 1), 1U);
    EXPECT_EQ(WTF::clzConstexpr<uint32_t>(0), 32U);

    EXPECT_EQ(WTF::clzConstexpr<int8_t>(42), 2U);
    EXPECT_EQ(WTF::clzConstexpr<int8_t>(3), 6U);
    EXPECT_EQ(WTF::clzConstexpr<uint8_t>(static_cast<uint8_t>(-1)), 0U);
    EXPECT_EQ(WTF::clzConstexpr<uint8_t>(0), 8U);

    EXPECT_EQ(WTF::clzConstexpr<int64_t>(-1), 0U);
    EXPECT_EQ(WTF::clzConstexpr<int64_t>(1), 63U);
    EXPECT_EQ(WTF::clzConstexpr<int64_t>(3), 62U);
    EXPECT_EQ(WTF::clzConstexpr<uint64_t>(42), 58U);
    EXPECT_EQ(WTF::clzConstexpr<uint64_t>(0), 64U);
}

TEST(WTF, ctzConstexpr)
{
    EXPECT_EQ(WTF::ctzConstexpr<int32_t>(1), 0U);
    EXPECT_EQ(WTF::ctzConstexpr<int32_t>(42), 1U);
    EXPECT_EQ(WTF::ctzConstexpr<uint32_t>(static_cast<uint32_t>(-1)), 0U);
    EXPECT_EQ(WTF::ctzConstexpr<uint32_t>(static_cast<uint32_t>(std::numeric_limits<int32_t>::min()) >> 1), 30U);
    EXPECT_EQ(WTF::ctzConstexpr<uint32_t>(0), 32U);

    EXPECT_EQ(WTF::ctzConstexpr<int8_t>(42), 1U);
    EXPECT_EQ(WTF::ctzConstexpr<int8_t>(3), 0U);
    EXPECT_EQ(WTF::ctzConstexpr<uint8_t>(static_cast<uint8_t>(-1)), 0U);
    EXPECT_EQ(WTF::ctzConstexpr<uint8_t>(0), 8U);

    EXPECT_EQ(WTF::ctzConstexpr<int64_t>(static_cast<uint32_t>(-1)), 0U);
    EXPECT_EQ(WTF::ctzConstexpr<int64_t>(1), 0U);
    EXPECT_EQ(WTF::ctzConstexpr<int64_t>(3), 0U);
    EXPECT_EQ(WTF::ctzConstexpr<uint64_t>(42), 1U);
    EXPECT_EQ(WTF::ctzConstexpr<uint64_t>(0), 64U);
}

TEST(WTF, getLSBSetConstexpr)
{
    EXPECT_EQ(WTF::getLSBSetConstexpr<int32_t>(1), 0U);
    EXPECT_EQ(WTF::getLSBSetConstexpr<int32_t>(42), 1U);
    EXPECT_EQ(WTF::getLSBSetConstexpr<uint32_t>(static_cast<uint32_t>(-1)), 0U);
    EXPECT_EQ(WTF::getLSBSetConstexpr<uint32_t>(static_cast<uint32_t>(std::numeric_limits<int32_t>::min()) >> 1), 30U);

    EXPECT_EQ(WTF::getLSBSetConstexpr<int8_t>(42), 1U);
    EXPECT_EQ(WTF::getLSBSetConstexpr<int8_t>(3), 0U);
    EXPECT_EQ(WTF::getLSBSetConstexpr<uint8_t>(static_cast<uint8_t>(-1)), 0U);

    EXPECT_EQ(WTF::getLSBSetConstexpr<int64_t>(-1), 0U);
    EXPECT_EQ(WTF::getLSBSetConstexpr<int64_t>(1), 0U);
    EXPECT_EQ(WTF::getLSBSetConstexpr<int64_t>(3), 0U);
    EXPECT_EQ(WTF::getLSBSetConstexpr<uint64_t>(42), 1U);
}

TEST(WTF, getMSBSetConstexpr)
{
    EXPECT_EQ(WTF::getMSBSetConstexpr<int32_t>(1), 0U);
    EXPECT_EQ(WTF::getMSBSetConstexpr<int32_t>(42), 5U);
    EXPECT_EQ(WTF::getMSBSetConstexpr<uint32_t>(static_cast<uint32_t>(-1)), 31U);
    EXPECT_EQ(WTF::getMSBSetConstexpr<uint32_t>(static_cast<uint32_t>(std::numeric_limits<int32_t>::min()) >> 1), 30U);

    EXPECT_EQ(WTF::getMSBSetConstexpr<int8_t>(42), 5U);
    EXPECT_EQ(WTF::getMSBSetConstexpr<int8_t>(3), 1U);
    EXPECT_EQ(WTF::getMSBSetConstexpr<uint8_t>(static_cast<uint8_t>(-1)), 7U);

    EXPECT_EQ(WTF::getMSBSetConstexpr<int64_t>(-1), 63U);
    EXPECT_EQ(WTF::getMSBSetConstexpr<int64_t>(1), 0U);
    EXPECT_EQ(WTF::getMSBSetConstexpr<int64_t>(3), 1U);
    EXPECT_EQ(WTF::getMSBSetConstexpr<uint64_t>(42), 5U);
}

} // namespace TestWebKitAPI
