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

#include "Length.h"
#include "StylePrimitiveNumericTypes.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

// <'row-gap/column-gap'> = normal | <length-percentage [0,∞]>
// https://drafts.csswg.org/css-align/#column-row-gap
struct GapGutter {
    using Specified = LengthPercentage<CSS::Nonnegative>;
    using Fixed = typename Specified::Dimension;
    using Percentage = typename Specified::Percentage;
    using Calc = typename Specified::Calc;

    GapGutter(CSS::Keyword::Normal) : m_value(WebCore::LengthType::Normal) { }

    GapGutter(Fixed&& fixed) : m_value(fixed.value, WebCore::LengthType::Fixed) { }
    GapGutter(const Fixed& fixed) : m_value(fixed.value, WebCore::LengthType::Fixed) { }
    GapGutter(Percentage&& percent) : m_value(percent.value, WebCore::LengthType::Percent) { }
    GapGutter(const Percentage& percent) : m_value(percent.value, WebCore::LengthType::Percent) { }

    GapGutter(CSS::ValueLiteral<CSS::LengthUnit::Px> literal) : m_value(static_cast<float>(literal.value), WebCore::LengthType::Fixed) { }
    GapGutter(CSS::ValueLiteral<CSS::PercentageUnit::Percentage> literal) : m_value(static_cast<float>(literal.value), WebCore::LengthType::Percent) { }

    explicit GapGutter(WebCore::Length&& other) : m_value(WTFMove(other)) { RELEASE_ASSERT(isValid(m_value)); }
    explicit GapGutter(const WebCore::Length& other) : m_value(other) { RELEASE_ASSERT(isValid(m_value)); }

    ALWAYS_INLINE bool isNormal() const { return m_value.isNormal(); }

    ALWAYS_INLINE bool isFixed() const { return m_value.isFixed(); }
    ALWAYS_INLINE bool isPercent() const { return m_value.isPercent(); }
    ALWAYS_INLINE bool isCalculated() const { return m_value.isCalculated(); }
    ALWAYS_INLINE bool isPercentOrCalculated() const { return m_value.isPercentOrCalculated(); }
    ALWAYS_INLINE bool isSpecified() const { return m_value.isSpecified(); }

    std::optional<Fixed> tryFixed() const { return isFixed() ? std::make_optional(Fixed { m_value.value() }) : std::nullopt; }
    std::optional<Percentage> tryPercentage() const { return isPercent() ? std::make_optional(Percentage { m_value.value() }) : std::nullopt; }
    std::optional<Calc> tryCalc() const { return isCalculated() ? std::make_optional(Calc { m_value.calculationValue() }) : std::nullopt; }

    template<typename T> bool holdsAlternative() const
    {
             if constexpr (std::same_as<T, Fixed>)                return isFixed();
        else if constexpr (std::same_as<T, Percentage>)           return isPercent();
        else if constexpr (std::same_as<T, Calc>)                 return isCalculated();
        else if constexpr (std::same_as<T, CSS::Keyword::Normal>) return isNormal();
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_value.type()) {
        case WebCore::LengthType::Fixed:            return visitor(Fixed { m_value.value() });
        case WebCore::LengthType::Percent:          return visitor(Percentage { m_value.value() });
        case WebCore::LengthType::Calculated:       return visitor(Calc { m_value.calculationValue() });
        case WebCore::LengthType::Normal:           return visitor(CSS::Keyword::Normal { });

        case WebCore::LengthType::Auto:
        case WebCore::LengthType::Intrinsic:
        case WebCore::LengthType::MinIntrinsic:
        case WebCore::LengthType::MinContent:
        case WebCore::LengthType::MaxContent:
        case WebCore::LengthType::FillAvailable:
        case WebCore::LengthType::FitContent:
        case WebCore::LengthType::Content:
        case WebCore::LengthType::Relative:
        case WebCore::LengthType::Undefined:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const GapGutter&) const = default;

private:
    friend struct Blending<GapGutter>;
    friend struct Evaluation<GapGutter>;
    friend LayoutUnit evaluateMinimum(const GapGutter&, LayoutUnit);
    friend WTF::TextStream& operator<<(WTF::TextStream&, const GapGutter&);

    static bool isValid(const WebCore::Length& length)
    {
        switch (length.type()) {
        case WebCore::LengthType::Fixed:
            return CSS::isWithinRange<Fixed::range>(length.value());
        case WebCore::LengthType::Percent:
            return CSS::isWithinRange<Percentage::range>(length.value());
        case WebCore::LengthType::Calculated:
        case WebCore::LengthType::Normal:
            return true;
        case WebCore::LengthType::Auto:
        case WebCore::LengthType::Intrinsic:
        case WebCore::LengthType::MinIntrinsic:
        case WebCore::LengthType::MinContent:
        case WebCore::LengthType::MaxContent:
        case WebCore::LengthType::FillAvailable:
        case WebCore::LengthType::FitContent:
        case WebCore::LengthType::Content:
        case WebCore::LengthType::Relative:
        case WebCore::LengthType::Undefined:
            break;
        }
        return false;
    }

    WebCore::Length m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<GapGutter> { auto operator()(BuilderState&, const CSSValue&) -> GapGutter; };

// MARK: - Evaluation

template<> struct Evaluation<GapGutter> {
    auto operator()(const GapGutter&, LayoutUnit) -> LayoutUnit;
};

auto evaluateMinimum(const GapGutter&, LayoutUnit)  -> LayoutUnit;

// MARK: - Blending

template<> struct Blending<GapGutter> {
    auto canBlend(const GapGutter&, const GapGutter&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const GapGutter&, const GapGutter&) -> bool;
    auto blend(const GapGutter&, const GapGutter&, const BlendingContext&) -> GapGutter;
};

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const GapGutter&);

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::GapGutter> = true;
