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
#include "CSSPropertyParserConsumer+List.h"
#include "CSSPropertyParserConsumer+String.h"
#include "CSSShadowValue.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

static RefPtr<CSSValue> consumeSingleTextShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <single-text-shadow> = [ <color>? && <length>{2,3} ]
    // https://drafts.csswg.org/css-text-decor-3/#propdef-text-shadow

    RefPtr<CSSPrimitiveValue> color;

    RefPtr<CSSPrimitiveValue> horizontalOffset;
    RefPtr<CSSPrimitiveValue> verticalOffset;
    RefPtr<CSSPrimitiveValue> blurRadius;

    for (size_t i = 0; i < 2; i++) {
        if (range.atEnd())
            break;

        const CSSParserToken& nextToken = range.peek();

        // If we have come to a comma (e.g. if this range represents a comma-separated list of <single-text-shadow>s), we are done parsing this <single-text-shadow>.
        if (nextToken.type() == CommaToken)
            break;

        auto maybeColor = consumeColor(range, context);
        if (maybeColor) {
            if (color)
                return nullptr;
            color = maybeColor;
            continue;
        }

        if (horizontalOffset || verticalOffset || blurRadius)
            return nullptr;

        horizontalOffset = consumeLength(range, context);
        if (!horizontalOffset)
            return nullptr;
        verticalOffset = consumeLength(range, context);
        if (!verticalOffset)
            return nullptr;
        blurRadius = consumeLength(range, context, ValueRange::NonNegative);
    }

    // In order for this to be a valid <shadow>, at least these lengths must be present.
    if (!horizontalOffset || !verticalOffset)
        return nullptr;
    return CSSShadowValue::create(WTFMove(horizontalOffset), WTFMove(verticalOffset), WTFMove(blurRadius), nullptr, nullptr, WTFMove(color));
}


RefPtr<CSSValue> consumeTextShadow(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'text-shadow'> = none | [ <color>? && <length>{2,3} ]#
    // https://drafts.csswg.org/css-text-decor-3/#propdef-text-shadow

    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    return consumeCommaSeparatedListWithoutSingleValueOptimization(range, consumeSingleTextShadow, context);
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
