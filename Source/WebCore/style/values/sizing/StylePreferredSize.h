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

struct FlexBasis;
struct MaximumSize;
struct MinimumSize;

// <'width'>/<'height'> = auto | <length-percentage [0,∞]> | min-content | max-content | fit-content(<length-percentage [0,∞]>) | <calc-size()> | stretch | fit-content | contain

// What is actually implemented is:

// <'width'>/<'height'> = auto | <length-percentage [0,∞]> | min-content | max-content | fit-content | intrinsic | min-intrinsic | -webkit-fill-available

// MISSING:
//    fit-content(<length-percentage [0,∞]>)
//    <calc-size()>
//    stretch
//    contain

// NON-STANDARD:
//    intrinsic
//    min-intrinsic
//    -webkit-fill-available

// https://drafts.csswg.org/css-sizing-3/#preferred-size-properties
// https://drafts.csswg.org/css-sizing-4/#sizing-values (additional values added)
struct PreferredSize {
    using Fixed = LengthPercentage<CSS::Nonnegative>::Dimension;
    using Percentage = LengthPercentage<CSS::Nonnegative>::Percentage;
    using Calc = LengthPercentage<CSS::Nonnegative>::Calc;

    PreferredSize() : PreferredSize(CSS::Keyword::Auto { }) { }
    PreferredSize(CSS::Keyword::Auto) : m_value(WebCore::LengthType::Auto) { }

    PreferredSize(CSS::Keyword::MinContent) : m_value(WebCore::LengthType::MinContent) { }
    PreferredSize(CSS::Keyword::MaxContent) : m_value(WebCore::LengthType::MaxContent) { }
    PreferredSize(CSS::Keyword::FitContent) : m_value(WebCore::LengthType::FitContent) { }
    PreferredSize(CSS::Keyword::WebkitFillAvailable) : m_value(WebCore::LengthType::FillAvailable) { }
    PreferredSize(CSS::Keyword::Intrinsic) : m_value(WebCore::LengthType::Intrinsic) { }
    PreferredSize(CSS::Keyword::MinIntrinsic) : m_value(WebCore::LengthType::MinIntrinsic) { }

    PreferredSize(Fixed&& fixed) : m_value(fixed.value, WebCore::LengthType::Fixed) { }
    PreferredSize(const Fixed& fixed) : m_value(fixed.value, WebCore::LengthType::Fixed) { }
    PreferredSize(Percentage&& percent) : m_value(percent.value, WebCore::LengthType::Percent) { }
    PreferredSize(const Percentage& percent) : m_value(percent.value, WebCore::LengthType::Percent) { }

    PreferredSize(CSS::ValueLiteral<CSS::LengthUnit::Px> literal) : m_value(static_cast<float>(literal.value), WebCore::LengthType::Fixed) { }
    PreferredSize(CSS::ValueLiteral<CSS::PercentageUnit::Percentage> literal) : m_value(static_cast<float>(literal.value), WebCore::LengthType::Percent) { }

    PreferredSize(WebCore::Length&& other) : m_value(WTFMove(other)) { RELEASE_ASSERT(isValid(m_value)); }
    PreferredSize(const WebCore::Length& other) : m_value(other) { RELEASE_ASSERT(isValid(m_value)); }

    bool operator==(const PreferredSize&) const = default;

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

        case WebCore::LengthType::Content:
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

        case WebCore::LengthType::Auto:
        case WebCore::LengthType::Intrinsic:
        case WebCore::LengthType::MinIntrinsic:
        case WebCore::LengthType::MinContent:
        case WebCore::LengthType::MaxContent:
        case WebCore::LengthType::FillAvailable:
        case WebCore::LengthType::FitContent:
        case WebCore::LengthType::Calculated:
            return true;

        case WebCore::LengthType::Content:
        case WebCore::LengthType::Normal:
        case WebCore::LengthType::Relative:
        case WebCore::LengthType::Undefined:
            break;
        }
        return false;
    }

    const WebCore::Length& value() const { return m_value; }

private:
    friend FlexBasis;
    friend MaximumSize;
    friend MinimumSize;

    WebCore::Length m_value;
};

// MARK: - Conversion

// FIXME: There is no CSS::PreferredSize yet, so direct conversion from CSSValue is provided instead.

template<> struct CSSValueConversions<PreferredSize> { auto operator()(BuilderState&, const CSSValue&) -> PreferredSize; };
template<> struct CSSValueCreation<PreferredSize> { auto operator()(const PreferredSize&, const RenderStyle&) -> Ref<CSSValue>; };

// MARK: - Evaluation

template<> struct Evaluation<PreferredSize> {
    auto operator()(const PreferredSize&, double) -> double;
    auto operator()(const PreferredSize&, float) -> float;
    auto operator()(const PreferredSize&, LayoutUnit) -> LayoutUnit;
};

LayoutUnit evaluateMinimum(const PreferredSize&, LayoutUnit maximumValue);

// MARK: - Blending

template<> struct Blending<PreferredSize> {
    auto canBlend(const PreferredSize&, const PreferredSize&) -> bool;
    auto blend(const PreferredSize&, const PreferredSize&, const BlendingContext&) -> PreferredSize;
};

// MARK: - TextStream

WTF::TextStream& operator<<(WTF::TextStream&, PreferredSize);

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::PreferredSize> = true;
