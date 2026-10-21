/*
 * Copyright (C) 2013-2026 Apple Inc. All rights reserved.
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

#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <string>
#include <wtf/DataLog.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Vector.h>
#include <wtf/WeakRandom.h>
#include <wtf/dtoa.h>
#include <wtf/dtoa/double-conversion.h>

namespace TestWebKitAPI {

using DoubleToStringConverter = WTF::double_conversion::DoubleToStringConverter;
using DoubleConversionStringBuilder = WTF::double_conversion::StringBuilder;

static std::string zmijToString(double x)
{
    NumberToStringBuffer buffer;
    auto span = numberToStringAndSize(x, buffer);
    std::array<char, numberToShortestStringBufferSize> shortestBuffer;
    size_t length = numberToShortestString(x, shortestBuffer);
    EXPECT_EQ(std::string(span.data(), span.size()), std::string(shortestBuffer.data(), length));
    return std::string(span.data(), span.size());
}

static std::string zmijToString(float x)
{
    NumberToStringBuffer buffer;
    auto span = numberToStringAndSize(x, buffer);
    return std::string(span.data(), span.size());
}

static std::string zmijToExponential(double x)
{
    NumberToStringBuffer buffer;
    auto span = numberToShortestExponentialString(x, buffer);
    return std::string(span.data(), span.size());
}

static std::string doubleConversionToString(double x)
{
    NumberToStringBuffer buffer;
    DoubleConversionStringBuilder builder { std::span<char> { buffer } };
    DoubleToStringConverter::EcmaScriptConverter().ToShortest(x, &builder);
    auto span = builder.Finalize();
    return std::string(span.data(), span.size());
}

static std::string doubleConversionToString(float x)
{
    NumberToStringBuffer buffer;
    DoubleConversionStringBuilder builder { std::span<char> { buffer } };
    DoubleToStringConverter::EcmaScriptConverter().ToShortestSingle(x, &builder);
    auto span = builder.Finalize();
    return std::string(span.data(), span.size());
}

static std::string doubleConversionToExponential(double x)
{
    NumberToStringBuffer buffer;
    DoubleConversionStringBuilder builder { std::span<char> { buffer } };
    DoubleToStringConverter::EcmaScriptConverter().ToExponential(x, -1, &builder);
    auto span = builder.Finalize();
    return std::string(span.data(), span.size());
}

static void checkDouble(double x)
{
    auto expected = doubleConversionToString(x);
    auto actual = zmijToString(x);
    EXPECT_EQ(actual, expected) << "ToString bits=" << std::bit_cast<uint64_t>(x);
    EXPECT_LE(actual.size(), 25u);

    if (std::isfinite(x))
        EXPECT_EQ(zmijToExponential(x), doubleConversionToExponential(x)) << "ToExponential bits=" << std::bit_cast<uint64_t>(x);
}

static void checkFloat(float x)
{
    EXPECT_EQ(zmijToString(x), doubleConversionToString(x)) << "ToString float bits=" << std::bit_cast<uint32_t>(x);
}

TEST(WTF, ZmijSpecialValues)
{
    double doubles[] = {
        0, -0.0, 1, -1, 0.1, 0.5, 1.5, 100, 123.456, 1e21, 1e20, 123456789012345680000.0, 1e-6, 1e-7, 1.5e-7, 0.000001234,
        9007199254740992.0, 9007199254740993.0, 4294967295.0, 4294967296.0, 1e22, 1e23, 5e-324, 2.2250738585072014e-308,
        2.2250738585072009e-308, 1.7976931348623157e308, 1e100, 1e-100, 1e308, 1e-308, 123e-20, 0.3, 2.0 / 3.0,
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
    };
    for (double x : doubles) {
        checkDouble(x);
        checkDouble(-x);
    }

    for (int exponent = -330; exponent <= 310; ++exponent) {
        double power = std::pow(10.0, exponent);
        checkDouble(power);
        checkDouble(std::nextafter(power, 0.0));
        checkDouble(std::nextafter(power, std::numeric_limits<double>::infinity()));
    }

    for (int binaryExponent : { -31, -30, -13, -12, 52, 53, 70, 71 }) {
        double power = std::ldexp(1.0, binaryExponent);
        for (double x = power, i = 0; i < 64; ++i, x = std::nextafter(x, 0.0))
            checkDouble(x);
        for (double x = power, i = 0; i < 64; ++i, x = std::nextafter(x, std::numeric_limits<double>::infinity()))
            checkDouble(x);
    }

    float floats[] = {
        0, -0.0f, 1, 0.1f, 1e-7f, 1e-6f, 1e20f, 1e21f, 1.17549435e-38f, 1.4e-45f, 3.40282347e38f, 16777216.0f, 16777217.0f,
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(),
    };
    for (float x : floats) {
        checkFloat(x);
        checkFloat(-x);
    }
}

TEST(WTF, ZmijRandom)
{
    WeakRandom random;
    constexpr unsigned testCount = 100000;

    for (unsigned i = 0; i < testCount; ++i) {
        uint64_t bits = (static_cast<uint64_t>(random.getUint32()) << 32) | random.getUint32();
        checkDouble(std::bit_cast<double>(bits));
        checkFloat(std::bit_cast<float>(random.getUint32()));
    }

    double bases[] = { -1e10, -1e5, 10, 1e5, 1e10 };
    for (double base : bases) {
        for (unsigned i = 0; i < testCount / 10; ++i)
            checkDouble(random.get() * base);
    }

    // Subnormals.
    for (unsigned i = 0; i < testCount / 10; ++i) {
        checkDouble(std::bit_cast<double>((static_cast<uint64_t>(random.getUint32() & 0xfffff) << 32) | random.getUint32()));
        checkFloat(std::bit_cast<float>(random.getUint32() & 0x7fffff));
    }
}

TEST(WTF, Zmij_perf)
{
    static constexpr bool verbose = false;
    static constexpr size_t size = 10'000;

    WeakRandom random;
    auto makeNumbers = [&](double scale) {
        Vector<double> numbers(size, [&](size_t) {
            return random.get() * scale;
        });
        return numbers;
    };

    auto measure = [&](ASCIILiteral name, ASCIILiteral label, const Vector<double>& numbers, auto&& convert) {
        NumberToStringBuffer buffer;
        size_t totalLength = 0;
        auto start = MonotonicTime::now();
        for (double number : numbers)
            totalLength += convert(number, buffer);
        dataLogLnIf(verbose, name, " -> ", MonotonicTime::now() - start, " ", label, " (", totalLength, " chars)");
    };

    auto zmijToStringLength = [](double number, NumberToStringBuffer& buffer) {
        return numberToStringAndSize(number, buffer).size();
    };
    auto zmijToExponentialLength = [](double number, NumberToStringBuffer& buffer) {
        return numberToShortestExponentialString(number, buffer).size();
    };
    auto doubleConversionToStringLength = [](double number, NumberToStringBuffer& buffer) {
        DoubleConversionStringBuilder builder { std::span<char> { buffer } };
        DoubleToStringConverter::EcmaScriptConverter().ToShortest(number, &builder);
        return builder.Finalize().size();
    };
    auto doubleConversionToExponentialLength = [](double number, NumberToStringBuffer& buffer) {
        DoubleConversionStringBuilder builder { std::span<char> { buffer } };
        DoubleToStringConverter::EcmaScriptConverter().ToExponential(number, -1, &builder);
        return builder.Finalize().size();
    };

    std::pair<ASCIILiteral, Vector<double>> inputs[] = {
        { "base 1"_s, makeNumbers(1) },
        { "base 10"_s, makeNumbers(10) },
        { "base 100"_s, makeNumbers(100) },
        { "base 1000"_s, makeNumbers(1000) },
        { "base 2^-13"_s, makeNumbers(std::ldexp(1.0, -13)) },
        { "base 2^70"_s, makeNumbers(std::ldexp(1.0, 70)) },
    };

    for (auto& [label, numbers] : inputs) {
        measure("zmijToString                 "_s, label, numbers, zmijToStringLength);
        measure("doubleConversionToString     "_s, label, numbers, doubleConversionToStringLength);
        measure("zmijToExponential            "_s, label, numbers, zmijToExponentialLength);
        measure("doubleConversionToExponential"_s, label, numbers, doubleConversionToExponentialLength);
    }
}

} // namespace TestWebKitAPI
