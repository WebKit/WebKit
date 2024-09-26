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
#include "CSSPropertyParserConsumer+AnglePercentage.h"
#include "CSSPropertyParserConsumer+AnglePercentageDefinitions.h"

#include "CSSCalcSymbolTable.h"
#include "CSSCalcSymbolsAllowed.h"
#include "CSSCalcValue.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserHelpers.h"
#include "CalculationCategory.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

std::optional<CSS::AnglePercentageRaw> validatedRange(CSS::AnglePercentageRaw value, CSSPropertyParserOptions options)
{
    if (options.valueRange == ValueRange::NonNegative && value.value < 0)
        return std::nullopt;
    if (std::isinf(value.value))
        return std::nullopt;
    return value;
}

std::optional<CSS::UnevaluatedCalc<CSS::AnglePercentageRaw>> AnglePercentageKnownTokenTypeFunctionConsumer::consume(CSSParserTokenRange& range, const CSSParserContext& context, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options)
{
    ASSERT(range.peek().type() == FunctionToken);

    auto rangeCopy = range;
    if (RefPtr value = CSSCalcValue::parse(rangeCopy, context, Calculation::Category::AnglePercentage, WTFMove(symbolsAllowed), options)) {
        range = rangeCopy;
        return {{ value.releaseNonNull() }};
    }
    return std::nullopt;
}

std::optional<CSS::AnglePercentageRaw> AnglePercentageKnownTokenTypeDimensionConsumer::consume(CSSParserTokenRange& range, const CSSParserContext&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
{
    ASSERT(range.peek().type() == DimensionToken);

    auto& token = range.peek();

    auto unitType = token.unitType();
    switch (unitType) {
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_TURN:
        break;
    default:
        return std::nullopt;
    }

    if (auto validatedValue = validatedRange(CSS::AnglePercentageRaw { unitType, token.numericValue() }, options)) {
        range.consumeIncludingWhitespace();
        return validatedValue;
    }

    return std::nullopt;
}

std::optional<CSS::AnglePercentageRaw> AnglePercentageKnownTokenTypePercentConsumer::consume(CSSParserTokenRange& range, const CSSParserContext&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
{
    ASSERT(range.peek().type() == PercentageToken);

    if (auto validatedValue = validatedRange(CSS::AnglePercentageRaw { CSSUnitType::CSS_PERCENTAGE, range.peek().numericValue() }, options)) {
        range.consumeIncludingWhitespace();
        return validatedValue;
    }
    return std::nullopt;
}


std::optional<CSS::AnglePercentageRaw> AnglePercentageKnownTokenTypeNumberConsumer::consume(CSSParserTokenRange& range, const CSSParserContext&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
{
    ASSERT(range.peek().type() == NumberToken);

    auto numericValue = range.peek().numericValue();
    if (!shouldAcceptUnitlessValue(numericValue, options))
        return std::nullopt;

    if (auto validatedValue = validatedRange(CSS::AnglePercentageRaw { CSSUnitType::CSS_DEG, numericValue }, options)) {
        range.consumeIncludingWhitespace();
        return validatedValue;
    }
    return std::nullopt;
}


// MARK: - Consumer functions

RefPtr<CSSPrimitiveValue> consumeAnglePercentage(CSSParserTokenRange& range, const CSSParserContext& context, ValueRange valueRange, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, NegativePercentagePolicy negativePercentage)
{
    const auto options = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .valueRange = valueRange,
        .negativePercentage = negativePercentage,
        .unitless = unitless,
        .unitlessZero = unitlessZero
    };

    return CSSPrimitiveValueResolver<CSS::AnglePercentage>::consumeAndResolve(range, context, { }, { }, options);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
