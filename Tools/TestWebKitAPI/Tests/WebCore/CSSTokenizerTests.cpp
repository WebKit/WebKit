/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <WebCore/CSSParserToken.h>
#include <WebCore/CSSParserTokenRange.h>
#include <WebCore/CSSTokenizer.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

using namespace WebCore;

TEST(CSSTokenizerTest, TokenizeWhitespaceLikeInput)
{
    CSSTokenizer tokenizer("foo           \n\n\n bar"_s);
    auto tokenRange = tokenizer.tokenRange();
    auto tokenRangeSpan = tokenRange.span();

    ASSERT_EQ(tokenRangeSpan.size(), 7ul);
    EXPECT_EQ(CSSParserTokenType::IdentToken, tokenRangeSpan[0].type());
    EXPECT_EQ(CSSParserTokenType::NonNewlineWhitespaceToken, tokenRangeSpan[1].type());
    EXPECT_EQ(CSSParserTokenType::NewlineToken, tokenRangeSpan[2].type());
    EXPECT_EQ(CSSParserTokenType::NewlineToken, tokenRangeSpan[3].type());
    EXPECT_EQ(CSSParserTokenType::NewlineToken, tokenRangeSpan[4].type());
    EXPECT_EQ(CSSParserTokenType::NonNewlineWhitespaceToken, tokenRangeSpan[5].type());
    EXPECT_EQ(CSSParserTokenType::IdentToken, tokenRangeSpan[6].type());
}

TEST(CSSTokenizerTest, TokenizeIdentAndCommaInput)
{
    CSSTokenizer tokenizer("this, "_s);
    auto tokenRange = tokenizer.tokenRange();
    auto tokenRangeSpan = tokenRange.span();

    ASSERT_EQ((size_t)3, tokenRangeSpan.size());
    EXPECT_EQ(CSSParserTokenType::IdentToken, tokenRangeSpan[0].type());
    EXPECT_EQ(CSSParserTokenType::CommaToken, tokenRangeSpan[1].type());
    EXPECT_EQ(CSSParserTokenType::NonNewlineWhitespaceToken, tokenRangeSpan[2].type());
}

TEST(CSSTokenizerTest, TokenizeMultipleWhitespaceInput)
{
    CSSTokenizer tokenizer("          "_s);
    auto tokenRange = tokenizer.tokenRange();
    auto tokenRangeSpan = tokenRange.span();

    ASSERT_EQ((size_t)1, tokenRangeSpan.size());
    EXPECT_EQ(CSSParserTokenType::NonNewlineWhitespaceToken, tokenRangeSpan[0].type());
}

TEST(CSSTokenizerTest, TokenizeNewlineTokenOnlyInput)
{
    CSSTokenizer tokenizer("\n"_s);
    auto tokenRange = tokenizer.tokenRange();
    auto tokenRangeSpan = tokenRange.span();

    ASSERT_EQ((size_t)1, tokenRangeSpan.size());
    EXPECT_EQ(CSSParserTokenType::NewlineToken, tokenRangeSpan[0].type());
}

TEST(CSSTokenizerTest, TokenizeNewlineAndWhitespaceInput)
{
    CSSTokenizer tokenizer("\n "_s);
    auto tokenRange = tokenizer.tokenRange();
    auto tokenRangeSpan = tokenRange.span();

    ASSERT_EQ((size_t)2, tokenRangeSpan.size());
    EXPECT_EQ(CSSParserTokenType::NewlineToken, tokenRangeSpan[0].type());
    EXPECT_EQ(CSSParserTokenType::NonNewlineWhitespaceToken, tokenRangeSpan[1].type());
}

TEST(CSSTokenizerTest, TokenizeNewlineWhitespaceAndIdentInput)
{
    CSSTokenizer tokenizer("\n identifiervalue"_s);
    auto tokenRange = tokenizer.tokenRange();
    auto tokenRangeSpan = tokenRange.span();

    ASSERT_EQ((size_t)3, tokenRangeSpan.size());
    EXPECT_EQ(CSSParserTokenType::NewlineToken, tokenRangeSpan[0].type());
    EXPECT_EQ(CSSParserTokenType::NonNewlineWhitespaceToken, tokenRangeSpan[1].type());
    EXPECT_EQ(CSSParserTokenType::IdentToken, tokenRangeSpan[2].type());
}

TEST(CSSTokenizerTest, TokenizeURLWithProperInput)
{
    CSSTokenizer tokenizer("url(foo)"_s);
    auto tokenRange = tokenizer.tokenRange();
    auto tokenRangeSpan = tokenRange.span();

    ASSERT_EQ((size_t)1, tokenRangeSpan.size());
    EXPECT_EQ(CSSParserTokenType::UrlToken, tokenRangeSpan[0].type());
}

TEST(CSSTokenizerTest, TokenizeURLWithNewlineWhitespaceInput)
{
    CSSTokenizer tokenizer("url(\n   \n foo)"_s);
    auto tokenRange = tokenizer.tokenRange();
    auto tokenRangeSpan = tokenRange.span();

    ASSERT_EQ((size_t)1, tokenRangeSpan.size());
    EXPECT_EQ(CSSParserTokenType::UrlToken, tokenRangeSpan[0].type());
}

} // namespace TestWebKitAPI
