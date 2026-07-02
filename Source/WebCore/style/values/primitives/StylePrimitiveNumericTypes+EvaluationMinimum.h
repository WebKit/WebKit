/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/StylePrimitiveNumeric.h>
#include <WebCore/StylePrimitiveNumericOrKeyword.h>
#include <WebCore/StyleZoomPrimitives.h>

namespace WebCore {
namespace Style {

// `EvaluationMinimum` is a specialized version of the `Evaluation` protocol
// for `LengthPercentage` and `LengthPercentage + Keywords` types.
//
// For pure `LengthPercentage`, it is identical to `Evaluation`.
//
// For `LengthPercentage + Keywords` types, it is identical to `Evaluation`
// if the dynamic type is a `LengthPercentage`, but will return zero for
// any keyword, unlike `Evaluation`, which will return the "reference" value.

template<typename, typename> struct EvaluationMinimum;

template<typename Result> struct EvaluationMinimumInvoker {
    template<typename StyleType, typename Reference, typename Zoom> decltype(auto) operator()(const StyleType& value, NOESCAPE Reference&& reference, Zoom&& zoom) const
    {
        return EvaluationMinimum<StyleType, Result> { }(value, std::forward<Reference>(reference), std::forward<Zoom>(zoom));
    }
};
template<typename Result> inline constexpr EvaluationMinimumInvoker<Result> evaluateMinimum{};

template<TupleLike StyleType, typename Result> requires (std::tuple_size_v<StyleType> == 1) struct EvaluationMinimum<StyleType, Result> {
    template<typename... Rest> Result operator()(const StyleType& value, Rest&&... rest)
    {
        return evaluateMinimum<Result>(get<0>(value), std::forward<Rest>(rest)...);
    }
};

template<auto R, typename V, typename Result> struct EvaluationMinimum<LengthPercentage<R, V>, Result> {
    using StyleType = LengthPercentage<R, V>;

    auto operator()(const StyleType& value, NOESCAPE const Invocable<Result()> auto& lazyMaximumValueFunctor, ZoomNeeded token) -> Result
        requires (StyleType::range.zoomOptions == CSS::RangeZoomOptions::Default)
    {
        return value.m_value.template minimumValueForPrimitiveDataWithLazyMaximum<StyleType::range, LayoutUnit, LayoutUnit>(value.evaluationKind(), lazyMaximumValueFunctor, token);
    }
    auto operator()(const StyleType& value, Result maximumValue, ZoomNeeded token) -> Result
        requires (StyleType::range.zoomOptions == CSS::RangeZoomOptions::Default)
    {
        return value.m_value.template minimumValueForPrimitiveDataWithLazyMaximum<StyleType::range, LayoutUnit, LayoutUnit>(value.evaluationKind(), [&] ALWAYS_INLINE_LAMBDA { return LayoutUnit(maximumValue); }, token);
    }

    auto operator()(const StyleType& value, NOESCAPE const Invocable<Result()> auto& lazyMaximumValueFunctor, ZoomFactor zoom) -> Result
        requires (StyleType::range.zoomOptions == CSS::RangeZoomOptions::Unzoomed)
    {
        return value.m_value.template minimumValueForPrimitiveDataWithLazyMaximum<StyleType::range, LayoutUnit, LayoutUnit>(value.evaluationKind(), lazyMaximumValueFunctor, zoom);
    }
    auto operator()(const StyleType& value, Result maximumValue, ZoomFactor zoom) -> Result
        requires (StyleType::range.zoomOptions == CSS::RangeZoomOptions::Unzoomed)
    {
        return value.m_value.template minimumValueForPrimitiveDataWithLazyMaximum<StyleType::range, LayoutUnit, LayoutUnit>(value.evaluationKind(), [&] ALWAYS_INLINE_LAMBDA { return LayoutUnit(maximumValue); }, zoom);
    }
};

template<LengthPercentageOrKeywordDerived StyleType, typename Result> struct EvaluationMinimum<StyleType, Result> {
    auto operator()(const StyleType& value, NOESCAPE const Invocable<Result()> auto& lazyMaximumValueFunctor, ZoomNeeded token) -> Result
        requires (StyleType::Numeric::range.zoomOptions == CSS::RangeZoomOptions::Default)
    {
        return value.m_value.template minimumValueForPrimitiveDataWithLazyMaximum<StyleType::Numeric, LayoutUnit, LayoutUnit>(value.evaluationKind(), lazyMaximumValueFunctor, token);
    }
    auto operator()(const StyleType& value, Result maximumValue, ZoomNeeded token) -> Result
        requires (StyleType::Numeric::range.zoomOptions == CSS::RangeZoomOptions::Default)
    {
        return value.m_value.template minimumValueForPrimitiveDataWithLazyMaximum<StyleType::Numeric::range, LayoutUnit, LayoutUnit>(value.evaluationKind(), [&] ALWAYS_INLINE_LAMBDA { return LayoutUnit(maximumValue); }, token);
    }

    auto operator()(const StyleType& value, NOESCAPE const Invocable<Result()> auto& lazyMaximumValueFunctor, ZoomFactor zoom) -> Result
        requires (StyleType::Numeric::range.zoomOptions == CSS::RangeZoomOptions::Unzoomed)
    {
        return value.m_value.template minimumValueForPrimitiveDataWithLazyMaximum<StyleType::Numeric::range, LayoutUnit, LayoutUnit>(value.evaluationKind(), lazyMaximumValueFunctor, zoom);
    }
    auto operator()(const StyleType& value, Result maximumValue, ZoomFactor zoom) -> Result
        requires (StyleType::Numeric::range.zoomOptions == CSS::RangeZoomOptions::Unzoomed)
    {
        return value.m_value.template minimumValueForPrimitiveDataWithLazyMaximum<StyleType::Numeric::range, LayoutUnit, LayoutUnit>(value.evaluationKind(), [&] ALWAYS_INLINE_LAMBDA { return LayoutUnit(maximumValue); }, zoom);
    }
};

} // namespace Style
} // namespace WebCore
