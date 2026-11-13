/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include <wtf/unicode/ListFormatter.h>

#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

static String format(const ListFormatter& formatter, Vector<String>&& strings)
{
    auto result = formatter.format(strings.span());
    return result ? *result : String { "<failure>"_s };
}

TEST(WTF_ListFormatter, Conjunction)
{
    ListFormatter formatter { "en"_s, ListFormatter::Type::Conjunction, ListFormatter::Style::Long };
    ASSERT_TRUE(!!formatter);
    EXPECT_EQ(format(formatter, { }), ""_s);
    EXPECT_EQ(format(formatter, { "Cheese"_s }), "Cheese"_s);
    EXPECT_EQ(format(formatter, { "Cheese"_s, "Olives"_s }), "Cheese and Olives"_s);
    EXPECT_EQ(format(formatter, { "Cheese"_s, "Ham"_s, "Olives"_s }), "Cheese, Ham, and Olives"_s);
}

TEST(WTF_ListFormatter, TypesAndStyles)
{
    ListFormatter disjunction { "en"_s, ListFormatter::Type::Disjunction, ListFormatter::Style::Long };
    EXPECT_EQ(format(disjunction, { "Cheese"_s, "Olives"_s }), "Cheese or Olives"_s);

    ListFormatter unit { "en"_s, ListFormatter::Type::Unit, ListFormatter::Style::Narrow };
    EXPECT_EQ(format(unit, { "1h"_s, "5m"_s }), "1h 5m"_s);
}

TEST(WTF_ListFormatter, Locale)
{
    ListFormatter formatter { "es"_s, ListFormatter::Type::Conjunction, ListFormatter::Style::Long };
    EXPECT_EQ(format(formatter, { "Queso"_s, "Aceitunas"_s }), "Queso y Aceitunas"_s);
}

TEST(WTF_ListFormatter, Spans)
{
    ListFormatter formatter { "en"_s, ListFormatter::Type::Conjunction, ListFormatter::Style::Long };
    String characters = "CheeseHamOlives"_s;
    characters.convertTo16Bit();
    auto buffer = characters.span16();
    Vector<std::span<const char16_t>> strings { buffer.subspan(0, 6), buffer.subspan(6, 3), buffer.subspan(9, 6) };
    auto result = formatter.format(strings.span());
    ASSERT_TRUE(!!result);
    EXPECT_EQ(*result, "Cheese, Ham, and Olives"_s);
}

TEST(WTF_ListFormatter, FormatToParts)
{
    ListFormatter formatter { "en"_s, ListFormatter::Type::Conjunction, ListFormatter::Style::Long };
    Vector<String> strings { "Cheese"_s, "Ham"_s, "Olives"_s };
    auto formattedList = formatter.formatToParts(strings.span());
    ASSERT_TRUE(!!formattedList);
    EXPECT_EQ(formattedList->string, "Cheese, Ham, and Olives"_s);
    Vector<Range<unsigned>> expectedRanges { { 0, 6 }, { 8, 11 }, { 17, 23 } };
    EXPECT_EQ(formattedList->elementRanges, expectedRanges);

    Vector<String> stringsWithEmpty { "Cheese"_s, emptyString(), "Olives"_s };
    formattedList = formatter.formatToParts(stringsWithEmpty.span());
    ASSERT_TRUE(!!formattedList);
    EXPECT_EQ(formattedList->string, "Cheese, , and Olives"_s);
    Vector<Range<unsigned>> expectedRangesWithEmpty { { 0, 6 }, { 14, 20 } };
    EXPECT_EQ(formattedList->elementRanges, expectedRangesWithEmpty);
}

TEST(WTF_ListFormatter, Empty)
{
    ListFormatter formatter;
    EXPECT_FALSE(!!formatter);
    Vector<String> strings { "Cheese"_s };
    EXPECT_FALSE(formatter.format(strings.span()));
    EXPECT_FALSE(formatter.formatToParts(strings.span()));
}

} // namespace TestWebKitAPI
