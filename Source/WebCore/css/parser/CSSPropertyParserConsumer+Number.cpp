/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "CSSPropertyParserConsumer+Number.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"

#include "CSSCalcSymbolTable.h"
#include "CSSCalcSymbolsAllowed.h"
#include "CSSCalcValue.h"
#include "CSSParserContext.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CalculationCategory.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

std::optional<CSS::NumberRaw> validatedRange(CSS::NumberRaw value, CSSPropertyParserOptions options)
{
    if (options.valueRange == ValueRange::NonNegative && value.value < 0)
        return std::nullopt;
    return value;
}

std::optional<CSS::UnevaluatedCalc<CSS::NumberRaw>> NumberKnownTokenTypeFunctionConsumer::consume(CSSParserTokenRange& range, const CSSParserContext& context, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options)
{
    ASSERT(range.peek().type() == FunctionToken);

    auto rangeCopy = range;
    if (RefPtr value = CSSCalcValue::parse(rangeCopy, context, Calculation::Category::Number, WTFMove(symbolsAllowed), options)) {
        range = rangeCopy;
        return {{ value.releaseNonNull() }};
    }

    return std::nullopt;
}

std::optional<CSS::NumberRaw> NumberKnownTokenTypeNumberConsumer::consume(CSSParserTokenRange& range, const CSSParserContext&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
{
    ASSERT(range.peek().type() == NumberToken);

    if (auto validatedValue = validatedRange(CSS::NumberRaw { range.peek().numericValue() }, options)) {
        range.consumeIncludingWhitespace();
        return validatedValue;
    }
    return std::nullopt;
}

// MARK: - Consumer functions

RefPtr<CSSPrimitiveValue> consumeNumber(CSSParserTokenRange& range, const CSSParserContext& context, ValueRange valueRange)
{
    const auto options = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .valueRange = valueRange
    };
    return CSSPrimitiveValueResolver<CSS::Number>::consumeAndResolve(range, context, { }, { }, options);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
