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

#include "CSSAnchorValue.h"
#include "CSSCalcParser.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcSymbolsAllowed.h"
#include "CSSCalcValue.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+PercentDefinitions.h"
#include "CSSPropertyParserConsumer+RawResolver.h"
#include "CSSPropertyParserHelpers.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

std::optional<LengthRaw> validatedRange(LengthRaw value, CSSPropertyParserOptions options)
{
    if (options.valueRange == ValueRange::NonNegative && value.value < 0)
        return std::nullopt;
    if (std::isinf(value.value))
        return std::nullopt;
    return value;
}

std::optional<UnevaluatedCalc<LengthRaw>> LengthKnownTokenTypeFunctionConsumer::consume(CSSParserTokenRange& range, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options)
{
    ASSERT(range.peek().type() == FunctionToken);

    auto rangeCopy = range;
    if (RefPtr value = consumeCalcRawWithKnownTokenTypeFunction(rangeCopy, CalculationCategory::Length, WTFMove(symbolsAllowed), options)) {
        range = rangeCopy;
        return {{ value.releaseNonNull() }};
    }
    return std::nullopt;
}

std::optional<LengthRaw> LengthKnownTokenTypeDimensionConsumer::consume(CSSParserTokenRange& range, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
{
    ASSERT(range.peek().type() == DimensionToken);

    auto& token = range.peek();

    auto unitType = token.unitType();
    switch (unitType) {
    case CSSUnitType::QuirkyEm:
        if (!isUASheetBehavior(options.parserMode))
            return std::nullopt;
        FALLTHROUGH;
    case CSSUnitType::Em:
    case CSSUnitType::RootEm:
    case CSSUnitType::LineHeight:
    case CSSUnitType::RootLineHeight:
    case CSSUnitType::CapHeight:
    case CSSUnitType::RootCapHeight:
    case CSSUnitType::CharacterWidth:
    case CSSUnitType::RootCharacterWidth:
    case CSSUnitType::IdeographicCharacter:
    case CSSUnitType::RootIdeographicCharacter:
    case CSSUnitType::Ex:
    case CSSUnitType::RootEx:
    case CSSUnitType::Pixel:
    case CSSUnitType::Centimeter:
    case CSSUnitType::Millimeter:
    case CSSUnitType::Inch:
    case CSSUnitType::Point:
    case CSSUnitType::Pica:
    case CSSUnitType::ViewportWidth:
    case CSSUnitType::ViewportHeight:
    case CSSUnitType::ViewportMinimum:
    case CSSUnitType::ViewportMaximum:
    case CSSUnitType::ViewportBlock:
    case CSSUnitType::ViewportInline:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportMinimum:
    case CSSUnitType::SmallViewportMaximum:
    case CSSUnitType::SmallViewportBlock:
    case CSSUnitType::SmallViewportInline:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportMinimum:
    case CSSUnitType::LargeViewportMaximum:
    case CSSUnitType::LargeViewportBlock:
    case CSSUnitType::LargeViewportInline:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportMinimum:
    case CSSUnitType::DynamicViewportMaximum:
    case CSSUnitType::DynamicViewportBlock:
    case CSSUnitType::DynamicViewportInline:
    case CSSUnitType::Quarter:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInline:
    case CSSUnitType::ContainerQueryBlock:
    case CSSUnitType::ContainerQueryMinimum:
    case CSSUnitType::ContainerQueryMaximum:
        break;
    default:
        return std::nullopt;
    }

    if (auto validatedValue = validatedRange(LengthRaw { unitType, token.numericValue() }, options)) {
        range.consumeIncludingWhitespace();
        return validatedValue;
    }
    return std::nullopt;
}

std::optional<LengthRaw> LengthKnownTokenTypeNumberConsumer::consume(CSSParserTokenRange& range, CSSCalcSymbolsAllowed, CSSPropertyParserOptions options)
{
    ASSERT(range.peek().type() == NumberToken);

    auto numericValue = range.peek().numericValue();

    if (!shouldAcceptUnitlessValue(numericValue, options))
        return std::nullopt;

    if (auto validatedValue = validatedRange(LengthRaw { CSSUnitType::Pixel, numericValue }, options)) {
        range.consumeIncludingWhitespace();
        return validatedValue;
    }
    return std::nullopt;
}


// MARK: - Consumer functions

RefPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const auto options = CSSPropertyParserOptions {
        .parserMode = parserMode,
        .valueRange = valueRange,
        .unitless = unitless,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };
    return CSSPrimitiveValueResolver<LengthRaw>::consumeAndResolve(range, { }, { }, options);
}

std::optional<LengthOrPercentRaw> consumeLengthOrPercentRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    const auto options = CSSPropertyParserOptions {
        .parserMode = parserMode,
        .valueRange = ValueRange::NonNegative
    };
    return RawResolver<LengthRaw, PercentRaw>::consumeAndResolve(range, { }, { }, options);
}

// FIXME: This doesn't work with the current scheme due to the NegativePercentagePolicy parameter
RefPtr<CSSPrimitiveValue> consumeLengthOrPercent(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, NegativePercentagePolicy negativePercentage, AnchorPolicy anchorPolicy)
{
    auto& token = range.peek();

    const auto options = CSSPropertyParserOptions {
        .parserMode = parserMode,
        .valueRange = valueRange,
        .anchorPolicy = anchorPolicy,
        .negativePercentage = negativePercentage,
        .unitless = unitless,
        .unitlessZero = unitlessZero
    };

    switch (token.type()) {
    case FunctionToken: {
        if (range.peek().functionId() == CSSValueAnchor) {
            if (anchorPolicy == AnchorPolicy::Allow)
                return consumeAnchor(range, parserMode);
            return nullptr;
        }

        // FIXME: Should this be using trying to generate the calc with both Length and Percent destination category types?
        CalcParser parser(range, CalculationCategory::Length, { }, options);
        if (auto calculation = parser.value(); calculation && canConsumeCalcValue(calculation->category(), options))
            return parser.consumeValue();
        break;
    }

    case DimensionToken:
        if (auto value = LengthKnownTokenTypeDimensionConsumer::consume(range, { }, options))
            return CSSPrimitiveValueResolver<LengthRaw>::resolve(*value, { }, options);
        break;

    case NumberToken:
        if (auto value = LengthKnownTokenTypeNumberConsumer::consume(range, { }, options))
            return CSSPrimitiveValueResolver<LengthRaw>::resolve(*value, { }, options);
        break;

    case PercentageToken:
        if (auto value = PercentKnownTokenTypePercentConsumer::consume(range, { }, options))
            return CSSPrimitiveValueResolver<PercentRaw>::resolve(*value, { }, options);
        break;

    default:
        break;
    }

    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
