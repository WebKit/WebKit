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
#include "CSSPropertyParserConsumer+Inline.h"

#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+IntegerDefinitions.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

static RefPtr<CSSValue> consumeTextEdge(CSSParserTokenRange& range)
{
    // <text-edge> = [ text | cap | ex | ideographic | ideographic-ink ]
    //               [ text | alphabetic | ideographic | ideographic-ink ]?
    // https://drafts.csswg.org/css-inline-3/#typedef-text-edge

    auto firstValue = consumeIdent<CSSValueID::Text, CSSValueID::Cap, CSSValueID::Ex, CSSValueID::Ideographic, CSSValueID::IdeographicInk>(range);
    if (!firstValue)
        return nullptr;

    auto secondValue = consumeIdent<CSSValueID::Text, CSSValueID::Alphabetic, CSSValueID::Ideographic, CSSValueID::IdeographicInk>(range);

    // https://drafts.csswg.org/css-inline-3/#text-edges
    // "If only one value is specified, both edges are assigned that same keyword if possible; else text is assumed as the missing value."
    auto shouldSerializeSecondValue = [&]() {
        if (!secondValue)
            return false;
        if (firstValue->valueID() == CSSValueID::Cap || firstValue->valueID() == CSSValueID::Ex)
            return secondValue->valueID() != CSSValueID::Text;
        return firstValue->valueID() != secondValue->valueID();
    }();
    if (!shouldSerializeSecondValue)
        return firstValue;

    return CSSValuePair::create(firstValue.releaseNonNull(), secondValue.releaseNonNull());
}

RefPtr<CSSValue> consumeLineFitEdge(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    // <'line-fit-edge'> = leading | <text-edge>
    // https://drafts.csswg.org/css-inline-3/#propdef-line-fit-edge

    if (range.peek().id() == CSSValueID::Leading)
        return consumeIdent(range);
    return consumeTextEdge(range);
}

RefPtr<CSSValue> consumeTextBoxEdge(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    // <'text-box-edge'> = auto | <text-edge>
    // https://drafts.csswg.org/css-inline-3/#propdef-text-box-edge

    if (range.peek().id() == CSSValueID::Auto)
        return consumeIdent(range);
    return consumeTextEdge(range);
}

RefPtr<CSSValue> consumeInitialLetter(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'initial-letter'> = normal | <number [1,∞]> <integer [1,∞]> | <number [1,∞]> && [ drop | raise ]?
    // https://drafts.csswg.org/css-inline-3/#propdef-initial-letter

    if (range.peek().id() == CSSValueID::Normal)
        return consumeIdent(range);

    if (auto keyword = consumeIdent<CSSValueID::Drop, CSSValueID::Raise>(range)) {
        auto size = CSSPrimitiveValueResolver<CSS::Number<CSS::Positive>>::consumeAndResolve(range, state);
        if (!size)
            return nullptr;
        return CSSValuePair::create(size.releaseNonNull(), keyword.releaseNonNull());
    }

    auto size = CSSPrimitiveValueResolver<CSS::Number<CSS::Positive>>::consumeAndResolve(range, state);
    if (!size)
        return nullptr;

    if (auto keyword = consumeIdent<CSSValueID::Drop, CSSValueID::Raise>(range))
        return CSSValuePair::create(size.releaseNonNull(), keyword.releaseNonNull());

    // A list, not a pair, so the converter cannot mistake this for the legacy pair.
    if (auto sink = CSSPrimitiveValueResolver<CSS::Integer<CSS::Positive>>::consumeAndResolve(range, state))
        return CSSValueList::createSpaceSeparated(size.releaseNonNull(), sink.releaseNonNull());

    return size;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
