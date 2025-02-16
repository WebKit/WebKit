/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "StylePrimitiveNumericOrKeyword.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

template<auto R, typename V, CSS::PrimitiveKeyword... Ks> struct ToCSS<PrimitiveNumericOrKeyword<LengthPercentage<R, V>, Ks...>> {
    using Result = CSS::PrimitiveNumericOrKeyword<CSS::LengthPercentage<R, V>, Ks...>;

    auto operator()(const PrimitiveNumericOrKeyword<LengthPercentage<R, V>, Ks...>& value, const RenderStyle& style) -> Result
    {
        return WTF::switchOn(value,
            [&](const typename LengthPercentage<R, V>::Dimension& length) -> Result {
                return CSS::LengthPercentageRaw<R, V> { length.unit, adjustForZoom(length.value, style) };
            },
            [&](const typename LengthPercentage<R, V>::Percentage& percentage) -> Result {
                return CSS::LengthPercentageRaw<R, V> { percentage.unit, percentage.value };
            },
            [&](const typename LengthPercentage<R, V>::Calc& calculation) -> Result {
                return CSS::UnevaluatedCalc<CSS::LengthPercentageRaw<R, V>> { makeCalc(calculation.protectedCalculation(), style) };
            },
            [&]<CSSValueID Id>(const Constant<Id>& identifier) -> Result {
                return toCSS(identifier, style);
            }
        );
    }
};

template<CSS::Numeric N, CSS::PrimitiveKeyword... Ks> struct ToStyle<CSS::PrimitiveNumericOrKeyword<N, Ks...>> {
    using From = CSS::PrimitiveNumericOrKeyword<N, Ks...>;
    using To = typename ToStyleMapping<From>::type;

    template<typename... Rest> auto operator()(const From& value, Rest&&... rest) -> To
    {
        return WTF::switchOn(value, [&](const auto& value) -> To { return toStyle(value, std::forward<Rest>(rest)...); });
    }

    // Implement `BuilderState` overload to explicitly forward to the `CSSToLengthConversionData` overload.
    template<typename... Rest> auto operator()(const From& value, const BuilderState& state, Rest&&... rest) -> To
    {
        return toStyle(value, conversionData<typename N::Raw>(state), std::forward<Rest>(rest)...);
    }
};

} // namespace Style
} // namespace WebCore
