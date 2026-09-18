/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "StyleCalcSizeValue+Interpolation.h"
#include "StyleCalcSizeValue.h"
#include "StyleCalculationTree+Substitution.h"
#include "StylePrimitiveNumericOrKeyword+Blending.h"
#include "StylePrimitiveNumericTypes+Calculation.h"
#include "StyleSizeOrKeyword.h"

namespace WebCore {
namespace Style {

// MARK: - Blending

// Extends NumericOrKeywordBlending with calc-size() blending.
// https://drafts.csswg.org/css-values-5/#interpolate-calc-size
template<SizeOrKeywordDerived StyleType> struct Blending<StyleType> : NumericOrKeywordBlending<StyleType> {
    using Base = NumericOrKeywordBlending<StyleType>;
    using Numeric = typename StyleType::Numeric;
    using CalcSize = typename StyleType::CalcSize;

    // `interpolate-size` is read from the target style, which for a transition is the after-change style
    // the spec asks for. FIXME: a keyframe that sets it is not filtered out yet, so its value is used
    // where the non-animation value should be.
    auto canBlend(const StyleType& a, const StyleType& b, const ComputedStyle&, const ComputedStyle& toStyle) -> bool
    {
        if (interpolatesUsingCalcSize(a, b, toStyle.interpolateSize()))
            return !!preparePair(a, b);
        return Base::canBlend(a, b);
    }
    auto canBlend(const StyleType& a, const StyleType& b, const ComputedStyle& fromStyle, const ComputedStyle& toStyle, CompositeOperation) -> bool
    {
        return canBlend(a, b, fromStyle, toStyle);
    }
    auto requiresInterpolationForAccumulativeIteration(const StyleType& a, const StyleType& b, const ComputedStyle&, const ComputedStyle& toStyle) -> bool
    {
        if (interpolatesUsingCalcSize(a, b, toStyle.interpolateSize()))
            return true;
        return Base::requiresInterpolationForAccumulativeIteration(a, b);
    }
    auto blend(const StyleType& a, const StyleType& b, const ComputedStyle&, const ComputedStyle& toStyle, const BlendingContext& context) -> StyleType
    {
        if (interpolatesUsingCalcSize(a, b, toStyle.interpolateSize())) {
            if (auto result = blendCalcSize(a, b, context))
                return *result;
            return context.progress < 0.5 ? a : b;
        }
        return Base::blend(a, b, context);
    }

private:
    static bool isBareKeyword(const StyleType& value)
    {
        return !WTF::holdsAlternative<CalcSize>(value) && !WTF::holdsAlternative<Numeric>(value);
    }

    // A calc-size() on either side always interpolates this way. `allow-keywords` extends it to a bare
    // <size-keyword> paired with a <length-percentage>.
    static bool interpolatesUsingCalcSize(const StyleType& a, const StyleType& b, InterpolateSize combining)
    {
        if (WTF::holdsAlternative<CalcSize>(a) || WTF::holdsAlternative<CalcSize>(b))
            return true;
        if (combining != InterpolateSize::AllowKeywords)
            return false;
        return (isBareKeyword(a) && WTF::holdsAlternative<Numeric>(b))
            || (isBareKeyword(b) && WTF::holdsAlternative<Numeric>(a));
    }

    // Interpolating with a calc-size() makes a <length-percentage> act as calc-size(any, value) and a
    // <size-keyword> as calc-size(keyword, size).
    static std::optional<PreparedCalcSize> prepare(const StyleType& value)
    {
        if (WTF::holdsAlternative<CalcSize>(value))
            return prepareForInterpolation(protect(get<CalcSize>(value).calcSize()));

        if (WTF::holdsAlternative<Numeric>(value))
            return PreparedCalcSize { CSS::Keyword::Any { }, Calculation::Tree { .root = copyCalculation(get<Numeric>(value)) } };

        return WTF::switchOn(value,
            [&]<CSSValueID Id>(const Constant<Id>& keyword) -> std::optional<PreparedCalcSize> {
                auto basis = preparedBasisForKeyword(keyword);
                if (!basis)
                    return { };
                return PreparedCalcSize { *basis, Calculation::Tree { .root = Calculation::Size { } } };
            },
            [&](const auto&) -> std::optional<PreparedCalcSize> {
                return { };
            }
        );
    }

    struct PreparedPair {
        PreparedCalcSize from;
        PreparedCalcSize to;
        PreparedCalcSizeBasis basis;
    };

    static std::optional<PreparedPair> preparePair(const StyleType& a, const StyleType& b)
    {
        auto from = prepare(a);
        auto to = prepare(b);
        if (!from || !to)
            return { };

        auto basis = interpolatedBasis(from->basis, to->basis);
        if (!basis)
            return { };

        return PreparedPair { WTF::move(*from), WTF::move(*to), *basis };
    }

    static std::optional<StyleType> blendCalcSize(const StyleType& a, const StyleType& b, const BlendingContext& context)
    {
        auto prepared = preparePair(a, b);
        if (!prepared)
            return { };

        // Interrupting a transition feeds the blended tree back in as the next start value, so without
        // this repeated retargeting grows it without bound.
        if (Calculation::computeNodeCount(prepared->from.calculation) + Calculation::computeNodeCount(prepared->to.calculation) > Calculation::maximumCalcSizeNodeCount)
            return { };

        // Composition sums the calculations instead of weighting them, as it does for a plain calc().
        auto root = context.compositeOperation == CompositeOperation::Replace
            ? Calculation::blend(WTF::move(prepared->from.calculation.root), WTF::move(prepared->to.calculation.root), context.progress)
            : Calculation::add(WTF::move(prepared->from.calculation.root), WTF::move(prepared->to.calculation.root));

        return StyleType { CalcSize { makeCalcSizeValue(prepared->basis, Calculation::Tree { .root = WTF::move(root) }) } };
    }
};

} // namespace Style
} // namespace WebCore
