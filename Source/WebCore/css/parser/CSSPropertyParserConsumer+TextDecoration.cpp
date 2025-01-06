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
#include "CSSPropertyParserConsumer+TextDecoration.h"

#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Color.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Length.h"
#include "CSSPropertyParserConsumer+LengthDefinitions.h"
#include "CSSPropertyParserConsumer+List.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+String.h"
#include "CSSTextShadowPropertyValue.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

static std::optional<CSS::TextShadow> consumeSingleUnresolvedTextShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <single-text-shadow> = [ <color>? && <length>{2,3} ]
    // https://drafts.csswg.org/css-text-decor-3/#propdef-text-shadow

    // FIXME: CSS Text Decoration 4 has updated text-shadow to use the complete box-shadow grammar:
    // <shadow> = <color>? && [<length>{2} <length [0,∞]>? <length>?] && inset?
    // https://drafts.csswg.org/css-text-decor-4/#propdef-text-shadow

    auto rangeCopy = range;

    const auto lengthOptions = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };

    std::optional<CSS::Color> color;
    std::optional<CSS::Length<>> x;
    std::optional<CSS::Length<>> y;
    std::optional<CSS::Length<CSS::Nonnegative>> blur;

    auto consumeOptionalColor = [&] -> bool {
        if (color)
            return false;
        auto maybeColor = consumeUnresolvedColor(rangeCopy, context);
        if (!maybeColor)
            return false;
        color = CSS::Color(WTFMove(*maybeColor));
        return !!color;
    };

    auto consumeLengths = [&] -> bool {
        if (x)
            return false;
        x = MetaConsumer<CSS::Length<>>::consume(rangeCopy, context, { }, lengthOptions);
        if (!x)
            return false;
        y = MetaConsumer<CSS::Length<>>::consume(rangeCopy, context, { }, lengthOptions);
        if (!y)
            return false;

        blur = MetaConsumer<CSS::Length<CSS::Nonnegative>>::consume(rangeCopy, context, { }, lengthOptions);
        return true;
    };

    while (!rangeCopy.atEnd()) {
        if (consumeOptionalColor() || consumeLengths())
            continue;
        break;
    }

    if (!y)
        return { };

    range = rangeCopy;

    return CSS::TextShadow {
        .color = WTFMove(color),
        .location = { WTFMove(*x), WTFMove(*y) },
        .blur = WTFMove(blur)
    };
}

static std::optional<CSS::TextShadowProperty::List> consumeUnresolvedTextShadowList(CSSParserTokenRange& range, const CSSParserContext& context)
{
    auto rangeCopy = range;

    CSS::TextShadowProperty::List list;

    do {
        auto shadow = consumeSingleUnresolvedTextShadow(rangeCopy, context);
        if (!shadow)
            return { };
        list.value.append(WTFMove(*shadow));
    } while (consumeCommaIncludingWhitespace(rangeCopy));

    range = rangeCopy;

    return list;
}

static std::optional<CSS::TextShadowProperty> consumeUnresolvedTextShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().id() == CSSValueNone) {
        range.consumeIncludingWhitespace();
        return CSS::TextShadowProperty { CSS::Keyword::None { } };
    }
    if (auto textShadowList = consumeUnresolvedTextShadowList(range, context))
        return CSS::TextShadowProperty { WTFMove(*textShadowList) };
    return { };
}

RefPtr<CSSValue> consumeTextShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'text-shadow'> = none | [ <color>? && <length>{2,3} ]#
    // https://drafts.csswg.org/css-text-decor-3/#propdef-text-shadow

    if (auto property = consumeUnresolvedTextShadow(range, context))
        return CSSTextShadowPropertyValue::create({ WTFMove(*property) });
    return nullptr;
}

RefPtr<CSSValue> consumeTextDecorationLine(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'text-decoration-line'> = none | [ underline || overline || line-through || blink ]
    // https://drafts.csswg.org/css-text-decor-3/#text-decoration-line-property

    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    Vector<CSSValueID, 4> list;
    while (true) {
        auto ident = consumeIdentRaw<CSSValueBlink, CSSValueUnderline, CSSValueOverline, CSSValueLineThrough>(range);
        if (!ident)
            break;
        if (list.contains(*ident))
            return nullptr;
        list.append(*ident);
    }
    if (list.isEmpty())
        return nullptr;
    CSSValueListBuilder builder;
    for (auto ident : list)
        builder.append(CSSPrimitiveValue::create(ident));
    return CSSValueList::createSpaceSeparated(WTFMove(builder));
}

RefPtr<CSSValue> consumeTextEmphasisStyle(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'text-emphasis-style'> = none | [ [ filled | open ] || [ dot | circle | double-circle | triangle | sesame ] ] | <string>
    // https://drafts.csswg.org/css-text-decor-3/#text-emphasis-style-property

    CSSValueID id = range.peek().id();
    if (id == CSSValueNone)
        return consumeIdent(range);

    if (auto textEmphasisStyle = consumeString(range))
        return textEmphasisStyle;

    auto fill = consumeIdent<CSSValueFilled, CSSValueOpen>(range);
    auto shape = consumeIdent<CSSValueDot, CSSValueCircle, CSSValueDoubleCircle, CSSValueTriangle, CSSValueSesame>(range);
    if (!fill)
        fill = consumeIdent<CSSValueFilled, CSSValueOpen>(range);
    if (fill && shape)
        return CSSValueList::createSpaceSeparated(fill.releaseNonNull(), shape.releaseNonNull());
    return fill ? fill : shape;
}

RefPtr<CSSValue> consumeTextEmphasisPosition(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'text-emphasis-position'> = [ over | under ] && [ right | left ]?
    // https://drafts.csswg.org/css-text-decor-3/#text-emphasis-position-property

    std::optional<CSSValueID> overUnderValueID;
    std::optional<CSSValueID> leftRightValueID;
    while (!range.atEnd()) {
        auto valueID = range.peek().id();
        switch (valueID) {
        case CSSValueOver:
        case CSSValueUnder:
            if (overUnderValueID)
                return nullptr;
            overUnderValueID = valueID;
            break;
        case CSSValueLeft:
        case CSSValueRight:
            if (leftRightValueID)
                return nullptr;
            leftRightValueID = valueID;
            break;
        default:
            return nullptr;
        }
        range.consumeIncludingWhitespace();
    }
    if (!overUnderValueID)
        return nullptr;
    if (!leftRightValueID)
        return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(*overUnderValueID));
    return CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(*overUnderValueID),
        CSSPrimitiveValue::create(*leftRightValueID));
}

//
RefPtr<CSSValue> consumeTextUnderlinePosition(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'text-underline-position'> = auto | [ [ under | from-font ] || [ left | right ] ]
    // https://drafts.csswg.org/css-text-decor-4/#text-underline-position-property

    if (auto ident = consumeIdent<CSSValueAuto>(range))
        return ident;

    auto metric = consumeIdentRaw<CSSValueUnder, CSSValueFromFont>(range);

    std::optional<CSSValueID> side;
    if (context.cssTextUnderlinePositionLeftRightEnabled)
        side = consumeIdentRaw<CSSValueLeft, CSSValueRight>(range);

    if (side && !metric)
        metric = consumeIdentRaw<CSSValueUnder, CSSValueFromFont>(range);

    if (metric && side)
        return CSSValuePair::create(CSSPrimitiveValue::create(*metric), CSSPrimitiveValue::create(*side));
    if (metric)
        return CSSPrimitiveValue::create(*metric);
    if (side)
        return CSSPrimitiveValue::create(*side);

    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
