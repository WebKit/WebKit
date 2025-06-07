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
#include "LengthFunctions.h"
#include "StylePrimitiveNumericTypes.h"
#include "StyleValueTypes.h"

namespace WebCore {

class CSSValue;
class RenderStyle;

namespace Style {

class BuilderState;
struct PreferredSize;

// <'flex-basis'> = content | <‘width’>
// https://drafts.csswg.org/css-flexbox/#propdef-flex-basis
struct FlexBasis {
    using Specified = LengthPercentage<CSS::Nonnegative>;
    using Fixed = typename Specified::Dimension;
    using Percentage = typename Specified::Percentage;
    using Calc = typename Specified::Calc;

    FlexBasis(CSS::Keyword::Content) : m_value(WebCore::LengthType::Content) { }
    FlexBasis(CSS::Keyword::Auto) : m_value(WebCore::LengthType::Auto) { }
    FlexBasis(CSS::Keyword::MinContent) : m_value(WebCore::LengthType::MinContent) { }
    FlexBasis(CSS::Keyword::MaxContent) : m_value(WebCore::LengthType::MaxContent) { }
    FlexBasis(CSS::Keyword::FitContent) : m_value(WebCore::LengthType::FitContent) { }
    FlexBasis(CSS::Keyword::WebkitFillAvailable) : m_value(WebCore::LengthType::FillAvailable) { }
    FlexBasis(CSS::Keyword::Intrinsic) : m_value(WebCore::LengthType::Intrinsic) { }
    FlexBasis(CSS::Keyword::MinIntrinsic) : m_value(WebCore::LengthType::MinIntrinsic) { }

    FlexBasis(Fixed&& fixed) : m_value(fixed.value, WebCore::LengthType::Fixed) { }
    FlexBasis(const Fixed& fixed) : m_value(fixed.value, WebCore::LengthType::Fixed) { }
    FlexBasis(Percentage&& percent) : m_value(percent.value, WebCore::LengthType::Percent) { }
    FlexBasis(const Percentage& percent) : m_value(percent.value, WebCore::LengthType::Percent) { }

    FlexBasis(CSS::ValueLiteral<CSS::LengthUnit::Px> literal) : m_value(static_cast<float>(literal.value), WebCore::LengthType::Fixed) { }
    FlexBasis(CSS::ValueLiteral<CSS::PercentageUnit::Percentage> literal) : m_value(static_cast<float>(literal.value), WebCore::LengthType::Percent) { }

    explicit FlexBasis(WebCore::Length&& other) : m_value(WTFMove(other)) { RELEASE_ASSERT(isValid(m_value)); }
    explicit FlexBasis(const WebCore::Length& other) : m_value(other) { RELEASE_ASSERT(isValid(m_value)); }

    ALWAYS_INLINE bool isFixed() const { return m_value.isFixed(); }
    ALWAYS_INLINE bool isDimension() const { return m_value.isFixed(); }
    ALWAYS_INLINE bool isPercent() const { return m_value.isPercent(); }
    ALWAYS_INLINE bool isCalculated() const { return m_value.isCalculated(); }
    ALWAYS_INLINE bool isPercentOrCalculated() const { return m_value.isPercentOrCalculated(); }
    ALWAYS_INLINE bool isSpecified() const { return m_value.isSpecified(); }

    // `content` is a `FlexBasis` specific value.
    ALWAYS_INLINE bool isContent() const { return m_value.isContent(); }
    ALWAYS_INLINE bool isAuto() const { return m_value.isAuto(); }
    ALWAYS_INLINE bool isMinContent() const { return m_value.isMinContent(); }
    ALWAYS_INLINE bool isMaxContent() const { return m_value.isMaxContent(); }
    ALWAYS_INLINE bool isFitContent() const { return m_value.isFitContent(); }
    ALWAYS_INLINE bool isFillAvailable() const { return m_value.isFillAvailable(); }
    ALWAYS_INLINE bool isMinIntrinsic() const { return m_value.isMinIntrinsic(); }
    ALWAYS_INLINE bool isIntrinsicKeyword() const { return m_value.type() == LengthType::Intrinsic; }

    // FIXME: This is misleadingly named. One would expect this function checks `type == LengthType::Intrinsic` but instead it checks `type = LengthType::MinContent || type == LengthType::MaxContent || type == LengthType::FillAvailable || type == LengthType::FitContent`.
    ALWAYS_INLINE bool isIntrinsic() const { return m_value.isIntrinsic(); }
    ALWAYS_INLINE bool isLegacyIntrinsic() const { return m_value.isLegacyIntrinsic(); }
    ALWAYS_INLINE bool isIntrinsicOrLegacyIntrinsic() const { return isIntrinsic() || isLegacyIntrinsic(); }
    ALWAYS_INLINE bool isIntrinsicOrLegacyIntrinsicOrAuto() const { return m_value.isIntrinsicOrLegacyIntrinsicOrAuto(); }
    ALWAYS_INLINE bool isSpecifiedOrIntrinsic() const { return m_value.isSpecifiedOrIntrinsic(); }

    // For the following three functions, attempt to match the behaviors of the ones in WebCore::Length.
    ALWAYS_INLINE bool isZero() const { return m_value.isZero(); }
    ALWAYS_INLINE bool isPositive() const { return m_value.isPositive(); }
    ALWAYS_INLINE bool isNegative() const { return m_value.isNegative(); }

    ALWAYS_INLINE std::optional<Fixed> tryFixed() const { return isFixed() ? std::make_optional(Fixed { m_value.value() }) : std::nullopt; }
    ALWAYS_INLINE std::optional<Percentage> tryPercentage() const { return isPercent() ? std::make_optional(Percentage { m_value.value() }) : std::nullopt; }

    // `FlexBasis` is a superset of `PreferredSize` and therefore this conversion can fail when type is `content`.
    std::optional<PreferredSize> tryPreferredSize() const;

    template<typename T> bool holdsAlternative() const
    {
             if constexpr (std::same_as<T, Fixed>)                              return isFixed();
        else if constexpr (std::same_as<T, Percentage>)                         return isPercent();
        else if constexpr (std::same_as<T, Calc>)                               return isCalculated();
        else if constexpr (std::same_as<T, CSS::Keyword::Content>)              return isContent();
        else if constexpr (std::same_as<T, CSS::Keyword::Auto>)                 return isAuto();
        else if constexpr (std::same_as<T, CSS::Keyword::Intrinsic>)            return isIntrinsicKeyword();
        else if constexpr (std::same_as<T, CSS::Keyword::MinIntrinsic>)         return isMinIntrinsic();
        else if constexpr (std::same_as<T, CSS::Keyword::MinContent>)           return isMinContent();
        else if constexpr (std::same_as<T, CSS::Keyword::MaxContent>)           return isMaxContent();
        else if constexpr (std::same_as<T, CSS::Keyword::WebkitFillAvailable>)  return isFillAvailable();
        else if constexpr (std::same_as<T, CSS::Keyword::FitContent>)           return isFitContent();
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_value.type()) {
        case WebCore::LengthType::Fixed:            return visitor(Fixed { m_value.value() });
        case WebCore::LengthType::Percent:          return visitor(Percentage { m_value.value() });
        case WebCore::LengthType::Calculated:       return visitor(Calc { m_value });
        case WebCore::LengthType::Content:          return visitor(CSS::Keyword::Content { });
        case WebCore::LengthType::Auto:             return visitor(CSS::Keyword::Auto { });
        case WebCore::LengthType::Intrinsic:        return visitor(CSS::Keyword::Intrinsic { });
        case WebCore::LengthType::MinIntrinsic:     return visitor(CSS::Keyword::MinIntrinsic { });
        case WebCore::LengthType::MinContent:       return visitor(CSS::Keyword::MinContent { });
        case WebCore::LengthType::MaxContent:       return visitor(CSS::Keyword::MaxContent { });
        case WebCore::LengthType::FillAvailable:    return visitor(CSS::Keyword::WebkitFillAvailable { });
        case WebCore::LengthType::FitContent:       return visitor(CSS::Keyword::FitContent { });

        case WebCore::LengthType::Normal:
        case WebCore::LengthType::Relative:
        case WebCore::LengthType::Undefined:
            break;
        }

        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const FlexBasis&) const = default;

private:
    friend struct Blending<FlexBasis>;
    friend struct Evaluation<FlexBasis>;
    friend LayoutUnit evaluateMinimum(const FlexBasis&, NOESCAPE const Invocable<LayoutUnit()> auto&);
    friend LayoutUnit evaluateMinimum(const FlexBasis&, LayoutUnit);
    friend WTF::TextStream& operator<<(WTF::TextStream&, const FlexBasis&);

    static bool isValid(const WebCore::Length& length)
    {
        switch (length.type()) {
        case WebCore::LengthType::Percent:
        case WebCore::LengthType::Fixed:
            return length.value() >= 0;

        case WebCore::LengthType::Content:
        case WebCore::LengthType::Auto:
        case WebCore::LengthType::Intrinsic:
        case WebCore::LengthType::MinIntrinsic:
        case WebCore::LengthType::MinContent:
        case WebCore::LengthType::MaxContent:
        case WebCore::LengthType::FillAvailable:
        case WebCore::LengthType::FitContent:
        case WebCore::LengthType::Calculated:
            return true;

        case WebCore::LengthType::Normal:
        case WebCore::LengthType::Relative:
        case WebCore::LengthType::Undefined:
            break;
        }
        return false;
    }

    WebCore::Length m_value;
};

// MARK: - Conversion

FlexBasis flexBasisFromCSSValue(const CSSValue&, BuilderState&);

// MARK: - Evaluation

template<> struct Evaluation<FlexBasis> {
    auto operator()(const FlexBasis& edge, LayoutUnit referenceLength) -> LayoutUnit
    {
        return valueForLength(edge.m_value, referenceLength);
    }

    auto operator()(const FlexBasis& edge, int referenceLength) -> int
    {
        return static_cast<int>(valueForLength(edge.m_value, referenceLength));
    }

    auto operator()(const FlexBasis& edge, float referenceLength) -> float
    {
        return floatValueForLength(edge.m_value, referenceLength);
    }
};

inline LayoutUnit evaluateMinimum(const FlexBasis& edge, NOESCAPE const Invocable<LayoutUnit()> auto& lazyMaximumValueFunctor)
{
    return minimumValueForLengthWithLazyMaximum<LayoutUnit, LayoutUnit>(edge.m_value, lazyMaximumValueFunctor);
}

inline LayoutUnit evaluateMinimum(const FlexBasis& edge, LayoutUnit maximumValue)
{
    return minimumValueForLength(edge.m_value, maximumValue);
}

// MARK: - Blending

template<> struct Blending<FlexBasis> {
    auto canBlend(const FlexBasis&, const FlexBasis&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const FlexBasis&, const FlexBasis&) -> bool;
    auto blend(const FlexBasis&, const FlexBasis&, const BlendingContext&) -> FlexBasis;
};

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const FlexBasis&);

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::FlexBasis> = true;
