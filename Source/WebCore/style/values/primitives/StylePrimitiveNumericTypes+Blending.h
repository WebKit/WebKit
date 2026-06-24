/*
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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

#include "AnimationUtilities.h"
#include "StylePrimitiveNumericTypes+Calculation.h"
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {
namespace Style {

// MARK: Interpolation of base numeric types
// https://drafts.csswg.org/css-values/#combining-values
template<Numeric StyleType> struct Blending<StyleType> {
    auto blend(const StyleType& from, const StyleType& to, const BlendingContext& context) -> StyleType
    {
        if (!context.progress && context.isReplace())
            return from;

        if (context.progress == 1 && context.isReplace())
            return to;

        // FIXME: As interpolation may result in a value outside of the range allowed by the
        // primitive, we clamp the value back down to the allowed range. The spec states that
        // in some cases, an accumulated intermediate value should be allowed to be out of the
        // allowed range until after interpolation has completed, but we currently don't have
        // that concept.
        // https://drafts.csswg.org/css-values/#combining-range

        return StyleType { CSS::clampToRange<StyleType::range, typename StyleType::ResolvedValueType>(WebCore::blend(from.value, to.value, context)) };
    }
};

template<auto R, typename V> struct Blending<Length<R, V>> {
    using StyleType = Length<R, V>;

    auto blend(const StyleType& from, const StyleType& to, const BlendingContext& context) -> StyleType
    {
        if (!context.progress && context.isReplace())
            return from;

        if (context.progress == 1 && context.isReplace())
            return to;

        // FIXME: As interpolation may result in a value outside of the range allowed by the
        // primitive, we clamp the value back down to the allowed range. The spec states that
        // in some cases, an accumulated intermediate value should be allowed to be out of the
        // allowed range until after interpolation has completed, but we currently don't have
        // that concept.
        // https://drafts.csswg.org/css-values/#combining-range

        return StyleType { CSS::clampToRange<StyleType::range, typename StyleType::ResolvedValueType>(WebCore::blend(from.unresolvedValue(), to.unresolvedValue(), context)) };
    }
};

// MARK: Interpolation of mixed numeric types
// https://drafts.csswg.org/css-values/#combine-mixed

// Interpolation of dimension-percentage value combinations (e.g. <length-percentage>, <frequency-percentage>,
// <angle-percentage>, <time-percentage> or equivalent notations) is defined as:
//
//  - equivalent to interpolation of <length> if both VA and VB are pure <length> values
//  - equivalent to interpolation of <percentage> if both VA and VB are pure <percentage> values
//  - equivalent to converting both values into a calc() expression representing the sum of the
//    dimension type and a percentage (each possibly zero) and interpolating each component
//    individually (as a <length>/<frequency>/<angle>/<time> and as a <percentage>, respectively)

inline constexpr size_t maximumBlendTreeDepthForLengthPercentage = 128;

template<auto R, typename V> struct Blending<LengthPercentage<R, V>> {
    using StyleType = LengthPercentage<R, V>;
    using Length = typename StyleType::Dimension;
    using Percentage = typename StyleType::Percentage;
    using Calc = typename StyleType::Calc;

    auto requiresInterpolationForAccumulativeIteration(const StyleType& from, const StyleType& to) -> bool
    {
        return WTF::holdsAlternative<Calc>(from) || WTF::holdsAlternative<Calc>(to) || !from.hasSameType(to);
    }

    auto blend(const StyleType& from, const StyleType& to, const BlendingContext& context) -> StyleType
    {
        if (WTF::holdsAlternative<Calc>(from) || WTF::holdsAlternative<Calc>(to) || !from.hasSameType(to))
            return blendMixedTypes(from, to, context);

        if (!context.progress && context.isReplace())
            return from;

        if (context.progress == 1 && context.isReplace())
            return to;

        if (WTF::holdsAlternative<Length>(to))
            return WebCore::Style::blend(get<Length>(from), get<Length>(to), context);
        return WebCore::Style::blend(get<Percentage>(from), get<Percentage>(to), context);
    }

    bool isTooDeepToBlendWithNode(const StyleType& value)
    {
        if (!WTF::holdsAlternative<Calc>(value))
            return false;
        auto treeDepth = computeDepth(value.m_value.calculationValue().tree());
        return treeDepth > maximumBlendTreeDepthForLengthPercentage;
    }

    bool isLengthZero(const StyleType& value)
    {
        ASSERT(WTF::holdsAlternative<Length>(value) || WTF::holdsAlternative<Calc>(value));
        if (auto length = value.template tryGet<Length>())
            return length->isZero();
        return false;
    };

    StyleType blendMixedTypes(const StyleType& from, const StyleType& to, const BlendingContext& context)
    {
        using namespace CSS::Literals;

        ASSERT(WTF::holdsAlternative<Length>(from) || WTF::holdsAlternative<Percentage>(from) || WTF::holdsAlternative<Calc>(from));
        ASSERT(WTF::holdsAlternative<Length>(to) || WTF::holdsAlternative<Percentage>(to) || WTF::holdsAlternative<Calc>(to));

        if (context.compositeOperation != CompositeOperation::Replace)
            return Calc { Calculation::add(copyCalculation(from), copyCalculation(to)) };

        if (!WTF::holdsAlternative<Calc>(to) && !WTF::holdsAlternative<Percentage>(from) && (context.progress == 1 || isLengthZero(from))) {
            if (WTF::holdsAlternative<Length>(to))
                return Style::blend(Length { 0_css_px }, get<Length>(to), context);
            return Style::blend(Percentage { 0_css_percentage }, get<Percentage>(to), context);
        }

        if (!WTF::holdsAlternative<Calc>(from) && !WTF::holdsAlternative<Calc>(to) && !WTF::holdsAlternative<Percentage>(to) && (!context.progress || isLengthZero(to))) {
            if (WTF::holdsAlternative<Length>(from))
                return WebCore::Style::blend(get<Length>(from), Length { 0_css_px }, context);
            return WebCore::Style::blend(get<Percentage>(from), Percentage { 0_css_percentage }, context);
        }

        // Limit the tree depth generated by blending to avoid blowing up the stack in recursive functions.
        if (isTooDeepToBlendWithNode(from) || isTooDeepToBlendWithNode(to))
            return Calc { copyCalculation(to) };

        return Calc { Calculation::blend(copyCalculation(from), copyCalculation(to), context.progress) };
    }
};

template<auto nR, auto pR, typename V> struct Blending<NumberOrPercentage<nR, pR, V>> {
    using StyleType = NumberOrPercentage<nR, pR, V>;

    auto canBlend(const StyleType& a, const StyleType& b) -> bool
    {
        return a.value.index() == b.value.index();
    }
    auto blend(const StyleType& a, const StyleType& b, const BlendingContext& context) -> StyleType
    {
        if (context.isDiscrete) {
            ASSERT(!context.progress || context.progress == 1);
            return context.progress ? b : a;
        }

        return WTF::visit(WTF::makeVisitor(
            [&]<typename T>(const T& a, const T& b) -> StyleType {
                return WebCore::Style::blend(a, b, context);
            },
            [&](const CSS::PrimitiveDataEmptyToken&, const CSS::PrimitiveDataEmptyToken&) -> StyleType {
                RELEASE_ASSERT_NOT_REACHED();
            },
            [&](const auto&, const auto&) -> StyleType {
                RELEASE_ASSERT_NOT_REACHED();
            }
        ), a.value, b.value);
    }
};

// `NumberOrPercentageResolvedToNumber<nR, pR, V>` forwards to `Number<nR, V>`.
template<auto nR, auto pR, typename V> struct Blending<NumberOrPercentageResolvedToNumber<nR, pR, V>> {
    using StyleType = NumberOrPercentageResolvedToNumber<nR, pR, V>;

    auto canBlend(const StyleType& a, const StyleType& b) -> bool
    {
        return Style::canBlend(a.value, b.value);
    }
    auto blend(const StyleType& a, const StyleType& b, const BlendingContext& context) -> StyleType
    {
        return Style::blend(a.value, b.value, context);
    }
};

} // namespace Style
} // namespace WebCore
