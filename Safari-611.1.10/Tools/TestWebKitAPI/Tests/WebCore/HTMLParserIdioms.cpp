/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include <WebCore/HTMLParserIdioms.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace TestWebKitAPI {

static int testParseHTMLInteger(const String& input)
{
    auto optionalResult = parseHTMLInteger(input);
    EXPECT_TRUE(!!optionalResult);
    return optionalResult.value_or(0);
}

static bool parseHTMLIntegerFails(const String& input)
{
    return !parseHTMLInteger(input);
}

TEST(WebCoreHTMLParserIdioms, parseHTMLInteger)
{
    EXPECT_EQ(0, testParseHTMLInteger("0"));
    EXPECT_EQ(0, testParseHTMLInteger("-0"));
    EXPECT_EQ(0, testParseHTMLInteger("+0"));
    EXPECT_EQ(123, testParseHTMLInteger("123"));
    EXPECT_EQ(123, testParseHTMLInteger("+123"));
    EXPECT_EQ(-123, testParseHTMLInteger("-123"));
    EXPECT_EQ(123, testParseHTMLInteger("  123"));
    EXPECT_EQ(123, testParseHTMLInteger("123   "));
    EXPECT_EQ(123, testParseHTMLInteger("   123   "));
    EXPECT_EQ(123, testParseHTMLInteger("123abc"));
    EXPECT_EQ(-123, testParseHTMLInteger("-123abc"));
    EXPECT_EQ(123, testParseHTMLInteger("  +123"));
    EXPECT_EQ(-123, testParseHTMLInteger("  -123"));
    EXPECT_EQ(12, testParseHTMLInteger("   12 3"));
    EXPECT_EQ(1, testParseHTMLInteger("1.0"));
    EXPECT_EQ(1, testParseHTMLInteger("1."));
    EXPECT_EQ(1, testParseHTMLInteger("1e1"));

    // All HTML whitespaces.
    EXPECT_EQ(123, testParseHTMLInteger(" \t\r\n\f123"));

    // Boundaries.
    EXPECT_EQ(-2147483648, testParseHTMLInteger("-2147483648"));
    EXPECT_EQ(2147483647, testParseHTMLInteger("2147483647"));

    // Failure cases.
    EXPECT_TRUE(parseHTMLIntegerFails("-2147483649"));
    EXPECT_TRUE(parseHTMLIntegerFails("2147483648"));
    EXPECT_TRUE(parseHTMLIntegerFails("111111111111111111"));
    EXPECT_TRUE(parseHTMLIntegerFails(""));
    EXPECT_TRUE(parseHTMLIntegerFails(" "));
    EXPECT_TRUE(parseHTMLIntegerFails("   "));
    EXPECT_TRUE(parseHTMLIntegerFails("+"));
    EXPECT_TRUE(parseHTMLIntegerFails("+ 123"));
    EXPECT_TRUE(parseHTMLIntegerFails("-"));
    EXPECT_TRUE(parseHTMLIntegerFails("- 123"));
    EXPECT_TRUE(parseHTMLIntegerFails("a"));
    EXPECT_TRUE(parseHTMLIntegerFails("-a"));
    EXPECT_TRUE(parseHTMLIntegerFails("+-123"));
    EXPECT_TRUE(parseHTMLIntegerFails("-+123"));
    EXPECT_TRUE(parseHTMLIntegerFails("++123"));
    EXPECT_TRUE(parseHTMLIntegerFails("--123"));
    EXPECT_TRUE(parseHTMLIntegerFails("\v123")); // '\v' is an ASCII space but not an HTML whitespace.
    EXPECT_TRUE(parseHTMLIntegerFails("a123"));
    EXPECT_TRUE(parseHTMLIntegerFails("+a123"));
    EXPECT_TRUE(parseHTMLIntegerFails("-a123"));
    EXPECT_TRUE(parseHTMLIntegerFails(".1"));
    EXPECT_TRUE(parseHTMLIntegerFails("infinity"));
}

static unsigned testParseHTMLNonNegativeInteger(const String& input)
{
    auto optionalResult = parseHTMLNonNegativeInteger(input);
    EXPECT_TRUE(!!optionalResult);
    return optionalResult.value_or(0);
}

static bool parseHTMLNonNegativeIntegerFails(const String& input)
{
    return !parseHTMLNonNegativeInteger(input);
}

TEST(WebCoreHTMLParserIdioms, parseHTMLNonNegativeInteger)
{
    EXPECT_EQ(123u, testParseHTMLNonNegativeInteger("123"));
    EXPECT_EQ(123u, testParseHTMLNonNegativeInteger("+123"));
    EXPECT_EQ(123u, testParseHTMLNonNegativeInteger("  123"));
    EXPECT_EQ(123u, testParseHTMLNonNegativeInteger("123   "));
    EXPECT_EQ(123u, testParseHTMLNonNegativeInteger("   123   "));
    EXPECT_EQ(123u, testParseHTMLNonNegativeInteger("123abc"));
    EXPECT_EQ(123u, testParseHTMLNonNegativeInteger("  +123"));
    EXPECT_EQ(12u, testParseHTMLNonNegativeInteger("   12 3"));
    EXPECT_EQ(1u, testParseHTMLNonNegativeInteger("1.0"));
    EXPECT_EQ(1u, testParseHTMLNonNegativeInteger("1."));
    EXPECT_EQ(1u, testParseHTMLNonNegativeInteger("1e1"));

    // All HTML whitespaces.
    EXPECT_EQ(123u, testParseHTMLNonNegativeInteger(" \t\r\n\f123"));

    // Boundaries.
    EXPECT_EQ(0u, testParseHTMLNonNegativeInteger("+0"));
    EXPECT_EQ(0u, testParseHTMLNonNegativeInteger("0"));
    EXPECT_EQ(0u, testParseHTMLNonNegativeInteger("-0"));
    EXPECT_EQ(2147483647u, testParseHTMLNonNegativeInteger("2147483647"));

    // Failure cases.
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("-1"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("2147483648"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("2147483649"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("111111111111111111"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("  -123"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("-123"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("-123abc"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails(""));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails(" "));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("   "));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("+"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("+ 123"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("-"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("- 123"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("a"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("-a"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("+-123"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("-+123"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("++123"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("--123"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("\v123")); // '\v' is an ASCII space but not an HTML whitespace.
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("a123"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("+a123"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("-a123"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails(".1"));
    EXPECT_TRUE(parseHTMLNonNegativeIntegerFails("infinity"));
}

} // namespace TestWebKitAPI
