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

#include "CSSParserMode.h"
#include "CSSParserToken.h"
#include "CSSPropertyParserConsumer+Meta.h"
#include "CSSPropertyParserConsumer+None.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "Length.h"
#include <optional>

#include <wtf/RefPtr.h>

namespace WebCore {

class CSSPrimitiveValue;
class CSSParserTokenRange;
class CSSCalcSymbolTable;

namespace CSSPropertyParserHelpers {

// MARK: Number (Raw)

std::optional<NumberRaw> validatedNumberRaw(double, ValueRange);

struct NumberRawKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static std::optional<NumberRaw> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

struct NumberRawKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static std::optional<NumberRaw> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

struct NumberRawKnownTokenTypeIdentConsumer {
    static constexpr CSSParserTokenType tokenType = IdentToken;
    static std::optional<NumberRaw> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

// MARK: Number (CSSPrimitiveValue - maintaining calc)

struct NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

struct NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static RefPtr<CSSPrimitiveValue> consume(CSSParserTokenRange&, const CSSCalcSymbolTable&, ValueRange, CSSParserMode, UnitlessQuirk, UnitlessZeroQuirk);
};

// MARK: - Consumer definitions.

template<typename T>
struct NumberRawConsumer {
    using Transformer = T;
    using Result = typename Transformer::Result;

    using FunctionToken = NumberRawKnownTokenTypeFunctionConsumer;
    using NumberToken = NumberRawKnownTokenTypeNumberConsumer;
};

template<typename Transformer>
struct NumberRawAllowingSymbolTableIdentConsumer : NumberRawConsumer<Transformer> {
    using IdentToken = NumberRawKnownTokenTypeIdentConsumer;
};

struct NumberConsumer {
    using Result = RefPtr<CSSPrimitiveValue>;

    using FunctionToken = NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeFunctionConsumer;
    using NumberToken = NumberCSSPrimitiveValueWithCalcWithKnownTokenTypeNumberConsumer;
};

// MARK: - Combination consumer definitions.

// MARK: Number + None

template<typename Transformer>
struct NumberOrNoneRawConsumer : NumberRawConsumer<Transformer> {
    using IdentToken = NoneRawKnownTokenTypeIdentConsumer;
};

template<typename Transformer>
struct NumberOrNoneRawAllowingSymbolTableIdentConsumer : NumberRawConsumer<Transformer> {
    using IdentToken = SameTokenMetaConsumer<
        Transformer,
        NoneRawKnownTokenTypeIdentConsumer,
        NumberRawKnownTokenTypeIdentConsumer
    >;
};

// MARK: - Consumer functions

std::optional<NumberRaw> consumeNumberRaw(CSSParserTokenRange&, ValueRange = ValueRange::All);
RefPtr<CSSPrimitiveValue> consumeNumber(CSSParserTokenRange&, ValueRange);

// MARK: - Transformable

template<typename Transformer = RawIdentityTransformer<NumberOrNoneRaw>>
auto consumeNumberOrNoneRaw(CSSParserTokenRange& range, ValueRange valueRange = ValueRange::All) -> typename Transformer::Result
{
    return consumeMetaConsumer<NumberOrNoneRawConsumer<Transformer>>(range, { }, valueRange, CSSParserMode::HTMLStandardMode, UnitlessQuirk::Forbid, UnitlessZeroQuirk::Forbid);
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
