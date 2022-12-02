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

#include "Test.h"
#include <limits>
#include <sstream>
#include <wtf/MathExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF, StringCreationFromLiteral)
{
    String stringFromLiteralViaASCII("Explicit construction syntax"_s);
    EXPECT_EQ(strlen("Explicit construction syntax"), stringFromLiteralViaASCII.length());
    EXPECT_EQ("Explicit construction syntax"_s, stringFromLiteralViaASCII);
    EXPECT_TRUE(stringFromLiteralViaASCII.is8Bit());
    EXPECT_EQ(String("Explicit construction syntax"_s), stringFromLiteralViaASCII);

    String stringFromLiteral = "String Literal"_str;
    EXPECT_EQ(strlen("String Literal"), stringFromLiteral.length());
    EXPECT_EQ("String Literal"_s, stringFromLiteral);
    EXPECT_TRUE(stringFromLiteral.is8Bit());
    EXPECT_EQ(String("String Literal"_s), stringFromLiteral);
}

TEST(WTF, StringASCII)
{
    CString output;

    // Null String.
    output = String().ascii();
    EXPECT_STREQ("", output.data());

    // Empty String.
    output = emptyString().ascii();
    EXPECT_STREQ("", output.data());

    // Regular String.
    output = String("foobar"_s).ascii();
    EXPECT_STREQ("foobar", output.data());
}

TEST(WTF, StringStartsWithEmptyVsNull)
{
    String nullString;
    String emptyString = WTF::emptyString();
    String stringWithCharacters("hello"_s);

    EXPECT_TRUE(stringWithCharacters.startsWith(nullString));
    EXPECT_TRUE(stringWithCharacters.startsWith(emptyString));
}

static inline const char* testStringNumberFixedPrecision(double number)
{
    static char testBuffer[100] = { };
    std::strncpy(testBuffer, String::numberToStringFixedPrecision(number).utf8().data(), 99);
    return testBuffer;
}

TEST(WTF, StringNumberFixedPrecision)
{
    using Limits = std::numeric_limits<double>;

    EXPECT_STREQ("Infinity", testStringNumberFixedPrecision(Limits::infinity()));
    EXPECT_STREQ("-Infinity", testStringNumberFixedPrecision(-Limits::infinity()));

    EXPECT_STREQ("NaN", testStringNumberFixedPrecision(-Limits::quiet_NaN()));

    EXPECT_STREQ("0", testStringNumberFixedPrecision(0));
    EXPECT_STREQ("0", testStringNumberFixedPrecision(-0));

    EXPECT_STREQ("2.22507e-308", testStringNumberFixedPrecision(Limits::min()));
    EXPECT_STREQ("-1.79769e+308", testStringNumberFixedPrecision(Limits::lowest()));
    EXPECT_STREQ("1.79769e+308", testStringNumberFixedPrecision(Limits::max()));

    EXPECT_STREQ("3.14159", testStringNumberFixedPrecision(piDouble));
    EXPECT_STREQ("3.14159", testStringNumberFixedPrecision(piFloat));
    EXPECT_STREQ("1.5708", testStringNumberFixedPrecision(piOverTwoDouble));
    EXPECT_STREQ("1.5708", testStringNumberFixedPrecision(piOverTwoFloat));
    EXPECT_STREQ("0.785398", testStringNumberFixedPrecision(piOverFourDouble));
    EXPECT_STREQ("0.785398", testStringNumberFixedPrecision(piOverFourFloat));

    EXPECT_STREQ("2.71828", testStringNumberFixedPrecision(2.71828182845904523536028747135266249775724709369995));

    EXPECT_STREQ("2.99792e+8", testStringNumberFixedPrecision(299792458));

    EXPECT_STREQ("1.61803", testStringNumberFixedPrecision(1.6180339887498948482));

    EXPECT_STREQ("1000", testStringNumberFixedPrecision(1e3));
    EXPECT_STREQ("1e+10", testStringNumberFixedPrecision(1e10));
    EXPECT_STREQ("1e+20", testStringNumberFixedPrecision(1e20));
    EXPECT_STREQ("1e+21", testStringNumberFixedPrecision(1e21));
    EXPECT_STREQ("1e+30", testStringNumberFixedPrecision(1e30));

    EXPECT_STREQ("1100", testStringNumberFixedPrecision(1.1e3));
    EXPECT_STREQ("1.1e+10", testStringNumberFixedPrecision(1.1e10));
    EXPECT_STREQ("1.1e+20", testStringNumberFixedPrecision(1.1e20));
    EXPECT_STREQ("1.1e+21", testStringNumberFixedPrecision(1.1e21));
    EXPECT_STREQ("1.1e+30", testStringNumberFixedPrecision(1.1e30));
}

static inline const char* testStringNumberFixedWidth(double number)
{
    static char testBuffer[100] = { };
    std::strncpy(testBuffer, String::numberToStringFixedWidth(number, 6).utf8().data(), 99);
    return testBuffer;
}

TEST(WTF, StringNumberFixedWidth)
{
    using Limits = std::numeric_limits<double>;

    EXPECT_STREQ("Infinity", testStringNumberFixedWidth(Limits::infinity()));
    EXPECT_STREQ("-Infinity", testStringNumberFixedWidth(-Limits::infinity()));

    EXPECT_STREQ("NaN", testStringNumberFixedWidth(-Limits::quiet_NaN()));

    EXPECT_STREQ("0.000000", testStringNumberFixedWidth(0));
    EXPECT_STREQ("0.000000", testStringNumberFixedWidth(-0));

    EXPECT_STREQ("0.000000", testStringNumberFixedWidth(Limits::min()));
    EXPECT_STREQ("", testStringNumberFixedWidth(Limits::lowest()));
    EXPECT_STREQ("", testStringNumberFixedWidth(Limits::max()));

    EXPECT_STREQ("3.141593", testStringNumberFixedWidth(piDouble));
    EXPECT_STREQ("3.141593", testStringNumberFixedWidth(piFloat));
    EXPECT_STREQ("1.570796", testStringNumberFixedWidth(piOverTwoDouble));
    EXPECT_STREQ("1.570796", testStringNumberFixedWidth(piOverTwoFloat));
    EXPECT_STREQ("0.785398", testStringNumberFixedWidth(piOverFourDouble));
    EXPECT_STREQ("0.785398", testStringNumberFixedWidth(piOverFourFloat));

    EXPECT_STREQ("2.718282", testStringNumberFixedWidth(2.71828182845904523536028747135266249775724709369995));

    EXPECT_STREQ("299792458.000000", testStringNumberFixedWidth(299792458));

    EXPECT_STREQ("1.618034", testStringNumberFixedWidth(1.6180339887498948482));

    EXPECT_STREQ("1000.000000", testStringNumberFixedWidth(1e3));
    EXPECT_STREQ("10000000000.000000", testStringNumberFixedWidth(1e10));
    EXPECT_STREQ("100000000000000000000.000000", testStringNumberFixedWidth(1e20));
    EXPECT_STREQ("", testStringNumberFixedWidth(1e21));

    EXPECT_STREQ("1100.000000", testStringNumberFixedWidth(1.1e3));
    EXPECT_STREQ("11000000000.000000", testStringNumberFixedWidth(1.1e10));
    EXPECT_STREQ("110000000000000000000.000000", testStringNumberFixedWidth(1.1e20));
    EXPECT_STREQ("", testStringNumberFixedWidth(1.1e21));
}

static inline const char* testStringNumber(double number)
{
    static char testBuffer[100] = { };
    std::strncpy(testBuffer, String::number(number).utf8().data(), 99);
    return testBuffer;
}

TEST(WTF, StringNumber)
{
    using Limits = std::numeric_limits<double>;

    EXPECT_STREQ("Infinity", testStringNumber(Limits::infinity()));
    EXPECT_STREQ("-Infinity", testStringNumber(-Limits::infinity()));

    EXPECT_STREQ("NaN", testStringNumber(-Limits::quiet_NaN()));

    EXPECT_STREQ("0", testStringNumber(0));
    EXPECT_STREQ("0", testStringNumber(-0));

    EXPECT_STREQ("2.2250738585072014e-308", testStringNumber(Limits::min()));
    EXPECT_STREQ("-1.7976931348623157e+308", testStringNumber(Limits::lowest()));
    EXPECT_STREQ("1.7976931348623157e+308", testStringNumber(Limits::max()));

    EXPECT_STREQ("3.141592653589793", testStringNumber(piDouble));
    EXPECT_STREQ("3.1415927410125732", testStringNumber(piFloat));
    EXPECT_STREQ("1.5707963267948966", testStringNumber(piOverTwoDouble));
    EXPECT_STREQ("1.5707963705062866", testStringNumber(piOverTwoFloat));
    EXPECT_STREQ("0.7853981633974483", testStringNumber(piOverFourDouble));
    EXPECT_STREQ("0.7853981852531433", testStringNumber(piOverFourFloat));

    EXPECT_STREQ("2.718281828459045", testStringNumber(2.71828182845904523536028747135266249775724709369995));

    EXPECT_STREQ("299792458", testStringNumber(299792458));

    EXPECT_STREQ("1.618033988749895", testStringNumber(1.6180339887498948482));

    EXPECT_STREQ("1000", testStringNumber(1e3));
    EXPECT_STREQ("10000000000", testStringNumber(1e10));
    EXPECT_STREQ("100000000000000000000", testStringNumber(1e20));
    EXPECT_STREQ("1e+21", testStringNumber(1e21));
    EXPECT_STREQ("1e+30", testStringNumber(1e30));

    EXPECT_STREQ("1100", testStringNumber(1.1e3));
    EXPECT_STREQ("11000000000", testStringNumber(1.1e10));
    EXPECT_STREQ("110000000000000000000", testStringNumber(1.1e20));
    EXPECT_STREQ("1.1e+21", testStringNumber(1.1e21));
    EXPECT_STREQ("1.1e+30", testStringNumber(1.1e30));
}

TEST(WTF, StringNumberIntMin)
{
    constexpr int intMin = std::numeric_limits<int>::min();
    String result = String::number(intMin);

    std::stringstream stringStream;
    stringStream << intMin;
    std::string expectedString;
    stringStream >> expectedString;

    EXPECT_TRUE(result == String::fromLatin1(expectedString.c_str()));
}

TEST(WTF, StringReplaceWithLiteral)
{
    // Cases for 8Bit source.
    String testString = "1224"_s;
    EXPECT_TRUE(testString.is8Bit());
    testString = makeStringByReplacingAll(testString, '2', ""_s);
    EXPECT_STREQ("14", testString.utf8().data());

    testString = "1224"_s;
    EXPECT_TRUE(testString.is8Bit());
    testString = makeStringByReplacingAll(testString, '2', "3"_s);
    EXPECT_STREQ("1334", testString.utf8().data());

    testString = "1224"_s;
    EXPECT_TRUE(testString.is8Bit());
    testString = makeStringByReplacingAll(testString, '2', "555"_s);
    EXPECT_STREQ("15555554", testString.utf8().data());

    testString = "1224"_s;
    EXPECT_TRUE(testString.is8Bit());
    testString = makeStringByReplacingAll(testString, '3', "NotFound"_s);
    EXPECT_STREQ("1224", testString.utf8().data());

    // Cases for 16Bit source.
    testString = String::fromUTF8("résumé");
    EXPECT_FALSE(testString.is8Bit());
    testString = makeStringByReplacingAll(testString, UChar(0x00E9 /*U+00E9 is 'é'*/), "e"_s);
    EXPECT_STREQ("resume", testString.utf8().data());

    testString = String::fromUTF8("résumé");
    EXPECT_FALSE(testString.is8Bit());
    testString = makeStringByReplacingAll(testString, UChar(0x00E9 /*U+00E9 is 'é'*/), ""_s);
    EXPECT_STREQ("rsum", testString.utf8().data());

    testString = String::fromUTF8("résumé");
    EXPECT_FALSE(testString.is8Bit());
    testString = makeStringByReplacingAll(testString, '3', "NotFound"_s);
    EXPECT_STREQ("résumé", testString.utf8().data());
}

TEST(WTF, StringIsolatedCopy)
{
    String original = "1234"_s;
    auto copy = WTFMove(original).isolatedCopy();
    EXPECT_FALSE(original.impl() == copy.impl());
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

    EXPECT_EQ(0.0, String("0"_s).toDouble());
    EXPECT_EQ(0.0, String("0"_s).toDouble(&ok));
    EXPECT_TRUE(ok);

    EXPECT_EQ(1.0, String("1"_s).toDouble());
    EXPECT_EQ(1.0, String("1"_s).toDouble(&ok));
    EXPECT_TRUE(ok);

    // fail if we see leading junk
    EXPECT_EQ(0.0, String("x1"_s).toDouble());
    EXPECT_EQ(0.0, String("x1"_s).toDouble(&ok));
    EXPECT_FALSE(ok);

    // succeed if we see leading spaces
    EXPECT_EQ(1.0, String(" 1"_s).toDouble());
    EXPECT_EQ(1.0, String(" 1"_s).toDouble(&ok));
    EXPECT_TRUE(ok);

    // ignore trailing junk, but return false for "ok"
    // FIXME: This is an inconsistency with toInt, which always guarantees
    // it will return 0 if it's also going to return false for ok.
    EXPECT_EQ(1.0, String("1x"_s).toDouble());
    EXPECT_EQ(1.0, String("1x"_s).toDouble(&ok));
    EXPECT_FALSE(ok);

    // parse only numbers, not special values such as "infinity"
    EXPECT_EQ(0.0, String("infinity"_s).toDouble());
    EXPECT_EQ(0.0, String("infinity"_s).toDouble(&ok));
    EXPECT_FALSE(ok);

    // parse only numbers, not special values such as "nan"
    EXPECT_EQ(0.0, String("nan"_s).toDouble());
    EXPECT_EQ(0.0, String("nan"_s).toDouble(&ok));
    EXPECT_FALSE(ok);
}

TEST(WTF, StringhasInfixStartingAt)
{
    EXPECT_TRUE(String("Test"_s).is8Bit());
    EXPECT_TRUE(String("Te"_s).is8Bit());
    EXPECT_TRUE(String("st"_s).is8Bit());
    EXPECT_TRUE(String("Test"_s).hasInfixStartingAt("Te"_s, 0));
    EXPECT_FALSE(String("Test"_s).hasInfixStartingAt("Te"_s, 2));
    EXPECT_TRUE(String("Test"_s).hasInfixStartingAt("st"_s, 2));
    EXPECT_FALSE(String("Test"_s).hasInfixStartingAt("ST"_s, 2));

    EXPECT_FALSE(String::fromUTF8("中国").is8Bit());
    EXPECT_FALSE(String::fromUTF8("中").is8Bit());
    EXPECT_FALSE(String::fromUTF8("国").is8Bit());
    EXPECT_TRUE(String::fromUTF8("中国").hasInfixStartingAt(String::fromUTF8("中"), 0));
    EXPECT_FALSE(String::fromUTF8("中国").hasInfixStartingAt(String::fromUTF8("中"), 1));
    EXPECT_TRUE(String::fromUTF8("中国").hasInfixStartingAt(String::fromUTF8("国"), 1));

    EXPECT_FALSE(String::fromUTF8("中国").hasInfixStartingAt("Te"_s, 0));
    EXPECT_FALSE(String("Test"_s).hasInfixStartingAt(String::fromUTF8("中"), 2));
}

TEST(WTF, StringExistingHash)
{
    String string1("Template Literal"_s);
    EXPECT_FALSE(string1.isNull());
    EXPECT_FALSE(string1.impl()->hasHash());
    string1.impl()->hash();
    EXPECT_EQ(string1.existingHash(), string1.impl()->existingHash());
    String string2;
    EXPECT_EQ(string2.existingHash(), 0u);
}

TEST(WTF, StringUnicodeEqualUCharArray)
{
    String string1("abc"_s);
    EXPECT_FALSE(string1.isNull());
    EXPECT_TRUE(string1.is8Bit());
    UChar ab[] = { 'a', 'b' };
    UChar abc[] = { 'a', 'b', 'c' };
    UChar abcd[] = { 'a', 'b', 'c', 'd' };
    UChar aBc[] = { 'a', 'B', 'c' };
    EXPECT_FALSE(equal(string1, ab));
    EXPECT_TRUE(equal(string1, abc));
    EXPECT_FALSE(equal(string1, abcd));
    EXPECT_FALSE(equal(string1, aBc));

    String string2(abc, 3);
    EXPECT_FALSE(equal(string2, ab));
    EXPECT_TRUE(equal(string2, abc));
    EXPECT_FALSE(equal(string2, abcd));
    EXPECT_FALSE(equal(string2, aBc));
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

TEST(WTF, StringSplitWithConsecutiveSeparators)
{
    String string { " This     is  a       sentence. "_s };

    Vector<String> actual = string.split(' ');
    Vector<String> expected { "This"_s, "is"_s, "a"_s, "sentence."_s };
    ASSERT_EQ(expected.size(), actual.size());
    for (auto i = 0u; i < actual.size(); ++i)
        EXPECT_STREQ(expected[i].utf8().data(), actual[i].utf8().data()) << "Vectors differ at index " << i;

    actual = string.splitAllowingEmptyEntries(' ');
    expected = { ""_s, "This"_s, ""_s, ""_s, ""_s, ""_s, "is"_s, ""_s, "a"_s, ""_s, ""_s, ""_s, ""_s, ""_s, ""_s, "sentence."_s, ""_s };
    ASSERT_EQ(expected.size(), actual.size());
    for (auto i = 0u; i < actual.size(); ++i)
        EXPECT_STREQ(expected[i].utf8().data(), actual[i].utf8().data()) << "Vectors differ at index " << i;
}

TEST(WTF, StringMakeStringByJoining)
{
    Vector<String> test1 = { "hello"_s, "foo"_s, "bar"_s };
    auto test1_result = makeStringByJoining(test1, " "_s);
    ASSERT_EQ(test1_result, "hello foo bar"_s);

    std::array<String, 1> test2 = { "hello"_s };
    auto test2_result = makeStringByJoining(test2, "sdf"_s);
    ASSERT_EQ(test2_result, "hello"_s);
    
    std::vector<String> test3 = { "foo"_s, "bar"_s };
    auto test3_result = makeStringByJoining(test3, ", "_s);
    ASSERT_EQ(test3_result, "foo, bar"_s);
}

} // namespace TestWebKitAPI
