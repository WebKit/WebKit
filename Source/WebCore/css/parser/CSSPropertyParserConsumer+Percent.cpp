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
#include "CSSPropertyParserConsumer+Percent.h"

#include "CSSCalcParser.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSPropertyParserConsumer+Number.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: Percent (raw)

std::optional<PercentRaw> validatedPercentRaw(double value, ValueRange valueRange)
{
    if (valueRange == ValueRange::NonNegative && value < 0)
        return std::nullopt;
    if (std::isinf(value))
        return std::nullopt;
    return {{ value }};
}

std::optional<PercentRaw> PercentRawKnownTokenTypeFunctionConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == FunctionToken);

    auto rangeCopy = range;
    if (RefPtr value = consumeCalcRawWithKnownTokenTypeFunction(rangeCopy, CalculationCategory::Percent, symbolTable, valueRange)) {
        range = rangeCopy;

        // FIXME: Should this validate the calc value as is done for the NumberRaw variant?
        return {{ value->doubleValue() }};
    }

    return std::nullopt;
}

std::optional<PercentRaw> PercentRawKnownTokenTypePercentConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == PercentageToken);

    if (auto validatedValue = validatedPercentRaw(range.peek().numericValue(), valueRange)) {
        range.consumeIncludingWhitespace();
        return validatedValue;
    }
    return std::nullopt;
}

std::optional<PercentRaw> PercentRawKnownTokenTypeIdentConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == IdentToken);

    if (auto variable = symbolTable.get(range.peek().id())) {
        switch (variable->type) {
        case CSSUnitType::CSS_PERCENTAGE:
            if (auto validatedValue = validatedPercentRaw(variable->value, valueRange)) {
                range.consumeIncludingWhitespace();
                return validatedValue;
            }
            break;

        default:
            break;
        }
    }
    return std::nullopt;
}

// MARK: Percent (CSSPrimitiveValue - maintaining calc)

RefPtr<CSSPrimitiveValue> PercentCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == FunctionToken);

    CalcParser parser(range, CalculationCategory::Percent, valueRange, symbolTable);
    return parser.consumeValueIfCategory(CalculationCategory::Percent);
}

RefPtr<CSSPrimitiveValue> PercentCSSPrimitiveValueWithCalcWithKnownTokenTypePercentConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == PercentageToken);

    if (auto validatedValue = validatedPercentRaw(range.peek().numericValue(), valueRange)) {
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(validatedValue->value, CSSUnitType::CSS_PERCENTAGE);
    }
    return nullptr;
}

// MARK: Specialized combination consumers.

std::optional<NumberOrPercentRaw> NumberOrPercentRawKnownTokenTypeIdentConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == IdentToken);

    if (auto variable = symbolTable.get(range.peek().id())) {
        switch (variable->type) {
        case CSSUnitType::CSS_PERCENTAGE:
            if (auto validatedValue = validatedPercentRaw(variable->value, valueRange)) {
                range.consumeIncludingWhitespace();
                return {{ *validatedValue }};
            }
            break;

        case CSSUnitType::CSS_NUMBER:
            if (auto validatedValue = validatedNumberRaw(variable->value, valueRange)) {
                range.consumeIncludingWhitespace();
                return {{ *validatedValue }};
            }
            break;

        default:
            break;
        }
    }

    return std::nullopt;
}


// MARK: - Consumer functions

std::optional<PercentRaw> consumePercentRaw(CSSParserTokenRange& range)
{
    return consumeMetaConsumer<PercentRawConsumer<RawIdentityTransformer<PercentRaw>>>(range, { }, ValueRange::All, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

RefPtr<CSSPrimitiveValue> consumePercent(CSSParserTokenRange& range, ValueRange valueRange)
{
    return consumeMetaConsumer<PercentConsumer>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

// FIXME: This needs a more clear name to indicate its behavior of dividing percents by 100 only if an explicit percent token, not if a result of a calc().
RefPtr<CSSPrimitiveValue> consumeNumberOrPercent(CSSParserTokenRange& range, ValueRange valueRange)
{
    auto& token = range.peek();

    switch (token.type()) {
    case FunctionToken:
        if (RefPtr value = NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer::consume(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid))
            return value;
        return PercentCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer::consume(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);

    case NumberToken:
        return NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer::consume(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);

    case PercentageToken:
        if (auto percentRaw = PercentRawKnownTokenTypePercentConsumer::consume(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid))
            return CSSPrimitiveValue::create(percentRaw->value / 100.0);
        break;

    default:
        break;
    }

    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
