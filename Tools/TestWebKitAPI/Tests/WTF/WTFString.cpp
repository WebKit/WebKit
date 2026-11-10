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

#include "Helpers/Test.h"
#include <limits>
#include <numbers>
#include <sstream>
#include <wtf/ASCIICType.h>
#include <wtf/MathExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF, StringCreationFromLiteral)
{
    RefPtr<StringImpl> stringFromLiteralViaASCII("Explicit construction syntax"_si);
    EXPECT_EQ(strlen("Explicit construction syntax"), length(stringFromLiteralViaASCII));
    EXPECT_EQ("Explicit construction syntax"_s, stringFromLiteralViaASCII);
    EXPECT_TRUE(is8Bit(stringFromLiteralViaASCII));
    EXPECT_EQ("Explicit construction syntax"_si, stringFromLiteralViaASCII);

    RefPtr<StringImpl> stringFromLiteral = "String Literal"_str;
    EXPECT_EQ(strlen("String Literal"), length(stringFromLiteral));
    EXPECT_EQ("String Literal"_s, stringFromLiteral);
    EXPECT_TRUE(is8Bit(stringFromLiteral));
    EXPECT_EQ("String Literal"_si, stringFromLiteral);
}

TEST(WTF, StringASCII)
{
    ASCIICString output;

    // Null String.
    output = ascii(nullString());
    EXPECT_STREQ("", output.data());

    // Empty String.
    output = ascii(emptyString());
    EXPECT_STREQ("", output.data());

    // Regular String.
    output = ascii("foobar"_si);
    EXPECT_STREQ("foobar", output.data());
}

TEST(WTF, StringStartsWithEmptyVsNull)
{
    RefPtr<StringImpl> nullString;
    RefPtr<StringImpl> emptyString = WTF::emptyString();
    RefPtr<StringImpl> stringWithCharacters("hello"_si);

    EXPECT_TRUE(startsWith(stringWithCharacters, nullString));
    EXPECT_TRUE(startsWith(stringWithCharacters, emptyString));
}

static inline UTF8CString testStringNumberFixedPrecision(double number)
{
    return utf8(StringImpl::numberToStringFixedPrecision(number));
}

TEST(WTF, StringNumberFixedPrecision)
{
    using Limits = std::numeric_limits<double>;

    EXPECT_EQ("Infinity"_s, testStringNumberFixedPrecision(Limits::infinity()));
    EXPECT_EQ("-Infinity"_s, testStringNumberFixedPrecision(-Limits::infinity()));

    EXPECT_EQ("NaN"_s, testStringNumberFixedPrecision(-Limits::quiet_NaN()));

    EXPECT_EQ("0"_s, testStringNumberFixedPrecision(0));
    EXPECT_EQ("0"_s, testStringNumberFixedPrecision(-0));

    EXPECT_EQ("2.22507e-308"_s, testStringNumberFixedPrecision(Limits::min()));
    EXPECT_EQ("-1.79769e+308"_s, testStringNumberFixedPrecision(Limits::lowest()));
    EXPECT_EQ("1.79769e+308"_s, testStringNumberFixedPrecision(Limits::max()));

    EXPECT_EQ("3.14159"_s, testStringNumberFixedPrecision(std::numbers::pi));
    EXPECT_EQ("3.14159"_s, testStringNumberFixedPrecision(std::numbers::pi_v<float>));
    EXPECT_EQ("1.5708"_s, testStringNumberFixedPrecision(piOverTwoDouble));
    EXPECT_EQ("1.5708"_s, testStringNumberFixedPrecision(piOverTwoFloat));
    EXPECT_EQ("0.785398"_s, testStringNumberFixedPrecision(piOverFourDouble));
    EXPECT_EQ("0.785398"_s, testStringNumberFixedPrecision(piOverFourFloat));

    EXPECT_EQ("2.71828"_s, testStringNumberFixedPrecision(2.71828182845904523536028747135266249775724709369995));

    EXPECT_EQ("2.99792e+8"_s, testStringNumberFixedPrecision(299792458));

    EXPECT_EQ("1.61803"_s, testStringNumberFixedPrecision(1.6180339887498948482));

    EXPECT_EQ("1000"_s, testStringNumberFixedPrecision(1e3));
    EXPECT_EQ("1e+10"_s, testStringNumberFixedPrecision(1e10));
    EXPECT_EQ("1e+20"_s, testStringNumberFixedPrecision(1e20));
    EXPECT_EQ("1e+21"_s, testStringNumberFixedPrecision(1e21));
    EXPECT_EQ("1e+30"_s, testStringNumberFixedPrecision(1e30));

    EXPECT_EQ("1100"_s, testStringNumberFixedPrecision(1.1e3));
    EXPECT_EQ("1.1e+10"_s, testStringNumberFixedPrecision(1.1e10));
    EXPECT_EQ("1.1e+20"_s, testStringNumberFixedPrecision(1.1e20));
    EXPECT_EQ("1.1e+21"_s, testStringNumberFixedPrecision(1.1e21));
    EXPECT_EQ("1.1e+30"_s, testStringNumberFixedPrecision(1.1e30));
}

static inline UTF8CString testStringNumberFixedWidth(double number)
{
    return utf8(StringImpl::numberToStringFixedWidth(number, 6));
}

TEST(WTF, StringNumberFixedWidth)
{
    using Limits = std::numeric_limits<double>;

    EXPECT_EQ("Infinity"_s, testStringNumberFixedWidth(Limits::infinity()));
    EXPECT_EQ("-Infinity"_s, testStringNumberFixedWidth(-Limits::infinity()));

    EXPECT_EQ("NaN"_s, testStringNumberFixedWidth(-Limits::quiet_NaN()));

    EXPECT_EQ("0.000000"_s, testStringNumberFixedWidth(0));
    EXPECT_EQ("0.000000"_s, testStringNumberFixedWidth(-0));

    EXPECT_EQ("0.000000"_s, testStringNumberFixedWidth(Limits::min()));
    EXPECT_EQ(""_s, testStringNumberFixedWidth(Limits::lowest()));
    EXPECT_EQ(""_s, testStringNumberFixedWidth(Limits::max()));

    EXPECT_EQ("3.141593"_s, testStringNumberFixedWidth(std::numbers::pi));
    EXPECT_EQ("3.141593"_s, testStringNumberFixedWidth(std::numbers::pi_v<float>));
    EXPECT_EQ("1.570796"_s, testStringNumberFixedWidth(piOverTwoDouble));
    EXPECT_EQ("1.570796"_s, testStringNumberFixedWidth(piOverTwoFloat));
    EXPECT_EQ("0.785398"_s, testStringNumberFixedWidth(piOverFourDouble));
    EXPECT_EQ("0.785398"_s, testStringNumberFixedWidth(piOverFourFloat));

    EXPECT_EQ("2.718282"_s, testStringNumberFixedWidth(2.71828182845904523536028747135266249775724709369995));

    EXPECT_EQ("299792458.000000"_s, testStringNumberFixedWidth(299792458));

    EXPECT_EQ("1.618034"_s, testStringNumberFixedWidth(1.6180339887498948482));

    EXPECT_EQ("1000.000000"_s, testStringNumberFixedWidth(1e3));
    EXPECT_EQ("10000000000.000000"_s, testStringNumberFixedWidth(1e10));
    EXPECT_EQ("100000000000000000000.000000"_s, testStringNumberFixedWidth(1e20));
    EXPECT_EQ(""_s, testStringNumberFixedWidth(1e21));

    EXPECT_EQ("1100.000000"_s, testStringNumberFixedWidth(1.1e3));
    EXPECT_EQ("11000000000.000000"_s, testStringNumberFixedWidth(1.1e10));
    EXPECT_EQ("110000000000000000000.000000"_s, testStringNumberFixedWidth(1.1e20));
    EXPECT_EQ(""_s, testStringNumberFixedWidth(1.1e21));
}

static inline UTF8CString testStringNumber(double number)
{
    return utf8(makeString(number));
}

TEST(WTF, StringNumber)
{
    using Limits = std::numeric_limits<double>;

    EXPECT_EQ("Infinity"_s, testStringNumber(Limits::infinity()));
    EXPECT_EQ("-Infinity"_s, testStringNumber(-Limits::infinity()));

    EXPECT_EQ("NaN"_s, testStringNumber(-Limits::quiet_NaN()));

    EXPECT_EQ("0"_s, testStringNumber(0));
    EXPECT_EQ("0"_s, testStringNumber(-0));

    EXPECT_EQ("2.2250738585072014e-308"_s, testStringNumber(Limits::min()));
    EXPECT_EQ("-1.7976931348623157e+308"_s, testStringNumber(Limits::lowest()));
    EXPECT_EQ("1.7976931348623157e+308"_s, testStringNumber(Limits::max()));

    EXPECT_EQ("3.141592653589793"_s, testStringNumber(std::numbers::pi));
    EXPECT_EQ("3.1415927410125732"_s, testStringNumber(std::numbers::pi_v<float>));
    EXPECT_EQ("1.5707963267948966"_s, testStringNumber(piOverTwoDouble));
    EXPECT_EQ("1.5707963705062866"_s, testStringNumber(piOverTwoFloat));
    EXPECT_EQ("0.7853981633974483"_s, testStringNumber(piOverFourDouble));
    EXPECT_EQ("0.7853981852531433"_s, testStringNumber(piOverFourFloat));

    EXPECT_EQ("2.718281828459045"_s, testStringNumber(2.71828182845904523536028747135266249775724709369995));

    EXPECT_EQ("299792458"_s, testStringNumber(299792458));

    EXPECT_EQ("1.618033988749895"_s, testStringNumber(1.6180339887498948482));

    EXPECT_EQ("1000"_s, testStringNumber(1e3));
    EXPECT_EQ("10000000000"_s, testStringNumber(1e10));
    EXPECT_EQ("100000000000000000000"_s, testStringNumber(1e20));
    EXPECT_EQ("1e+21"_s, testStringNumber(1e21));
    EXPECT_EQ("1e+30"_s, testStringNumber(1e30));

    EXPECT_EQ("1100"_s, testStringNumber(1.1e3));
    EXPECT_EQ("11000000000"_s, testStringNumber(1.1e10));
    EXPECT_EQ("110000000000000000000"_s, testStringNumber(1.1e20));
    EXPECT_EQ("1.1e+21"_s, testStringNumber(1.1e21));
    EXPECT_EQ("1.1e+30"_s, testStringNumber(1.1e30));
}

TEST(WTF, StringNumberIntMin)
{
    constexpr int intMin = std::numeric_limits<int>::min();
    RefPtr<StringImpl> result = makeString(intMin);

    std::stringstream stringStream;
    stringStream << intMin;
    std::string expectedString;
    stringStream >> expectedString;

    EXPECT_TRUE(equal(result, StringImpl::fromLatin1(expectedString.c_str())));
}

TEST(WTF, StringReplaceWithLiteral)
{
    // Cases for 8Bit source.
    RefPtr<StringImpl> testString = "1224"_si;
    EXPECT_TRUE(is8Bit(testString));
    testString = makeStringByReplacingAll(testString, '2', ""_s);
    EXPECT_EQ("14"_s, testString);

    testString = "1224"_si;
    EXPECT_TRUE(is8Bit(testString));
    testString = makeStringByReplacingAll(testString, '2', "3"_s);
    EXPECT_EQ("1334"_s, testString);

    testString = "1224"_si;
    EXPECT_TRUE(is8Bit(testString));
    testString = makeStringByReplacingAll(testString, '2', "555"_s);
    EXPECT_EQ("15555554"_s, testString);

    testString = "1224"_si;
    EXPECT_TRUE(is8Bit(testString));
    testString = makeStringByReplacingAll(testString, '3', "NotFound"_s);
    EXPECT_EQ("1224"_s, testString);

    // Cases for 16Bit source.
    testString = StringImpl::fromUTF8("résumé");
    EXPECT_FALSE(is8Bit(testString));
    testString = makeStringByReplacingAll(testString, char16_t(0x00E9 /*U+00E9 is 'é'*/), "e"_s);
    EXPECT_EQ("resume"_s, testString);

    testString = StringImpl::fromUTF8("résumé");
    EXPECT_FALSE(is8Bit(testString));
    testString = makeStringByReplacingAll(testString, char16_t(0x00E9 /*U+00E9 is 'é'*/), ""_s);
    EXPECT_EQ("rsum"_s, testString);

    testString = StringImpl::fromUTF8("résumé");
    EXPECT_FALSE(is8Bit(testString));
    testString = makeStringByReplacingAll(testString, '3', "NotFound"_s);
    EXPECT_EQ(UTF8CString { u8"résumé"_span }, utf8(testString));
}

TEST(WTF, StringIsolatedCopy)
{
    RefPtr<StringImpl> original = "1234"_si;
    auto copy = isolatedCopy(WTF::move(original));
    EXPECT_FALSE(original.get() == copy.get());
}

TEST(WTF, StringToDouble)
{
    bool ok = false;

    EXPECT_EQ(0.0, toDouble(nullString()));
    EXPECT_EQ(0.0, toDouble(nullString(), &ok));
    EXPECT_FALSE(ok);

    EXPECT_EQ(0.0, toDouble(emptyString()));
    EXPECT_EQ(0.0, toDouble(emptyString(), &ok));
    EXPECT_FALSE(ok);

    EXPECT_EQ(0.0, toDouble("0"_si));
    EXPECT_EQ(0.0, toDouble("0"_si, &ok));
    EXPECT_TRUE(ok);

    EXPECT_EQ(1.0, toDouble("1"_si));
    EXPECT_EQ(1.0, toDouble("1"_si, &ok));
    EXPECT_TRUE(ok);

    // fail if we see leading junk
    EXPECT_EQ(0.0, toDouble("x1"_si));
    EXPECT_EQ(0.0, toDouble("x1"_si, &ok));
    EXPECT_FALSE(ok);

    // succeed if we see leading spaces
    EXPECT_EQ(1.0, toDouble(" 1"_si));
    EXPECT_EQ(1.0, toDouble(" 1"_si, &ok));
    EXPECT_TRUE(ok);

    // ignore trailing junk, but return false for "ok"
    // FIXME: This is an inconsistency with toInt, which always guarantees
    // it will return 0 if it's also going to return false for ok.
    EXPECT_EQ(1.0, toDouble("1x"_si));
    EXPECT_EQ(1.0, toDouble("1x"_si, &ok));
    EXPECT_FALSE(ok);

    // parse only numbers, not special values such as "infinity"
    EXPECT_EQ(0.0, toDouble("infinity"_si));
    EXPECT_EQ(0.0, toDouble("infinity"_si, &ok));
    EXPECT_FALSE(ok);

    // parse only numbers, not special values such as "nan"
    EXPECT_EQ(0.0, toDouble("nan"_si));
    EXPECT_EQ(0.0, toDouble("nan"_si, &ok));
    EXPECT_FALSE(ok);
}

TEST(WTF, StringToFloat)
{
    bool succeeded = false;

    EXPECT_EQ(0.0f, toFloat(nullString()));
    EXPECT_EQ(0.0f, toFloat(nullString(), &succeeded));
    EXPECT_FALSE(succeeded);

    EXPECT_EQ(0.0f, toFloat(emptyString()));
    EXPECT_EQ(0.0f, toFloat(emptyString(), &succeeded));
    EXPECT_FALSE(succeeded);

    EXPECT_EQ(0.0f, toFloat("0"_si));
    EXPECT_EQ(0.0f, toFloat("0"_si, &succeeded));
    EXPECT_TRUE(succeeded);

    EXPECT_EQ(1.0f, toFloat("1"_si));
    EXPECT_EQ(1.0f, toFloat("1"_si, &succeeded));
    EXPECT_TRUE(succeeded);

    // fail if we see leading junk
    EXPECT_EQ(0.0f, toFloat("x1"_si));
    EXPECT_EQ(0.0f, toFloat("x1"_si, &succeeded));
    EXPECT_FALSE(succeeded);

    // succeed if we see leading spaces
    EXPECT_EQ(1.0f, toFloat(" 1"_si));
    EXPECT_EQ(1.0f, toFloat(" 1"_si, &succeeded));
    EXPECT_TRUE(succeeded);

    // ignore trailing junk, but return false for "succeeded"
    EXPECT_EQ(1.0f, toFloat("1x"_si));
    EXPECT_EQ(1.0f, toFloat("1x"_si, &succeeded));
    EXPECT_FALSE(succeeded);

    // fits in a double, but overflows a float: succeeded should be false
    EXPECT_TRUE(std::isinf(toFloat("1e300"_si)));
    EXPECT_TRUE(std::isinf(toFloat("1e300"_si, &succeeded)));
    EXPECT_FALSE(succeeded);

    EXPECT_TRUE(std::isinf(toFloat("-1e300"_si, &succeeded)));
    EXPECT_FALSE(succeeded);

    // a value that fits comfortably in both double and float should still report success
    EXPECT_FLOAT_EQ(1e30f, toFloat("1e30"_si, &succeeded));
    EXPECT_TRUE(succeeded);
}

TEST(WTF, StringhasInfixStartingAt)
{
    EXPECT_TRUE(is8Bit("Test"_si));
    EXPECT_TRUE(is8Bit("Te"_si));
    EXPECT_TRUE(is8Bit("st"_si));
    EXPECT_TRUE(hasInfixStartingAt("Test"_si, "Te"_s, 0));
    EXPECT_FALSE(hasInfixStartingAt("Test"_si, "Te"_s, 2));
    EXPECT_TRUE(hasInfixStartingAt("Test"_si, "st"_s, 2));
    EXPECT_FALSE(hasInfixStartingAt("Test"_si, "ST"_s, 2));

    EXPECT_FALSE(is8Bit(StringImpl::fromUTF8("中国")));
    EXPECT_FALSE(is8Bit(StringImpl::fromUTF8("中")));
    EXPECT_FALSE(is8Bit(StringImpl::fromUTF8("国")));
    EXPECT_TRUE(hasInfixStartingAt(StringImpl::fromUTF8("中国"), StringImpl::fromUTF8("中"), 0));
    EXPECT_FALSE(hasInfixStartingAt(StringImpl::fromUTF8("中国"), StringImpl::fromUTF8("中"), 1));
    EXPECT_TRUE(hasInfixStartingAt(StringImpl::fromUTF8("中国"), StringImpl::fromUTF8("国"), 1));

    EXPECT_FALSE(hasInfixStartingAt(StringImpl::fromUTF8("中国"), "Te"_s, 0));
    EXPECT_FALSE(hasInfixStartingAt("Test"_si, StringImpl::fromUTF8("中"), 2));
}

TEST(WTF, StringExistingHash)
{
    RefPtr<StringImpl> string1 = StringImpl::create("Template Literal"_s);
    EXPECT_FALSE(!string1);
    EXPECT_FALSE(string1.get()->hasHash());
    string1.get()->hash();
    EXPECT_EQ(existingHash(string1), string1.get()->existingHash());
    RefPtr<StringImpl> string2;
    EXPECT_EQ(existingHash(string2), 0u);
}

TEST(WTF, StringUnicodeEqualUTF16Array)
{
    RefPtr<StringImpl> string1("abc"_si);
    EXPECT_FALSE(!string1);
    EXPECT_TRUE(is8Bit(string1));
    char16_t ab[] = { 'a', 'b' };
    char16_t abc[] = { 'a', 'b', 'c' };
    char16_t abcd[] = { 'a', 'b', 'c', 'd' };
    char16_t aBc[] = { 'a', 'B', 'c' };
    EXPECT_FALSE(equal(string1, ab));
    EXPECT_TRUE(equal(string1, abc));
    EXPECT_FALSE(equal(string1, abcd));
    EXPECT_FALSE(equal(string1, aBc));

    RefPtr<StringImpl> string2(StringImpl::create({ abc, 3 }));
    EXPECT_FALSE(equal(string2, ab));
    EXPECT_TRUE(equal(string2, abc));
    EXPECT_FALSE(equal(string2, abcd));
    EXPECT_FALSE(equal(string2, aBc));
}

TEST(WTF, StringRightBasic)
{
    auto reference = StringImpl::fromUTF8("Cappuccino");
    EXPECT_EQ(StringImpl::fromUTF8(""), right(reference, 0));
    EXPECT_EQ(StringImpl::fromUTF8("o"), right(reference, 1));
    EXPECT_EQ(StringImpl::fromUTF8("no"), right(reference, 2));
    EXPECT_EQ(StringImpl::fromUTF8("ino"), right(reference, 3));
    EXPECT_EQ(StringImpl::fromUTF8("cino"), right(reference, 4));
    EXPECT_EQ(StringImpl::fromUTF8("ccino"), right(reference, 5));
    EXPECT_EQ(StringImpl::fromUTF8("uccino"), right(reference, 6));
    EXPECT_EQ(StringImpl::fromUTF8("puccino"), right(reference, 7));
    EXPECT_EQ(StringImpl::fromUTF8("ppuccino"), right(reference, 8));
    EXPECT_EQ(StringImpl::fromUTF8("appuccino"), right(reference, 9));
    EXPECT_EQ(StringImpl::fromUTF8("Cappuccino"), right(reference, 10));
}

TEST(WTF, StringLeftBasic)
{
    auto reference = StringImpl::fromUTF8("Cappuccino");
    EXPECT_EQ(StringImpl::fromUTF8(""), left(reference, 0));
    EXPECT_EQ(StringImpl::fromUTF8("C"), left(reference, 1));
    EXPECT_EQ(StringImpl::fromUTF8("Ca"), left(reference, 2));
    EXPECT_EQ(StringImpl::fromUTF8("Cap"), left(reference, 3));
    EXPECT_EQ(StringImpl::fromUTF8("Capp"), left(reference, 4));
    EXPECT_EQ(StringImpl::fromUTF8("Cappu"), left(reference, 5));
    EXPECT_EQ(StringImpl::fromUTF8("Cappuc"), left(reference, 6));
    EXPECT_EQ(StringImpl::fromUTF8("Cappucc"), left(reference, 7));
    EXPECT_EQ(StringImpl::fromUTF8("Cappucci"), left(reference, 8));
    EXPECT_EQ(StringImpl::fromUTF8("Cappuccin"), left(reference, 9));
    EXPECT_EQ(StringImpl::fromUTF8("Cappuccino"), left(reference, 10));
}

TEST(WTF, StringReverseFindBasic)
{
    auto reference = StringImpl::fromUTF8("Cappuccino");
    EXPECT_EQ(reverseFind(reference, 'o'), 9U);
    EXPECT_EQ(reverseFind(reference, 'n'), 8U);
    EXPECT_EQ(reverseFind(reference, 'c'), 6U);
    EXPECT_EQ(reverseFind(reference, 'p'), 3U);
    EXPECT_EQ(reverseFind(reference, 'k'), notFound);

    EXPECT_EQ(reverseFind(reference, 'o', 8), notFound);
    EXPECT_EQ(reverseFind(reference, 'c', 8), 6U);
    EXPECT_EQ(reverseFind(reference, 'c', 6), 6U);
    EXPECT_EQ(reverseFind(reference, 'c', 5), 5U);
    EXPECT_EQ(reverseFind(reference, 'c', 4), notFound);
}

TEST(WTF, StringSplitWithConsecutiveSeparators)
{
    RefPtr<StringImpl> string { " This     is  a       sentence. "_si };

    Vector<RefPtr<StringImpl>> actual = split(string, ' ');
    Vector<RefPtr<StringImpl>> expected { "This"_s, "is"_s, "a"_s, "sentence."_s };
    ASSERT_EQ(expected.size(), actual.size());
    for (auto i = 0u; i < actual.size(); ++i)
        EXPECT_EQ(expected[i], actual[i]) << "Vectors differ at index " << i;

    actual = splitAllowingEmptyEntries(string, ' ');
    expected = { ""_s, "This"_s, ""_s, ""_s, ""_s, ""_s, "is"_s, ""_s, "a"_s, ""_s, ""_s, ""_s, ""_s, ""_s, ""_s, "sentence."_s, ""_s };
    ASSERT_EQ(expected.size(), actual.size());
    for (auto i = 0u; i < actual.size(); ++i)
        EXPECT_EQ(expected[i], actual[i]) << "Vectors differ at index " << i;
}

TEST(WTF, StringMakeStringByJoining)
{
    Vector<RefPtr<StringImpl>> test1 = { "hello"_s, "foo"_s, "bar"_s };
    auto test1_result = makeStringByJoining(test1, " "_si);
    ASSERT_EQ(test1_result, "hello foo bar"_s);

    std::array<RefPtr<StringImpl>, 1> test2 = { "hello"_s };
    auto test2_result = makeStringByJoining(test2, "sdf"_si);
    ASSERT_EQ(test2_result, "hello"_s);

    std::vector<RefPtr<StringImpl>> test3 = { "foo"_s, "bar"_s };
    auto test3_result = makeStringByJoining(test3, ", "_si);
    ASSERT_EQ(test3_result, "foo, bar"_s);

    Vector<RefPtr<StringImpl>> test4 = { emptyString(), "a"_s };
    ASSERT_EQ(makeStringByJoining(test4, "\n"_si), "\na"_s);

    Vector<RefPtr<StringImpl>> test5 = { emptyString(), emptyString(), "a"_s, "b"_s };
    ASSERT_EQ(makeStringByJoining(test5, "\n"_si), "\n\na\nb"_s);

    Vector<RefPtr<StringImpl>> test6 = { emptyString(), emptyString() };
    ASSERT_EQ(makeStringByJoining(test6, "\n"_si), "\n"_s);

    Vector<RefPtr<StringImpl>> test7 = { nullString(), "a"_s };
    ASSERT_EQ(makeStringByJoining(test7, "\n"_si), "\na"_s);

    Vector<RefPtr<StringImpl>> test8 = { "a"_s, emptyString(), "b"_s };
    ASSERT_EQ(makeStringByJoining(test8, "\n"_si), "a\n\nb"_s);

    auto test9_result = makeStringByJoining(Vector<RefPtr<StringImpl>> { }, ", "_si);
    ASSERT_TRUE(isEmpty(test9_result));
    ASSERT_FALSE(!test9_result);
}

TEST(WTF, StringUTF8ConversionInvalidUTF16LenientMode)
{
    // Test invalid UTF-16 (orphan surrogates) with LenientConversion mode.
    // Invalid sequences should be replaced with U+FFFD (replacement character).

    // Create a string with an orphan high surrogate (0xD800)
    char16_t orphanHighSurrogate[] = { 'a', 'b', 'c', 0xD800, 'd', 'e', 'f', 0 };
    RefPtr<StringImpl> stringWithOrphanHigh = StringImpl::create(std::span { orphanHighSurrogate, 7 });

    auto result = utf8(stringWithOrphanHigh, LenientConversion);
    // U+FFFD in UTF-8 is 0xEF 0xBF 0xBD
    EXPECT_EQ(UTF8CString { u8"abc\xEF\xBF\xBD" "def"_span }, result);

    // Create a string with an orphan low surrogate (0xDC00)
    char16_t orphanLowSurrogate[] = { 'x', 0xDC00, 'y', 0 };
    RefPtr<StringImpl> stringWithOrphanLow = StringImpl::create(std::span { orphanLowSurrogate, 3 });

    auto resultLow = utf8(stringWithOrphanLow, LenientConversion);
    EXPECT_EQ(UTF8CString { u8"x\xEF\xBF\xBDy"_span }, resultLow);

    // Create a string with two consecutive orphan surrogates
    char16_t doubleOrphan[] = { 0xD800, 0xD800, 0 };
    RefPtr<StringImpl> stringWithDoubleOrphan = StringImpl::create(std::span { doubleOrphan, 2 });

    auto resultDouble = utf8(stringWithDoubleOrphan, LenientConversion);
    // Each orphan should become one replacement character
    EXPECT_EQ(UTF8CString { u8"\xEF\xBF\xBD\xEF\xBF\xBD"_span }, resultDouble);

    // Create a string with reversed surrogate pair (low then high)
    char16_t reversedPair[] = { 0xDC00, 0xD800, 0 };
    RefPtr<StringImpl> stringWithReversed = StringImpl::create(std::span { reversedPair, 2 });

    auto resultReversed = utf8(stringWithReversed, LenientConversion);
    // Both are invalid, should become two replacement characters
    EXPECT_EQ(UTF8CString { u8"\xEF\xBF\xBD\xEF\xBF\xBD"_span }, resultReversed);
}

TEST(WTF, StringUTF8ConversionStrictReplacingMode)
{
    // Test StrictConversionReplacingUnpairedSurrogatesWithFFFD mode.
    // This mode replaces unpaired surrogates with U+FFFD.

    // Create a string with an orphan high surrogate
    char16_t orphanHighSurrogate[] = { 'a', 'b', 0xD800, 'c', 0 };
    RefPtr<StringImpl> stringWithOrphan = StringImpl::create(std::span { orphanHighSurrogate, 4 });

    auto result = utf8(stringWithOrphan, StrictConversionReplacingUnpairedSurrogatesWithFFFD);
    EXPECT_EQ(UTF8CString { u8"ab\xEF\xBF\xBD" "c"_span }, result);
}

TEST(WTF, StringSimplifyWhiteSpace)
{
    auto simplify = [](StringImpl* string) {
        return simplifyWhiteSpace(string, isASCIIWhitespace);
    };

    // Null and empty are returned unchanged.
    EXPECT_TRUE(!simplify(nullString()));
    EXPECT_EQ(emptyString(), simplify(emptyString()));

    // Already-simplified strings come back equal (fast path).
    EXPECT_EQ("word"_s, simplify("word"_si));
    EXPECT_EQ("two words"_s, simplify("two words"_si));
    EXPECT_EQ("a b c d e"_s, simplify("a b c d e"_si));

    // Leading and trailing whitespace is stripped.
    EXPECT_EQ("word"_s, simplify(" word"_si));
    EXPECT_EQ("word"_s, simplify("word "_si));
    EXPECT_EQ("word"_s, simplify("   word   "_si));
    EXPECT_EQ("two words"_s, simplify("  two words  "_si));

    // Interior runs collapse to a single space.
    EXPECT_EQ("two words"_s, simplify("two  words"_si));
    EXPECT_EQ("a b c"_s, simplify("a    b     c"_si));

    // Non-space whitespace is converted to a single space.
    EXPECT_EQ("a b"_s, simplify("a\tb"_si));
    EXPECT_EQ("a b"_s, simplify("a\nb"_si));
    EXPECT_EQ("a b"_s, simplify("a\r\n\t b"_si));
    EXPECT_EQ("a b"_s, simplify("\t a \f b \n"_si));

    // All-whitespace and single whitespace collapse to empty.
    EXPECT_EQ(emptyString(), simplify(" "_si));
    EXPECT_EQ(emptyString(), simplify(" \t\n\r "_si));

    // Vertical tab is not ASCII whitespace, so it is preserved unchanged.
    EXPECT_EQ("a\vb"_s, simplify("a\vb"_si));

    // 16-bit strings take the same path.
    char16_t wide[] = { ' ', ' ', 0x4E2D, '\t', 0x6587, ' ', ' ', 0 };
    char16_t expected[] = { 0x4E2D, ' ', 0x6587, 0 };
    RefPtr<StringImpl> wideString { StringImpl::create(std::span { wide, 7 }) };
    EXPECT_FALSE(is8Bit(wideString));
    EXPECT_EQ(StringImpl::create(std::span { expected, 3 }).ptr(), simplify(wideString));
}

} // namespace TestWebKitAPI
