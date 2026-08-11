/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "CSSPropertyParserConsumer+Content.h"

#include "CSSContentValue.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSParserTokenRangeGuard.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+CounterStyles.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Image.h"
#include "CSSPropertyParserConsumer+Lists.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+String.h"
#include "CSSPropertyParserState.h"
#include "CSSPropertyParsing.h"
#include "CSSQuotesValue.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

static std::optional<CSS::Quotes> consumeUnresolvedQuotes(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    // <'quotes'> = auto | none | match-parent | [ <string> <string> ]+
    // https://drafts.csswg.org/css-content-3/#propdef-quotes

    // FIXME: Support `match-parent`.

    CSSParserTokenRangeGuard guard { range };

    switch (range.peek().id()) {
    case CSSValueNone:
        range.consumeIncludingWhitespace();
        guard.commit();
        return CSS::Quotes { CSS::Keyword::None { } };
    case CSSValueAuto:
        range.consumeIncludingWhitespace();
        guard.commit();
        return CSS::Quotes { CSS::Keyword::Auto { } };
    default:
        break;
    }

    CSS::Quotes::Data openClosePairs;
    while (!range.atEnd()) {
        auto openQuote = consumeUnresolvedString(range);
        if (!openQuote)
            return std::nullopt;
        auto closeQuote = consumeUnresolvedString(range);
        if (!closeQuote)
            return std::nullopt;

        openClosePairs.value.append(WTF::move(*openQuote));
        openClosePairs.value.append(WTF::move(*closeQuote));
    }
    if (openClosePairs.isEmpty())
        return std::nullopt;

    guard.commit();
    return CSS::Quotes { WTF::move(openClosePairs) };
}

RefPtr<CSSValue> consumeQuotes(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'quotes'> = auto | none | match-parent | [ <string> <string> ]+
    // https://drafts.csswg.org/css-content-3/#propdef-quotes

    // FIXME: Support `match-parent`.

    if (auto unresolved = consumeUnresolvedQuotes(range, state))
        return CSSQuotesValue::create(WTF::move(*unresolved));
    return nullptr;
}

static std::optional<CSS::Content::CounterFunction> consumeUnresolvedContentCounterFunction(CSSParserTokenRange args, CSS::PropertyParserState& state)
{
    // counter()  =  counter( <counter-name>, <counter-style>?@(default=decimal) )
    // https://www.w3.org/TR/css-lists-3/#funcdef-counter

    auto customIdent = consumeUnresolvedCustomIdent(args, state);
    if (!customIdent)
        return std::nullopt;

    std::optional<CSS::CounterStyle> counterStyle;
    if (consumeCommaIncludingWhitespace(args)) {
        counterStyle = consumeUnresolvedCounterStyle(args, state);
        if (!counterStyle)
            return std::nullopt;
    } else
        counterStyle = CSS::CounterStyle { CSS::Keyword { CSSValueDecimal } };

    if (!args.atEnd())
        return std::nullopt;

    return CSS::Content::CounterFunction {
        .parameters {
            WTF::move(*customIdent),
            WTF::move(*counterStyle)
        }
    };
}

static std::optional<CSS::Content::CountersFunction> consumeUnresolvedContentCountersFunction(CSSParserTokenRange args, CSS::PropertyParserState& state)
{
    // counters() = counters( <counter-name>, <string>, <counter-style>?@(default=decimal) )
    // https://www.w3.org/TR/css-lists-3/#funcdef-counters

    auto customIdent = consumeUnresolvedCustomIdent(args, state);
    if (!customIdent)
        return std::nullopt;

    if (!consumeCommaIncludingWhitespace(args))
        return std::nullopt;

    auto separator = consumeUnresolvedString(args);
    if (!separator)
        return std::nullopt;

    std::optional<CSS::CounterStyle> counterStyle;
    if (consumeCommaIncludingWhitespace(args)) {
        counterStyle = consumeUnresolvedCounterStyle(args, state);
        if (!counterStyle)
            return std::nullopt;
    } else
        counterStyle = CSS::CounterStyle { CSS::Keyword { CSSValueDecimal } };

    if (!args.atEnd())
        return std::nullopt;

    return CSS::Content::CountersFunction {
        .parameters {
            WTF::move(*customIdent),
            WTF::move(*separator),
            WTF::move(*counterStyle)
        }
    };
}

static std::optional<CSS::Content::LegacyAttrFunction> consumeUnresolvedContentLegacyAttrFunction(CSSParserTokenRange args, CSS::PropertyParserState& state)
{
    // FIXME: Remove this when removing the `cssAttrSubstitutionFunctionEnabled` setting.

    if (args.peek().type() != IdentToken)
        return std::nullopt;

    auto token = args.consumeIncludingWhitespace();

    auto attrName = [&] {
        if (state.context.isHTMLDocument)
            return CSS::CustomIdent { token.value().convertToASCIILowercaseAtom() };
        return CSS::CustomIdent { token.value().toAtomString() };
    }();

    if (!args.atEnd() && !consumeCommaIncludingWhitespace(args))
        return std::nullopt;

    std::optional<CSS::String> fallback;
    if (args.peek().type() == StringToken) {
        token = args.consumeIncludingWhitespace();
        fallback = CSS::String { token.value().toString() };
    }

    if (!args.atEnd())
        return std::nullopt;

    return CSS::Content::LegacyAttrFunction {
        .parameters = {
            WTF::move(attrName),
            WTF::move(fallback),
        }
    };
}

static std::optional<CSS::Content> consumeUnresolvedContent(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // Standard says this should be:
    //
    // <'content'> = normal | none | [ <content-replacement> | <content-list> ] [/ [ <string> | <counter> | <attr()> ]+ ]?
    // https://drafts.csswg.org/css-content-3/#propdef-content

    CSSParserTokenRangeGuard guard { range };

    switch (range.peek().id()) {
    case CSSValueNone:
        range.consumeIncludingWhitespace();
        guard.commit();
        return CSS::Content { CSS::Keyword::None { } };
    case CSSValueNormal:
        range.consumeIncludingWhitespace();
        guard.commit();
        return CSS::Content { CSS::Keyword::Normal { } };
    default:
        break;
    }

    auto consumeVisibleContentListItem = [&] -> std::optional<CSS::Content::VisibleContentListItem> {
        if (auto string = consumeUnresolvedString(range))
            return CSS::Content::Text { WTF::move(*string) };

        if (auto image = consumeImage(range, state))
            return CSS::Content::Image { CSS::ImageWrapper { image.releaseNonNull() } };

        if (auto quote = consumeSpecificUnresolvedIdent<CSS::Content::Quote::Value>(range))
            return CSS::Content::Quote { WTF::move(*quote) };

        switch (range.peek().functionId()) {
        case CSSValueAttr:
            return consumeUnresolvedContentLegacyAttrFunction(consumeFunction(range), state);
        case CSSValueCounter:
            return consumeUnresolvedContentCounterFunction(consumeFunction(range), state);
        case CSSValueCounters:
            return consumeUnresolvedContentCountersFunction(consumeFunction(range), state);
        default:
            break;
        }

        return std::nullopt;
    };

    auto consumeVisibleContentList = [&] -> std::optional<CSS::Content::VisibleContentList> {
        CSS::Content::VisibleContentList result;

        bool shouldEnd = false;
        do {
            auto item = consumeVisibleContentListItem();
            if (!item)
                return std::nullopt;

            result.value.append(WTF::move(*item));

            // Visible content parsing ends at '/' or end of range.
            if (!range.atEnd()) {
                auto value = range.peek();
                if (value.type() == DelimiterToken && value.delimiter() == '/')
                    shouldEnd = true;
            }
            shouldEnd = shouldEnd || range.atEnd();
        } while (!shouldEnd);

        return result;
    };

    auto consumeAltContentListItem = [&] -> std::optional<CSS::Content::AltContentListItem> {
        if (auto string = consumeUnresolvedString(range))
            return CSS::Content::Text { WTF::move(*string) };

        // FIXME: <alt-content> should support <counter> as well.
        switch (range.peek().functionId()) {
        case CSSValueAttr:
            return consumeUnresolvedContentLegacyAttrFunction(consumeFunction(range), state);
        default:
            break;
        }

        return std::nullopt;
    };

    auto consumeAltContentList = [&] -> std::optional<CSS::Content::AltContentList> {
        CSS::Content::AltContentList result;

        do {
            auto item = consumeAltContentListItem();
            if (!item)
                return std::nullopt;

            result.value.append(WTF::move(*item));
        } while (!range.atEnd());

        return result;
    };

    auto visibleContent = consumeVisibleContentList();
    if (!visibleContent)
        return std::nullopt;

    if (consumeSlashIncludingWhitespace(range)) {
        auto altContent = consumeAltContentList();
        if (!altContent)
            return std::nullopt;

        guard.commit();
        return CSS::Content::Data {
            WTF::move(*visibleContent),
            WTF::move(altContent)
        };
    }

    guard.commit();
    return CSS::Content::Data {
        WTF::move(*visibleContent),
        std::nullopt
    };
}

RefPtr<CSSValue> consumeContent(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto unresolved = consumeUnresolvedContent(range, state))
        return CSSContentValue::create(WTF::move(*unresolved));
    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
