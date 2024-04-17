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

#pragma once

#include "CSSCalcParser.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSParserMode.h"
#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Meta.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CalculationCategory.h"
#include "Length.h"
#include <limits>
#include <optional>
#include <wtf/RefPtr.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: - Primitive value consumers for callers that know the token type.

// MARK: Integer (Raw)

constexpr double computeMinimumValue(IntegerValueRange range)
{
    switch (range) {
    case IntegerValueRange::All:
        return -std::numeric_limits<double>::infinity();
    case IntegerValueRange::NonNegative:
        return 0.0;
    case IntegerValueRange::Positive:
        return 1.0;
    }

    RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();

    return 0.0;
}

// MARK: Integer (Raw)

template<typename IntType, IntegerValueRange integerRange>
struct IntegerTypeRawKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static std::optional<IntType> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == FunctionToken);

        auto rangeCopy = range;
        if (RefPtr value = consumeCalcRawWithKnownTokenTypeFunction(rangeCopy, CalculationCategory::Number, { }, ValueRange::All)) {
            range = rangeCopy;
            // https://drafts.csswg.org/css-values-4/#integers
            // Rounding to the nearest integer requires rounding in the direction of +∞ when the fractional portion is exactly 0.5.
            return clampTo<IntType>(std::floor(std::max(value->doubleValue(), computeMinimumValue(integerRange)) + 0.5));
        }

        return std::nullopt;
    }
};

template<typename IntType, IntegerValueRange integerRange>
struct IntegerTypeRawKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static std::optional<IntType> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk)
    {
        ASSERT(range.peek().type() == NumberToken);

        if (range.peek().numericValueType() == NumberValueType || range.peek().numericValue() < computeMinimumValue(integerRange))
            return std::nullopt;
        return clampTo<IntType>(range.consumeIncludingWhitespace().numericValue());
    }
};

// MARK: Integer (CSSPrimitiveValue - maintaining calc)

template<typename IntType, IntegerValueRange integerRange>
struct IntegerTypeKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
    {
        ASSERT(range.peek().type() == FunctionToken);

        if (auto integer = IntegerTypeRawKnownTokenTypeFunctionConsumer<IntType, integerRange>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
            return CSSPrimitiveValue::createInteger(*integer);
        return nullptr;
    }
};

template<typename IntType, IntegerValueRange integerRange>
struct IntegerTypeKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
    {
        ASSERT(range.peek().type() == NumberToken);

        if (auto integer = IntegerTypeRawKnownTokenTypeNumberConsumer<IntType, integerRange>::consume(range, symbolTable, valueRange, parserMode, unitless, unitlessZero))
            return CSSPrimitiveValue::createInteger(*integer);
        return nullptr;
    }
};

// MARK: - Consumer definitions.

template<typename IntType, IntegerValueRange integerRange>
struct IntegerTypeRawConsumer {
    using Result = std::optional<IntType>;

    using FunctionToken = IntegerTypeRawKnownTokenTypeFunctionConsumer<IntType, integerRange>;
    using NumberToken = IntegerTypeRawKnownTokenTypeNumberConsumer<IntType, integerRange>;
};

template<typename IntType, IntegerValueRange integerRange>
struct IntegerTypeConsumer {
    using Result = RefPtr<CSSPrimitiveValue>;

    using FunctionToken = IntegerTypeKnownTokenTypeFunctionConsumer<IntType, integerRange>;
    using NumberToken = IntegerTypeKnownTokenTypeNumberConsumer<IntType, integerRange>;
};

std::optional<int> consumeIntegerRaw(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange&);
std::optional<int> consumeNonNegativeIntegerRaw(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumeNonNegativeInteger(CSSParserTokenRange&);
std::optional<unsigned> consumePositiveIntegerRaw(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumePositiveInteger(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange&, IntegerValueRange);

template<typename IntType, IntegerValueRange integerRange>
std::optional<IntType> consumeIntegerTypeRaw(CSSParserTokenRange& range)
{
    return consumeMetaConsumer<IntegerTypeRawConsumer<IntType, integerRange>>(range, { }, ValueRange::All, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

template<typename IntType, IntegerValueRange integerRange>
RefPtr<CSSPrimitiveValue> consumeIntegerType(CSSParserTokenRange& range)
{
    return consumeMetaConsumer<IntegerTypeConsumer<IntType, integerRange>>(range, { }, ValueRange::All, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
