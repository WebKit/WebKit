/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

#include <charconv>
#include <cstddef>
#include <cstdio>
#include <random>
#include <wtf/DataLog.h>
#include <wtf/MonotonicTime.h>
#include <wtf/WeakRandom.h>
#include <wtf/dragonbox/dragonbox_to_chars.h>
#include <wtf/dtoa.h>
#include <wtf/dtoa/utils.h>

namespace TestWebKitAPI {

using DoubleToStringConverter = WTF::double_conversion::DoubleToStringConverter;
typedef WTF::double_conversion::StringBuilder DoubleConversionStringBuilder;

class DragonBoxTest {
public:
    DragonBoxTest()
        : builder1(&buffer1[0], sizeof(buffer1))
        , builder2(&buffer2[0], sizeof(buffer2))
        , converter(DoubleToStringConverter::EcmaScriptConverter())
    {
    }

    float randomFloat()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        return dis(gen);
    }

    double randomDouble()
    {
        return weakRandom.get();
    }

    char* dragonBoxToString(double x)
    {
        builder1.Reset();
        WTF::dragonbox::ToShortest(x, &builder1);
        return builder1.Finalize();
    }

    char* dragonBoxToString(float x)
    {
        builder1.Reset();
        WTF::dragonbox::ToShortest(x, &builder1);
        return builder1.Finalize();
    }

    char* dragonBoxToExponential(double x)
    {
        builder1.Reset();
        WTF::dragonbox::ToExponential(x, &builder1);
        return builder1.Finalize();
    }

    char* doubleConversionToString(double x)
    {
        builder2.Reset();
        converter.ToShortest(x, &builder2);
        return builder2.Finalize();
    }

    char* doubleConversionToString(float x)
    {
        builder2.Reset();
        converter.ToShortestSingle(x, &builder2);
        return builder2.Finalize();
    }

    char* doubleConversionToExponential(double x, int requested_decimal_digits = -1)
    {
        builder2.Reset();
        converter.ToExponential(x, requested_decimal_digits, &builder2);
        return builder2.Finalize();
    }

    NumberToStringBuffer buffer1;
    NumberToStringBuffer buffer2;
    DoubleConversionStringBuilder builder1;
    DoubleConversionStringBuilder builder2;
    const DoubleToStringConverter& converter;
    WeakRandom weakRandom;
    static constexpr bool verbose = false;
};

TEST(WTF, DragonBox)
{
    DragonBoxTest t;
    {
        auto toExponentialTest = [&](double x) {
            std::string str1(t.dragonBoxToExponential(x));
            std::string str2(t.doubleConversionToExponential(x));
            if (str1 != str2)
                printf("ToExponential double x=%.100f, str1=%s, str2=%s\n", x, str1.c_str(), str2.c_str());
            EXPECT_EQ(str1, str2);
        };

        auto toStringTest = [&](double x) {
            std::string str1(t.dragonBoxToString(x));
            std::string str2(t.doubleConversionToString(x));
            if (str1 != str2)
                printf("ToString double x=%.100f, str1=%s, str2=%s\n", x, str1.c_str(), str2.c_str());
            EXPECT_EQ(str1, str2);
        };

        unsigned testCount = 1e4;
        double bases[] = { -1e10, -1e5, 0, 10, 1e5, 1e10 };
        for (uint64_t base : bases) {
            for (unsigned i = 0; i < testCount; ++i) {
                double x = t.randomDouble() * base;
                toExponentialTest(x);
                toStringTest(x);
            }
        }

        double nums[] = { 0, -0 };
        for (double x : nums) {
            toExponentialTest(x);
            toStringTest(x);
        }
    }

    {
        auto toStringTest = [&](float x) {
            std::string str1(t.dragonBoxToString(x));
            std::string str2(t.doubleConversionToString(x));
            if (str1 != str2)
                printf("ToString float x=%.100f, str1=%s, str2=%s\n", x, str1.c_str(), str2.c_str());
            EXPECT_EQ(str1, str2);
        };

        unsigned testCount = 1e4;
        float bases[] = { -1e6, -1e5,  -1e4, -1e3, -1e2, -10, 0, 10, 1e2, 1e3, 1e4, 1e5, 1e6, };
        for (uint64_t base : bases) {
            for (unsigned i = 0; i < testCount; ++i) {
                float x = t.randomFloat() * base;
                toStringTest(x);
            }
        }

        float nums[] = { 0, -0 };
        for (float x : nums)
            toStringTest(x);
    }
}

TEST(WTF, DragonBox_perf)
{
    DragonBoxTest t;

    constexpr size_t size = 1e4;
    double nums1[size];
    double nums2[size];
    double nums3[size];
    double nums4[size];

    for (size_t i = 0; i < size; ++i) {
        double num1 = t.randomDouble();
        double num2 = num1 * 10;
        double num3 = num1 * 100;
        double num4 = num1 * 1000;
        nums1[i] = num1;
        nums2[i] = num2;
        nums3[i] = num3;
        nums4[i] = num4;
    }

    auto start = MonotonicTime::now();
    for (size_t i = 0; i < size; ++i) {
        t.dragonBoxToExponential(nums1[i]);
        t.dragonBoxToExponential(nums2[i]);
        t.dragonBoxToExponential(nums3[i]);
        t.dragonBoxToExponential(nums4[i]);
    }
    dataLogLnIf(DragonBoxTest::verbose, "dragonBoxToExponential        -> ", MonotonicTime::now() - start);

    start = MonotonicTime::now();
    for (size_t i = 0; i < size; ++i) {
        t.doubleConversionToExponential(nums1[i]);
        t.doubleConversionToExponential(nums2[i]);
        t.doubleConversionToExponential(nums3[i]);
        t.doubleConversionToExponential(nums4[i]);
    }
    dataLogLnIf(DragonBoxTest::verbose, "doubleConversionToExponential -> ", MonotonicTime::now() - start);

    start = MonotonicTime::now();
    for (size_t i = 0; i < size; ++i)
        t.dragonBoxToString(nums1[i]);
    dataLogLnIf(DragonBoxTest::verbose, "dragonBoxToString        -> ", MonotonicTime::now() - start, " base 0");

    start = MonotonicTime::now();
    for (size_t i = 0; i < size; ++i)
        t.doubleConversionToString(nums1[i]);
    dataLogLnIf(DragonBoxTest::verbose, "doubleConversionToString -> ", MonotonicTime::now() - start, " base 0");

    start = MonotonicTime::now();
    for (size_t i = 0; i < size; ++i)
        t.dragonBoxToString(nums2[i]);
    dataLogLnIf(DragonBoxTest::verbose, "dragonBoxToString        -> ", MonotonicTime::now() - start, " base 10");

    start = MonotonicTime::now();
    for (size_t i = 0; i < size; ++i)
        t.doubleConversionToString(nums2[i]);
    dataLogLnIf(DragonBoxTest::verbose, "doubleConversionToString -> ", MonotonicTime::now() - start, " base 10");

    start = MonotonicTime::now();
    for (size_t i = 0; i < size; ++i)
        t.dragonBoxToString(nums3[i]);
    dataLogLnIf(DragonBoxTest::verbose, "dragonBoxToString        -> ", MonotonicTime::now() - start, " base 100");

    start = MonotonicTime::now();
    for (size_t i = 0; i < size; ++i)
        t.doubleConversionToString(nums3[i]);
    dataLogLnIf(DragonBoxTest::verbose, "doubleConversionToString -> ", MonotonicTime::now() - start, " base 100");

    start = MonotonicTime::now();
    for (size_t i = 0; i < size; ++i)
        t.dragonBoxToString(nums4[i]);
    dataLogLnIf(DragonBoxTest::verbose, "dragonBoxToString        -> ", MonotonicTime::now() - start, " base 1000");

    start = MonotonicTime::now();
    for (size_t i = 0; i < size; ++i)
        t.doubleConversionToString(nums4[i]);
    dataLogLnIf(DragonBoxTest::verbose, "doubleConversionToString -> ", MonotonicTime::now() - start, " base 1000");
}

} // namespace TestWebKitAPI
