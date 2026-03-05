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
#include "CSSPropertyParserConsumer+Ratio.h"

#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserState.h"
#include "CSSRatioValue.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

std::optional<CSS::Ratio> consumeUnresolvedRatio(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <ratio> = <number [0,∞]> [ / <number [0,∞]> ]?
    // https://drafts.csswg.org/css-values-4/#ratio-value

    auto rangeCopy = range;

    auto numerator = MetaConsumer<CSS::Number<CSS::Nonnegative>>::consume(rangeCopy, state);
    if (!numerator)
        return { };

    if (!CSSPropertyParserHelpers::consumeSlashIncludingWhitespace(rangeCopy)) {
        range = rangeCopy;
        return CSS::Ratio { WTF::move(*numerator) };
    }

    auto denominator = MetaConsumer<CSS::Number<CSS::Nonnegative>>::consume(rangeCopy, state);
    if (!denominator)
        return { };

    range = rangeCopy;
    return CSS::Ratio { WTF::move(*numerator), WTF::move(*denominator) };
}

std::optional<CSS::Ratio> consumeUnresolvedRatioWithBothNumeratorAndDenominator(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <ratio> = <number [0,∞]> [ / <number [0,∞]> ]?
    // https://drafts.csswg.org/css-values-4/#ratio-value

    auto rangeCopy = range;

    auto numerator = MetaConsumer<CSS::Number<CSS::Nonnegative>>::consume(rangeCopy, state);
    if (!numerator)
        return { };

    if (!CSSPropertyParserHelpers::consumeSlashIncludingWhitespace(rangeCopy))
        return { };

    auto denominator = MetaConsumer<CSS::Number<CSS::Nonnegative>>::consume(rangeCopy, state);
    if (!denominator)
        return { };

    range = rangeCopy;
    return CSS::Ratio { WTF::move(*numerator), WTF::move(*denominator) };
}

RefPtr<CSSValue> consumeRatio(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto ratio = consumeUnresolvedRatio(range, state))
        return CSSRatioValue::create(WTF::move(*ratio));
    return nullptr;
}

RefPtr<CSSValue> consumeRatioWithBothNumeratorAndDenominator(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (auto ratio = consumeUnresolvedRatioWithBothNumeratorAndDenominator(range, state))
        return CSSRatioValue::create(WTF::move(*ratio));
    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
