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

#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+MetaResolver.h"
#include "CSSPropertyParserConsumer+RawTypes.h"
#include "CSSPropertyParserConsumer+UnevaluatedCalc.h"
#include <optional>

namespace WebCore {
namespace CSSPropertyParserHelpers {

// MARK: Resolver for users that want to get the raw result.

/// Non-template base type for code sharing.
struct RawResolverBase {
    static std::optional<AngleRaw> resolve(AngleRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static std::optional<LengthRaw> resolve(LengthRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static std::optional<NumberRaw> resolve(NumberRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static std::optional<PercentRaw> resolve(PercentRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static std::optional<ResolutionRaw> resolve(ResolutionRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static std::optional<TimeRaw> resolve(TimeRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static std::optional<NoneRaw> resolve(NoneRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions);

    // FIXME: SymbolRaw is special cased to return NumberRaw. This works for now, as all symbols are defined to be numbers, but that if symbols are used anywhere else by CSS Color, this may not be true.
    static std::optional<NumberRaw> resolve(SymbolRaw, const CSSCalcSymbolTable&, CSSPropertyParserOptions);

    template<typename IntType, IntegerValueRange integerRange>
    static std::optional<IntegerRaw<IntType, integerRange>> resolve(IntegerRaw<IntType, integerRange> value, const CSSCalcSymbolTable&, CSSPropertyParserOptions)
    {
        return value;
    }

    static std::optional<AngleRaw> resolve(UnevaluatedCalc<AngleRaw>, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static std::optional<LengthRaw> resolve(UnevaluatedCalc<LengthRaw>, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static std::optional<NumberRaw> resolve(UnevaluatedCalc<NumberRaw>, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static std::optional<PercentRaw> resolve(UnevaluatedCalc<PercentRaw>, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static std::optional<ResolutionRaw> resolve(UnevaluatedCalc<ResolutionRaw>, const CSSCalcSymbolTable&, CSSPropertyParserOptions);
    static std::optional<TimeRaw> resolve(UnevaluatedCalc<TimeRaw>, const CSSCalcSymbolTable&, CSSPropertyParserOptions);

    template<typename IntType, IntegerValueRange integerRange>
    static std::optional<IntegerRaw<IntType, integerRange>> resolve(UnevaluatedCalc<IntegerRaw<IntType, integerRange>> calc, const CSSCalcSymbolTable& symbolTable, CSSPropertyParserOptions)
    {
        // https://drafts.csswg.org/css-values-4/#integers
        // Rounding to the nearest integer requires rounding in the direction of +∞ when the fractional portion is exactly 0.5.
        return {{ clampTo<IntType>(std::floor(std::max(calc.calc->doubleValue(symbolTable), computeMinimumValue(integerRange)) + 0.5)) }};
    }
};

template<typename... Ts>
using RawResolverVariantWrapper = typename std::variant<Ts...>;

template<typename... Ts>
struct RawResolverResultType {
    /// The result of a resolver that resolves all the way down to the `raw` representation.
    /// To be used with a list of `raw` types. e.g. `RawResolvedResultType<AngleRaw, PercentRaw, NoneRaw>`.
    using TypeList = brigand::list<Ts...>;
    using ResultTypeList = brigand::remove_if<TypeList, std::is_same<SymbolRaw, brigand::_1>>;

    using type = std::optional<
        std::conditional_t<
            brigand::size<ResultTypeList>::value == 1,
            brigand::front<ResultTypeList>,
            brigand::wrap<ResultTypeList, RawResolverVariantWrapper>
        >
    >;
};

template<typename... Ts>
using RawResolverResult = typename RawResolverResultType<Ts...>::type;

template<typename... Ts>
struct RawResolver : MetaResolver<RawResolverResult<Ts...>, RawResolverBase, Ts...> {
    using MetaResolver<RawResolverResult<Ts...>, RawResolverBase, Ts...>::resolve;
    using MetaResolver<RawResolverResult<Ts...>, RawResolverBase, Ts...>::consumeAndResolve;
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
