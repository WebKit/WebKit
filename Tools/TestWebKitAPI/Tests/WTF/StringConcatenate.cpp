/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "WTFStringUtilities.h"
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <cstddef>
#include <cstdint>
#include <unicode/uvernum.h>

namespace TestWebKitAPI {

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
    EXPECT_EQ(5u, WTF::lengthOfNumberAsStringSigned(17890));
    EXPECT_STREQ("hello 17890 world", makeString("hello ", 17890 , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", 17890l , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", 17890ll , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", static_cast<int64_t>(17890) , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", static_cast<int64_t>(17890) , " world").utf8().data());

    EXPECT_EQ(6u, WTF::lengthOfNumberAsStringSigned(-17890));
    EXPECT_STREQ("hello -17890 world", makeString("hello ", -17890 , " world").utf8().data());
    EXPECT_STREQ("hello -17890 world", makeString("hello ", -17890l , " world").utf8().data());
    EXPECT_STREQ("hello -17890 world", makeString("hello ", -17890ll , " world").utf8().data());
    EXPECT_STREQ("hello -17890 world", makeString("hello ", static_cast<int64_t>(-17890) , " world").utf8().data());
    EXPECT_STREQ("hello -17890 world", makeString("hello ", static_cast<int64_t>(-17890) , " world").utf8().data());

    EXPECT_EQ(1u, WTF::lengthOfNumberAsStringSigned(0));
    EXPECT_STREQ("hello 0 world", makeString("hello ", 0 , " world").utf8().data());

    EXPECT_STREQ("hello 42 world", makeString("hello ", static_cast<signed char>(42) , " world").utf8().data());
    EXPECT_STREQ("hello 42 world", makeString("hello ", static_cast<short>(42) , " world").utf8().data());
    EXPECT_STREQ("hello 1 world", makeString("hello ", &arr[1] - &arr[0] , " world").utf8().data());
}

TEST(WTF, StringConcatenate_Unsigned)
{
    EXPECT_EQ(5u, WTF::lengthOfNumberAsStringUnsigned(17890u));
    EXPECT_STREQ("hello 17890 world", makeString("hello ", 17890u , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", 17890ul , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", 17890ull , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", static_cast<uint64_t>(17890) , " world").utf8().data());
    EXPECT_STREQ("hello 17890 world", makeString("hello ", static_cast<uint64_t>(17890) , " world").utf8().data());

    EXPECT_EQ(1u, WTF::lengthOfNumberAsStringSigned(0u));
    EXPECT_STREQ("hello 0 world", makeString("hello ", 0u , " world").utf8().data());

    EXPECT_STREQ("hello 42 world", makeString("hello ", static_cast<unsigned char>(42) , " world").utf8().data());
#if PLATFORM(WIN) || (U_ICU_VERSION_MAJOR_NUM >= 59 && !PLATFORM(COCOA))
    EXPECT_STREQ("hello 42 world", makeString("hello ", static_cast<unsigned short>(42) , " world").utf8().data());
#else
    EXPECT_STREQ("hello * world", makeString("hello ", static_cast<unsigned short>(42) , " world").utf8().data()); // Treated as a character.
#endif
    EXPECT_STREQ("hello 4 world", makeString("hello ", sizeof(int) , " world").utf8().data()); // size_t
    EXPECT_STREQ("hello 4 world", makeString("hello ", offsetof(S, i) , " world").utf8().data()); // size_t
    EXPECT_STREQ("hello 3235839742 world", makeString("hello ", static_cast<size_t>(0xc0defefe), " world").utf8().data());
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

}
