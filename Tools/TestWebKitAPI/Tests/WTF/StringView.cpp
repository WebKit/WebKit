/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>

namespace TestWebKitAPI {

StringView stringViewFromLiteral(const char* characters)
{
    return StringView::fromLatin1(characters);
}

StringView stringViewFromUTF8(String& ref, const char* characters)
{
    ref = String::fromUTF8(characters);
    return ref;
}

TEST(WTF, StringViewStartsWithEmptyVsNull)
{
    StringView nullView;
    StringView emptyView = StringView::empty();
    String stringWithCharacters("hello"_s);
    StringView viewWithCharacters(stringWithCharacters);

    EXPECT_TRUE(viewWithCharacters.startsWith(nullView));
    EXPECT_TRUE(viewWithCharacters.startsWith(emptyView));
}

TEST(WTF, StringViewEmptyVsNull)
{
    StringView nullView;
    EXPECT_TRUE(nullView.isNull());
    EXPECT_TRUE(nullView.isEmpty());

    // Test in a boolean context to test operator bool().
    if (nullView)
        FAIL();
    else
        SUCCEED();

    if (!nullView)
        SUCCEED();
    else
        FAIL();

    StringView emptyView = StringView::empty();
    EXPECT_FALSE(emptyView.isNull());
    EXPECT_TRUE(emptyView.isEmpty());

    // Test in a boolean context to test operator bool().
    if (emptyView)
        SUCCEED();
    else
        FAIL();

    if (!emptyView)
        FAIL();
    else
        SUCCEED();

    StringView viewWithCharacters(String("hello"_s));
    EXPECT_FALSE(viewWithCharacters.isNull());
    EXPECT_FALSE(viewWithCharacters.isEmpty());

    // Test in a boolean context to test operator bool().
    if (viewWithCharacters)
        SUCCEED();
    else
        FAIL();

    if (!viewWithCharacters)
        FAIL();
    else
        SUCCEED();
}

bool compareLoopIterations(StringView::GraphemeClusters graphemeClusters, std::vector<StringView> expected)
{
    std::vector<StringView> actual;
    for (auto graphemeCluster : graphemeClusters)
        actual.push_back(graphemeCluster);
    return actual == expected;
}

bool compareLoopIterations(StringView::CodePoints codePoints, std::vector<UChar32> expected)
{
    std::vector<UChar32> actual;
    for (auto codePoint : codePoints)
        actual.push_back(codePoint);
    return actual == expected;
}

static bool compareLoopIterations(StringView::CodeUnits codeUnits, std::vector<UChar> expected)
{
    std::vector<UChar> actual;
    for (auto codeUnit : codeUnits)
        actual.push_back(codeUnit);
    return actual == expected;
}

static void build(StringBuilder& builder, std::vector<UChar> input)
{
    builder.clear();
    for (auto codeUnit : input)
        builder.append(codeUnit);
}

TEST(WTF, StringViewIterators)
{
    EXPECT_TRUE(compareLoopIterations(StringView().codePoints(), { }));
    EXPECT_TRUE(compareLoopIterations(StringView().codeUnits(), { }));
    EXPECT_TRUE(compareLoopIterations(StringView().graphemeClusters(), { }));

    EXPECT_TRUE(compareLoopIterations(StringView::empty().codePoints(), { }));
    EXPECT_TRUE(compareLoopIterations(StringView::empty().codeUnits(), { }));
    EXPECT_TRUE(compareLoopIterations(StringView::empty().graphemeClusters(), { }));

    String helo("helo"_s);
    StringView heloView(helo);

    auto codePoints = heloView.codePoints();
    auto codePointsIterator = codePoints.begin();
    EXPECT_EQ(*codePointsIterator, 'h');
    EXPECT_EQ(*codePointsIterator, 'h');
    ++codePointsIterator;
    ++codePointsIterator;
    EXPECT_EQ(*codePointsIterator, 'l');
    auto savedIterator = codePointsIterator;
    codePointsIterator = codePoints.begin();
    EXPECT_EQ(*codePointsIterator, 'h');
    codePointsIterator = savedIterator;
    EXPECT_EQ(*codePointsIterator, 'l');
    String webkit("webkit"_s);
    auto webkitCodePoints = StringView(webkit).codePoints();
    codePointsIterator = webkitCodePoints.begin();
    ++codePointsIterator;
    ++codePointsIterator;
    EXPECT_EQ(*codePointsIterator, 'b');
    while (codePointsIterator != webkitCodePoints.end())
        ++codePointsIterator;

    EXPECT_TRUE(compareLoopIterations(heloView.codePoints(), {'h', 'e', 'l', 'o'}));
    EXPECT_TRUE(compareLoopIterations(heloView.codeUnits(), {'h', 'e', 'l', 'o'}));
    EXPECT_TRUE(compareLoopIterations(heloView.graphemeClusters(), {
        StringView(heloView.characters8(), 1),
        StringView(heloView.characters8() + 1, 1),
        StringView(heloView.characters8() + 2, 1),
        StringView(heloView.characters8() + 3, 1)}));

    StringBuilder b;
    build(b, {0xD800, 0xDD55}); // Surrogates for unicode code point U+10155
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {0x10155}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {0xD800, 0xDD55}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).graphemeClusters(), {StringView(b.toString())}));

    build(b, {0xD800}); // Leading surrogate only
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {0xD800}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {0xD800}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).graphemeClusters(), {StringView(b.toString())}));

    build(b, {0xD800, 0xD801}); // Two leading surrogates
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {0xD800, 0xD801}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {0xD800, 0xD801}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).graphemeClusters(), {StringView(b.characters16(), 1), StringView(b.characters16() + 1, 1)}));

    build(b, {0xDD55}); // Trailing surrogate only
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {0xDD55}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {0xDD55}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).graphemeClusters(), {StringView(b.toString())}));

    build(b, {0xD800, 'h'}); // Leading surrogate followed by non-surrogate
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {0xD800, 'h'}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {0xD800, 'h'}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).graphemeClusters(), {StringView(b.characters16(), 1), StringView(b.characters16() + 1, 1)}));

    build(b, {0x0306}); // "COMBINING BREVE"
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {0x0306}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {0x0306}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).graphemeClusters(), {StringView(b.toString())}));

    build(b, {0x0306, 0xD800, 0xDD55, 'h', 'e', 'l', 'o'}); // Mix of single code unit and multi code unit code points
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {0x0306, 0x10155, 'h', 'e', 'l', 'o'}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {0x0306, 0xD800, 0xDD55, 'h', 'e', 'l', 'o'}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).graphemeClusters(), {
        StringView(b.characters16(), 1),
        StringView(b.characters16() + 1, 2),
        StringView(b.characters16() + 3, 1),
        StringView(b.characters16() + 4, 1),
        StringView(b.characters16() + 5, 1),
        StringView(b.characters16() + 6, 1)}));

    build(b, {'e', 0x0301}); // "COMBINING ACUTE"
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {'e', 0x0301}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {'e', 0x0301}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).graphemeClusters(), {StringView(b.toString())}));

    build(b, {'e', 0x0301, 0x0306, 'a'}); // "COMBINING ACUTE" "COMBINING BREVE"
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {'e', 0x0301, 0x0306, 'a'}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {'e', 0x0301, 0x0306, 'a'}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).graphemeClusters(), {
        StringView(b.characters16(), 3),
        StringView(b.characters16() + 3, 1),
        }));

    build(b, {0x1112, 0x116f, 0x11b6, 0x1107, 0x1161, 0x11B8}); // Korean combining Jamo
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {0x1112, 0x116f, 0x11b6, 0x1107, 0x1161, 0x11B8}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {0x1112, 0x116f, 0x11b6, 0x1107, 0x1161, 0x11B8}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).graphemeClusters(), {
        StringView(b.characters16(), 3),
        StringView(b.characters16() + 3, 3)}));
}

static Vector<String> vectorFromSplitResult(const StringView::SplitResult& substrings)
{
    Vector<String> result;
    for (StringView substring : substrings)
        result.append(substring.toString());
    return result;
}

TEST(WTF, StringViewSplitEmptyAndNullStrings)
{
    StringView a = emptyString();
    auto splitResult = a.split('b');
    EXPECT_TRUE(splitResult.begin() == splitResult.end());

    a = { String { } };
    splitResult = a.split('b');
    EXPECT_TRUE(splitResult.begin() == splitResult.end());

    a = { };
    splitResult = a.split('b');
    EXPECT_TRUE(splitResult.begin() == splitResult.end());
}

TEST(WTF, StringViewSplitBasic)
{
    String referenceHolder;
    StringView a = stringViewFromUTF8(referenceHolder, "This is a sentence.");

    // Simple
    Vector<String> actual = vectorFromSplitResult(a.split('T'));
    Vector<String> expected({ "his is a sentence."_s });
    ASSERT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < actual.size(); ++i)
        EXPECT_STREQ(expected[i].utf8().data(), actual[i].utf8().data()) << "Vectors differ at index " << i;

    actual = vectorFromSplitResult(a.split('.'));
    expected = { "This is a sentence"_s };
    ASSERT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < actual.size(); ++i)
        EXPECT_STREQ(expected[i].utf8().data(), actual[i].utf8().data()) << "Vectors differ at index " << i;

    actual = vectorFromSplitResult(a.split('a'));
    expected = { "This is "_s, " sentence."_s };
    ASSERT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < actual.size(); ++i)
        EXPECT_STREQ(expected[i].utf8().data(), actual[i].utf8().data()) << "Vectors differ at index " << i;

    actual = vectorFromSplitResult(a.split(' '));
    expected = { "This"_s, "is"_s, "a"_s, "sentence."_s };
    ASSERT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < actual.size(); ++i)
        EXPECT_STREQ(expected[i].utf8().data(), actual[i].utf8().data()) << "Vectors differ at index " << i;

    // Non-existent separator
    actual = vectorFromSplitResult(a.split('z'));
    expected = { "This is a sentence."_s };
    ASSERT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < actual.size(); ++i)
        EXPECT_STREQ(expected[i].utf8().data(), actual[i].utf8().data()) << "Vectors differ at index " << i;
}

TEST(WTF, StringViewSplitWithConsecutiveSeparators)
{
    String referenceHolder;
    StringView a = stringViewFromUTF8(referenceHolder, " This     is  a       sentence. ");

    Vector<String> actual = vectorFromSplitResult(a.split(' '));
    Vector<String> expected({ "This"_s, "is"_s, "a"_s, "sentence."_s });
    ASSERT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < actual.size(); ++i)
        EXPECT_STREQ(expected[i].utf8().data(), actual[i].utf8().data()) << "Vectors differ at index " << i;

    actual = vectorFromSplitResult(a.splitAllowingEmptyEntries(' '));
    expected = { ""_s, "This"_s, ""_s, ""_s, ""_s, ""_s, "is"_s, ""_s, "a"_s, ""_s, ""_s, ""_s, ""_s, ""_s, ""_s, "sentence."_s, ""_s };
    ASSERT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < actual.size(); ++i)
        EXPECT_STREQ(expected[i].utf8().data(), actual[i].utf8().data()) << "Vectors differ at index " << i;
}

TEST(WTF, StringViewEqualBasic)
{
    String referenceHolder;
    StringView a = stringViewFromUTF8(referenceHolder, "Hello World!");
    EXPECT_TRUE(a == "Hello World!"_s);
    EXPECT_FALSE(a == "Hello World"_s);
    EXPECT_FALSE(a == "Hello World!!"_s);

    auto test = "Hell\0";
    a = StringView { (const LChar*)test, 5 };
    EXPECT_FALSE(a == "Hell\0"_s);
    EXPECT_FALSE(a == "Hell"_s);

    StringView test3 = "Hello"_s;
    EXPECT_TRUE(test3 == "Hello\0"_s);
    EXPECT_TRUE(test3 == "Hello"_s);
}

TEST(WTF, StringViewEqualIgnoringASCIICaseBasic)
{
    RefPtr<StringImpl> a = StringImpl::create("aBcDeFG"_s);
    RefPtr<StringImpl> b = StringImpl::create("ABCDEFG"_s);
    RefPtr<StringImpl> c = StringImpl::create("abcdefg"_s);
    constexpr auto d = "aBcDeFG"_s;
    RefPtr<StringImpl> empty = StringImpl::create(reinterpret_cast<const LChar*>(""), 0);
    RefPtr<StringImpl> shorter = StringImpl::create("abcdef"_s);
    RefPtr<StringImpl> different = StringImpl::create("abcrefg"_s);

    StringView stringViewA(*a.get());
    StringView stringViewB(*b.get());
    StringView stringViewC(*c.get());
    StringView emptyStringView(*empty.get());
    StringView shorterStringView(*shorter.get());
    StringView differentStringView(*different.get());

    ASSERT_TRUE(equalIgnoringASCIICase(stringViewA, stringViewB));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewB, stringViewC));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewB, stringViewC));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewA, d));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewB, d));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewC, d));

    // Identity.
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewA, stringViewA));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewB, stringViewB));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewC, stringViewC));

    // Transitivity.
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewA, stringViewB));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewB, stringViewC));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewA, stringViewC));

    // Negative cases.
    ASSERT_FALSE(equalIgnoringASCIICase(stringViewA, emptyStringView));
    ASSERT_FALSE(equalIgnoringASCIICase(stringViewB, emptyStringView));
    ASSERT_FALSE(equalIgnoringASCIICase(stringViewC, emptyStringView));
    ASSERT_FALSE(equalIgnoringASCIICase(stringViewA, shorterStringView));
    ASSERT_FALSE(equalIgnoringASCIICase(stringViewB, shorterStringView));
    ASSERT_FALSE(equalIgnoringASCIICase(stringViewC, shorterStringView));
    ASSERT_FALSE(equalIgnoringASCIICase(stringViewA, differentStringView));
    ASSERT_FALSE(equalIgnoringASCIICase(stringViewB, differentStringView));
    ASSERT_FALSE(equalIgnoringASCIICase(stringViewC, differentStringView));
    ASSERT_FALSE(equalIgnoringASCIICase(emptyStringView, d));
    ASSERT_FALSE(equalIgnoringASCIICase(shorterStringView, d));
    ASSERT_FALSE(equalIgnoringASCIICase(differentStringView, d));
}

TEST(WTF, StringViewEqualIgnoringASCIICaseWithEmpty)
{
    RefPtr<StringImpl> a = StringImpl::create(reinterpret_cast<const LChar*>(""), 0);
    RefPtr<StringImpl> b = StringImpl::create(reinterpret_cast<const LChar*>(""), 0);
    StringView stringViewA(*a.get());
    StringView stringViewB(*b.get());
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewA, stringViewB));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewB, stringViewA));
}

TEST(WTF, StringViewEqualIgnoringASCIICaseWithLatin1Characters)
{
    RefPtr<StringImpl> a = StringImpl::create(reinterpret_cast<const LChar*>("aBcéeFG"), 7);
    RefPtr<StringImpl> b = StringImpl::create(reinterpret_cast<const LChar*>("ABCÉEFG"), 7);
    RefPtr<StringImpl> c = StringImpl::create(reinterpret_cast<const LChar*>("ABCéEFG"), 7);
    RefPtr<StringImpl> d = StringImpl::create(reinterpret_cast<const LChar*>("abcéefg"), 7);
    StringView stringViewA(*a.get());
    StringView stringViewB(*b.get());
    StringView stringViewC(*c.get());
    StringView stringViewD(*d.get());

    // Identity.
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewA, stringViewA));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewB, stringViewB));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewC, stringViewC));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewD, stringViewD));

    // All combination.
    ASSERT_FALSE(equalIgnoringASCIICase(stringViewA, stringViewB));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewA, stringViewC));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewA, stringViewD));
    ASSERT_FALSE(equalIgnoringASCIICase(stringViewB, stringViewC));
    ASSERT_FALSE(equalIgnoringASCIICase(stringViewB, stringViewD));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewC, stringViewD));
}

TEST(WTF, StringViewFindIgnoringASCIICaseBasic)
{
    String referenceAHolder;
    StringView referenceA = stringViewFromUTF8(referenceAHolder, "aBcéeFG");
    String referenceBHolder;
    StringView referenceB = stringViewFromUTF8(referenceBHolder, "ABCÉEFG");

    // Search the exact string.
    EXPECT_EQ(static_cast<size_t>(0), referenceA.findIgnoringASCIICase(referenceA));
    EXPECT_EQ(static_cast<size_t>(0), referenceB.findIgnoringASCIICase(referenceB));

    // A and B are distinct by the non-ascii character é/É.
    EXPECT_EQ(static_cast<size_t>(notFound), referenceA.findIgnoringASCIICase(referenceB));
    EXPECT_EQ(static_cast<size_t>(notFound), referenceB.findIgnoringASCIICase(referenceA));

    String tempStringHolder;
    // Find the prefix.
    EXPECT_EQ(static_cast<size_t>(0), referenceA.findIgnoringASCIICase(stringViewFromLiteral("a")));
    EXPECT_EQ(static_cast<size_t>(0), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "abcé")));
    EXPECT_EQ(static_cast<size_t>(0), referenceA.findIgnoringASCIICase(stringViewFromLiteral("A")));
    EXPECT_EQ(static_cast<size_t>(0), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABCé")));
    EXPECT_EQ(static_cast<size_t>(0), referenceB.findIgnoringASCIICase(stringViewFromLiteral("a")));
    EXPECT_EQ(static_cast<size_t>(0), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "abcÉ")));
    EXPECT_EQ(static_cast<size_t>(0), referenceB.findIgnoringASCIICase(stringViewFromLiteral("A")));
    EXPECT_EQ(static_cast<size_t>(0), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABCÉ")));

    // Not a prefix.
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromLiteral("x")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "accé")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "abcÉ")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromLiteral("X")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABDé")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABCÉ")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromLiteral("y")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "accÉ")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "abcé")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromLiteral("Y")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABdÉ")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABCé")));

    // Find the infix.
    EXPECT_EQ(static_cast<size_t>(2), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "cée")));
    EXPECT_EQ(static_cast<size_t>(3), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ée")));
    EXPECT_EQ(static_cast<size_t>(2), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "cé")));
    EXPECT_EQ(static_cast<size_t>(2), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "c")));
    EXPECT_EQ(static_cast<size_t>(3), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "é")));
    EXPECT_EQ(static_cast<size_t>(2), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "Cée")));
    EXPECT_EQ(static_cast<size_t>(3), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "éE")));
    EXPECT_EQ(static_cast<size_t>(2), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "Cé")));
    EXPECT_EQ(static_cast<size_t>(2), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "C")));

    EXPECT_EQ(static_cast<size_t>(2), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "cÉe")));
    EXPECT_EQ(static_cast<size_t>(3), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "Ée")));
    EXPECT_EQ(static_cast<size_t>(2), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "cÉ")));
    EXPECT_EQ(static_cast<size_t>(2), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "c")));
    EXPECT_EQ(static_cast<size_t>(3), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "É")));
    EXPECT_EQ(static_cast<size_t>(2), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "CÉe")));
    EXPECT_EQ(static_cast<size_t>(3), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ÉE")));
    EXPECT_EQ(static_cast<size_t>(2), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "CÉ")));
    EXPECT_EQ(static_cast<size_t>(2), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "C")));

    // Not an infix.
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "céd")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "Ée")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "bé")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "x")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "É")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "CÉe")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "éd")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "CÉ")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "Y")));

    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "cée")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "Éc")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "cé")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "W")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "é")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "bÉe")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "éE")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "BÉ")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "z")));

    // Find the suffix.
    EXPECT_EQ(static_cast<size_t>(6), referenceA.findIgnoringASCIICase(stringViewFromLiteral("g")));
    EXPECT_EQ(static_cast<size_t>(4), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "efg")));
    EXPECT_EQ(static_cast<size_t>(3), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "éefg")));
    EXPECT_EQ(static_cast<size_t>(6), referenceA.findIgnoringASCIICase(stringViewFromLiteral("G")));
    EXPECT_EQ(static_cast<size_t>(4), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "EFG")));
    EXPECT_EQ(static_cast<size_t>(3), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "éEFG")));

    EXPECT_EQ(static_cast<size_t>(6), referenceB.findIgnoringASCIICase(stringViewFromLiteral("g")));
    EXPECT_EQ(static_cast<size_t>(4), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "efg")));
    EXPECT_EQ(static_cast<size_t>(3), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "Éefg")));
    EXPECT_EQ(static_cast<size_t>(6), referenceB.findIgnoringASCIICase(stringViewFromLiteral("G")));
    EXPECT_EQ(static_cast<size_t>(4), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "EFG")));
    EXPECT_EQ(static_cast<size_t>(3), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ÉEFG")));

    // Not a suffix.
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromLiteral("X")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "edg")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "Éefg")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromLiteral("w")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "dFG")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ÉEFG")));

    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromLiteral("Z")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ffg")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "éefg")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromLiteral("r")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "EgG")));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "éEFG")));
}

TEST(WTF, StringViewFindIgnoringASCIICaseWithValidOffset)
{
    String referenceHolder;
    StringView reference = stringViewFromUTF8(referenceHolder, "ABCÉEFGaBcéeFG");
    String tempStringHolder;

    EXPECT_EQ(static_cast<size_t>(0), reference.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABC"), 0));
    EXPECT_EQ(static_cast<size_t>(7), reference.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABC"), 1));
    EXPECT_EQ(static_cast<size_t>(0), reference.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABCÉ"), 0));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABCÉ"), 1));
    EXPECT_EQ(static_cast<size_t>(7), reference.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABCé"), 0));
    EXPECT_EQ(static_cast<size_t>(7), reference.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABCé"), 1));
}

TEST(WTF, StringViewFindIgnoringASCIICaseWithInvalidOffset)
{
    String referenceHolder;
    StringView reference = stringViewFromUTF8(referenceHolder, "ABCÉEFGaBcéeFG");
    String tempStringHolder;

    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABC"), 15));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABC"), 16));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABCÉ"), 17));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABCÉ"), 42));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference.findIgnoringASCIICase(stringViewFromUTF8(tempStringHolder, "ABCÉ"), std::numeric_limits<unsigned>::max()));
}

TEST(WTF, StringViewFindIgnoringASCIICaseOnEmpty)
{
    String referenceHolder;
    StringView reference = stringViewFromUTF8(referenceHolder, "ABCÉEFG");
    StringView empty = stringViewFromLiteral("");
    EXPECT_EQ(static_cast<size_t>(0), reference.findIgnoringASCIICase(empty));
    EXPECT_EQ(static_cast<size_t>(0), reference.findIgnoringASCIICase(empty, 0));
    EXPECT_EQ(static_cast<size_t>(3), reference.findIgnoringASCIICase(empty, 3));
    EXPECT_EQ(static_cast<size_t>(7), reference.findIgnoringASCIICase(empty, 7));
    EXPECT_EQ(static_cast<size_t>(7), reference.findIgnoringASCIICase(empty, 8));
    EXPECT_EQ(static_cast<size_t>(7), reference.findIgnoringASCIICase(empty, 42));
    EXPECT_EQ(static_cast<size_t>(7), reference.findIgnoringASCIICase(empty, std::numeric_limits<unsigned>::max()));
}

TEST(WTF, StringViewFindIgnoringASCIICaseWithPatternLongerThanReference)
{
    String referenceHolder;
    StringView reference = stringViewFromUTF8(referenceHolder, "ABCÉEFG");
    String patternHolder;
    StringView pattern = stringViewFromUTF8(patternHolder, "ABCÉEFGA");

    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference.findIgnoringASCIICase(pattern));
    EXPECT_EQ(static_cast<size_t>(0), pattern.findIgnoringASCIICase(reference));
}

TEST(WTF, StringViewStartsWithBasic)
{
    StringView reference = stringViewFromLiteral("abcdefg");
    String referenceUTF8Ref;
    StringView referenceUTF8 = stringViewFromUTF8(referenceUTF8Ref, "àîûèô");

    StringView oneLetterPrefix = stringViewFromLiteral("a");
    StringView shortPrefix = stringViewFromLiteral("abc");
    StringView longPrefix = stringViewFromLiteral("abcdef");
    StringView upperCasePrefix = stringViewFromLiteral("ABC");
    StringView empty = stringViewFromLiteral("");
    StringView notPrefix = stringViewFromLiteral("bc");

    String oneLetterPrefixUTF8Ref;
    StringView oneLetterPrefixUTF8 = stringViewFromUTF8(oneLetterPrefixUTF8Ref, "à");
    String shortPrefixUTF8Ref;
    StringView shortPrefixUTF8 = stringViewFromUTF8(shortPrefixUTF8Ref, "àî");
    String longPrefixUTF8Ref;
    StringView longPrefixUTF8 = stringViewFromUTF8(longPrefixUTF8Ref, "àîûè");
    String upperCasePrefixUTF8Ref;
    StringView upperCasePrefixUTF8 = stringViewFromUTF8(upperCasePrefixUTF8Ref, "ÀÎ");
    String notPrefixUTF8Ref;
    StringView notPrefixUTF8 = stringViewFromUTF8(notPrefixUTF8Ref, "îû");

    EXPECT_TRUE(reference.startsWith(reference));
    EXPECT_TRUE(reference.startsWith(oneLetterPrefix));
    EXPECT_TRUE(reference.startsWith(shortPrefix));
    EXPECT_TRUE(reference.startsWith(longPrefix));
    EXPECT_TRUE(reference.startsWith(empty));

    EXPECT_TRUE(referenceUTF8.startsWith(referenceUTF8));
    EXPECT_TRUE(referenceUTF8.startsWith(oneLetterPrefixUTF8));
    EXPECT_TRUE(referenceUTF8.startsWith(shortPrefixUTF8));
    EXPECT_TRUE(referenceUTF8.startsWith(longPrefixUTF8));
    EXPECT_TRUE(referenceUTF8.startsWith(empty));

    EXPECT_FALSE(reference.startsWith(notPrefix));
    EXPECT_FALSE(reference.startsWith(upperCasePrefix));
    EXPECT_FALSE(reference.startsWith(notPrefixUTF8));
    EXPECT_FALSE(reference.startsWith(upperCasePrefixUTF8));
    EXPECT_FALSE(referenceUTF8.startsWith(notPrefix));
    EXPECT_FALSE(referenceUTF8.startsWith(upperCasePrefix));
    EXPECT_FALSE(referenceUTF8.startsWith(notPrefixUTF8));
    EXPECT_FALSE(referenceUTF8.startsWith(upperCasePrefixUTF8));
}

TEST(WTF, StringViewStartsWithEmpty)
{
    StringView a = stringViewFromLiteral("");
    String refB;
    StringView b = stringViewFromUTF8(refB, "");

    EXPECT_TRUE(a.startsWith(a));
    EXPECT_TRUE(a.startsWith(b));
    EXPECT_TRUE(b.startsWith(a));
    EXPECT_TRUE(b.startsWith(b));
}

TEST(WTF, StringViewStartsWithIgnoringASCIICaseBasic)
{
    StringView reference = stringViewFromLiteral("abcdefg");

    String referenceUTF8Ref;
    StringView referenceUTF8 = stringViewFromUTF8(referenceUTF8Ref, "àîûèô");

    StringView oneLetterPrefix = stringViewFromLiteral("a");
    StringView shortPrefix = stringViewFromLiteral("abc");
    StringView longPrefix = stringViewFromLiteral("abcdef");
    StringView upperCasePrefix = stringViewFromLiteral("ABC");
    StringView mixedCasePrefix = stringViewFromLiteral("aBcDe");
    StringView empty = stringViewFromLiteral("");
    StringView notPrefix = stringViewFromLiteral("bc");

    String oneLetterPrefixUTF8Ref;
    StringView oneLetterPrefixUTF8 = stringViewFromUTF8(oneLetterPrefixUTF8Ref, "à");
    String shortPrefixUTF8Ref;
    StringView shortPrefixUTF8 = stringViewFromUTF8(shortPrefixUTF8Ref, "àî");
    String longPrefixUTF8Ref;
    StringView longPrefixUTF8 = stringViewFromUTF8(longPrefixUTF8Ref, "àîûè");
    String upperCasePrefixUTF8Ref;
    StringView upperCasePrefixUTF8 = stringViewFromUTF8(upperCasePrefixUTF8Ref, "ÀÎ");
    String notPrefixUTF8Ref;
    StringView notPrefixUTF8 = stringViewFromUTF8(notPrefixUTF8Ref, "îû");

    EXPECT_TRUE(reference.startsWithIgnoringASCIICase(reference));
    EXPECT_TRUE(reference.startsWithIgnoringASCIICase(oneLetterPrefix));
    EXPECT_TRUE(reference.startsWithIgnoringASCIICase(shortPrefix));
    EXPECT_TRUE(reference.startsWithIgnoringASCIICase(longPrefix));
    EXPECT_TRUE(reference.startsWithIgnoringASCIICase(empty));
    EXPECT_TRUE(reference.startsWithIgnoringASCIICase(upperCasePrefix));
    EXPECT_TRUE(reference.startsWithIgnoringASCIICase(mixedCasePrefix));

    EXPECT_TRUE(referenceUTF8.startsWithIgnoringASCIICase(referenceUTF8));
    EXPECT_TRUE(referenceUTF8.startsWithIgnoringASCIICase(oneLetterPrefixUTF8));
    EXPECT_TRUE(referenceUTF8.startsWithIgnoringASCIICase(shortPrefixUTF8));
    EXPECT_TRUE(referenceUTF8.startsWithIgnoringASCIICase(longPrefixUTF8));
    EXPECT_TRUE(referenceUTF8.startsWithIgnoringASCIICase(empty));

    EXPECT_FALSE(reference.startsWithIgnoringASCIICase(notPrefix));
    EXPECT_FALSE(reference.startsWithIgnoringASCIICase(notPrefixUTF8));
    EXPECT_FALSE(reference.startsWithIgnoringASCIICase(upperCasePrefixUTF8));
    EXPECT_FALSE(referenceUTF8.startsWithIgnoringASCIICase(notPrefix));
    EXPECT_FALSE(referenceUTF8.startsWithIgnoringASCIICase(notPrefixUTF8));
    EXPECT_FALSE(referenceUTF8.startsWithIgnoringASCIICase(upperCasePrefix));
    EXPECT_FALSE(referenceUTF8.startsWithIgnoringASCIICase(upperCasePrefixUTF8));
}


TEST(WTF, StringViewStartsWithIgnoringASCIICaseEmpty)
{
    StringView a = stringViewFromLiteral("");
    String refB;
    StringView b = stringViewFromUTF8(refB, "");

    EXPECT_TRUE(a.startsWithIgnoringASCIICase(a));
    EXPECT_TRUE(a.startsWithIgnoringASCIICase(b));
    EXPECT_TRUE(b.startsWithIgnoringASCIICase(a));
    EXPECT_TRUE(b.startsWithIgnoringASCIICase(b));
}

TEST(WTF, StringViewStartsWithIgnoringASCIICaseWithLatin1Characters)
{
    StringView reference = stringViewFromLiteral("aBcéeFG");
    String referenceUTF8Ref;
    StringView referenceUTF8 = stringViewFromUTF8(referenceUTF8Ref, "aBcéeFG");

    StringView a = stringViewFromLiteral("aBcéeF");
    StringView b = stringViewFromLiteral("ABCéEF");
    StringView c = stringViewFromLiteral("abcéef");
    StringView d = stringViewFromLiteral("Abcéef");

    String refE;
    StringView e = stringViewFromUTF8(refE, "aBcéeF");
    String refF;
    StringView f = stringViewFromUTF8(refF, "ABCéEF");
    String refG;
    StringView g = stringViewFromUTF8(refG, "abcéef");
    String refH;
    StringView h = stringViewFromUTF8(refH, "Abcéef");
    
    EXPECT_TRUE(reference.startsWithIgnoringASCIICase(a));
    EXPECT_TRUE(reference.startsWithIgnoringASCIICase(b));
    EXPECT_TRUE(reference.startsWithIgnoringASCIICase(c));
    EXPECT_TRUE(reference.startsWithIgnoringASCIICase(d));

    EXPECT_TRUE(referenceUTF8.startsWithIgnoringASCIICase(e));
    EXPECT_TRUE(referenceUTF8.startsWithIgnoringASCIICase(f));
    EXPECT_TRUE(referenceUTF8.startsWithIgnoringASCIICase(g));
    EXPECT_TRUE(referenceUTF8.startsWithIgnoringASCIICase(h));

    EXPECT_FALSE(reference.endsWithIgnoringASCIICase(referenceUTF8));
    EXPECT_FALSE(referenceUTF8.endsWithIgnoringASCIICase(reference));
}

TEST(WTF, StringViewEndsWithBasic)
{
    StringView reference = stringViewFromLiteral("abcdefg");
    String referenceUTF8Ref;
    StringView referenceUTF8 = stringViewFromUTF8(referenceUTF8Ref, "àîûèô");

    StringView oneLetterSuffix = stringViewFromLiteral("g");
    StringView shortSuffix = stringViewFromLiteral("efg");
    StringView longSuffix = stringViewFromLiteral("cdefg");
    StringView upperCaseSuffix = stringViewFromLiteral("EFG");
    StringView empty = stringViewFromLiteral("");
    StringView notSuffix = stringViewFromLiteral("bc");

    String oneLetterSuffixUTF8Ref;
    StringView oneLetterSuffixUTF8 = stringViewFromUTF8(oneLetterSuffixUTF8Ref, "ô");
    String shortSuffixUTF8Ref;
    StringView shortSuffixUTF8 = stringViewFromUTF8(shortSuffixUTF8Ref, "èô");
    String longSuffixUTF8Ref;
    StringView longSuffixUTF8 = stringViewFromUTF8(longSuffixUTF8Ref, "îûèô");
    String upperCaseSuffixUTF8Ref;
    StringView upperCaseSuffixUTF8 = stringViewFromUTF8(upperCaseSuffixUTF8Ref, "ÈÔ");
    String notSuffixUTF8Ref;
    StringView notSuffixUTF8 = stringViewFromUTF8(notSuffixUTF8Ref, "îû");

    EXPECT_TRUE(reference.endsWith(reference));
    EXPECT_TRUE(reference.endsWith(oneLetterSuffix));
    EXPECT_TRUE(reference.endsWith(shortSuffix));
    EXPECT_TRUE(reference.endsWith(longSuffix));
    EXPECT_TRUE(reference.endsWith(empty));

    EXPECT_TRUE(referenceUTF8.endsWith(referenceUTF8));
    EXPECT_TRUE(referenceUTF8.endsWith(oneLetterSuffixUTF8));
    EXPECT_TRUE(referenceUTF8.endsWith(shortSuffixUTF8));
    EXPECT_TRUE(referenceUTF8.endsWith(longSuffixUTF8));
    EXPECT_TRUE(referenceUTF8.endsWith(empty));

    EXPECT_FALSE(reference.endsWith(notSuffix));
    EXPECT_FALSE(reference.endsWith(upperCaseSuffix));
    EXPECT_FALSE(reference.endsWith(notSuffixUTF8));
    EXPECT_FALSE(reference.endsWith(upperCaseSuffixUTF8));
    EXPECT_FALSE(referenceUTF8.endsWith(notSuffix));
    EXPECT_FALSE(referenceUTF8.endsWith(upperCaseSuffix));
    EXPECT_FALSE(referenceUTF8.endsWith(notSuffixUTF8));
    EXPECT_FALSE(referenceUTF8.endsWith(upperCaseSuffixUTF8));
}

TEST(WTF, StringViewEndsWithEmpty)
{
    StringView a = stringViewFromLiteral("");
    String refB;
    StringView b = stringViewFromUTF8(refB, "");

    EXPECT_TRUE(a.endsWith(a));
    EXPECT_TRUE(a.endsWith(b));
    EXPECT_TRUE(b.endsWith(a));
    EXPECT_TRUE(b.endsWith(b));
}

TEST(WTF, StringViewEndsWithIgnoringASCIICaseBasic)
{
    StringView reference = stringViewFromLiteral("abcdefg");
    String referenceUTF8Ref;
    StringView referenceUTF8 = stringViewFromUTF8(referenceUTF8Ref, "àîûèô");

    StringView oneLetterSuffix = stringViewFromLiteral("g");
    StringView shortSuffix = stringViewFromLiteral("efg");
    StringView longSuffix = stringViewFromLiteral("bcdefg");
    StringView upperCaseSuffix = stringViewFromLiteral("EFG");
    StringView mixedCaseSuffix = stringViewFromLiteral("bCdeFg");
    StringView empty = stringViewFromLiteral("");
    StringView notSuffix = stringViewFromLiteral("bc");

    String oneLetterSuffixUTF8Ref;
    StringView oneLetterSuffixUTF8 = stringViewFromUTF8(oneLetterSuffixUTF8Ref, "ô");
    String shortSuffixUTF8Ref;
    StringView shortSuffixUTF8 = stringViewFromUTF8(shortSuffixUTF8Ref, "èô");
    String longSuffixUTF8Ref;
    StringView longSuffixUTF8 = stringViewFromUTF8(longSuffixUTF8Ref, "îûèô");
    String upperCaseSuffixUTF8Ref;
    StringView upperCaseSuffixUTF8 = stringViewFromUTF8(upperCaseSuffixUTF8Ref, "ÈÔ");
    String notSuffixUTF8Ref;
    StringView notSuffixUTF8 = stringViewFromUTF8(notSuffixUTF8Ref, "îû");

    EXPECT_TRUE(reference.endsWithIgnoringASCIICase(reference));
    EXPECT_TRUE(reference.endsWithIgnoringASCIICase(oneLetterSuffix));
    EXPECT_TRUE(reference.endsWithIgnoringASCIICase(shortSuffix));
    EXPECT_TRUE(reference.endsWithIgnoringASCIICase(longSuffix));
    EXPECT_TRUE(reference.endsWithIgnoringASCIICase(empty));
    EXPECT_TRUE(reference.endsWithIgnoringASCIICase(upperCaseSuffix));
    EXPECT_TRUE(reference.endsWithIgnoringASCIICase(mixedCaseSuffix));

    EXPECT_TRUE(referenceUTF8.endsWithIgnoringASCIICase(referenceUTF8));
    EXPECT_TRUE(referenceUTF8.endsWithIgnoringASCIICase(oneLetterSuffixUTF8));
    EXPECT_TRUE(referenceUTF8.endsWithIgnoringASCIICase(shortSuffixUTF8));
    EXPECT_TRUE(referenceUTF8.endsWithIgnoringASCIICase(longSuffixUTF8));
    EXPECT_TRUE(referenceUTF8.endsWithIgnoringASCIICase(empty));

    EXPECT_FALSE(reference.endsWithIgnoringASCIICase(notSuffix));
    EXPECT_FALSE(reference.endsWithIgnoringASCIICase(notSuffixUTF8));
    EXPECT_FALSE(reference.endsWithIgnoringASCIICase(upperCaseSuffixUTF8));
    EXPECT_FALSE(referenceUTF8.endsWithIgnoringASCIICase(notSuffix));
    EXPECT_FALSE(referenceUTF8.endsWithIgnoringASCIICase(notSuffixUTF8));
    EXPECT_FALSE(referenceUTF8.endsWithIgnoringASCIICase(upperCaseSuffix));
    EXPECT_FALSE(referenceUTF8.endsWithIgnoringASCIICase(upperCaseSuffixUTF8));
}

TEST(WTF, StringViewEndsWithIgnoringASCIICaseEmpty)
{
    StringView a = stringViewFromLiteral("");
    String refB;
    StringView b = stringViewFromUTF8(refB, "");

    EXPECT_TRUE(a.endsWithIgnoringASCIICase(a));
    EXPECT_TRUE(a.endsWithIgnoringASCIICase(b));
    EXPECT_TRUE(b.endsWithIgnoringASCIICase(a));
    EXPECT_TRUE(b.endsWithIgnoringASCIICase(b));
}

TEST(WTF, StringViewEndsWithIgnoringASCIICaseWithLatin1Characters)
{
    StringView reference = stringViewFromLiteral("aBcéeFG");
    String referenceUTF8Ref;
    StringView referenceUTF8 = stringViewFromUTF8(referenceUTF8Ref, "aBcéeFG");

    StringView a = stringViewFromLiteral("BcéeFG");
    StringView b = stringViewFromLiteral("BCéEFG");
    StringView c = stringViewFromLiteral("bcéefG");
    StringView d = stringViewFromLiteral("bcéefg");

    String refE;
    StringView e = stringViewFromUTF8(refE, "bcéefG");
    String refF;
    StringView f = stringViewFromUTF8(refF, "BCéEFG");
    String refG;
    StringView g = stringViewFromUTF8(refG, "bcéefG");
    String refH;
    StringView h = stringViewFromUTF8(refH, "bcéefg");
    
    EXPECT_TRUE(reference.endsWithIgnoringASCIICase(a));
    EXPECT_TRUE(reference.endsWithIgnoringASCIICase(b));
    EXPECT_TRUE(reference.endsWithIgnoringASCIICase(c));
    EXPECT_TRUE(reference.endsWithIgnoringASCIICase(d));

    EXPECT_TRUE(referenceUTF8.endsWithIgnoringASCIICase(e));
    EXPECT_TRUE(referenceUTF8.endsWithIgnoringASCIICase(f));
    EXPECT_TRUE(referenceUTF8.endsWithIgnoringASCIICase(g));
    EXPECT_TRUE(referenceUTF8.endsWithIgnoringASCIICase(h));

    EXPECT_FALSE(reference.endsWithIgnoringASCIICase(referenceUTF8));
    EXPECT_FALSE(referenceUTF8.endsWithIgnoringASCIICase(reference));
}

TEST(WTF, StringView8Bit)
{
    EXPECT_TRUE(StringView().is8Bit());
    EXPECT_TRUE(StringView::empty().is8Bit());

    LChar* lcharPtr = nullptr;
    UChar* ucharPtr = nullptr;
    EXPECT_TRUE(StringView(lcharPtr, 0).is8Bit());
    EXPECT_FALSE(StringView(ucharPtr, 0).is8Bit());

    EXPECT_TRUE(StringView(String(lcharPtr, 0)).is8Bit());
    EXPECT_TRUE(StringView(String(ucharPtr, 0)).is8Bit());

    EXPECT_TRUE(StringView(String().impl()).is8Bit());
    EXPECT_TRUE(StringView(emptyString().impl()).is8Bit());
}

TEST(WTF, StringViewRightBasic)
{
    auto reference = stringViewFromLiteral("Cappuccino");
    EXPECT_TRUE(reference.right(0) == stringViewFromLiteral(""));
    EXPECT_TRUE(reference.right(1) == stringViewFromLiteral("o"));
    EXPECT_TRUE(reference.right(2) == stringViewFromLiteral("no"));
    EXPECT_TRUE(reference.right(3) == stringViewFromLiteral("ino"));
    EXPECT_TRUE(reference.right(4) == stringViewFromLiteral("cino"));
    EXPECT_TRUE(reference.right(5) == stringViewFromLiteral("ccino"));
    EXPECT_TRUE(reference.right(6) == stringViewFromLiteral("uccino"));
    EXPECT_TRUE(reference.right(7) == stringViewFromLiteral("puccino"));
    EXPECT_TRUE(reference.right(8) == stringViewFromLiteral("ppuccino"));
    EXPECT_TRUE(reference.right(9) == stringViewFromLiteral("appuccino"));
    EXPECT_TRUE(reference.right(10) == stringViewFromLiteral("Cappuccino"));
}

TEST(WTF, StringViewLeftBasic)
{
    auto reference = stringViewFromLiteral("Cappuccino");
    EXPECT_TRUE(reference.left(0) == stringViewFromLiteral(""));
    EXPECT_TRUE(reference.left(1) == stringViewFromLiteral("C"));
    EXPECT_TRUE(reference.left(2) == stringViewFromLiteral("Ca"));
    EXPECT_TRUE(reference.left(3) == stringViewFromLiteral("Cap"));
    EXPECT_TRUE(reference.left(4) == stringViewFromLiteral("Capp"));
    EXPECT_TRUE(reference.left(5) == stringViewFromLiteral("Cappu"));
    EXPECT_TRUE(reference.left(6) == stringViewFromLiteral("Cappuc"));
    EXPECT_TRUE(reference.left(7) == stringViewFromLiteral("Cappucc"));
    EXPECT_TRUE(reference.left(8) == stringViewFromLiteral("Cappucci"));
    EXPECT_TRUE(reference.left(9) == stringViewFromLiteral("Cappuccin"));
    EXPECT_TRUE(reference.left(10) == stringViewFromLiteral("Cappuccino"));
}

TEST(WTF, StringViewReverseFindBasic)
{
    auto reference = stringViewFromLiteral("Cappuccino");
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

TEST(WTF, StringViewStripLeadingAndTrailingMatchedCharacters)
{
    auto isA = [] (UChar c) { 
        return c == 'A';
    };

    EXPECT_TRUE(stringViewFromLiteral("AAABBBAAA").stripLeadingAndTrailingMatchedCharacters(isA) == stringViewFromLiteral("BBB"));
    EXPECT_TRUE(stringViewFromLiteral("AAABBBCCC").stripLeadingAndTrailingMatchedCharacters(isA) == stringViewFromLiteral("BBBCCC"));
    EXPECT_TRUE(stringViewFromLiteral("CCCBBBAAA").stripLeadingAndTrailingMatchedCharacters(isA) == stringViewFromLiteral("CCCBBB"));
    EXPECT_TRUE(stringViewFromLiteral("CCCBBBCCC").stripLeadingAndTrailingMatchedCharacters(isA) == stringViewFromLiteral("CCCBBBCCC"));
    EXPECT_TRUE(stringViewFromLiteral("AAAAAACCC").stripLeadingAndTrailingMatchedCharacters(isA) == stringViewFromLiteral("CCC"));
    EXPECT_TRUE(stringViewFromLiteral("BBBAAAAAA").stripLeadingAndTrailingMatchedCharacters(isA) == stringViewFromLiteral("BBB"));
    EXPECT_TRUE(stringViewFromLiteral("CCCAAABBB").stripLeadingAndTrailingMatchedCharacters(isA) == stringViewFromLiteral("CCCAAABBB"));
    EXPECT_TRUE(stringViewFromLiteral("AAAAAAAAA").stripLeadingAndTrailingMatchedCharacters(isA) == StringView::empty());

    StringView emptyView = StringView::empty();
    EXPECT_TRUE(emptyView.stripLeadingAndTrailingMatchedCharacters(isA) == emptyView);

    StringView nullView;
    EXPECT_TRUE(nullView.stripLeadingAndTrailingMatchedCharacters(isA) == nullView);
}

TEST(WTF, StringViewIsAllASCII)
{
    EXPECT_TRUE(StringView(String("Hello"_s)).isAllASCII());
    EXPECT_TRUE(StringView(String("Cocoa"_s)).isAllASCII());
    EXPECT_FALSE(StringView(String::fromLatin1("📱")).isAllASCII());
    EXPECT_FALSE(StringView(String::fromLatin1("\u0080")).isAllASCII());
    EXPECT_TRUE(StringView(String(bitwise_cast<const UChar*>(u"Hello"), 0)).isAllASCII());
}

TEST(WTF, StringViewUpconvert)
{
    constexpr ASCIILiteral data[] = {
        "Hello"_s,
        "This is a relatively long string to use SIMD optimized path. We invoke our optimized copying only when the string length gets longer than 64."_s,
    };
    for (auto literal : data) {
        StringView string(literal);
        auto upconverted = string.upconvertedCharacters();
        const UChar* characters = upconverted.get();
        for (unsigned index = 1; index < string.length(); ++index)
            EXPECT_EQ(characters[index], string[index]) << index << " " << literal.characters();
    }
}

} // namespace TestWebKitAPI
