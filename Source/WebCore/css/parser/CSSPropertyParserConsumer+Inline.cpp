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
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSValueKeywords.h"
#include "CSSValuePair.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

static RefPtr<CSSValue> consumeTextEdge(CSSParserTokenRange& range)
{
    // <text-edge> = [ text | cap | ex | ideographic | ideographic-ink ]
    //               [ text | alphabetic | ideographic | ideographic-ink ]?
    // https://drafts.csswg.org/css-inline-3/#typedef-text-edge

    auto firstValue = consumeIdent<CSSValueText, CSSValueCap, CSSValueEx, CSSValueIdeographic, CSSValueIdeographicInk>(range);
    if (!firstValue)
        return nullptr;

    auto secondValue = consumeIdent<CSSValueText, CSSValueAlphabetic, CSSValueIdeographic, CSSValueIdeographicInk>(range);

    // https://drafts.csswg.org/css-inline-3/#text-edges
    // "If only one value is specified, both edges are assigned that same keyword if possible; else text is assumed as the missing value."
    auto shouldSerializeSecondValue = [&]() {
        if (!secondValue)
            return false;
        if (firstValue->valueID() == CSSValueCap || firstValue->valueID() == CSSValueEx)
            return secondValue->valueID() != CSSValueText;
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

    if (range.peek().id() == CSSValueLeading)
        return consumeIdent(range);
    return consumeTextEdge(range);
}

RefPtr<CSSValue> consumeTextBoxEdge(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    // <'text-box-edge'> = auto | <text-edge>
    // https://drafts.csswg.org/css-inline-3/#propdef-text-box-edge

    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);
    return consumeTextEdge(range);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
