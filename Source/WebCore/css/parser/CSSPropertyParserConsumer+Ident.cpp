/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSPropertyParserConsumer+Ident.h"

#include "CSSCustomIdentValue.h"
#include "CSSParserIdioms.h"
#include "CSSValuePool.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

std::optional<CSSValueID> consumeIdentRaw(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return std::nullopt;
    return range.consumeIncludingWhitespace().id();
}

std::optional<CSS::Keyword> consumeUnresolvedIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return std::nullopt;
    return CSS::Keyword { range.consumeIncludingWhitespace().id() };
}

RefPtr<CSSKeywordValue> consumeIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return nullptr;
    return CSSKeywordValue::create(CSS::Keyword { range.consumeIncludingWhitespace().id() });
}

std::optional<CSSValueID> consumeIdentRangeRaw(CSSParserTokenRange& range, CSSValueID lower, CSSValueID upper)
{
    if (range.peek().id() < lower || range.peek().id() > upper)
        return std::nullopt;
    return consumeIdentRaw(range);
}

std::optional<CSS::Keyword> consumeUnresolvedIdentRange(CSSParserTokenRange& range, CSSValueID lower, CSSValueID upper)
{
    if (range.peek().id() < lower || range.peek().id() > upper)
        return std::nullopt;
    return consumeUnresolvedIdent(range);
}

RefPtr<CSSKeywordValue> consumeIdentRange(CSSParserTokenRange& range, CSSValueID lower, CSSValueID upper)
{
    if (range.peek().id() < lower || range.peek().id() > upper)
        return nullptr;
    return consumeIdent(range);
}

// MARK: <custom-ident>
// https://drafts.csswg.org/css-values/#custom-idents

StringView consumeEagerlyResolvableCustomIdentRaw(CSSParserTokenRange& range)
{
    // FIXME: When support for the ident() function is added, this will only succeed if the ident() function can be eagerly resolved without additional context.

    if (range.peek().type() != IdentToken || !isValidCustomIdentifier(range.peek().id()))
        return { };
    return range.consumeIncludingWhitespace().value();
}

StringView consumeEagerlyResolvableCustomIdentRawExcluding(CSSParserTokenRange& range, std::initializer_list<CSSValueID> excluding)
{
    // FIXME: When support for the ident() function is added, this will only succeed if the ident() function can be eagerly resolved without additional context.

    if (range.peek().type() != IdentToken || !isValidCustomIdentifier(range.peek().id()) || std::ranges::find(excluding, range.peek().id()) != excluding.end())
        return { };
    return range.consumeIncludingWhitespace().value();
}

std::optional<CSS::CustomIdent> consumeUnresolvedCustomIdent(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    if (range.peek().type() != IdentToken || !isValidCustomIdentifier(range.peek().id()))
        return { };
    return CSS::CustomIdent { range.consumeIncludingWhitespace().value().toAtomString() };
}

std::optional<CSS::CustomIdent> consumeUnresolvedCustomIdentExcluding(CSSParserTokenRange& range, CSS::PropertyParserState&, std::initializer_list<CSSValueID> excluding)
{
    if (range.peek().type() != IdentToken || !isValidCustomIdentifier(range.peek().id()) || std::ranges::find(excluding, range.peek().id()) != excluding.end())
        return { };
    return CSS::CustomIdent { range.consumeIncludingWhitespace().value().toAtomString() };
}

RefPtr<CSSValue> consumeCustomIdent(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto unresolved = consumeUnresolvedCustomIdent(range, state))
        return CSSCustomIdentValue::create(WTF::move(*unresolved));
    return nullptr;
}

RefPtr<CSSValue> consumeCustomIdentExcluding(CSSParserTokenRange& range, CSS::PropertyParserState& state, std::initializer_list<CSSValueID> excluding)
{
    if (auto unresolved = consumeUnresolvedCustomIdentExcluding(range, state, excluding))
        return CSSCustomIdentValue::create(WTF::move(*unresolved));
    return nullptr;
}

// MARK: <dashed-ident>
// https://drafts.csswg.org/css-values/#dashed-idents

StringView consumeEagerlyResolvableDashedIdentRaw(CSSParserTokenRange& range)
{
    // FIXME: When support for the ident() function is added, this will only succeed if the ident() function can be eagerly resolved without additional context.

    if (range.peek().type() != IdentToken || !range.peek().value().startsWith("--"_s))
        return { };
    return range.consumeIncludingWhitespace().value();
}

std::optional<CSS::CustomIdent> consumeUnresolvedDashedIdent(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    if (range.peek().type() != IdentToken || !range.peek().value().startsWith("--"_s))
        return { };
    return CSS::CustomIdent { range.consumeIncludingWhitespace().value().toAtomString() };
}

RefPtr<CSSValue> consumeDashedIdent(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto unresolved = consumeUnresolvedDashedIdent(range, state))
        return CSSCustomIdentValue::create(WTF::move(*unresolved));
    return nullptr;
}

// MARK: <CSS-wide keyword>
// https://drafts.csswg.org/css-values/#common-keywords

std::optional<CSSWideKeyword> consumeCSSWideKeyword(CSSParserTokenRange& range)
{
    auto rangeCopy = range;
    auto valueID = rangeCopy.consumeIncludingWhitespace().id();
    if (!rangeCopy.atEnd())
        return { };

    auto keyword = parseCSSWideKeyword(valueID);
    if (!keyword)
        return { };

    range = rangeCopy;
    return keyword;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
