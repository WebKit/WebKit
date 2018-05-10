/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#include <wtf/text/AtomicString.h>

namespace TestWebKitAPI {

TEST(WTF, AtomicStringCreationFromLiteral)
{
    AtomicString stringWithTemplate("Template Literal", AtomicString::ConstructFromLiteral);
    ASSERT_EQ(strlen("Template Literal"), stringWithTemplate.length());
    ASSERT_TRUE(stringWithTemplate == "Template Literal");
    ASSERT_TRUE(stringWithTemplate.string().is8Bit());

    const char* programmaticStringData = "Explicit Size Literal";
    AtomicString programmaticString(programmaticStringData, strlen(programmaticStringData), AtomicString::ConstructFromLiteral);
    ASSERT_EQ(strlen(programmaticStringData), programmaticString.length());
    ASSERT_TRUE(programmaticString.string().is8Bit());
    ASSERT_EQ(programmaticStringData, reinterpret_cast<const char*>(programmaticString.string().characters8()));
}

TEST(WTF, AtomicStringCreationFromLiteralUniqueness)
{
    AtomicString string1("Template Literal", AtomicString::ConstructFromLiteral);
    AtomicString string2("Template Literal", AtomicString::ConstructFromLiteral);
    ASSERT_EQ(string1.impl(), string2.impl());

    AtomicString string3("Template Literal");
    ASSERT_EQ(string1.impl(), string3.impl());
}

TEST(WTF, AtomicStringExistingHash)
{
    AtomicString string1("Template Literal", AtomicString::ConstructFromLiteral);
    ASSERT_EQ(string1.existingHash(), string1.impl()->existingHash());
    AtomicString string2;
    ASSERT_EQ(string2.existingHash(), 0u);
}

static inline const char* testAtomicStringNumber(double number)
{
    static char testBuffer[100] = { };
    std::strncpy(testBuffer, AtomicString::number(number).string().utf8().data(), 99);
    return testBuffer;
}

TEST(WTF, AtomicStringNumberDouble)
{
    using Limits = std::numeric_limits<double>;

    EXPECT_STREQ("Infinity", testAtomicStringNumber(Limits::infinity()));
    EXPECT_STREQ("-Infinity", testAtomicStringNumber(-Limits::infinity()));

    EXPECT_STREQ("NaN", testAtomicStringNumber(-Limits::quiet_NaN()));

    EXPECT_STREQ("0", testAtomicStringNumber(0));
    EXPECT_STREQ("0", testAtomicStringNumber(-0));

    EXPECT_STREQ("2.2250738585072014e-308", testAtomicStringNumber(Limits::min()));
    EXPECT_STREQ("-1.7976931348623157e+308", testAtomicStringNumber(Limits::lowest()));
    EXPECT_STREQ("1.7976931348623157e+308", testAtomicStringNumber(Limits::max()));

    EXPECT_STREQ("3.141592653589793", testAtomicStringNumber(piDouble));
    EXPECT_STREQ("3.1415927410125732", testAtomicStringNumber(piFloat));
    EXPECT_STREQ("1.5707963267948966", testAtomicStringNumber(piOverTwoDouble));
    EXPECT_STREQ("1.5707963705062866", testAtomicStringNumber(piOverTwoFloat));
    EXPECT_STREQ("0.7853981633974483", testAtomicStringNumber(piOverFourDouble));
    EXPECT_STREQ("0.7853981852531433", testAtomicStringNumber(piOverFourFloat));

    EXPECT_STREQ("2.718281828459045", testAtomicStringNumber(2.71828182845904523536028747135266249775724709369995));

    EXPECT_STREQ("299792458", testAtomicStringNumber(299792458));

    EXPECT_STREQ("1.618033988749895", testAtomicStringNumber(1.6180339887498948482));

    EXPECT_STREQ("1000", testAtomicStringNumber(1e3));
    EXPECT_STREQ("10000000000", testAtomicStringNumber(1e10));
    EXPECT_STREQ("100000000000000000000", testAtomicStringNumber(1e20));
    EXPECT_STREQ("1e+21", testAtomicStringNumber(1e21));
    EXPECT_STREQ("1e+30", testAtomicStringNumber(1e30));

    EXPECT_STREQ("1100", testAtomicStringNumber(1.1e3));
    EXPECT_STREQ("11000000000", testAtomicStringNumber(1.1e10));
    EXPECT_STREQ("110000000000000000000", testAtomicStringNumber(1.1e20));
    EXPECT_STREQ("1.1e+21", testAtomicStringNumber(1.1e21));
    EXPECT_STREQ("1.1e+30", testAtomicStringNumber(1.1e30));
}

} // namespace TestWebKitAPI
