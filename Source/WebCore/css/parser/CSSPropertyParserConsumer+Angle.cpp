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

#include "CSSCalcParser.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

static inline bool shouldAcceptUnitlessValue(double value, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    // FIXME: Presentational HTML attributes shouldn't use the CSS parser for lengths.

    if (!value && unitlessZero == UnitlessZeroQuirk::Allow)
        return true;

    if (isUnitlessValueParsingEnabledForMode(parserMode))
        return true;

    return parserMode == HTMLQuirksMode && unitless == UnitlessQuirk::Allow;
}

// MARK: Angle (raw)

std::optional<AngleRaw> AngleRawKnownTokenTypeFunctionConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    auto rangeCopy = range;
    if (RefPtr value = consumeCalcRawWithKnownTokenTypeFunction(rangeCopy, CalculationCategory::Angle, symbolTable, valueRange)) {
        range = rangeCopy;
        return { { value->primitiveType(), value->doubleValue() } };
    }
    return std::nullopt;
}

std::optional<AngleRaw> AngleRawKnownTokenTypeDimensionConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == DimensionToken);

    auto unitType = range.peek().unitType();
    switch (unitType) {
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_TURN:
        return { { unitType, range.consumeIncludingWhitespace().numericValue() } };
    default:
        break;
    }

    return std::nullopt;
}

std::optional<AngleRaw> AngleRawKnownTokenTypeNumberConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    ASSERT(range.peek().type() == NumberToken);

    if (shouldAcceptUnitlessValue(range.peek().numericValue(), parserMode, unitless, unitlessZero))
        return { { CSSUnitType::CSS_DEG, range.consumeIncludingWhitespace().numericValue() } };
    return std::nullopt;
}

// MARK: Angle (CSSPrimitiveValue - maintaining calc)

RefPtr<CSSPrimitiveValue> AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == FunctionToken);

    CalcParser parser(range, CalculationCategory::Angle, valueRange, symbolTable);
    return parser.consumeValueIfCategory(CalculationCategory::Angle);
}

RefPtr<CSSPrimitiveValue> AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    ASSERT(range.peek().type() == DimensionToken);

    if (auto angleRaw = AngleRawKnownTokenTypeDimensionConsumer::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
        return CSSPrimitiveValue::create(angleRaw->value, angleRaw->type);
    return nullptr;
}

RefPtr<CSSPrimitiveValue> AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    ASSERT(range.peek().type() == NumberToken);

    if (auto angleRaw = AngleRawKnownTokenTypeNumberConsumer::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
        return CSSPrimitiveValue::create(angleRaw->value, angleRaw->type);
    return nullptr;
}

// MARK: Specialized combination consumers.

std::optional<AngleOrNumberRaw> AngleOrNumberRawKnownTokenTypeIdentConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == IdentToken);

    if (auto variable = symbolTable.get(range.peek().id())) {
        switch (variable->type) {
        case CSSUnitType::CSS_DEG:
        case CSSUnitType::CSS_RAD:
        case CSSUnitType::CSS_GRAD:
        case CSSUnitType::CSS_TURN:
            range.consumeIncludingWhitespace();
            return AngleRaw { variable->type, variable->value };

        case CSSUnitType::CSS_NUMBER:
            range.consumeIncludingWhitespace();
            return NumberRaw { variable->value };

        default:
            break;
        }
    }

    return std::nullopt;
}


// MARK: - Consumer functions

std::optional<AngleRaw> consumeAngleRaw(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    return consumeMetaConsumer<AngleRawConsumer<RawIdentityTransformer<AngleRaw>>>(range, { }, ValueRange::All, parserMode, unitless, unitlessZero);
}

RefPtr<CSSPrimitiveValue> consumeAngle(CSSParserTokenRange& range, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    return consumeMetaConsumer<AngleConsumer>(range, { }, ValueRange::All, parserMode, unitless, unitlessZero);
}

RefPtr<CSSPrimitiveValue> consumeAngleOrPercent(CSSParserTokenRange& range, CSSParserMode parserMode)
{
    return consumeMetaConsumer<AngleOrPercentConsumer>(range, { }, ValueRange::All, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Allow);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
