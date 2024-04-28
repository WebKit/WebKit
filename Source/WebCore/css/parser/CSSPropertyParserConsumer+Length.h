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

#include "CSSParserToken.h"
#include "CSSPropertyParserConsumer+Meta.h"
#include "CSSPropertyParserConsumer+Percent.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include <optional>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSPrimitiveValue;
class CSSParserTokenRange;

namespace CSSPropertyParserHelpers {

inline bool shouldAcceptUnitlessValue(double value, CSSParserMode parserMode, UnitlessQuirk unitless, UnitlessZeroQuirk unitlessZero)
{
    // FIXME: Presentational HTML attributes shouldn't use the CSS parser for lengths.

    if (!value && unitlessZero == UnitlessZeroQuirk::Allow)
        return true;

    if (isUnitlessValueParsingEnabledForMode(parserMode))
        return true;

    return parserMode == HTMLQuirksMode && unitless == UnitlessQuirk::Allow;
}

// MARK: - Primitive value consumers for callers that know the token type.

// MARK: Length (raw)

struct LengthRawKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static std::optional<LengthRaw> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

struct LengthRawKnownTokenTypeDimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;
    static std::optional<LengthRaw> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

struct LengthRawKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static std::optional<LengthRaw> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

// MARK: Length (CSSPrimitiveValue - maintaining calc)

struct LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

struct LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

struct LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

// MARK: - Consumer definitions.

// MARK: Length

template<typename T>
struct LengthRawConsumer {
    using Transformer = T;
    using Result = typename Transformer::Result;

    using FunctionToken = LengthRawKnownTokenTypeFunctionConsumer;
    using DimensionToken = LengthRawKnownTokenTypeDimensionConsumer;
    using NumberToken = LengthRawKnownTokenTypeNumberConsumer;
};

struct LengthConsumer {
    using Result = RefPtr<CSSPrimitiveValue>;

    using FunctionToken = LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer;
    using NumberToken = LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer;
    using DimensionToken = LengthCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer;
};


// MARK: - Combination consumer definitions.

// MARK: Length + Percent

template<typename T>
struct LengthOrPercentRawConsumer {
    using Transformer = T;
    using Result = typename Transformer::Result;

    using FunctionToken = SameTokenMetaConsumer<
        Transformer,
        PercentRawKnownTokenTypeFunctionConsumer,
        LengthRawKnownTokenTypeFunctionConsumer
    >;
    using NumberToken = LengthRawKnownTokenTypeNumberConsumer;
    using PercentageToken = PercentRawKnownTokenTypePercentConsumer;
    using DimensionToken = LengthRawKnownTokenTypeDimensionConsumer;
};

// MARK: - Consumer functions

// MARK: - Length
RefPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange&, CSSParserMode, ValueRange, UnitlessQuirk = UnitlessQuirk::Forbid);

// MARK: - Length or Percent
std::optional<LengthOrPercentRaw> consumeLengthOrPercentRaw(CSSParserTokenRange&, CSSParserMode);
RefPtr<CSSPrimitiveValue> consumeLengthOrPercent(CSSParserTokenRange&, CSSParserMode, ValueRange, UnitlessQuirk = UnitlessQuirk::Forbid, UnitlessZeroQuirk = UnitlessZeroQuirk::Allow, NegativePercentagePolicy = NegativePercentagePolicy::Forbid);

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
