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
#include "StyleSizing.h"

namespace WebCore {
namespace Style {

struct PreferredSize;

// <'flex-basis'> = content | <'width'>
// https://drafts.csswg.org/css-flexbox-1/#flex-basis-property
struct FlexBasis {
    using Fixed = LengthPercentage<CSS::Nonnegative>::Dimension;
    using Percentage = LengthPercentage<CSS::Nonnegative>::Percentage;
    using Calc = LengthPercentage<CSS::Nonnegative>::Calc;

    FlexBasis() : FlexBasis(CSS::Keyword::Auto { }) { }
    FlexBasis(CSS::Keyword::Auto) : m_value(WebCore::LengthType::Auto) { }
    FlexBasis(CSS::Keyword::Content) : m_value(WebCore::LengthType::Content) { }

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

    FlexBasis(WebCore::Length&& other) : m_value(WTFMove(other)) { RELEASE_ASSERT(isValid(m_value)); }
    FlexBasis(const WebCore::Length& other) : m_value(other) { RELEASE_ASSERT(isValid(m_value)); }

    bool operator==(const FlexBasis&) const = default;

    // `FlexBasis` is a strict superset of `PreferredSize` so `FlexBasis` can always be constructed from one.
    explicit FlexBasis(PreferredSize&&);
    explicit FlexBasis(const PreferredSize&);
    // However, to go the other way, a value of `content` must be transformed to `max-content`.
    PreferredSize asPreferredSize() const;

    std::optional<Fixed> fixed() const
    {
        if (!isFixed())
            return { };
        return Fixed { m_value.value() };
    }

    std::optional<Percentage> percentage() const
    {
        if (!isPercent())
            return { };
        return Percentage { m_value.value() };
    }

    std::optional<Calc> calc() const
    {
        if (!isCalculated())
            return { };
        return Calc { m_value };
    }

    ALWAYS_INLINE bool isFixed() const { return m_value.isFixed(); }
    ALWAYS_INLINE bool isDimension() const { return m_value.isFixed(); }
    ALWAYS_INLINE bool isPercent() const { return m_value.isPercent(); }
    ALWAYS_INLINE bool isCalculated() const { return m_value.isCalculated(); }

    ALWAYS_INLINE bool isPercentOrCalculated() const { return isPercent() || isCalculated(); }
    ALWAYS_INLINE bool isSpecified() const { return isFixed() || isPercent() || isCalculated(); }

    ALWAYS_INLINE bool isContent() const { return m_value.isContent(); }
    ALWAYS_INLINE bool isAuto() const { return m_value.isAuto(); }

    ALWAYS_INLINE bool isMinContent() const { return m_value.isMinContent(); }
    ALWAYS_INLINE bool isMaxContent() const { return m_value.isMaxContent(); }
    ALWAYS_INLINE bool isFitContent() const { return m_value.isFitContent(); }
    ALWAYS_INLINE bool isFillAvailable() const { return m_value.isFillAvailable(); }
    ALWAYS_INLINE bool isIntrinsicKeyword() const { return m_value.isIntrinsicKeyword(); }
    ALWAYS_INLINE bool isMinIntrinsic() const { return m_value.isMinIntrinsic(); }

    // FIXME: This is misleadingly named. One would expect this function does `holdsAlternative<Intrinsic>()`.
    ALWAYS_INLINE bool isIntrinsic() const { return isMinContent() || isMaxContent() || isFillAvailable() || isFitContent(); }
    ALWAYS_INLINE bool isLegacyIntrinsic() const { return isIntrinsicKeyword() || isMinIntrinsic(); }
    ALWAYS_INLINE bool isSpecifiedOrIntrinsic() const { return isSpecified() || isIntrinsic(); }

    // For the following three functions, attempt to match the behaviors of the ones in WebCore::Length.
    ALWAYS_INLINE bool isZero() const { return m_value.isZero(); }
    ALWAYS_INLINE bool isPositive() const { return m_value.isPositive(); }
    ALWAYS_INLINE bool isNegative() const { return m_value.isNegative(); }

    // FIXME: Remove this. Its only currently used to do a quick "these are not the same" check in the blend code.
    ALWAYS_INLINE WebCore::LengthType type() const { return m_value.type(); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_value.type()) {
        case WebCore::LengthType::Percent:
            return visitor(Percentage { m_value.value() });
        case WebCore::LengthType::Fixed:
            return visitor(Fixed { m_value.value() });
        case WebCore::LengthType::Calculated:
            return visitor(Calc { m_value });
        case WebCore::LengthType::Content:
            return visitor(CSS::Keyword::Content { });
        case WebCore::LengthType::Auto:
            return visitor(CSS::Keyword::Auto { });
        case WebCore::LengthType::Intrinsic:
            return visitor(CSS::Keyword::Intrinsic { });
        case WebCore::LengthType::MinIntrinsic:
            return visitor(CSS::Keyword::MinIntrinsic { });
        case WebCore::LengthType::MinContent:
            return visitor(CSS::Keyword::MinContent { });
        case WebCore::LengthType::MaxContent:
            return visitor(CSS::Keyword::MaxContent { });
        case WebCore::LengthType::FillAvailable:
            return visitor(CSS::Keyword::WebkitFillAvailable { });
        case WebCore::LengthType::FitContent:
            return visitor(CSS::Keyword::FitContent { });

        case WebCore::LengthType::Normal:
        case WebCore::LengthType::Relative:
        case WebCore::LengthType::Undefined:
            break;
        }

        RELEASE_ASSERT_NOT_REACHED();
    }

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

    const WebCore::Length& value() const { return m_value; }

private:
    WebCore::Length m_value;
};

// MARK: - Conversion

// FIXME: There is no CSS::FlexBasis yet, so direct conversion from CSSValue is provided instead.

template<> struct CSSValueConversions<FlexBasis> { auto operator()(BuilderState&, const CSSValue&) -> FlexBasis; };
template<> struct CSSValueCreation<FlexBasis> { auto operator()(const FlexBasis&, const RenderStyle&) -> Ref<CSSValue>; };

// MARK: - Evaluation

template<> struct Evaluation<FlexBasis> {
    auto operator()(const FlexBasis&, double) -> double;
    auto operator()(const FlexBasis&, float) -> float;
    auto operator()(const FlexBasis&, LayoutUnit) -> LayoutUnit;
};

LayoutUnit evaluateMinimum(const FlexBasis&, LayoutUnit maximumValue);

// MARK: - Blending

template<> struct Blending<FlexBasis> {
    auto canBlend(const FlexBasis&, const FlexBasis&) -> bool;
    auto blend(const FlexBasis&, const FlexBasis&, const BlendingContext&) -> FlexBasis;
};

// MARK: - TextStream

WTF::TextStream& operator<<(WTF::TextStream&, FlexBasis);

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::FlexBasis> = true;
