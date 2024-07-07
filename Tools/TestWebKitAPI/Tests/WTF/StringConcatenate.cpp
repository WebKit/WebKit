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

#include "MoveOnly.h"
#include "Test.h"
#include <cstddef>
#include <cstdint>
#include <unicode/uvernum.h>
#include <wtf/HexNumber.h>
#include <wtf/text/MakeString.h>

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
    EXPECT_STREQ("hello world", makeString("hello"_s, " "_s, "world"_s).utf8().data());
}

TEST(WTF, StringConcatenate_Int)
{
    EXPECT_EQ(5u, WTF::lengthOfIntegerAsString(17890));
    EXPECT_STREQ("hello 17890 world", makeString("hello "_s, 17890 , " world"_s).utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello "_s, 17890l , " world"_s).utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello "_s, 17890ll , " world"_s).utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello "_s, static_cast<int64_t>(17890) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello "_s, static_cast<int64_t>(17890) , " world"_s).utf8().data());

    EXPECT_EQ(6u, WTF::lengthOfIntegerAsString(-17890));
    EXPECT_STREQ("hello -17890 world", makeString("hello "_s, -17890 , " world"_s).utf8().data());
    EXPECT_STREQ("hello -17890 world", makeString("hello "_s, -17890l , " world"_s).utf8().data());
    EXPECT_STREQ("hello -17890 world", makeString("hello "_s, -17890ll , " world"_s).utf8().data());
    EXPECT_STREQ("hello -17890 world", makeString("hello "_s, static_cast<int64_t>(-17890) , " world"_s).utf8().data());
    EXPECT_STREQ("hello -17890 world", makeString("hello "_s, static_cast<int64_t>(-17890) , " world"_s).utf8().data());

    EXPECT_EQ(1u, WTF::lengthOfIntegerAsString(0));
    EXPECT_STREQ("hello 0 world", makeString("hello "_s, 0 , " world"_s).utf8().data());

    EXPECT_STREQ("hello 42 world", makeString("hello "_s, static_cast<signed char>(42) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 42 world", makeString("hello "_s, static_cast<short>(42) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 1 world", makeString("hello "_s, &arr[1] - &arr[0] , " world"_s).utf8().data());
}

TEST(WTF, StringConcatenate_Unsigned)
{
    EXPECT_EQ(5u, WTF::lengthOfIntegerAsString(17890u));
    EXPECT_STREQ("hello 17890 world", makeString("hello "_s, 17890u , " world"_s).utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello "_s, 17890ul , " world"_s).utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello "_s, 17890ull , " world"_s).utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello "_s, static_cast<uint64_t>(17890) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello "_s, static_cast<uint64_t>(17890) , " world"_s).utf8().data());

    EXPECT_EQ(1u, WTF::lengthOfIntegerAsString(0u));
    EXPECT_STREQ("hello 0 world", makeString("hello "_s, 0u , " world"_s).utf8().data());

    EXPECT_STREQ("hello 42 world", makeString("hello "_s, static_cast<unsigned char>(42) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 42 world", makeString("hello "_s, static_cast<unsigned short>(42) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 4 world", makeString("hello "_s, sizeof(int) , " world"_s).utf8().data()); // size_t
    EXPECT_STREQ("hello 4 world", makeString("hello "_s, offsetof(S, i) , " world"_s).utf8().data()); // size_t
    EXPECT_STREQ("hello 3235839742 world", makeString("hello "_s, static_cast<size_t>(0xc0defefe), " world"_s).utf8().data());
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
    EXPECT_STREQ("hello 17890 world", makeString("hello "_s, 17890.0f , " world"_s).utf8().data());
    EXPECT_STREQ("hello 17890.5 world", makeString("hello "_s, 17890.5f , " world"_s).utf8().data());

    EXPECT_STREQ("hello -17890 world", makeString("hello "_s, -17890.0f , " world"_s).utf8().data());
    EXPECT_STREQ("hello -17890.5 world", makeString("hello "_s, -17890.5f , " world"_s).utf8().data());

    EXPECT_STREQ("hello 0 world", makeString("hello "_s, 0.0f , " world"_s).utf8().data());
}

TEST(WTF, StringConcatenate_Double)
{
    EXPECT_STREQ("hello 17890 world", makeString("hello "_s, 17890.0 , " world"_s).utf8().data());
    EXPECT_STREQ("hello 17890.5 world", makeString("hello "_s, 17890.5 , " world"_s).utf8().data());

    EXPECT_STREQ("hello -17890 world", makeString("hello "_s, -17890.0 , " world"_s).utf8().data());
    EXPECT_STREQ("hello -17890.5 world", makeString("hello "_s, -17890.5 , " world"_s).utf8().data());

    EXPECT_STREQ("hello 0 world", makeString("hello "_s, 0.0 , " world"_s).utf8().data());
}

TEST(WTF, StringConcatenate_FormattedDoubleFixedPrecision)
{
    EXPECT_STREQ("hello 17890 world", makeString("hello "_s, FormattedNumber::fixedPrecision(17890.0) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 17890.0 world", makeString("hello "_s, FormattedNumber::fixedPrecision(17890.0, 6, TrailingZerosPolicy::Keep) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 1.79e+4 world", makeString("hello "_s, FormattedNumber::fixedPrecision(17890.0, 3, TrailingZerosPolicy::Keep) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 17890.000 world", makeString("hello "_s, FormattedNumber::fixedPrecision(17890.0, 8, TrailingZerosPolicy::Keep) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello "_s, FormattedNumber::fixedPrecision(17890.0, 8) , " world"_s).utf8().data());

    EXPECT_STREQ("hello 17890.5 world", makeString("hello "_s, FormattedNumber::fixedPrecision(17890.5) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 1.79e+4 world", makeString("hello "_s, FormattedNumber::fixedPrecision(17890.5, 3, TrailingZerosPolicy::Keep) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 17890.500 world", makeString("hello "_s, FormattedNumber::fixedPrecision(17890.5, 8, TrailingZerosPolicy::Keep) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 17890.5 world", makeString("hello "_s, FormattedNumber::fixedPrecision(17890.5, 8) , " world"_s).utf8().data());

    EXPECT_STREQ("hello -17890 world", makeString("hello "_s, FormattedNumber::fixedPrecision(-17890.0) , " world"_s).utf8().data());
    EXPECT_STREQ("hello -17890.0 world", makeString("hello "_s, FormattedNumber::fixedPrecision(-17890.0, 6, TrailingZerosPolicy::Keep) , " world"_s).utf8().data());
    EXPECT_STREQ("hello -1.79e+4 world", makeString("hello "_s, FormattedNumber::fixedPrecision(-17890.0, 3, TrailingZerosPolicy::Keep) , " world"_s).utf8().data());
    EXPECT_STREQ("hello -17890.000 world", makeString("hello "_s, FormattedNumber::fixedPrecision(-17890.0, 8, TrailingZerosPolicy::Keep) , " world"_s).utf8().data());
    EXPECT_STREQ("hello -17890 world", makeString("hello "_s, FormattedNumber::fixedPrecision(-17890.0, 8) , " world"_s).utf8().data());

    EXPECT_STREQ("hello -17890.5 world", makeString("hello "_s, FormattedNumber::fixedPrecision(-17890.5) , " world"_s).utf8().data());
    EXPECT_STREQ("hello -1.79e+4 world", makeString("hello "_s, FormattedNumber::fixedPrecision(-17890.5, 3, TrailingZerosPolicy::Keep) , " world"_s).utf8().data());
    EXPECT_STREQ("hello -17890.500 world", makeString("hello "_s, FormattedNumber::fixedPrecision(-17890.5, 8, TrailingZerosPolicy::Keep) , " world"_s).utf8().data());
    EXPECT_STREQ("hello -17890.5 world", makeString("hello "_s, FormattedNumber::fixedPrecision(-17890.5, 8) , " world"_s).utf8().data());

    EXPECT_STREQ("hello 0 world", makeString("hello "_s, FormattedNumber::fixedPrecision(0.0) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 0.00000 world", makeString("hello "_s, FormattedNumber::fixedPrecision(0.0, 6, TrailingZerosPolicy::Keep) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 0.00 world", makeString("hello "_s, FormattedNumber::fixedPrecision(0.0, 3, TrailingZerosPolicy::Keep) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 0.0000000 world", makeString("hello "_s, FormattedNumber::fixedPrecision(0.0, 8, TrailingZerosPolicy::Keep) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 0 world", makeString("hello "_s, FormattedNumber::fixedPrecision(0.0, 8) , " world"_s).utf8().data());
}

TEST(WTF, StringConcatenate_FormattedDoubleFixedWidth)
{
    EXPECT_STREQ("hello 17890.000 world", makeString("hello "_s, FormattedNumber::fixedWidth(17890.0, 3) , " world"_s).utf8().data());
    EXPECT_STREQ("hello 17890.500 world", makeString("hello "_s, FormattedNumber::fixedWidth(17890.5, 3) , " world"_s).utf8().data());

    EXPECT_STREQ("hello -17890.000 world", makeString("hello "_s, FormattedNumber::fixedWidth(-17890.0, 3) , " world"_s).utf8().data());
    EXPECT_STREQ("hello -17890.500 world", makeString("hello "_s, FormattedNumber::fixedWidth(-17890.5, 3) , " world"_s).utf8().data());

    EXPECT_STREQ("hello 0.000 world", makeString("hello "_s, FormattedNumber::fixedWidth(0.0, 3) , " world"_s).utf8().data());
}

TEST(WTF, StringConcatenate_Pad)
{
    EXPECT_STREQ("", makeString(pad('x', 0, ""_s)).utf8().data());
    EXPECT_STREQ("x", makeString(pad('x', 1, ""_s)).utf8().data());
    EXPECT_STREQ("y", makeString(pad('x', 1, "y"_s)).utf8().data());
    EXPECT_STREQ("xy", makeString(pad('x', 2, "y"_s)).utf8().data());

    EXPECT_STREQ("xxxxxxxxxxxxxxx1E240", makeString(pad('x', 20, hex(123456))).utf8().data());
    EXPECT_STREQ("xxxxxxxxxxxxxxx1E2400.000", makeString(pad('x', 20, hex(123456)), FormattedNumber::fixedWidth(0.f, 3)).utf8().data());
    EXPECT_STREQ(" B32AF0071F9 id 1231232312313231 (0.000,0.000-0.000,0.000) 0.00KB", makeString(pad(' ', 12, hex(12312312312313)), " id "_s, 1231232312313231, " ("_s, FormattedNumber::fixedWidth(0.f, 3), ',', FormattedNumber::fixedWidth(0.f, 3), '-', FormattedNumber::fixedWidth(0.f, 3), ',', FormattedNumber::fixedWidth(0.f, 3), ") "_s, FormattedNumber::fixedWidth(0.f, 2), "KB"_s).utf8().data());
}

TEST(WTF, StringConcatenate_Tuple)
{
    EXPECT_STREQ("hello 42 world", makeString("hello"_s, ' ', 42, ' ', "world"_s).utf8().data());
    EXPECT_STREQ("hello 42 world", makeString("hello"_s, ' ', std::make_tuple(unsigned(42)), ' ', "world"_s).utf8().data());
    EXPECT_STREQ("hello 42 world", makeString("hello"_s, ' ', std::make_tuple(unsigned(42), ' ', "world"_s)).utf8().data());
    EXPECT_STREQ("hello 42 world", makeString(std::make_tuple("hello"_s, ' ', unsigned(42), ' ', "world"_s)).utf8().data());

    UChar checkmarkCodepoint = 0x2713;
    EXPECT_STREQ("hello \xE2\x9C\x93 world", makeString("hello"_s, ' ', checkmarkCodepoint, ' ', "world"_s).utf8().data());
    EXPECT_STREQ("hello \xE2\x9C\x93 world", makeString("hello"_s, ' ', std::make_tuple(checkmarkCodepoint), ' ', "world"_s).utf8().data());
    EXPECT_STREQ("hello \xE2\x9C\x93 world", makeString("hello"_s, ' ', std::make_tuple(checkmarkCodepoint, ' ', "world"_s)).utf8().data());
    EXPECT_STREQ("hello \xE2\x9C\x93 world", makeString(std::make_tuple("hello"_s, ' ', checkmarkCodepoint, ' ', "world"_s)).utf8().data());

    const UChar helloCodepoints[] = { 'h', 'e', 'l', 'l', 'o', '\0' };
    EXPECT_STREQ("hello 42 world", makeString(helloCodepoints, ' ', unsigned(42), ' ', "world"_s).utf8().data());
    EXPECT_STREQ("hello 42 world", makeString(std::make_tuple(helloCodepoints), ' ', unsigned(42), ' ', "world"_s).utf8().data());
    EXPECT_STREQ("hello 42 world", makeString(std::make_tuple(helloCodepoints, ' ', unsigned(42)), ' ', "world"_s).utf8().data());
    EXPECT_STREQ("hello 42 world", makeString(std::make_tuple(helloCodepoints, ' ', unsigned(42), ' ', "world"_s)).utf8().data());
}

TEST(WTF, StringConcatenate_Interleave)
{
    std::array strings { "a"_s, "b"_s, "c"_s };

    {
        auto result = makeString('[', interleave(strings, [](auto& builder, auto& s) { builder.append(s); }, ", "_s), ']');

        EXPECT_EQ(String("[a, b, c]"_s), result);
    }

    {
        auto result = makeString('[', interleave(strings, [](auto& s) { return s; }, ", "_s), ']');

        EXPECT_EQ(String("[a, b, c]"_s), result);
    }

    {
        auto result = makeString('[', interleave(strings, ", "_s), ']');

        EXPECT_EQ(String("[a, b, c]"_s), result);
    }
}

struct A { int value; };
struct B { int value; };

[[maybe_unused]] static void serializeOverloadBuilder(StringBuilder& builder, const A& a)
{
    builder.append(a.value);
}

[[maybe_unused]] static void serializeOverloadBuilder(StringBuilder& builder, const B& b)
{
    builder.append(b.value);
}

[[maybe_unused]] static int serializeOverloadReturn(const A& a)
{
    return a.value;
}

[[maybe_unused]] static int serializeOverloadReturn(const B& b)
{
    return b.value;
}

TEST(WTF, StringConcatenate_InterleaveOverload)
{
    std::array as { A { 1 }, A { 2 }, A { 3 } };

    {
        auto result = makeString('[', interleave(as, serializeOverloadBuilder, ", "_s), ']');

        EXPECT_EQ(String("[1, 2, 3]"_s), result);
    }
}

TEST(WTF, StringConcatenate_InterleaveNoCopies)
{
    std::array values { MoveOnly { 1 }, MoveOnly { 2 }, MoveOnly { 3 } };

    {
        auto result = makeString('[', interleave(values, [](auto& builder, auto& moveOnly) { builder.append(moveOnly.value()); }, ", "_s), ']');

        EXPECT_EQ(String("[1, 2, 3]"_s), result);
    }

    {
        auto result = makeString('[', interleave(values, [](auto& moveOnly) { return moveOnly.value(); }, ", "_s), ']');

        EXPECT_EQ(String("[1, 2, 3]"_s), result);
    }

    {
        auto result = makeString('[', interleave(values, ", "_s), ']');

        EXPECT_EQ(String("[1, 2, 3]"_s), result);
    }
}

}
