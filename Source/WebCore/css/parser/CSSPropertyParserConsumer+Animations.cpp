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
#include "CSSPropertyParserConsumer+Animations.h"

#include "CSSCalcSymbolTable.h"
#include "CSSParserContext.h"
#include "CSSParserIdioms.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParser.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Percentage.h"
#include "CSSPropertyParserConsumer+Timeline.h"
#include "Length.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

Vector<std::pair<CSSValueID, double>> consumeKeyframeKeyList(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <keyframe-selector> = from | to | <percentage [0,100]> | <timeline-range-name> <percentage>
    // https://drafts.csswg.org/css-animations-1/#typedef-keyframe-selector

    auto consumeAndConvertPercentage = [&](CSSParserTokenRange& range) -> std::optional<double> {
        // FIXME: We use resolveAsPercentageDeprecated() to deal with calc() and % values.
        // We will eventually want to return a CSS value that can be kept as-is on a
        // BlendingKeyframe so that resolution happens when we have the necessary context
        // when the keyframes are associated with a target element.
        if (auto percentageValue = consumePercentage(range, context, ValueRange::NonNegative)) {
            auto resolvedPercentage = percentageValue->resolveAsPercentageDeprecated();
            if (resolvedPercentage >= 0 && resolvedPercentage <= 100)
                return resolvedPercentage / 100;
        }
        return { };
    };

    auto timelineRange = [&](CSSParserTokenRange& range, CSSValueID id) -> std::optional<std::pair<CSSValueID, double>> {
        if (CSSPropertyParserHelpers::isAnimationRangeKeyword(id)) {
            // "normal" will be considered valid by isAnimationRangeKeyword() but is not valid for a @keyframes rule.
            if (id == CSSValueNormal)
                return { };
            if (auto convertedPercentage = consumeAndConvertPercentage(range))
                return { { id, *convertedPercentage } };
        }
        return { };
    };

    Vector<std::pair<CSSValueID, double>> result;
    while (true) {
        range.consumeWhitespace();

        if (auto tokenValue = consumeIdent(range)) {
            auto valueId = tokenValue->valueID();
            if (valueId == CSSValueFrom)
                result.append({ CSSValueNormal, 0 });
            else if (valueId == CSSValueTo)
                result.append({ CSSValueNormal, 1 });
            else if (auto pair = timelineRange(range, valueId))
                result.append(*pair);
            else
                return { }; // Parser error, invalid value in keyframe selector
        } else if (auto convertedPercentage = consumeAndConvertPercentage(range))
            result.append({ CSSValueNormal, *convertedPercentage });
        else
            return { }; // Parser error, invalid value in keyframe selector

        if (range.atEnd()) {
            result.shrinkToFit();
            return result;
        }

        if (range.consume().type() != CommaToken)
            return { }; // Parser error
    }
}

RefPtr<CSSValue> consumeKeyframesName(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <keyframes-name> = <custom-ident> | <string>
    // https://drafts.csswg.org/css-animations/#typedef-keyframes-name

    if (range.peek().type() == StringToken) {
        auto& token = range.consumeIncludingWhitespace();
        auto valueId = cssValueKeywordID(token.value());
        if (isValidCustomIdentifier(valueId) && valueId != CSSValueNone)
            return CSSPrimitiveValue::createCustomIdent(token.value().toString());
        return CSSPrimitiveValue::create(token.value().toString());
    }

    return consumeCustomIdent(range);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
