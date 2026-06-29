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
#include "CSSCustomIdentValue.h"
#include "CSSParserContext.h"
#include "CSSParserIdioms.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParser.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Timeline.h"
#include "CSSPropertyParserState.h"
#include "CSSStringValue.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

Vector<std::pair<CSSValueID, CSS::Percentage<>>> consumeKeyframeKeyList(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <keyframe-selector> = from | to | <percentage [0,100]> | <timeline-range-name> <percentage>
    // https://drafts.csswg.org/css-animations-1/#typedef-keyframe-selector
    //   and extended by
    // https://drafts.csswg.org/scroll-animations/#named-range-keyframes

    auto timelineRange = [&](CSSParserTokenRange& range, CSSValueID id) -> std::optional<std::pair<CSSValueID, CSS::Percentage<>>> {
        if (CSSPropertyParserHelpers::isTimelineRangeName(id)) {
            if (auto percentage = MetaConsumer<CSS::Percentage<>>::consume(range, state))
                return { { id, WTF::move(*percentage) } };
        }
        return { };
    };

    Vector<std::pair<CSSValueID, CSS::Percentage<>>> result;
    while (true) {
        range.consumeWhitespace();

        if (auto tokenValue = consumeIdent(range)) {
            auto valueId = tokenValue->valueID();
            if (valueId == CSSValueFrom)
                result.append({ CSSValueNormal, CSS::Percentage<> { 0 } });
            else if (valueId == CSSValueTo)
                result.append({ CSSValueNormal, CSS::Percentage<> { 100 } });
            else if (auto pair = timelineRange(range, valueId))
                result.append(*pair);
            else
                return { }; // Parser error, invalid value in keyframe selector
        } else if (auto percentage = MetaConsumer<CSS::Percentage<CSS::ClosedPercentageRange>>::consume(range, state))
            result.append({ CSSValueNormal, CSS::rangeExpandingCast<CSS::All>(*percentage) });
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

Vector<std::pair<CSSValueID, CSS::Percentage<>>> parseKeyframeKeyList(const String& string, const CSSParserContext& context)
{
    auto tokenizer = CSSTokenizer(string);
    auto range = tokenizer.tokenRange();

    // Handle leading whitespace.
    range.consumeWhitespace();

    auto state = CSS::PropertyParserState { .context = context };
    auto result = consumeKeyframeKeyList(range, state);

    // Handle trailing whitespace.
    range.consumeWhitespace();

    if (!range.atEnd())
        return { };

    return result;
}

RefPtr<CSSValue> consumeKeyframesName(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <keyframes-name> = <custom-ident> | <string>
    // https://drafts.csswg.org/css-animations/#typedef-keyframes-name

    if (range.peek().type() == StringToken) {
        auto& token = range.consumeIncludingWhitespace();

        if (token.value().isEmpty())
            return nullptr;

        auto valueId = cssValueKeywordID(token.value());
        if (isValidCustomIdentifier(valueId) && valueId != CSSValueNone)
            return CSSCustomIdentValue::create(CSS::CustomIdent { token.value().toAtomString() });
        return CSSStringValue::create(CSS::String { token.value().toString() });
    }

    return consumeCustomIdent(range, state);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
