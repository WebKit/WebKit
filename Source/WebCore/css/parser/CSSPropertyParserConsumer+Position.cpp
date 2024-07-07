/*
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
#include "CSSPropertyParserConsumer+Position.h"

#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Length.h"
#include "CSSValueKeywords.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include "RenderStyleConstants.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: <position>
// https://drafts.csswg.org/css-values/#position

// <position> = [
//   [ left | center | right ] || [ top | center | bottom ]
// |
//   [ left | center | right | <length-percentage> ]
//   [ top | center | bottom | <length-percentage> ]?
// |
//   [ [ left | right ] <length-percentage> ] &&
//   [ [ top | bottom ] <length-percentage> ]

// MARK: <bg-position>
// https://drafts.csswg.org/css-backgrounds-3/#propdef-background-position

// background-position has special parsing rules, allowing a 3-value syntax:
//
// <bg-position> =  [ left | center | right | top | bottom | <length-percentage> ]
// |
//   [ left | center | right | <length-percentage> ]
//   [ top | center | bottom | <length-percentage> ]
// |
//   [ center | [ left | right ] <length-percentage>? ] &&
//   [ center | [ top | bottom ] <length-percentage>? ]

static RefPtr<CSSPrimitiveValue> consumePositionComponent(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, NegativePercentagePolicy negativePercentagePolicy = NegativePercentagePolicy::Forbid)
{
    if (range.peek().type() == IdentToken)
        return consumeIdent<CSSValueLeft, CSSValueTop, CSSValueBottom, CSSValueRight, CSSValueCenter>(range);
    return consumeLengthOrPercent(range, parserMode, ValueRange::All, unitless, UnitlessZeroQuirk::Allow, negativePercentagePolicy);
}

static bool isHorizontalPositionKeywordOnly(const CSSPrimitiveValue& value)
{
    return value.isValueID() && (value.valueID() == CSSValueLeft || value.valueID() == CSSValueRight);
}

static bool isVerticalPositionKeywordOnly(const CSSPrimitiveValue& value)
{
    return value.isValueID() && (value.valueID() == CSSValueTop || value.valueID() == CSSValueBottom);
}

static PositionCoordinates positionFromOneValue(CSSPrimitiveValue& value)
{
    bool valueAppliesToYAxisOnly = isVerticalPositionKeywordOnly(value);
    if (valueAppliesToYAxisOnly)
        return { CSSPrimitiveValue::create(CSSValueCenter), value };
    return { value, CSSPrimitiveValue::create(CSSValueCenter) };
}

static std::optional<PositionCoordinates> positionFromTwoValues(CSSPrimitiveValue& value1, CSSPrimitiveValue& value2)
{
    bool mustOrderAsXY = isHorizontalPositionKeywordOnly(value1) || isVerticalPositionKeywordOnly(value2) || !value1.isValueID() || !value2.isValueID();
    bool mustOrderAsYX = isVerticalPositionKeywordOnly(value1) || isHorizontalPositionKeywordOnly(value2);
    if (mustOrderAsXY && mustOrderAsYX)
        return std::nullopt;
    if (mustOrderAsYX)
        return PositionCoordinates { value2, value1 };
    return PositionCoordinates { value1, value2 };
}

static std::optional<PositionCoordinates> backgroundPositionFromThreeValues(std::array<RefPtr<CSSPrimitiveValue>, 5>&& values)
{
    RefPtr<CSSValue> resultX;
    RefPtr<CSSValue> resultY;

    RefPtr<CSSPrimitiveValue> center;
    for (int i = 0; values[i]; ++i) {
        auto& currentValue = values[i];
        if (!currentValue->isValueID())
            return std::nullopt;

        auto id = currentValue->valueID();
        if (id == CSSValueCenter) {
            if (center)
                return std::nullopt;
            center = WTFMove(currentValue);
            continue;
        }

        RefPtr<CSSValue> result;
        if (auto& nextValue = values[i + 1]; nextValue && !nextValue->isValueID()) {
            result = CSSValuePair::create(currentValue.releaseNonNull(), nextValue.releaseNonNull());
            ++i;
        } else
            result = WTFMove(currentValue);

        if (id == CSSValueLeft || id == CSSValueRight) {
            if (resultX)
                return std::nullopt;
            resultX = result;
        } else {
            ASSERT(id == CSSValueTop || id == CSSValueBottom);
            if (resultY)
                return std::nullopt;
            resultY = result;
        }
    }

    if (center) {
        ASSERT(resultX || resultY);
        if (resultX && resultY)
            return std::nullopt;
        if (!resultX)
            resultX = WTFMove(center);
        else
            resultY = WTFMove(center);
    }

    return { { resultX.releaseNonNull(), resultY.releaseNonNull() } };
}

static std::optional<PositionCoordinates> positionFromFourValues(std::array<RefPtr<CSSPrimitiveValue>, 5>&& values)
{
    RefPtr<CSSValue> resultX;
    RefPtr<CSSValue> resultY;

    for (int i = 0; values[i]; ++i) {
        auto& currentValue = values[i];
        if (!currentValue->isValueID())
            return std::nullopt;

        auto id = currentValue->valueID();
        if (id == CSSValueCenter)
            return std::nullopt;

        RefPtr<CSSValue> result;
        if (auto& nextValue = values[i + 1]; nextValue && !nextValue->isValueID()) {
            result = CSSValuePair::create(currentValue.releaseNonNull(), nextValue.releaseNonNull());
            ++i;
        } else
            result = WTFMove(currentValue);

        if (id == CSSValueLeft || id == CSSValueRight) {
            if (resultX)
                return std::nullopt;
            resultX = result;
        } else {
            ASSERT(id == CSSValueTop || id == CSSValueBottom);
            if (resultY)
                return std::nullopt;
            resultY = result;
        }
    }

    return PositionCoordinates { resultX.releaseNonNull(), resultY.releaseNonNull() };
}

// FIXME: This may consume from the range upon failure. The background
// shorthand works around it, but we should just fix it here.
std::optional<PositionCoordinates> consumePositionCoordinates(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, PositionSyntax positionSyntax, NegativePercentagePolicy negativePercentagePolicy)
{
    auto value1 = consumePositionComponent(range, parserMode, unitless, negativePercentagePolicy);
    if (!value1)
        return std::nullopt;

    auto value2 = consumePositionComponent(range, parserMode, unitless, negativePercentagePolicy);
    if (!value2)
        return positionFromOneValue(*value1);

    auto value3 = consumePositionComponent(range, parserMode, unitless, negativePercentagePolicy);
    if (!value3)
        return positionFromTwoValues(*value1, *value2);

    auto value4 = consumePositionComponent(range, parserMode, unitless, negativePercentagePolicy);

    std::array<RefPtr<CSSPrimitiveValue>, 5> values {
        WTFMove(value1),
        WTFMove(value2),
        WTFMove(value3),
        value4.copyRef(),
        nullptr
    };

    if (value4)
        return positionFromFourValues(WTFMove(values));

    if (positionSyntax != PositionSyntax::BackgroundPosition)
        return std::nullopt;

    return backgroundPositionFromThreeValues(WTFMove(values));
}

RefPtr<CSSValue> consumePosition(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, PositionSyntax positionSyntax)
{
    if (auto coordinates = consumePositionCoordinates(range, parserMode, unitless, positionSyntax))
        return CSSValuePair::createNoncoalescing(WTFMove(coordinates->x), WTFMove(coordinates->y));
    return nullptr;
}

std::optional<PositionCoordinates> consumeOneOrTwoValuedPositionCoordinates(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless)
{
    auto value1 = consumePositionComponent(range, parserMode, unitless);
    if (!value1)
        return std::nullopt;
    auto value2 = consumePositionComponent(range, parserMode, unitless);
    if (!value2)
        return positionFromOneValue(*value1);
    return positionFromTwoValues(*value1, *value2);
}

static RefPtr<CSSValue> consumeSingleAxisPosition(CSSParserTokenRange& range, CSSParserMode parserMode, BoxOrient orientation)
{
    RefPtr<CSSPrimitiveValue> value1;

    if (range.peek().type() == IdentToken) {
        switch (orientation) {
        case BoxOrient::Horizontal:
            value1 = consumeIdent<CSSValueLeft, CSSValueRight, CSSValueCenter>(range);
            break;
        case BoxOrient::Vertical:
            value1 = consumeIdent<CSSValueTop, CSSValueBottom, CSSValueCenter>(range);
            break;
        }
        if (!value1)
            return nullptr;

        if (value1->valueID() == CSSValueCenter)
            return value1;
    }

    auto value2 = consumeLengthOrPercent(range, parserMode);
    if (value1 && value2)
        return CSSValuePair::create(value1.releaseNonNull(), value2.releaseNonNull());

    return value1 ? value1 : value2;
}

RefPtr<CSSValue> consumePositionX(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeSingleAxisPosition(range, context.mode, BoxOrient::Horizontal);
}

RefPtr<CSSValue> consumePositionY(CSSParserTokenRange& range, const CSSParserContext& context)
{
    return consumeSingleAxisPosition(range, context.mode, BoxOrient::Vertical);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
