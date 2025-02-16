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

using namespace CSS::Literals;

// MARK: Interpolation of base numeric types
// https://drafts.csswg.org/css-values/#combining-values
template<Numeric StyleType> struct Blending<StyleType> {
    constexpr auto canBlend(const StyleType&, const StyleType&) -> bool
    {
        return true;
    }

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
        // that concept, and the `WebCore::Length` code path did clamping in the same fashion.
        // https://drafts.csswg.org/css-values/#combining-range

        return StyleType { CSS::clampToRange<StyleType::range>(WebCore::blend(from.value, to.value, context)) };
    }
};

// MARK: Interpolation of mixed numeric types
// https://drafts.csswg.org/css-values/#combine-mixed
template<auto R, typename V> struct Blending<LengthPercentage<R, V>> {
    constexpr auto canBlend(const LengthPercentage<R, V>&, const LengthPercentage<R, V>&) -> bool
    {
        return true;
    }

    auto blend(const LengthPercentage<R, V>& from, const LengthPercentage<R, V>& to, const BlendingContext& context) -> LengthPercentage<R, V>
    {
        using Length = typename LengthPercentage<R, V>::Dimension;
        using Percentage = typename LengthPercentage<R, V>::Percentage;
        using Calc = typename LengthPercentage<R, V>::Calc;

        // Interpolation of dimension-percentage value combinations (e.g. <length-percentage>, <frequency-percentage>,
        // <angle-percentage>, <time-percentage> or equivalent notations) is defined as:
        //
        //  - equivalent to interpolation of <length> if both VA and VB are pure <length> values
        //  - equivalent to interpolation of <percentage> if both VA and VB are pure <percentage> values
        //  - equivalent to converting both values into a calc() expression representing the sum of the
        //    dimension type and a percentage (each possibly zero) and interpolating each component
        //    individually (as a <length>/<frequency>/<angle>/<time> and as a <percentage>, respectively)

        if (WTF::holdsAlternative<Calc>(from) || WTF::holdsAlternative<Calc>(to) || (from.index() != to.index())) {
            if (context.compositeOperation != CompositeOperation::Replace)
                return Calc { Calculation::add(copyCalculation(from), copyCalculation(to)) };

            bool fromIsZero = from.isZero();
            bool toIsZero = to.isZero();

            // 0% to 0px -> calc(0px + 0%) to calc(0px + 0%) -> 0px
            // 0px to 0% -> calc(0px + 0%) to calc(0px + 0%) -> 0px
            if (fromIsZero && toIsZero)
                return 0_css_px;

            if (!WTF::holdsAlternative<Calc>(to) && !WTF::holdsAlternative<Percentage>(from) && (context.progress == 1 || fromIsZero)) {
                if (WTF::holdsAlternative<Length>(to))
                    return WebCore::Style::blend<Length>(0_css_px, get<Length>(to), context);
                return WebCore::Style::blend<Percentage>(0_css_percentage, get<Percentage>(to), context);
            }

            if (!WTF::holdsAlternative<Calc>(from) && !WTF::holdsAlternative<Percentage>(to) && (!context.progress || toIsZero)) {
                if (WTF::holdsAlternative<Length>(from))
                    return WebCore::Style::blend<Length>(get<Length>(from), 0_css_px, context);
                return WebCore::Style::blend<Percentage>(get<Percentage>(from), 0_css_percentage, context);
            }

            return Calc { Calculation::blend(copyCalculation(from), copyCalculation(to), context.progress) };
        }

        if (!context.progress && context.isReplace())
            return from;

        if (context.progress == 1 && context.isReplace())
            return to;

        if (WTF::holdsAlternative<Length>(to))
            return WebCore::Style::blend(get<Length>(from), get<Length>(to), context);
        return WebCore::Style::blend(get<Percentage>(from), get<Percentage>(to), context);
    }
};

// `NumberOrPercentageResolvedToNumber<nR, pR, V>` forwards to `Number<nR, V>`.
template<auto nR, auto pR, typename V> struct Blending<NumberOrPercentageResolvedToNumber<nR, pR, V>> {
    auto canBlend(const NumberOrPercentageResolvedToNumber<nR, pR, V>& a, const NumberOrPercentageResolvedToNumber<nR, pR, V>& b) -> bool
    {
        return Style::canBlend(a.value, b.value);
    }
    auto blend(const NumberOrPercentageResolvedToNumber<nR, pR, V>& a, const NumberOrPercentageResolvedToNumber<nR, pR, V>& b, const BlendingContext& context) -> NumberOrPercentageResolvedToNumber<nR, pR, V>
    {
        return Style::blend(a.value, b.value, context);
    }
};

} // namespace Style
} // namespace WebCore
