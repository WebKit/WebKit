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

    StringView viewWithCharacters(String("hello"));
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
    compareLoopIterations(StringView().codePoints(), { });
    compareLoopIterations(StringView().codeUnits(), { });

    compareLoopIterations(StringView::empty().codePoints(), { });
    compareLoopIterations(StringView::empty().codeUnits(), { });

    compareLoopIterations(StringView(String("hello")).codePoints(), {'h', 'e', 'l', 'l', 'o'});
    compareLoopIterations(StringView(String("hello")).codeUnits(), {'h', 'e', 'l', 'l', 'o'});

    StringBuilder b;
    build(b, {0xD800, 0xDD55}); // Surrogates for unicode code point U+10155
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {0x10155}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {0xD800, 0xDD55}));

    build(b, {0xD800}); // Leading surrogate only
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {0xD800}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {0xD800}));

    build(b, {0xD800, 0xD801}); // Two leading surrogates
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {0xD800, 0xD801}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {0xD800, 0xD801}));

    build(b, {0xDD55}); // Trailing surrogate only
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {0xDD55}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {0xDD55}));

    build(b, {0xD800, 'h'}); // Leading surrogate followed by non-surrogate
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {0xD800, 'h'}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {0xD800, 'h'}));

    build(b, {0x0306}); // "COMBINING BREVE"
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {0x0306}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {0x0306}));

    build(b, {0x0306, 0xD800, 0xDD55, 'h', 'e', 'l', 'o'}); // Mix of single code unit and multi code unit code points
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codePoints(), {0x0306, 0x10155, 'h', 'e', 'l', 'o'}));
    EXPECT_TRUE(compareLoopIterations(StringView(b.toString()).codeUnits(), {0x0306, 0xD800, 0xDD55, 'h', 'e', 'l', 'o'}));
}

TEST(WTF, StringViewEqualIgnoringASCIICaseBasic)
{
    RefPtr<StringImpl> a = StringImpl::createFromLiteral("aBcDeFG");
    RefPtr<StringImpl> b = StringImpl::createFromLiteral("ABCDEFG");
    RefPtr<StringImpl> c = StringImpl::createFromLiteral("abcdefg");
    RefPtr<StringImpl> empty = StringImpl::create(reinterpret_cast<const LChar*>(""));
    RefPtr<StringImpl> shorter = StringImpl::createFromLiteral("abcdef");

    StringView stringViewA(*a.get());
    StringView stringViewB(*b.get());
    StringView stringViewC(*c.get());
    StringView emptyStringView(*empty.get());
    StringView shorterStringView(*shorter.get());

    ASSERT_TRUE(equalIgnoringASCIICase(stringViewA, stringViewB));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewB, stringViewC));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewB, stringViewC));

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
}

TEST(WTF, StringViewEqualIgnoringASCIICaseWithEmpty)
{
    RefPtr<StringImpl> a = StringImpl::create(reinterpret_cast<const LChar*>(""));
    RefPtr<StringImpl> b = StringImpl::create(reinterpret_cast<const LChar*>(""));
    StringView stringViewA(*a.get());
    StringView stringViewB(*b.get());
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewA, stringViewB));
    ASSERT_TRUE(equalIgnoringASCIICase(stringViewB, stringViewA));
}

TEST(WTF, StringViewEqualIgnoringASCIICaseWithLatin1Characters)
{
    RefPtr<StringImpl> a = StringImpl::create(reinterpret_cast<const LChar*>("aBcéeFG"));
    RefPtr<StringImpl> b = StringImpl::create(reinterpret_cast<const LChar*>("ABCÉEFG"));
    RefPtr<StringImpl> c = StringImpl::create(reinterpret_cast<const LChar*>("ABCéEFG"));
    RefPtr<StringImpl> d = StringImpl::create(reinterpret_cast<const LChar*>("abcéefg"));
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

} // namespace TestWebKitAPI
