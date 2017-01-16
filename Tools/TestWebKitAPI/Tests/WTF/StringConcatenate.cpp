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

namespace TestWebKitAPI {

TEST(WTF, StringConcatenate)
{
    EXPECT_EQ("hello world", makeString("hello", " ", "world"));
}

TEST(WTF, StringConcatenate_Int)
{
    EXPECT_EQ(5u, WTF::lengthOfNumberAsStringSigned(17890));
    EXPECT_EQ("hello 17890 world", makeString("hello ", 17890 , " world"));

    EXPECT_EQ(6u, WTF::lengthOfNumberAsStringSigned(-17890));
    EXPECT_EQ("hello -17890 world", makeString("hello ", -17890 , " world"));

    EXPECT_EQ(1u, WTF::lengthOfNumberAsStringSigned(0));
    EXPECT_EQ("hello 0 world", makeString("hello ", 0 , " world"));
}

TEST(WTF, StringConcatenate_Unsigned)
{
    EXPECT_EQ(5u, WTF::lengthOfNumberAsStringUnsigned(17890u));
    EXPECT_EQ("hello 17890 world", makeString("hello ", 17890u , " world"));

    EXPECT_EQ(1u, WTF::lengthOfNumberAsStringSigned(0u));
    EXPECT_EQ("hello 0 world", makeString("hello ", 0u , " world"));
}

TEST(WTF, StringConcatenate_Float)
{
    EXPECT_EQ("hello 17890 world", makeString("hello ", 17890.0f , " world"));
    EXPECT_EQ("hello 17890.5 world", makeString("hello ", 17890.5f , " world"));

    EXPECT_EQ("hello -17890 world", makeString("hello ", -17890.0f , " world"));
    EXPECT_EQ("hello -17890.5 world", makeString("hello ", -17890.5f , " world"));

    EXPECT_EQ("hello 0 world", makeString("hello ", 0.0f , " world"));
}

TEST(WTF, StringConcatenate_Double)
{
    EXPECT_EQ("hello 17890 world", makeString("hello ", 17890.0 , " world"));
    EXPECT_EQ("hello 17890.5 world", makeString("hello ", 17890.5 , " world"));

    EXPECT_EQ("hello -17890 world", makeString("hello ", -17890.0 , " world"));
    EXPECT_EQ("hello -17890.5 world", makeString("hello ", -17890.5 , " world"));

    EXPECT_EQ("hello 0 world", makeString("hello ", 0.0 , " world"));
}

TEST(WTF, StringConcatenate_FormattedDoubleFixedPrecision)
{
    EXPECT_EQ("hello 17890.0 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.0) , " world"));
    EXPECT_EQ("hello 1.79e+4 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.0, 3) , " world"));
    EXPECT_EQ("hello 17890.000 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.0, 8) , " world"));
    EXPECT_EQ("hello 17890 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.0, 8, true) , " world"));

    EXPECT_EQ("hello 17890.5 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.5) , " world"));
    EXPECT_EQ("hello 1.79e+4 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.5, 3) , " world"));
    EXPECT_EQ("hello 17890.500 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.5, 8) , " world"));
    EXPECT_EQ("hello 17890.5 world", makeString("hello ", FormattedNumber::fixedPrecision(17890.5, 8, true) , " world"));

    EXPECT_EQ("hello -17890.0 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.0) , " world"));
    EXPECT_EQ("hello -1.79e+4 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.0, 3) , " world"));
    EXPECT_EQ("hello -17890.000 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.0, 8) , " world"));
    EXPECT_EQ("hello -17890 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.0, 8, true) , " world"));

    EXPECT_EQ("hello -17890.5 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.5) , " world"));
    EXPECT_EQ("hello -1.79e+4 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.5, 3) , " world"));
    EXPECT_EQ("hello -17890.500 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.5, 8) , " world"));
    EXPECT_EQ("hello -17890.5 world", makeString("hello ", FormattedNumber::fixedPrecision(-17890.5, 8, true) , " world"));

    EXPECT_EQ("hello 0.00000 world", makeString("hello ", FormattedNumber::fixedPrecision(0.0) , " world"));
    EXPECT_EQ("hello 0.00 world", makeString("hello ", FormattedNumber::fixedPrecision(0.0, 3) , " world"));
    EXPECT_EQ("hello 0.0000000 world", makeString("hello ", FormattedNumber::fixedPrecision(0.0, 8) , " world"));
    EXPECT_EQ("hello 0 world", makeString("hello ", FormattedNumber::fixedPrecision(0.0, 8, true) , " world"));
}

TEST(WTF, StringConcatenate_FormattedDoubleFixedWidth)
{
    EXPECT_EQ("hello 17890.000 world", makeString("hello ", FormattedNumber::fixedWidth(17890.0, 3) , " world"));
    EXPECT_EQ("hello 17890.500 world", makeString("hello ", FormattedNumber::fixedWidth(17890.5, 3) , " world"));

    EXPECT_EQ("hello -17890.000 world", makeString("hello ", FormattedNumber::fixedWidth(-17890.0, 3) , " world"));
    EXPECT_EQ("hello -17890.500 world", makeString("hello ", FormattedNumber::fixedWidth(-17890.5, 3) , " world"));

    EXPECT_EQ("hello 0.000 world", makeString("hello ", FormattedNumber::fixedWidth(0.0, 3) , " world"));
}

}
