/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include <limits>
#include <wtf/MathExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF, StringCreationFromLiteral)
{
    String stringFromLiteral(ASCIILiteral("Explicit construction syntax"));
    ASSERT_EQ(strlen("Explicit construction syntax"), stringFromLiteral.length());
    ASSERT_TRUE(stringFromLiteral == "Explicit construction syntax");
    ASSERT_TRUE(stringFromLiteral.is8Bit());
    ASSERT_TRUE(String("Explicit construction syntax") == stringFromLiteral);

    String stringWithTemplate("Template Literal", String::ConstructFromLiteral);
    ASSERT_EQ(strlen("Template Literal"), stringWithTemplate.length());
    ASSERT_TRUE(stringWithTemplate == "Template Literal");
    ASSERT_TRUE(stringWithTemplate.is8Bit());
    ASSERT_TRUE(String("Template Literal") == stringWithTemplate);
}

TEST(WTF, StringASCII)
{
    CString output;

    // Null String.
    output = String().ascii();
    ASSERT_STREQ("", output.data());

    // Empty String.
    output = emptyString().ascii();
    ASSERT_STREQ("", output.data());

    // Regular String.
    output = String(ASCIILiteral("foobar")).ascii();
    ASSERT_STREQ("foobar", output.data());
}

static void testNumberToStringECMAScript(double number, const char* reference)
{
    CString numberString = String::numberToStringECMAScript(number).latin1();
    ASSERT_STREQ(reference, numberString.data());
}

TEST(WTF, StringNumberToStringECMAScriptBoundaries)
{
    typedef std::numeric_limits<double> Limits;

    // Infinity.
    testNumberToStringECMAScript(Limits::infinity(), "Infinity");
    testNumberToStringECMAScript(-Limits::infinity(), "-Infinity");

    // NaN.
    testNumberToStringECMAScript(-Limits::quiet_NaN(), "NaN");

    // Zeros.
    testNumberToStringECMAScript(0, "0");
    testNumberToStringECMAScript(-0, "0");

    // Min-Max.
    testNumberToStringECMAScript(Limits::min(), "2.2250738585072014e-308");
    testNumberToStringECMAScript(Limits::max(), "1.7976931348623157e+308");
}

TEST(WTF, StringNumberToStringECMAScriptRegularNumbers)
{
    // Pi.
    testNumberToStringECMAScript(piDouble, "3.141592653589793");
    testNumberToStringECMAScript(piFloat, "3.1415927410125732");
    testNumberToStringECMAScript(piOverTwoDouble, "1.5707963267948966");
    testNumberToStringECMAScript(piOverTwoFloat, "1.5707963705062866");
    testNumberToStringECMAScript(piOverFourDouble, "0.7853981633974483");
    testNumberToStringECMAScript(piOverFourFloat, "0.7853981852531433");

    // e.
    const double e = 2.71828182845904523536028747135266249775724709369995;
    testNumberToStringECMAScript(e, "2.718281828459045");

    // c, speed of light in m/s.
    const double c = 299792458;
    testNumberToStringECMAScript(c, "299792458");

    // Golen ratio.
    const double phi = 1.6180339887498948482;
    testNumberToStringECMAScript(phi, "1.618033988749895");
}

TEST(WTF, StringReplaceWithLiteral)
{
    // Cases for 8Bit source.
    String testString = "1224";
    ASSERT_TRUE(testString.is8Bit());
    testString.replaceWithLiteral('2', "");
    ASSERT_STREQ("14", testString.utf8().data());

    testString = "1224";
    ASSERT_TRUE(testString.is8Bit());
    testString.replaceWithLiteral('2', "3");
    ASSERT_STREQ("1334", testString.utf8().data());

    testString = "1224";
    ASSERT_TRUE(testString.is8Bit());
    testString.replaceWithLiteral('2', "555");
    ASSERT_STREQ("15555554", testString.utf8().data());

    testString = "1224";
    ASSERT_TRUE(testString.is8Bit());
    testString.replaceWithLiteral('3', "NotFound");
    ASSERT_STREQ("1224", testString.utf8().data());

    // Cases for 16Bit source.
    testString = String::fromUTF8("résumé");
    ASSERT_FALSE(testString.is8Bit());
    testString.replaceWithLiteral(UChar(0x00E9 /*U+00E9 is 'é'*/), "e");
    ASSERT_STREQ("resume", testString.utf8().data());

    testString = String::fromUTF8("résumé");
    ASSERT_FALSE(testString.is8Bit());
    testString.replaceWithLiteral(UChar(0x00E9 /*U+00E9 is 'é'*/), "");
    ASSERT_STREQ("rsum", testString.utf8().data());

    testString = String::fromUTF8("résumé");
    ASSERT_FALSE(testString.is8Bit());
    testString.replaceWithLiteral('3', "NotFound");
    ASSERT_STREQ("résumé", testString.utf8().data());
}

TEST(WTF, StringIsolatedCopy)
{
    String original = "1234";
    auto copy = WTFMove(original).isolatedCopy();
    ASSERT_FALSE(original.impl() == copy.impl());
}

TEST(WTF, StringToInt)
{
    bool ok = false;

    EXPECT_EQ(0, String().toInt());
    EXPECT_EQ(0, String().toInt(&ok));
    EXPECT_FALSE(ok);

    EXPECT_EQ(0, emptyString().toInt());
    EXPECT_EQ(0, emptyString().toInt(&ok));
    EXPECT_FALSE(ok);

    EXPECT_EQ(0, String("0").toInt());
    EXPECT_EQ(0, String("0").toInt(&ok));
    EXPECT_TRUE(ok);

    EXPECT_EQ(1, String("1").toInt());
    EXPECT_EQ(1, String("1").toInt(&ok));
    EXPECT_TRUE(ok);

    EXPECT_EQ(2147483647, String("2147483647").toInt());
    EXPECT_EQ(2147483647, String("2147483647").toInt(&ok));
    EXPECT_TRUE(ok);

    EXPECT_EQ(0, String("2147483648").toInt());
    EXPECT_EQ(0, String("2147483648").toInt(&ok));
    EXPECT_FALSE(ok);

    EXPECT_EQ(-2147483648, String("-2147483648").toInt());
    EXPECT_EQ(-2147483648, String("-2147483648").toInt(&ok));
    EXPECT_TRUE(ok);

    EXPECT_EQ(0, String("-2147483649").toInt());
    EXPECT_EQ(0, String("-2147483649").toInt(&ok));
    EXPECT_FALSE(ok);

    // fail if we see leading junk
    EXPECT_EQ(0, String("x1").toInt());
    EXPECT_EQ(0, String("x1").toInt(&ok));
    EXPECT_FALSE(ok);

    // succeed if we see leading spaces
    EXPECT_EQ(1, String(" 1").toInt());
    EXPECT_EQ(1, String(" 1").toInt(&ok));
    EXPECT_TRUE(ok);

    // silently ignore trailing junk
    EXPECT_EQ(1, String("1x").toInt());
    EXPECT_EQ(1, String("1x").toInt(&ok));
    EXPECT_TRUE(ok);
}

TEST(WTF, StringToDouble)
{
    bool ok = false;

    EXPECT_EQ(0.0, String().toDouble());
    EXPECT_EQ(0.0, String().toDouble(&ok));
    EXPECT_FALSE(ok);

    EXPECT_EQ(0.0, emptyString().toDouble());
    EXPECT_EQ(0.0, emptyString().toDouble(&ok));
    EXPECT_FALSE(ok);

    EXPECT_EQ(0.0, String("0").toDouble());
    EXPECT_EQ(0.0, String("0").toDouble(&ok));
    EXPECT_TRUE(ok);

    EXPECT_EQ(1.0, String("1").toDouble());
    EXPECT_EQ(1.0, String("1").toDouble(&ok));
    EXPECT_TRUE(ok);

    // fail if we see leading junk
    EXPECT_EQ(0.0, String("x1").toDouble());
    EXPECT_EQ(0.0, String("x1").toDouble(&ok));
    EXPECT_FALSE(ok);

    // succeed if we see leading spaces
    EXPECT_EQ(1.0, String(" 1").toDouble());
    EXPECT_EQ(1.0, String(" 1").toDouble(&ok));
    EXPECT_TRUE(ok);

    // ignore trailing junk, but return false for "ok"
    // FIXME: This is an inconsistency with toInt, which always guarantees
    // it will return 0 if it's also going to return false for ok.
    EXPECT_EQ(1.0, String("1x").toDouble());
    EXPECT_EQ(1.0, String("1x").toDouble(&ok));
    EXPECT_FALSE(ok);

    // parse only numbers, not special values such as "infinity"
    EXPECT_EQ(0.0, String("infinity").toDouble());
    EXPECT_EQ(0.0, String("infinity").toDouble(&ok));
    EXPECT_FALSE(ok);

    // parse only numbers, not special values such as "nan"
    EXPECT_EQ(0.0, String("nan").toDouble());
    EXPECT_EQ(0.0, String("nan").toDouble(&ok));
    EXPECT_FALSE(ok);
}

TEST(WTF, StringhasInfixStartingAt)
{
    EXPECT_TRUE(String("Test").is8Bit());
    EXPECT_TRUE(String("Te").is8Bit());
    EXPECT_TRUE(String("st").is8Bit());
    EXPECT_TRUE(String("Test").hasInfixStartingAt(String("Te"), 0));
    EXPECT_FALSE(String("Test").hasInfixStartingAt(String("Te"), 2));
    EXPECT_TRUE(String("Test").hasInfixStartingAt(String("st"), 2));
    EXPECT_FALSE(String("Test").hasInfixStartingAt(String("ST"), 2));

    EXPECT_FALSE(String::fromUTF8("中国").is8Bit());
    EXPECT_FALSE(String::fromUTF8("中").is8Bit());
    EXPECT_FALSE(String::fromUTF8("国").is8Bit());
    EXPECT_TRUE(String::fromUTF8("中国").hasInfixStartingAt(String::fromUTF8("中"), 0));
    EXPECT_FALSE(String::fromUTF8("中国").hasInfixStartingAt(String::fromUTF8("中"), 1));
    EXPECT_TRUE(String::fromUTF8("中国").hasInfixStartingAt(String::fromUTF8("国"), 1));

    EXPECT_FALSE(String::fromUTF8("中国").hasInfixStartingAt(String("Te"), 0));
    EXPECT_FALSE(String("Test").hasInfixStartingAt(String::fromUTF8("中"), 2));
}

TEST(WTF, StringExistingHash)
{
    String string1("Template Literal");
    ASSERT_FALSE(string1.isNull());
    ASSERT_FALSE(string1.impl()->hasHash());
    string1.impl()->hash();
    ASSERT_EQ(string1.existingHash(), string1.impl()->existingHash());
    String string2;
    ASSERT_EQ(string2.existingHash(), 0u);
}

TEST(WTF, StringUnicodeEqualUCharArray)
{
    String string1("abc");
    ASSERT_FALSE(string1.isNull());
    ASSERT_TRUE(string1.is8Bit());
    UChar ab[] = { 'a', 'b' };
    UChar abc[] = { 'a', 'b', 'c' };
    UChar abcd[] = { 'a', 'b', 'c', 'd' };
    UChar aBc[] = { 'a', 'B', 'c' };
    ASSERT_FALSE(equal(string1, ab));
    ASSERT_TRUE(equal(string1, abc));
    ASSERT_FALSE(equal(string1, abcd));
    ASSERT_FALSE(equal(string1, aBc));

    String string2(abc, 3);
    ASSERT_FALSE(equal(string2, ab));
    ASSERT_TRUE(equal(string2, abc));
    ASSERT_FALSE(equal(string2, abcd));
    ASSERT_FALSE(equal(string2, aBc));
}

TEST(WTF, StringRightBasic)
{
    auto reference = String::fromUTF8("Cappuccino");
    EXPECT_EQ(String::fromUTF8(""), reference.right(0));
    EXPECT_EQ(String::fromUTF8("o"), reference.right(1));
    EXPECT_EQ(String::fromUTF8("no"), reference.right(2));
    EXPECT_EQ(String::fromUTF8("ino"), reference.right(3));
    EXPECT_EQ(String::fromUTF8("cino"), reference.right(4));
    EXPECT_EQ(String::fromUTF8("ccino"), reference.right(5));
    EXPECT_EQ(String::fromUTF8("uccino"), reference.right(6));
    EXPECT_EQ(String::fromUTF8("puccino"), reference.right(7));
    EXPECT_EQ(String::fromUTF8("ppuccino"), reference.right(8));
    EXPECT_EQ(String::fromUTF8("appuccino"), reference.right(9));
    EXPECT_EQ(String::fromUTF8("Cappuccino"), reference.right(10));
}

TEST(WTF, StringLeftBasic)
{
    auto reference = String::fromUTF8("Cappuccino");
    EXPECT_EQ(String::fromUTF8(""), reference.left(0));
    EXPECT_EQ(String::fromUTF8("C"), reference.left(1));
    EXPECT_EQ(String::fromUTF8("Ca"), reference.left(2));
    EXPECT_EQ(String::fromUTF8("Cap"), reference.left(3));
    EXPECT_EQ(String::fromUTF8("Capp"), reference.left(4));
    EXPECT_EQ(String::fromUTF8("Cappu"), reference.left(5));
    EXPECT_EQ(String::fromUTF8("Cappuc"), reference.left(6));
    EXPECT_EQ(String::fromUTF8("Cappucc"), reference.left(7));
    EXPECT_EQ(String::fromUTF8("Cappucci"), reference.left(8));
    EXPECT_EQ(String::fromUTF8("Cappuccin"), reference.left(9));
    EXPECT_EQ(String::fromUTF8("Cappuccino"), reference.left(10));
}

TEST(WTF, StringReverseFindBasic)
{
    auto reference = String::fromUTF8("Cappuccino");
    EXPECT_EQ(reference.reverseFind('o'), 9U);
    EXPECT_EQ(reference.reverseFind('n'), 8U);
    EXPECT_EQ(reference.reverseFind('c'), 6U);
    EXPECT_EQ(reference.reverseFind('p'), 3U);
    EXPECT_EQ(reference.reverseFind('k'), notFound);

    EXPECT_EQ(reference.reverseFind('o', 8), notFound);
    EXPECT_EQ(reference.reverseFind('c', 8), 6U);
    EXPECT_EQ(reference.reverseFind('c', 6), 6U);
    EXPECT_EQ(reference.reverseFind('c', 5), 5U);
    EXPECT_EQ(reference.reverseFind('c', 4), notFound);
}

} // namespace TestWebKitAPI
