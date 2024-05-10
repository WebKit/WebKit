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
#include "CSSPropertyParserConsumer+None.h"
#include "CSSPropertyParserConsumer+Number.h"
#include "CSSPropertyParserConsumer+Percent.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "Length.h"
#include <optional>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSPrimitiveValue;
class CSSParserTokenRange;

namespace CSSPropertyParserHelpers {

// MARK: - Primitive value consumers for callers that know the token type.

// MARK: Angle (raw)

struct AngleRawKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static std::optional<AngleRaw> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

struct AngleRawKnownTokenTypeDimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;
    static std::optional<AngleRaw> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

struct AngleRawKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static std::optional<AngleRaw> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

// MARK: Angle (CSSPrimitiveValue - maintaining calc)

struct AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

struct AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

struct AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

// MARK: Specialized combination consumers.

// FIXME: It would be good to find a way to synthesize this from an angle and number specific variants.
struct AngleOrNumberRawKnownTokenTypeIdentConsumer {
    static constexpr CSSParserTokenType tokenType = IdentToken;
    static std::optional<AngleOrNumberRaw> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

// MARK: - Consumer definitions.

// MARK: Angle

template<typename T>
struct AngleRawConsumer {
    using Transformer = T;
    using Result = typename Transformer::Result;

    using FunctionToken = AngleRawKnownTokenTypeFunctionConsumer;
    using NumberToken = AngleRawKnownTokenTypeNumberConsumer;
    using DimensionToken = AngleRawKnownTokenTypeDimensionConsumer;
};

struct AngleConsumer {
    using Result = RefPtr<CSSPrimitiveValue>;

    using FunctionToken = AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer;
    using NumberToken = AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer;
    using DimensionToken = AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer;
};

// MARK: - Combination consumer definitions.

// MARK: Angle + Percent

struct AngleOrPercentConsumer {
    using Result = RefPtr<CSSPrimitiveValue>;

    using FunctionToken = SameTokenMetaConsumer<
        IdentityTransformer<RefPtr<CSSPrimitiveValue>>,
        AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer,
        PercentCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer
    >;
    using NumberToken = AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer;
    using PercentageToken = PercentCSSPrimitiveValueWithCalcWithKnownTokenTypePercentConsumer;
    using DimensionToken = AngleCSSPrimitiveValueWithCalcWithKnownTokenTypeDimensionConsumer;
};

// MARK: Angle + Number

template<typename T>
struct AngleOrNumberRawConsumer {
    using Transformer = T;
    using Result = typename Transformer::Result;

    using FunctionToken = SameTokenMetaConsumer<
        Transformer,
        NumberRawKnownTokenTypeFunctionConsumer,
        AngleRawKnownTokenTypeFunctionConsumer
    >;
    using NumberToken = NumberRawKnownTokenTypeNumberConsumer;
    using DimensionToken = AngleRawKnownTokenTypeDimensionConsumer;
};

template<typename Transformer>
struct AngleOrNumberRawAllowingSymbolTableIdentConsumer : AngleOrNumberRawConsumer<Transformer> {
    using IdentToken = AngleOrNumberRawKnownTokenTypeIdentConsumer;
};

// MARK: Angle + Number + None

template<typename Transformer>
struct AngleOrNumberOrNoneRawConsumer : AngleOrNumberRawConsumer<Transformer> {
    using IdentToken = NoneRawKnownTokenTypeIdentConsumer;
};

template<typename Transformer>
struct AngleOrNumberOrNoneRawAllowingSymbolTableIdentConsumer : AngleOrNumberRawConsumer<Transformer> {
    using IdentToken = SameTokenMetaConsumer<
        Transformer,
        NoneRawKnownTokenTypeIdentConsumer,
        AngleOrNumberRawKnownTokenTypeIdentConsumer
    >;
};

// MARK: - Consumer functions

// MARK: - Angle
std::optional<AngleRaw> consumeAngleRaw(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
RefPtr<CSSPrimitiveValue> consumeAngle(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk = UnitlessQuirk::Forbid, UnitlessZeroQuirk = UnitlessZeroQuirk::Forbid);

// MARK: - Angle or Percent
RefPtr<CSSPrimitiveValue> consumeAngleOrPercent(CSSParserTokenRange&, CSSParserMode);

// MARK: - Transformable

template<typename Transformer = RawIdentityTransformer<AngleOrNumberOrNoneRaw>>
auto consumeAngleOrNumberOrNoneRaw(CSSParserTokenRange& range, CSSParserMode parserMode) -> typename Transformer::Result
{
    return consumeMetaConsumer<AngleOrNumberOrNoneRawConsumer<Transformer>>(range, { }, ValueRange::All, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

template<typename Transformer = RawIdentityTransformer<AngleOrNumberOrNoneRaw>>
auto consumeAngleOrNumberOrNoneRawAllowingSymbolTableIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, CSSParserMode parserMode) -> typename Transformer::Result
{
    return consumeMetaConsumer<AngleOrNumberOrNoneRawAllowingSymbolTableIdentConsumer<Transformer>>(range, symbolTable, ValueRange::All, parserMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

// MARK: Consumer Lookup

template<> struct ConsumerLookup<AngleOrNumberOrNoneRaw> {
    std::optional<AngleOrNumberOrNoneRaw> operator()(CSSParserTokenRange& args, CSSParserMode parserMode)
    {
        return consumeAngleOrNumberOrNoneRaw(args, parserMode);
    }

    std::optional<AngleOrNumberOrNoneRaw> operator()(CSSParserTokenRange& args, CSSParserMode parserMode, const CSSCalcSymbolTable& symbolTable)
    {
        return consumeAngleOrNumberOrNoneRawAllowingSymbolTableIdent(args, symbolTable, parserMode);
    }
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
