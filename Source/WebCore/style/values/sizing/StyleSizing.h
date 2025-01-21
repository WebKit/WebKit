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
#include "StylePrimitiveNumericTypes.h"

namespace WebCore {
namespace Style {

// Shared base type for the sizing properties: preferred size, minimum size and maximum size.
// https://drafts.csswg.org/css-sizing-3/#sizing-properties

using FixedSize = Length<CSS::Nonnegative, float>;
using PercentageSize = Percentage<CSS::Nonnegative, float>;

template<typename... AdditionalKeywords> using SizeValue = PrimitiveNumericOrKeyword<
    LengthPercentage<CSS::Nonnegative>,
    CSS::Keyword::MinContent,
    CSS::Keyword::MaxContent,
    CSS::Keyword::FitContent,
    CSS::Keyword::Intrinsic,
    CSS::Keyword::MinIntrinsic,
    CSS::Keyword::WebkitFillAvailable,
    AdditionalKeywords...
>;

template<typename InitialKeyword, typename... AdditionalKeywords> struct SizeBase : SizeValue<InitialKeyword, AdditionalKeywords...> {
    using Base = SizeValue<InitialKeyword, AdditionalKeywords...>;
    using Base::Base;
    using Base::operator=;
    using Base::holdsAlternative;

    using Dimension = typename Base::Dimension;
    using Percentage = typename Base::Percentage;
    using Calc = typename Base::Calc;

    SizeBase()
        : Base { InitialKeyword() }
    {
    }

    // `Fixed` is added as an alias for the dimension type for consistency with `WebCore::Length`.
    using Fixed = LengthPercentage<WebCore::CSS::Nonnegative>::Dimension;
    bool isFixed() const { return Base::template holdsAlternative<Fixed>(); }
    std::optional<Fixed> fixed() const { return Base::dimension(); }

    // Ensure utility aliases `SizeFixed` and `SizePercentage` match for all SizeBase derived classes.
    static_assert(std::same_as<Fixed, FixedSize>);
    static_assert(std::same_as<Percentage, PercentageSize>);

    bool isInitial() const { return Base::template holdsAlternative<InitialKeyword>(); }

    bool isDimension() const { return Base::template holdsAlternative<LengthPercentage<WebCore::CSS::Nonnegative>::Dimension>(); }
    bool isPercent() const { return Base::template holdsAlternative<LengthPercentage<WebCore::CSS::Nonnegative>::Percentage>(); }
    bool isCalculated() const { return Base::template holdsAlternative<LengthPercentage<WebCore::CSS::Nonnegative>::Calc>(); }
    bool isMinContent() const { return Base::template holdsAlternative<WebCore::CSS::Keyword::MinContent>(); }
    bool isMaxContent() const { return Base::template holdsAlternative<WebCore::CSS::Keyword::MaxContent>(); }
    bool isFitContent() const { return Base::template holdsAlternative<WebCore::CSS::Keyword::FitContent>(); }
    bool isIntrinsicKeyword() const { return Base::template holdsAlternative<WebCore::CSS::Keyword::Intrinsic>(); }
    bool isMinIntrinsic() const { return Base::template holdsAlternative<WebCore::CSS::Keyword::MinIntrinsic>(); }
    bool isFillAvailable() const { return Base::template holdsAlternative<WebCore::CSS::Keyword::WebkitFillAvailable>(); }

    bool isPercentOrCalculated() const { return isPercent() || isCalculated(); }
    bool isSpecified() const { return isFixed() || isPercent() || isCalculated(); }

    // FIXME: This is misleadingly named. One would expect this function does `holdsAlternative<Intrinsic>()`.
    bool isIntrinsic() const { return isMinContent() || isMaxContent() || isFillAvailable() || isFitContent(); }
    bool isLegacyIntrinsic() const { return isIntrinsicKeyword() || isMinIntrinsic(); }
    bool isSpecifiedOrIntrinsic() const { return isSpecified() || isIntrinsic(); }

    // For the following three functions, attempt to match the behaviors of the ones in WebCore::Length.
    bool isZero() const;
    bool isPositive() const;
    bool isNegative() const;
};

template<typename InitialKeyword, typename... AdditionalKeywords> bool SizeBase<InitialKeyword, AdditionalKeywords...>::isZero() const
{
    return Base::template switchOn(
        [](const Fixed& fixed) {
            return !fixed.value;
        },
        [](const Percentage& percentage) {
            return !percentage.value;
        },
        [](const Calc&) {
            return false;
        },
        [](const WebCore::CSS::Keyword::Auto&) {
            return false;
        },
        [](const auto&) {
            return true;
        }
    );
}

template<typename InitialKeyword, typename... AdditionalKeywords> bool SizeBase<InitialKeyword, AdditionalKeywords...>::isPositive() const
{
    return Base::template switchOn(
        [](const Fixed& fixed) {
            return fixed.value > 0;
        },
        [](const Percentage& percentage) {
            return percentage.value > 0;
        },
        [](const Calc&) {
            return true;
        },
        [](const auto&) {
            return false;
        }
    );
}

template<typename InitialKeyword, typename... AdditionalKeywords> bool SizeBase<InitialKeyword, AdditionalKeywords...>::isNegative() const
{
    return Base::template switchOn(
        [](const Fixed& fixed) {
            return fixed.value < 0;
        },
        [](const Percentage& percentage) {
            return percentage.value < 0;
        },
        [](const auto&) {
            return false;
        }
    );
}

LayoutUnit evaluateMinimum(const LengthPercentage<CSS::Nonnegative>::Dimension&, LayoutUnit maximumValue);
LayoutUnit evaluateMinimum(const LengthPercentage<CSS::Nonnegative>::Percentage&, LayoutUnit maximumValue);
LayoutUnit evaluateMinimum(const LengthPercentage<CSS::Nonnegative>::Calc&, LayoutUnit maximumValue);

} // namespace Style
} // namespace WebCore

template<typename InitialKeyword, typename... AdditionalKeywords> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::SizeBase<InitialKeyword, AdditionalKeywords...>> = true;
