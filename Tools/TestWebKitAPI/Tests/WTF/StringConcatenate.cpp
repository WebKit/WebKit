/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#include "Test.h"
#include <cstddef>
#include <cstdint>
#include <unicode/uvernum.h>
#include <wtf/HexNumber.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace TestWebKitAPI {

enum class BoolEnum : bool {
    A,
    B
};

enum class SignedEnum : int8_t {
    A = -1,
    B = 0,
    C = 1
};

enum class UnsignedEnum : uint8_t {
    A,
    B,
    C
};

static int arr[2];
struct S {
    char c;
    int i;
};

TEST(WTF, StringConcatenate)
{
    EXPECT_STREQ("hello world", makeString("hello", " ", "world").utf8().data());
}

TEST(WTF, StringConcatenate_Int)
{
    EXPECT_EQ(5u, WTF::lengthOfIntegerAsString(17890));
    EXPECT_STREQ("hello 17890 world", makeString("hello ", 17890 , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", 17890l , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", 17890ll , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", static_cast<int64_t>(17890) , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", static_cast<int64_t>(17890) , " world").utf8().data());

    EXPECT_EQ(6u, WTF::lengthOfIntegerAsString(-17890));
    EXPECT_STREQ("hello -17890 world", makeString("hello ", -17890 , " world").utf8().data());
    EXPECT_STREQ("hello -17890 world", makeString("hello ", -17890l , " world").utf8().data());
    EXPECT_STREQ("hello -17890 world", makeString("hello ", -17890ll , " world").utf8().data());
    EXPECT_STREQ("hello -17890 world", makeString("hello ", static_cast<int64_t>(-17890) , " world").utf8().data());
    EXPECT_STREQ("hello -17890 world", makeString("hello ", static_cast<int64_t>(-17890) , " world").utf8().data());

    EXPECT_EQ(1u, WTF::lengthOfIntegerAsString(0));
    EXPECT_STREQ("hello 0 world", makeString("hello ", 0 , " world").utf8().data());

    EXPECT_STREQ("hello 42 world", makeString("hello ", static_cast<signed char>(42) , " world").utf8().data());
    EXPECT_STREQ("hello 42 world", makeString("hello ", static_cast<short>(42) , " world").utf8().data());
    EXPECT_STREQ("hello 1 world", makeString("hello ", &arr[1] - &arr[0] , " world").utf8().data());
}

TEST(WTF, StringConcatenate_Unsigned)
{
    EXPECT_EQ(5u, WTF::lengthOfIntegerAsString(17890u));
    EXPECT_STREQ("hello 17890 world", makeString("hello ", 17890u , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", 17890ul , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", 17890ull , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", static_cast<uint64_t>(17890) , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", static_cast<uint64_t>(17890) , " world").utf8().data());

    EXPECT_EQ(1u, WTF::lengthOfIntegerAsString(0u));
    EXPECT_STREQ("hello 0 world", makeString("hello ", 0u , " world").utf8().data());

    EXPECT_STREQ("hello 42 world", makeString("hello ", static_cast<unsigned char>(42) , " world").utf8().data());
    EXPECT_STREQ("hello 42 world", makeString("hello ", static_cast<unsigned short>(42) , " world").utf8().data());
    EXPECT_STREQ("hello 4 world", makeString("hello ", sizeof(int) , " world").utf8().data()); // size_t
    EXPECT_STREQ("hello 4 world", makeString("hello ", offsetof(S, i) , " world").utf8().data()); // size_t
    EXPECT_STREQ("hello 3235839742 world", makeString("hello ", static_cast<size_t>(0xc0defefe), " world").utf8().data());
}

TEST(WTF, StringConcatenate_Enum)
{
    EXPECT_STREQ("0", makeString(BoolEnum::A).utf8().data());
    EXPECT_STREQ("1", makeString(BoolEnum::B).utf8().data());

    EXPECT_STREQ("-1", makeString(SignedEnum::A).utf8().data());
    EXPECT_STREQ("0", makeString(SignedEnum::B).utf8().data());
    EXPECT_STREQ("1", makeString(SignedEnum::C).utf8().data());

    EXPECT_STREQ("0", makeString(UnsignedEnum::A).utf8().data());
    EXPECT_STREQ("1", makeString(UnsignedEnum::B).utf8().data());
    EXPECT_STREQ("2", makeString(UnsignedEnum::C).utf8().data());
}

TEST(WTF, StringConcatenate_Float)
{
    EXPECT_STREQ("hello 17890 world", makeString("hello ", 17890.0f , " world").utf8().data());
    EXPECT_STREQ("hello 17890.5 world", makeString("hello ", 17890.5f , " world").utf8().data());

    EXPECT_STREQ("hello -17890 world", makeString("hello ", -17890.0f , " world").utf8().data());
    EXPECT_STREQ("hello -17890.5 world", makeString("hello ", -17890.5f , " world").utf8().data());

    EXPECT_STREQ("hello 0 world", makeString("hello ", 0.0f , " world").utf8().data());
}

TEST(WTF, StringConcatenate_Double)
{
    EXPECT_STREQ("hello 17890 world", makeString("hello ", 17890.0 , " world").utf8().data());
    EXPECT_STREQ("hello 17890.5 world", makeString("hello ", 17890.5 , " world").utf8().data());

    EXPECT_STREQ("hello -17890 world", makeString("hello ", -17890.0 , " world").utf8().data());
    EXPECT_STREQ("hello -17890.5 world", makeString("hello ", -17890.5 , " world").utf8().data());

    EXPECT_STREQ("hello 0 world", makeString("hello ", 0.0 , " world").utf8().data());
}

TEST(WTF, StringConcatenate_FormattedDoubleFixedPrecision)
{
    EXPECT_STREQ("hello 17890 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.0) , " world").utf8().data());
    EXPECT_STREQ("hello 17890.0 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.0, 6, KeepTrailingZeros) , " world").utf8().data());
    EXPECT_STREQ("hello 1.79e+4 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.0, 3, KeepTrailingZeros) , " world").utf8().data());
    EXPECT_STREQ("hello 17890.000 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.0, 8, KeepTrailingZeros) , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.0, 8) , " world").utf8().data());

    EXPECT_STREQ("hello 17890.5 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.5) , " world").utf8().data());
    EXPECT_STREQ("hello 1.79e+4 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.5, 3, KeepTrailingZeros) , " world").utf8().data());
    EXPECT_STREQ("hello 17890.500 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.5, 8, KeepTrailingZeros) , " world").utf8().data());
    EXPECT_STREQ("hello 17890.5 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.5, 8) , " world").utf8().data());

    EXPECT_STREQ("hello -17890 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.0) , " world").utf8().data());
    EXPECT_STREQ("hello -17890.0 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.0, 6, KeepTrailingZeros) , " world").utf8().data());
    EXPECT_STREQ("hello -1.79e+4 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.0, 3, KeepTrailingZeros) , " world").utf8().data());
    EXPECT_STREQ("hello -17890.000 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.0, 8, KeepTrailingZeros) , " world").utf8().data());
    EXPECT_STREQ("hello -17890 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.0, 8) , " world").utf8().data());

    EXPECT_STREQ("hello -17890.5 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.5) , " world").utf8().data());
    EXPECT_STREQ("hello -1.79e+4 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.5, 3, KeepTrailingZeros) , " world").utf8().data());
    EXPECT_STREQ("hello -17890.500 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.5, 8, KeepTrailingZeros) , " world").utf8().data());
    EXPECT_STREQ("hello -17890.5 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.5, 8) , " world").utf8().data());

    EXPECT_STREQ("hello 0 world", makeString("hello ", FormattedNumber::fixedPrecision(0.0) , " world").utf8().data());
    EXPECT_STREQ("hello 0.00000 world", makeString("hello ", FormattedNumber::fixedPrecision(0.0, 6, KeepTrailingZeros) , " world").utf8().data());
    EXPECT_STREQ("hello 0.00 world", makeString("hello ", FormattedNumber::fixedPrecision(0.0, 3, KeepTrailingZeros) , " world").utf8().data());
    EXPECT_STREQ("hello 0.0000000 world", makeString("hello ", FormattedNumber::fixedPrecision(0.0, 8, KeepTrailingZeros) , " world").utf8().data());
    EXPECT_STREQ("hello 0 world", makeString("hello ", FormattedNumber::fixedPrecision(0.0, 8) , " world").utf8().data());
}

TEST(WTF, StringConcatenate_FormattedDoubleFixedWidth)
{
    EXPECT_STREQ("hello 17890.000 world", makeString("hello ", FormattedNumber::fixedWidth(17890.0, 3) , " world").utf8().data());
    EXPECT_STREQ("hello 17890.500 world", makeString("hello ", FormattedNumber::fixedWidth(17890.5, 3) , " world").utf8().data());

    EXPECT_STREQ("hello -17890.000 world", makeString("hello ", FormattedNumber::fixedWidth(-17890.0, 3) , " world").utf8().data());
    EXPECT_STREQ("hello -17890.500 world", makeString("hello ", FormattedNumber::fixedWidth(-17890.5, 3) , " world").utf8().data());

    EXPECT_STREQ("hello 0.000 world", makeString("hello ", FormattedNumber::fixedWidth(0.0, 3) , " world").utf8().data());
}

TEST(WTF, StringConcatenate_Pad)
{
    EXPECT_STREQ("", makeString(pad('x', 0, "")).utf8().data());
    EXPECT_STREQ("x", makeString(pad('x', 1, "")).utf8().data());
    EXPECT_STREQ("y", makeString(pad('x', 1, "y")).utf8().data());
    EXPECT_STREQ("xy", makeString(pad('x', 2, "y")).utf8().data());

    EXPECT_STREQ("xxxxxxxxxxxxxxx1E240", makeString(pad('x', 20, hex(123456))).utf8().data());
    EXPECT_STREQ("xxxxxxxxxxxxxxx1E2400.000", makeString(pad('x', 20, hex(123456)), FormattedNumber::fixedWidth(0.f, 3)).utf8().data());
    EXPECT_STREQ(" B32AF0071F9 id 1231232312313231 (0.000,0.000-0.000,0.000) 0.00KB", makeString(pad(' ', 12, hex(12312312312313)), " id ", 1231232312313231, " (", FormattedNumber::fixedWidth(0.f, 3), ',', FormattedNumber::fixedWidth(0.f, 3), '-', FormattedNumber::fixedWidth(0.f, 3), ',', FormattedNumber::fixedWidth(0.f, 3), ") ", FormattedNumber::fixedWidth(0.f, 2), "KB").utf8().data());
}

TEST(WTF, StringConcatenate_Tuple)
{
    EXPECT_STREQ("hello 42 world", makeString("hello", ' ', 42, ' ', "world").utf8().data());
    EXPECT_STREQ("hello 42 world", makeString("hello", ' ', std::make_tuple(unsigned(42)), ' ', "world").utf8().data());
    EXPECT_STREQ("hello 42 world", makeString("hello", ' ', std::make_tuple(unsigned(42), ' ', "world")).utf8().data());
    EXPECT_STREQ("hello 42 world", makeString(std::make_tuple("hello", ' ', unsigned(42), ' ', "world")).utf8().data());

    UChar checkmarkCodepoint = 0x2713;
    EXPECT_STREQ("hello \xE2\x9C\x93 world", makeString("hello", ' ', checkmarkCodepoint, ' ', "world").utf8().data());
    EXPECT_STREQ("hello \xE2\x9C\x93 world", makeString("hello", ' ', std::make_tuple(checkmarkCodepoint), ' ', "world").utf8().data());
    EXPECT_STREQ("hello \xE2\x9C\x93 world", makeString("hello", ' ', std::make_tuple(checkmarkCodepoint, ' ', "world")).utf8().data());
    EXPECT_STREQ("hello \xE2\x9C\x93 world", makeString(std::make_tuple("hello", ' ', checkmarkCodepoint, ' ', "world")).utf8().data());

    const UChar helloCodepoints[] = { 'h', 'e', 'l', 'l', 'o', '\0' };
    EXPECT_STREQ("hello 42 world", makeString(helloCodepoints, ' ', unsigned(42), ' ', "world").utf8().data());
    EXPECT_STREQ("hello 42 world", makeString(std::make_tuple(helloCodepoints), ' ', unsigned(42), ' ', "world").utf8().data());
    EXPECT_STREQ("hello 42 world", makeString(std::make_tuple(helloCodepoints, ' ', unsigned(42)), ' ', "world").utf8().data());
    EXPECT_STREQ("hello 42 world", makeString(std::make_tuple(helloCodepoints, ' ', unsigned(42), ' ', "world")).utf8().data());
}

}
