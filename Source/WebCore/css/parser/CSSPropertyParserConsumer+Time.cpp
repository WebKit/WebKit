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
#include "CSSPropertyParserConsumer+Time.h"

#include "CSSPropertyParserConsumer+Meta.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: Time (CSSPrimitiveValue - maintaining calc)

RefPtr<CSSPrimitiveValue> TimeCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == FunctionToken);

    CalcParser parser(range, CalculationCategory::Time, valueRange, symbolTable);
    return parser.consumeValueIfCategory(CalculationCategory::Time);
}

RefPtr<CSSPrimitiveValue> TimeCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == DimensionToken);

    if (valueRange == ValueRange::NonNegative && range.peek().numericValue() < 0)
        return nullptr;

    if (auto unit = range.peek().unitType(); unit == CSSUnitType::CSS_MS || unit == CSSUnitType::CSS_S)
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), unit);

    return nullptr;
}

RefPtr<CSSPrimitiveValue> TimeCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer::consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk)
{
    ASSERT(range.peek().type() == NumberToken);

    if (unitless == UnitlessQuirk::Allow && shouldAcceptUnitlessValue(range.peek().numericValue(), parserMode, unitless, UnitlessZeroQuirk::Allow)) {
        if (valueRange == ValueRange::NonNegative && range.peek().numericValue() < 0)
            return nullptr;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), CSSUnitType::CSS_MS);
    }
    return nullptr;
}

RefPtr<CSSPrimitiveValue> consumeTime(CSSParserTokenRange& range, CSSParserMode parserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    return consumeMetaConsumer<TimeConsumer>(range, { }, valueRange, parserMode, unitless, UnitlessZeroQuirk::Forbid);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
