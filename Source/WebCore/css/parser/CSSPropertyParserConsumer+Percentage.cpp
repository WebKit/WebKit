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
#include "CSSPropertyParserConsumer+Percentage.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"

#include "CSSCalcSymbolTable.h"
#include "CSSCalcSymbolsAllowed.h"
#include "CSSCalcValue.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CalculationCategory.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

std::optional<CSS::PercentageRaw> validatedRange(CSS::PercentageRaw value, CSSPropertyParserOptions options)
{
    if (options.valueRange == ValueRange::NonNegative && value.value < 0)
        return std::nullopt;
    if (std::isinf(value.value))
        return std::nullopt;
    return value;
}

std::optional<CSS::UnevaluatedCalc<CSS::PercentageRaw>> PercentageKnownTokenTypeFunctionConsumer::consume(CSSParserTokenRange& range, const CSSParserContext& context, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options)
{
    ASSERT(range.peek().type() == FunctionToken);

    auto rangeCopy = range;
    if (RefPtr value = CSSCalcValue::parse(rangeCopy, context, Calculation::Category::Percentage, WTFMove(symbolsAllowed), options)) {
        range = rangeCopy;
        return {{ value.releaseNonNull() }};
    }

    return std::nullopt;
}

std::optional<CSS::PercentageRaw> PercentageKnownTokenTypePercentConsumer::consume(CSSParserTokenRange& range, const CSSParserContext&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
{
    ASSERT(range.peek().type() == PercentageToken);

    if (auto validatedValue = validatedRange(CSS::PercentageRaw { range.peek().numericValue() }, options)) {
        range.consumeIncludingWhitespace();
        return validatedValue;
    }
    return std::nullopt;
}

// MARK: - Consumer functions

RefPtr<CSSPrimitiveValue> consumePercentage(CSSParserTokenRange& range, const CSSParserContext& context, ValueRange valueRange)
{
    const auto options = CSSPropertyParserOptions {
        .valueRange = valueRange
    };
    return CSSPrimitiveValueResolver<CSS::Percentage>::consumeAndResolve(range, context, { }, { }, options);
}

RefPtr<CSSPrimitiveValue> consumePercentageOrNumber(CSSParserTokenRange& range, const CSSParserContext& context, ValueRange valueRange)
{
    const auto options = CSSPropertyParserOptions {
        .valueRange = valueRange
    };
    return CSSPrimitiveValueResolver<CSS::Percentage, CSS::Number>::consumeAndResolve(range, context, { }, { }, options);
}

RefPtr<CSSPrimitiveValue> consumePercentageDividedBy100OrNumber(CSSParserTokenRange& range, const CSSParserContext& context, ValueRange valueRange)
{
    auto& token = range.peek();

    const auto options = CSSPropertyParserOptions {
        .valueRange = valueRange
    };

    switch (token.type()) {
    case FunctionToken:
        if (auto value = NumberKnownTokenTypeFunctionConsumer::consume(range, context, { }, options))
            return CSSPrimitiveValueResolver<CSS::Number>::resolve(*value, { }, options);
        if (auto value = PercentageKnownTokenTypeFunctionConsumer::consume(range, context, { }, options))
            return CSSPrimitiveValueResolver<CSS::Percentage>::resolve(*value, { }, options);
        break;

    case NumberToken:
        if (auto value = NumberKnownTokenTypeNumberConsumer::consume(range, context, { }, options))
            return CSSPrimitiveValueResolver<CSS::Number>::resolve(*value, { }, options);
        break;

    case PercentageToken:
        if (auto value = PercentageKnownTokenTypePercentConsumer::consume(range, context, { }, options))
            return CSSPrimitiveValue::create(value->value / 100.0);
        break;

    default:
        break;
    }

    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
