/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
template<StyleNumeric StylePrimitive> struct Blending<StylePrimitive> {
    constexpr auto canBlend(const StylePrimitive&, const StylePrimitive&) -> bool
    {
        return true;
    }

    auto blend(const StylePrimitive& from, const StylePrimitive& to, const BlendingContext& context) -> StylePrimitive
    {
        if (!context.progress && context.isReplace())
            return from;

        if (context.progress == 1 && context.isReplace())
            return to;

        // FIXME: As interpolation may result in a value outside of the range allowed by the
        // primitive, we clamp the value back down to the allowed range. The spec states that
        // in some cases, an accumulated intermediate value should be allowed to be out of the
        // allowed range until after interpolation has completed, but we currently don't have
        // that concept, and the `WebCore::Length` code path did clamping in the same fashion.
        // https://drafts.csswg.org/css-values/#combining-range

        return StylePrimitive { CSS::clampToRange<StylePrimitive::range>(WebCore::blend(from.value, to.value, context)) };
    }
};

// MARK: Interpolation of mixed numeric types
// https://drafts.csswg.org/css-values/#combine-mixed
template<auto R> struct Blending<LengthPercentage<R>> {
    constexpr auto canBlend(const LengthPercentage<R>&, const LengthPercentage<R>&) -> bool
    {
        return true;
    }

    auto blend(const LengthPercentage<R>& from, const LengthPercentage<R>& to, const BlendingContext& context) -> LengthPercentage<R>
    {
        // Interpolation of dimension-percentage value combinations (e.g. <length-percentage>, <frequency-percentage>,
        // <angle-percentage>, <time-percentage> or equivalent notations) is defined as:
        //
        //  - equivalent to interpolation of <length> if both VA and VB are pure <length> values
        //  - equivalent to interpolation of <percentage> if both VA and VB are pure <percentage> values
        //  - equivalent to converting both values into a calc() expression representing the sum of the
        //    dimension type and a percentage (each possibly zero) and interpolating each component
        //    individually (as a <length>/<frequency>/<angle>/<time> and as a <percentage>, respectively)

        if (from.value.isCalculationValue() || to.value.isCalculationValue() || (from.value.tag() != to.value.tag())) {
            if (context.compositeOperation != CompositeOperation::Replace)
                return Calculation::add(copyCalculation(from), copyCalculation(to));

            // 0% to 0px -> calc(0px + 0%) to calc(0px + 0%) -> 0px
            // 0px to 0% -> calc(0px + 0%) to calc(0px + 0%) -> 0px
            if (from.isZero() && to.isZero())
                return Length<R> { 0 };

            if (!to.isCalculationValue() && !from.isPercentage() && (context.progress == 1 || from.isZero())) {
                if (to.isLength())
                    return WebCore::Style::blend(Length<R> { 0 }, to.asLength(), context);
                return WebCore::Style::blend(Percentage<R> { 0 }, to.asPercentage(), context);
            }

            if (!from.isCalculationValue() && !to.isPercentage() && (!context.progress || to.isZero())) {
                if (from.isLength())
                    return WebCore::Style::blend(from.asLength(), Length<R> { 0 }, context);
                return WebCore::Style::blend(from.asPercentage(), Percentage<R> { 0 }, context);
            }

            return Calculation::blend(copyCalculation(from), copyCalculation(to), context.progress);
        }

        if (!context.progress && context.isReplace())
            return from;

        if (context.progress == 1 && context.isReplace())
            return to;

        if (to.isLength())
            return WebCore::Style::blend(from.asLength(), to.asLength(), context);
        return WebCore::Style::blend(from.asPercentage(), to.asPercentage(), context);
    }
};

// `NumberOrPercentageResolvedToNumber<R>` forwards to `Number<R>`.
template<auto R> struct Blending<NumberOrPercentageResolvedToNumber<R>> {
    auto canBlend(const NumberOrPercentageResolvedToNumber<R>& a, const NumberOrPercentageResolvedToNumber<R>& b) -> bool
    {
        return Style::canBlend(a.value, b.value);
    }
    auto blend(const NumberOrPercentageResolvedToNumber<R>& a, const NumberOrPercentageResolvedToNumber<R>& b, const BlendingContext& context) -> NumberOrPercentageResolvedToNumber<R>
    {
        return Style::blend(a.value, b.value, context);
    }
};

} // namespace Style
} // namespace WebCore
