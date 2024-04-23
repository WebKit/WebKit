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

#include "CSSCalcParser.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: Length (raw)

static std::optional<double> validatedLengthRaw(double value, ValueRange valueRange)
{
    if (valueRange == ValueRange::NonNegative && value < 0)
        return std::nullopt;
    if (std::isinf(value))
        return std::nullopt;
    return value;
}

std::optional<LengthRaw> LengthRawKnownTokenTypeFunctionConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == FunctionToken);

    auto rangeCopy = range;
    if (RefPtr value = consumeCalcRawWithKnownTokenTypeFunction(rangeCopy, CalculationCategory::Length, symbolTable, valueRange)) {
        range = rangeCopy;

        // FIXME: Should this validate the calc value as is done for the NumberRaw variant?
        return { { value->primitiveType(), value->doubleValue() } };
    }
    return std::nullopt;
}

std::optional<LengthRaw> LengthRawKnownTokenTypeDimensionConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == DimensionToken);

    auto& token = range.peek();

    auto unitType = token.unitType();
    switch (unitType) {
    case CSSUnitType::CSS_QUIRKY_EM:
        if (!isUASheetBehavior(parserMode))
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

    if (auto validatedValue = validatedLengthRaw(token.numericValue(), valueRange)) {
        range.consumeIncludingWhitespace();
        return { { unitType, *validatedValue } };
    }
    return std::nullopt;
}

std::optional<LengthRaw> LengthRawKnownTokenTypeNumberConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    ASSERT(range.peek().type() == NumberToken);

    auto& token = range.peek();

    if (!shouldAcceptUnitlessValue(token.numericValue(), parserMode, unitless, unitlessZero))
        return std::nullopt;

    if (auto validatedValue = validatedLengthRaw(token.numericValue(), valueRange)) {
        range.consumeIncludingWhitespace();
        return { { CSSUnitType::CSS_PX, *validatedValue } };
    }
    return std::nullopt;
}

// MARK: Length (CSSPrimitiveValue - maintaining calc)

RefPtr<CSSPrimitiveValue> LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == FunctionToken);

    CalcParser parser(range, CalculationCategory::Length, valueRange, symbolTable);
    return parser.consumeValueIfCategory(CalculationCategory::Length);
}

RefPtr<CSSPrimitiveValue> LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    ASSERT(range.peek().type() == DimensionToken);

    if (auto lengthRaw = LengthRawKnownTokenTypeDimensionConsumer::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
        return CSSPrimitiveValue::create(lengthRaw->value, lengthRaw->type);
    return nullptr;
}

RefPtr<CSSPrimitiveValue> LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    ASSERT(range.peek().type() == NumberToken);

    if (auto lengthRaw = LengthRawKnownTokenTypeNumberConsumer::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
        return CSSPrimitiveValue::create(lengthRaw->value, lengthRaw->type);
    return nullptr;
}

// MARK: - Consumer functions

RefPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    return consumeMetaConsumer<LengthConsumer>(range, { }, valueRange, parserMode, unitless, UnitlessZeroQuirk::Allow);
}

std::optional<LengthOrPercentRaw> consumeLengthOrPercentRaw(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    return consumeMetaConsumer<LengthOrPercentRawConsumer<RawIdentityTransformer<LengthOrPercentRaw>>>(range, { }, ValueRange::NonNegative, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

// FIXME: This doesn't work with the current scheme due to the NegativePercentagePolicy parameter
RefPtr<CSSPrimitiveValue> consumeLengthOrPercent(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero, NegativePercentagePolicy negativePercentagePolicy)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken: {
        // FIXME: Should this be using trying to generate the calc with both Length and Percent destination category types?
        CalcParser parser(range, CalculationCategory::Length, valueRange, { }, negativePercentagePolicy);
        if (auto calculation = parser.value(); calculation && canConsumeCalcValue(calculation->category(), parserMode))
            return parser.consumeValue();
        break;
    }

    case DimensionToken:
        return LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer::consume(range, { }, valueRange, parserMode, unitless, unitlessZero);

    case NumberToken:
        return LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer::consume(range, { }, valueRange, parserMode, unitless, unitlessZero);

    case PercentageToken:
        return PercentCSSPrimitiveValueWithCalcWithKnownTokenTypePercentConsumer::consume(range, { }, valueRange, parserMode, unitless, unitlessZero);

    default:
        break;
    }

    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
