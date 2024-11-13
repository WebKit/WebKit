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

        // FIXME: This does not preserve the `quirk` bit for `Style::Length` values, matching
        // the behavior of the `WebCore::Length` code path. It is not clear if it ever makes
        // sense to preserve it during interpolation.

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
        // Interpolation of percentage-dimension value combinations (e.g. <length-percentage>, <frequency-percentage>,
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

            if (!to.value.isCalculationValue() && !from.value.isPercentage() && (context.progress == 1 || from.value.isZero())) {
                if (to.value.isLength())
                    return WebCore::Style::blend(Length<R> { 0 }, to.value.asLength(), context);
                return WebCore::Style::blend(Percentage<R> { 0 }, to.value.asPercentage(), context);
            }

            if (!from.value.isCalculationValue() && !to.value.isPercentage() && (!context.progress || to.value.isZero())) {
                if (from.value.isLength())
                    return WebCore::Style::blend(from.value.asLength(), Length<R> { 0 }, context);
                return WebCore::Style::blend(from.value.asPercentage(), Percentage<R> { 0 }, context);
            }

            return Calculation::blend(copyCalculation(from), copyCalculation(to), context.progress);
        }

        if (!context.progress && context.isReplace())
            return from;

        if (context.progress == 1 && context.isReplace())
            return to;

        if (to.value.isLength())
            return WebCore::Style::blend(from.value.asLength(), to.value.asLength(), context);
        return WebCore::Style::blend(from.value.asPercentage(), to.value.asPercentage(), context);
    }
};

} // namespace Style
} // namespace WebCore
