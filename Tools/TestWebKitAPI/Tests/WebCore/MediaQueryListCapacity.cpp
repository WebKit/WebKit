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

// MQ::MediaQueryList is Vector<MediaQuery, 0, CrashOnOverflow, 16, FastMalloc>, and the list that
// MQ::MediaQueryParser::consumeMediaQueryList() returns is moved into StyleRuleMedia and retained
// for the lifetime of the stylesheet. WTF's expandCapacity() is
//
//   reserveCapacity(max(newMinCapacity, max(minCapacity, nextCapacity(capacity()))))
//
// with minCapacity == 16, so before rdar://184884868 the first append() on the empty list jumped
// straight to a sixteen-element buffer no matter how many queries the list ended up holding. The
// dominant real-world case is exactly one query -- `@media screen`, `@media (min-width: 600px)` --
// i.e. one sizeof(MQ::MediaQuery) of data in sixteen slots' worth of buffer.
//
// consumeMediaQueryList() now derives the buffer size from the top-level comma count, so these
// tests assert capacity() == size() on the retained list.
//
// The capacity has to be read off the rule's own storage: WTF::Vector's copy constructor is
// Base(other.size(), other.size()), i.e. always tight, so a copy would pass these assertions even
// on the unfixed build. For the same reason nothing here copies a MediaQueryList by value -- doing
// so would also instantiate MQ::Value's Variant destructor, whose CSS::unevaluatedCalcDeref() is
// not exported from WebCore.

#include "config.h"

#include <WebCore/CSSParserContext.h>
#include <WebCore/MediaQuery.h>
#include <WebCore/ProcessWarming.h>
#include <WebCore/StyleRule.h>
#include <WebCore/StyleSheetContents.h>
#include <wtf/text/StringBuilder.h>

namespace TestWebKitAPI {

using namespace WebCore;

class MediaQueryListCapacity : public testing::Test {
public:
    void SetUp() override
    {
        WebCore::ProcessWarming::initializeNames();
    }
};

static const MQ::MediaQueryList* firstMediaRuleQueries(const StyleSheetContents& contents)
{
    for (auto& rule : contents.childRules()) {
        if (auto* mediaRule = dynamicDowncast<StyleRuleMedia>(rule.get()))
            return &mediaRule->mediaQueries();
    }
    return nullptr;
}

TEST_F(MediaQueryListCapacity, SingleQueryListIsTightlySized)
{
    auto contents = StyleSheetContents::create(strictCSSParserContext());
    EXPECT_TRUE(contents->parseString("@media (min-resolution: 2dppx) { body { color: red } }"_s));

    auto* queries = firstMediaRuleQueries(contents);
    ASSERT_TRUE(queries);
    EXPECT_EQ(queries->size(), static_cast<size_t>(1));
    EXPECT_EQ(queries->capacity(), static_cast<size_t>(1));
}

TEST_F(MediaQueryListCapacity, BareMediaTypeListIsTightlySized)
{
    auto contents = StyleSheetContents::create(strictCSSParserContext());
    EXPECT_TRUE(contents->parseString("@media screen { body { color: red } }"_s));

    auto* queries = firstMediaRuleQueries(contents);
    ASSERT_TRUE(queries);
    EXPECT_EQ(queries->size(), static_cast<size_t>(1));
    EXPECT_EQ(queries->capacity(), static_cast<size_t>(1));
    EXPECT_EQ((*queries)[0].mediaType, AtomString { "screen"_s });
}

TEST_F(MediaQueryListCapacity, MultipleQueryListIsTightlySized)
{
    auto contents = StyleSheetContents::create(strictCSSParserContext());
    EXPECT_TRUE(contents->parseString("@media screen, print, (min-resolution: 2dppx) { body { color: red } }"_s));

    auto* queries = firstMediaRuleQueries(contents);
    ASSERT_TRUE(queries);
    EXPECT_EQ(queries->size(), static_cast<size_t>(3));
    EXPECT_EQ(queries->capacity(), static_cast<size_t>(3));

    EXPECT_EQ((*queries)[0].mediaType, AtomString { "screen"_s });
    EXPECT_EQ((*queries)[1].mediaType, AtomString { "print"_s });
    // A bare <media-condition> carries no media type.
    EXPECT_TRUE((*queries)[2].mediaType.isNull());
    EXPECT_TRUE((*queries)[2].condition.has_value());
}

TEST_F(MediaQueryListCapacity, CommasInsideBlocksDoNotInflateCapacity)
{
    auto contents = StyleSheetContents::create(strictCSSParserContext());
    EXPECT_TRUE(contents->parseString("@media (min-width: clamp(1px, 2px, 3px)) { body { color: red } }"_s));

    auto* queries = firstMediaRuleQueries(contents);
    ASSERT_TRUE(queries);
    EXPECT_EQ(queries->size(), static_cast<size_t>(1));
    EXPECT_EQ(queries->capacity(), static_cast<size_t>(1));
}

TEST_F(MediaQueryListCapacity, ListWiderThanSixteenElementFloorIsSizedExactly)
{
    constexpr size_t queryCount = 20;

    StringBuilder builder;
    builder.append("@media "_s);
    for (size_t i = 0; i < queryCount; ++i) {
        if (i)
            builder.append(", "_s);
        builder.append("(min-width: "_s, i + 1, "px)"_s);
    }
    builder.append(" { body { color: red } }"_s);

    auto contents = StyleSheetContents::create(strictCSSParserContext());
    EXPECT_TRUE(contents->parseString(builder.toString()));

    auto* queries = firstMediaRuleQueries(contents);
    ASSERT_TRUE(queries);
    EXPECT_EQ(queries->size(), queryCount);
    EXPECT_EQ(queries->capacity(), queryCount);
}

TEST_F(MediaQueryListCapacity, InvalidComponentsAreStillCountedExactly)
{
    auto contents = StyleSheetContents::create(strictCSSParserContext());
    EXPECT_TRUE(contents->parseString("@media screen,,print, { body { color: red } }"_s));

    auto* queries = firstMediaRuleQueries(contents);
    ASSERT_TRUE(queries);
    // screen / (empty -> not all) / print / (empty -> not all)
    EXPECT_EQ(queries->size(), static_cast<size_t>(4));
    EXPECT_EQ(queries->capacity(), static_cast<size_t>(4));

    EXPECT_EQ((*queries)[0].mediaType, AtomString { "screen"_s });
    EXPECT_EQ((*queries)[1].mediaType, AtomString { "all"_s });
    EXPECT_TRUE((*queries)[1].prefix.has_value());
    EXPECT_EQ((*queries)[2].mediaType, AtomString { "print"_s });
    EXPECT_EQ((*queries)[3].mediaType, AtomString { "all"_s });
    EXPECT_TRUE((*queries)[3].prefix.has_value());
}

} // namespace TestWebKitAPI
