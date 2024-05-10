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
#include "CSSPropertyParserConsumer+Angle.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"

#include "CSSCalcParser.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+PercentDefinitions.h"
#include "CSSPropertyParserConsumer+RawResolver.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

std::optional<AngleRaw> validatedRange(AngleRaw value, CSSPropertyParserOptions options)
{
    if (options.valueRange == ValueRange::NonNegative && value.value < 0)
        return std::nullopt;
    return value;
}

std::optional<UnevaluatedCalc<AngleRaw>> AngleKnownTokenTypeFunctionConsumer::consume(CSSParserTokenRange& range, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options)
{
    auto rangeCopy = range;
    if (RefPtr value = consumeCalcRawWithKnownTokenTypeFunction(rangeCopy, CalculationCategory::Angle, WTFMove(symbolsAllowed), options)) {
        range = rangeCopy;
        return {{ value.releaseNonNull() }};
    }
    return std::nullopt;
}

std::optional<AngleRaw> AngleKnownTokenTypeDimensionConsumer::consume(CSSParserTokenRange& range, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
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

    if (auto validatedValue = validatedRange(AngleRaw { unitType, token.numericValue() }, options)) {
        range.consumeIncludingWhitespace();
        return validatedValue;
    }

    return std::nullopt;
}

std::optional<AngleRaw> AngleKnownTokenTypeNumberConsumer::consume(CSSParserTokenRange& range, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
{
    ASSERT(range.peek().type() == NumberToken);

    auto numericValue = range.peek().numericValue();
    if (!shouldAcceptUnitlessValue(numericValue, options))
        return std::nullopt;

    if (auto validatedValue = validatedRange(AngleRaw { CSSUnitType::CSS_DEG, numericValue }, options)) {
        range.consumeIncludingWhitespace();
        return validatedValue;
    }

    return std::nullopt;
}

// MARK: - Consumer functions

std::optional<AngleRaw> consumeAngleRaw(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    const auto options = CSSPropertyParserOptions {
        .parserMode = parserMode,
        .unitless = unitless,
        .unitlessZero = unitlessZero
    };
    return RawResolver<AngleRaw>::consumeAndResolve(range, { }, { }, options);
}

RefPtr<CSSPrimitiveValue> consumeAngle(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    const auto options = CSSPropertyParserOptions {
        .parserMode = parserMode,
        .unitless = unitless,
        .unitlessZero = unitlessZero
    };
    return CSSPrimitiveValueResolver<AngleRaw>::consumeAndResolve(range, { }, { }, options);
}

RefPtr<CSSPrimitiveValue> consumeAngleOrPercent(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    const auto options = CSSPropertyParserOptions {
        .parserMode = parserMode,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };
    return CSSPrimitiveValueResolver<AngleRaw, PercentRaw>::consumeAndResolve(range, { }, { }, options);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
