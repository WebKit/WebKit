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

#include "config.h"
#include "StyleCalcSizeValue+Interpolation.h"

#include "StyleCalcSizeValue.h"
#include "StyleCalculationTree+Copy.h"
#include "StyleCalculationTree+Substitution.h"

namespace WebCore {
namespace Style {

static Calculation::Tree hundredPercent()
{
    return Calculation::Tree { .root = Calculation::percentage(100) };
}

// https://drafts.csswg.org/css-values-5/#calc-size-prepare-for-interpolation
std::optional<PreparedCalcSize> prepareForInterpolation(const CalcSizeValue& value)
{
    return WTF::switchOn(value.basis(),
        [&](const Ref<CalcSizeValue>& nested) -> std::optional<PreparedCalcSize> {
            // The outer function takes the inner basis, and the inner calculation stands in for the
            // outer `size`.
            auto inner = prepareForInterpolation(nested);
            if (!inner)
                return { };
            auto calculation = Calculation::substituteSize(value.calculation(), inner->calculation.root);
            if (!calculation)
                return { };
            return PreparedCalcSize { inner->basis, WTF::move(*calculation) };
        },
        [&](const Calculation::Tree& basis) -> std::optional<PreparedCalcSize> {
            if (!value.basisHasPercentage()) {
                // A length basis is a definite size, so the function no longer depends on a basis.
                auto calculation = Calculation::substituteSize(value.calculation(), basis.root);
                if (!calculation)
                    return { };
                return PreparedCalcSize { CSS::Keyword::Any { }, WTF::move(*calculation) };
            }

            // A percentage basis has to stay a percentage so it still resolves against the containing
            // block. De-percentifying restates it in terms of the 100% it becomes.
            auto dePercentified = Calculation::dePercentify(basis);
            auto calculation = Calculation::substituteSize(value.calculation(), dePercentified.root);
            if (!calculation)
                return { };
            return PreparedCalcSize { HundredPercentBasis { }, WTF::move(*calculation) };
        },
        [&]<CSSValueID Id>(const Constant<Id>& keyword) -> std::optional<PreparedCalcSize> {
            auto basis = preparedBasisForKeyword(keyword);
            if (!basis)
                return { };
            return PreparedCalcSize { *basis, Calculation::copy(value.calculation()) };
        }
    );
}

// https://drafts.csswg.org/css-values-5/#interpolate-calc-size
std::optional<PreparedCalcSizeBasis> interpolatedBasis(const PreparedCalcSizeBasis& a, const PreparedCalcSizeBasis& b)
{
    if (a == b)
        return a;
    if (WTF::holdsAlternative<CSS::Keyword::Any>(a))
        return b;
    if (WTF::holdsAlternative<CSS::Keyword::Any>(b))
        return a;

    // Two different bases would each want the function to act a different way.
    return { };
}

Ref<CalcSizeValue> makeCalcSizeValue(PreparedCalcSizeBasis preparedBasis, Calculation::Tree&& calculation)
{
    auto basisHasPercentage = WTF::holdsAlternative<HundredPercentBasis>(preparedBasis);
    auto hasPercentage = basisHasPercentage || Calculation::containsPercentage(calculation);

    auto basis = WTF::switchOn(preparedBasis,
        [](const HundredPercentBasis&) -> CalcSizeValue::Basis {
            return hundredPercent();
        },
        [](const auto& keyword) -> CalcSizeValue::Basis {
            return keyword;
        }
    );

    return CalcSizeValue::create(WTF::move(basis), WTF::move(calculation), basisHasPercentage, hasPercentage);
}

} // namespace Style
} // namespace WebCore
