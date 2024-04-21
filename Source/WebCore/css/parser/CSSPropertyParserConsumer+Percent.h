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
#include "CSSPropertyParserConsumer+Primitives.h"
#include <optional>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSPrimitiveValue;
class CSSParserTokenRange;

namespace CSSPropertyParserHelpers {

// MARK: - Primitive value consumers for callers that know the token type.

// MARK: Percent (raw)

std::optional<PercentRaw> validatedPercentRaw(double, ValueRange);

struct PercentRawKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static std::optional<PercentRaw> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

struct PercentRawKnownTokenTypePercentConsumer {
    static constexpr CSSParserTokenType tokenType = PercentageToken;
    static std::optional<PercentRaw> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

struct PercentRawKnownTokenTypeIdentConsumer {
    static constexpr CSSParserTokenType tokenType = IdentToken;
    static std::optional<PercentRaw> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

// MARK: Percent (CSSPrimitiveValue - maintaining calc)

struct PercentCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

struct PercentCSSPrimitiveValueWithCalcWithKnownTokenTypePercentConsumer {
    static constexpr CSSParserTokenType tokenType = PercentageToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

// MARK: Specialized combination consumers.

// FIXME: Rename to PercentOrNumberRaw to remove cycle.
// FIXME: It would be good to find a way to synthesize this from an number and percent specific variants.
struct NumberOrPercentRawKnownTokenTypeIdentConsumer {
    static constexpr CSSParserTokenType tokenType = IdentToken;
    static std::optional<NumberOrPercentRaw> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

// MARK: - Consumer definitions.

// MARK: Percent

template<typename T>
struct PercentRawConsumer {
    using Transformer = T;
    using Result = typename Transformer::Result;

    using FunctionToken = PercentRawKnownTokenTypeFunctionConsumer;
    using PercentageToken = PercentRawKnownTokenTypePercentConsumer;
};

template<typename Transformer>
struct PercentRawAllowingSymbolTableIdentConsumer : PercentRawConsumer<Transformer> {
    using IdentToken = PercentRawKnownTokenTypeIdentConsumer;
};

struct PercentConsumer {
    using Result = RefPtr<CSSPrimitiveValue>;

    using FunctionToken = PercentCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer;
    using PercentageToken = PercentCSSPrimitiveValueWithCalcWithKnownTokenTypePercentConsumer;
};

// MARK: Percent + None

template<typename Transformer>
struct PercentOrNoneRawConsumer : PercentRawConsumer<Transformer> {
    using IdentToken = NoneRawKnownTokenTypeIdentConsumer;
};

template<typename Transformer>
struct PercentOrNoneRawAllowingSymbolTableIdentConsumer : PercentRawConsumer<Transformer> {
    using IdentToken = SameTokenMetaConsumer<
        Transformer,
        NoneRawKnownTokenTypeIdentConsumer,
        PercentRawKnownTokenTypeIdentConsumer
    >;
};

// MARK: Percent + Number

// FIXME: Rename to PercentOrNumberRawConsumer
template<typename T>
struct NumberOrPercentRawConsumer {
    using Transformer = T;
    using Result = typename Transformer::Result;

    using FunctionToken = SameTokenMetaConsumer<
        Transformer,
        PercentRawKnownTokenTypeFunctionConsumer,
        NumberRawKnownTokenTypeFunctionConsumer
    >;
    using NumberToken = NumberRawKnownTokenTypeNumberConsumer;
    using PercentageToken = PercentRawKnownTokenTypePercentConsumer;
};

// FIXME: Rename to PercentOrNumberRawAllowingSymbolTableIdentConsumer
template<typename Transformer>
struct NumberOrPercentRawAllowingSymbolTableIdentConsumer : NumberOrPercentRawConsumer<Transformer> {
    using IdentToken = NumberOrPercentRawKnownTokenTypeIdentConsumer;
};

// MARK: Percent + Number + None

// FIXME: Rename to PercentOrNumberOrNoneRawConsumer
template<typename Transformer>
struct NumberOrPercentOrNoneRawConsumer : NumberOrPercentRawConsumer<Transformer> {
    using IdentToken = NoneRawKnownTokenTypeIdentConsumer;
};

// FIXME: Rename to PercentOrNumberOrNoneRawAllowingSymbolTableIdentConsumer
template<typename Transformer>
struct NumberOrPercentOrNoneRawAllowingSymbolTableIdentConsumer : NumberOrPercentRawConsumer<Transformer> {
    using IdentToken = SameTokenMetaConsumer<
        Transformer,
        NoneRawKnownTokenTypeIdentConsumer,
        NumberOrPercentRawKnownTokenTypeIdentConsumer
    >;
};

// MARK: - Consumer functions

// MARK: - Percent
std::optional<PercentRaw> consumePercentRaw(CSSParserTokenRange&);
RefPtr<CSSPrimitiveValue> consumePercent(CSSParserTokenRange&, ValueRange);

// MARK: - Percent or Number
// FIXME: Rename to consumePercentOrNumber.
RefPtr<CSSPrimitiveValue> consumeNumberOrPercent(CSSParserTokenRange&, ValueRange);

// MARK: - Transformable

// FIXME: Rename to consumePercentOrNumberRaw.
template<typename Transformer = RawIdentityTransformer<NumberOrPercentRaw>>
auto consumeNumberOrPercentRaw(CSSParserTokenRange& range, ValueRange valueRange = ValueRange::All) -> typename Transformer::Result
{
    return consumeMetaConsumer<NumberOrPercentRawConsumer<Transformer>>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

// FIXME: Rename to consumePercentOrNumberOrNoneRaw.
template<typename Transformer = RawIdentityTransformer<NumberOrPercentOrNoneRaw>>
auto consumeNumberOrPercentOrNoneRaw(CSSParserTokenRange& range, ValueRange valueRange = ValueRange::All) -> typename Transformer::Result
{
    return consumeMetaConsumer<NumberOrPercentOrNoneRawConsumer<Transformer>>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

// FIXME: Rename to consumePercentOrNumberOrNoneRawAllowingSymbolTableIdent.
template<typename Transformer = RawIdentityTransformer<NumberOrPercentOrNoneRaw>>
auto consumeNumberOrPercentOrNoneRawAllowingSymbolTableIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange = ValueRange::All) -> typename Transformer::Result
{
    return consumeMetaConsumer<NumberOrPercentOrNoneRawAllowingSymbolTableIdentConsumer<Transformer>>(range, symbolTable, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

template<typename Transformer = RawIdentityTransformer<PercentOrNoneRaw>>
auto consumePercentOrNoneRaw(CSSParserTokenRange& range, ValueRange valueRange = ValueRange::All) -> typename Transformer::Result
{
    return consumeMetaConsumer<PercentOrNoneRawConsumer<Transformer>>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

template<typename Transformer = RawIdentityTransformer<PercentOrNoneRaw>>
auto consumePercentOrNoneRawAllowingSymbolTableIdent(CSSParserTokenRange& range, const CSSCalcSymbolTable& symbolTable, ValueRange valueRange = ValueRange::All) -> typename Transformer::Result
{
    return consumeMetaConsumer<PercentOrNoneRawAllowingSymbolTableIdentConsumer<Transformer>>(range, symbolTable, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
