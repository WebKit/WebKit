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
#include "CSSPropertyParserConsumer+Length.h"
#include "CSSPropertyParserConsumer+LengthDefinitions.h"

#include "CSSCalcSymbolTable.h"
#include "CSSCalcSymbolsAllowed.h"
#include "CSSCalcValue.h"
#include "CSSParserContext.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+PercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserHelpers.h"
#include "CalculationCategory.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

std::optional<CSS::LengthRaw> validatedRange(CSS::LengthRaw value, CSSPropertyParserOptions options)
{
    if (options.valueRange == ValueRange::NonNegative && value.value < 0)
        return std::nullopt;
    if (std::isinf(value.value))
        return std::nullopt;
    return value;
}

std::optional<CSS::UnevaluatedCalc<CSS::LengthRaw>> LengthKnownTokenTypeFunctionConsumer::consume(CSSParserTokenRange& range, const CSSParserContext& context, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options)
{
    ASSERT(range.peek().type() == FunctionToken);

    auto rangeCopy = range;
    if (RefPtr value = CSSCalcValue::parse(rangeCopy, context, Calculation::Category::Length, WTFMove(symbolsAllowed), options)) {
        range = rangeCopy;
        return {{ value.releaseNonNull() }};
    }
    return std::nullopt;
}

std::optional<CSS::LengthRaw> LengthKnownTokenTypeDimensionConsumer::consume(CSSParserTokenRange& range, const CSSParserContext&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
{
    ASSERT(range.peek().type() == DimensionToken);

    auto& token = range.peek();

    auto unitType = token.unitType();
    switch (unitType) {
    case CSSUnitType::CSS_QUIRKY_EM:
        if (!isUASheetBehavior(options.parserMode))
            return std::nullopt;
        FALLTHROUGH;
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_RLH:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VB:
    case CSSUnitType::CSS_VI:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_DVW:
    case CSSUnitType::CSS_DVH:
    case CSSUnitType::CSS_DVMIN:
    case CSSUnitType::CSS_DVMAX:
    case CSSUnitType::CSS_DVB:
    case CSSUnitType::CSS_DVI:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQMAX:
        break;
    default:
        return std::nullopt;
    }

    if (auto validatedValue = validatedRange(CSS::LengthRaw { unitType, token.numericValue() }, options)) {
        range.consumeIncludingWhitespace();
        return validatedValue;
    }
    return std::nullopt;
}

std::optional<CSS::LengthRaw> LengthKnownTokenTypeNumberConsumer::consume(CSSParserTokenRange& range, const CSSParserContext&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
{
    ASSERT(range.peek().type() == NumberToken);

    auto numericValue = range.peek().numericValue();

    if (!shouldAcceptUnitlessValue(numericValue, options))
        return std::nullopt;

    if (auto validatedValue = validatedRange(CSS::LengthRaw { CSSUnitType::CSS_PX, numericValue }, options)) {
        range.consumeIncludingWhitespace();
        return validatedValue;
    }
    return std::nullopt;
}


// MARK: - Consumer functions
RefPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange& range, const CSSParserContext& context, ValueRange valueRange, UnitlessQuirk unitless)
{
    const auto options = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .valueRange = valueRange,
        .unitless = unitless,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };
    return CSSPrimitiveValueResolver<CSS::Length>::consumeAndResolve(range, context, { }, { }, options);
}

RefPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange& range, const CSSParserContext& context, CSSParserMode overrideParserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const auto options = CSSPropertyParserOptions {
        .parserMode = overrideParserMode,
        .valueRange = valueRange,
        .unitless = unitless,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };
    return CSSPrimitiveValueResolver<CSS::Length>::consumeAndResolve(range, context, { }, { }, options);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
