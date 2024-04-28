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

#include "CSSCalcParser.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: Number (Raw)

std::optional<NumberRaw> validatedNumberRaw(double value, ValueRange valueRange)
{
    if (valueRange == ValueRange::NonNegative && value < 0)
        return std::nullopt;
    return {{ value }};
}

std::optional<NumberRaw> NumberRawKnownTokenTypeFunctionConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == FunctionToken);

    auto rangeCopy = range;
    if (RefPtr value = consumeCalcRawWithKnownTokenTypeFunction(rangeCopy, CalculationCategory::Number, symbolTable, valueRange)) {
        if (auto validatedValue = validatedNumberRaw(value->doubleValue(), valueRange)) {
            range = rangeCopy;
            return validatedValue;
        }
    }

    return std::nullopt;
}

std::optional<NumberRaw> NumberRawKnownTokenTypeNumberConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == NumberToken);

    if (auto validatedValue = validatedNumberRaw(range.peek().numericValue(), valueRange)) {
        range.consumeIncludingWhitespace();
        return validatedValue;
    }
    return std::nullopt;
}

std::optional<NumberRaw> NumberRawKnownTokenTypeIdentConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == IdentToken);

    if (auto variable = symbolTable.get(range.peek().id())) {
        switch (variable->type) {
        case CSSUnitType::CSS_NUMBER:
            if (auto validatedValue = validatedNumberRaw(variable->value, valueRange)) {
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

// MARK: Number (CSSPrimitiveValue - maintaining calc)

RefPtr<CSSPrimitiveValue> NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == FunctionToken);

    CalcParser parser(range, CalculationCategory::Number, valueRange, symbolTable);
    return parser.consumeValueIfCategory(CalculationCategory::Number);
}

RefPtr<CSSPrimitiveValue> NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == NumberToken);

    auto token = range.peek();

    if (auto validatedValue = validatedNumberRaw(token.numericValue(), valueRange)) {
        auto unitType = token.unitType();
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(validatedValue->value, unitType);
    }
    return nullptr;
}

// MARK: - Consumer functions

std::optional<NumberRaw> consumeNumberRaw(CSSParserTokenRange& range, ValueRange valueRange)
{
    return consumeMetaConsumer<NumberRawConsumer<RawIdentityTransformer<NumberRaw>>>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

RefPtr<CSSPrimitiveValue> consumeNumber(CSSParserTokenRange& range, ValueRange valueRange)
{
    return consumeMetaConsumer<NumberConsumer>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
