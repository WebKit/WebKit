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

#include "BoxExtents.h"
#include "Length.h"
#include "StylePrimitiveNumericTypes.h"
#include "StyleValueTypes.h"

namespace WebCore {

class CSSValue;
class LayoutRect;
class LayoutUnit;
class RenderStyle;

namespace Style {

class BuilderState;
struct ExtractorState;

// <'scroll-padding-*'> = auto | <length-percentage [0,∞]>
// https://drafts.csswg.org/css-scroll-snap-1/#padding-longhands-physical
struct ScrollPaddingEdge {
    using Specified = LengthPercentage<CSS::Nonnegative>;
    using Fixed = typename Specified::Dimension;
    using Percentage = typename Specified::Percentage;
    using Calc = typename Specified::Calc;

    explicit ScrollPaddingEdge(WebCore::Length&& value)
        : m_value { WTFMove(value) }
    {
        RELEASE_ASSERT(m_value.isSpecified() || m_value.isAuto());
    }

    ScrollPaddingEdge(CSS::Keyword::Auto)
        : m_value { WebCore::LengthType::Auto }
    {
    }

    ScrollPaddingEdge(CSS::ValueLiteral<CSS::LengthUnit::Px> pixels)
        : m_value { pixels.value, WebCore::LengthType::Fixed }
    {
    }

    ScrollPaddingEdge(CSS::ValueLiteral<CSS::PercentageUnit::Percentage> percentage)
        : m_value { percentage.value, WebCore::LengthType::Percent }
    {
    }

    ScrollPaddingEdge(Fixed pixels)
        : m_value { pixels.value, WebCore::LengthType::Fixed }
    {
    }

    ScrollPaddingEdge(Percentage percentage)
        : m_value { percentage.value, WebCore::LengthType::Percent }
    {
    }

    bool isZero() const { return m_value.isZero(); }

    bool isAuto() const { return m_value.isAuto(); }
    bool isFixed() const { return m_value.isFixed(); }
    bool isPercent() const { return m_value.isPercent(); }
    bool isCalculated() const { return m_value.isCalculated(); }
    bool isPercentOrCalculated() const { return m_value.isPercentOrCalculated(); }
    bool isSpecified() const { return m_value.isSpecified(); }

    std::optional<Fixed> tryFixed() const { return isFixed() ? std::make_optional(Fixed { m_value.value() }) : std::nullopt; }
    std::optional<Percentage> tryPercentage() const { return isPercent() ? std::make_optional(Percentage { m_value.value() }) : std::nullopt; }
    std::optional<Calc> tryCalc() const { return isCalculated() ? std::make_optional(Calc { m_value }) : std::nullopt; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_value.type()) {
        case WebCore::LengthType::Auto:
            return visitor(CSS::Keyword::Auto { });
        case WebCore::LengthType::Fixed:
            return visitor(Fixed { m_value.value() });
        case WebCore::LengthType::Percent:
            return visitor(Percentage { m_value.value() });
        case WebCore::LengthType::Calculated:
            return visitor(Calc { m_value });
        default:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const ScrollPaddingEdge&) const = default;

private:
    friend struct Evaluation<ScrollPaddingEdge>;
    friend WTF::TextStream& operator<<(WTF::TextStream&, const ScrollPaddingEdge&);

    WebCore::Length m_value;
};

// <'scroll-padding'> = [ auto | <length-percentage [0,∞]> ]{1,4}
// https://drafts.csswg.org/css-scroll-snap-1/#propdef-scroll-padding
using ScrollPaddingBox = MinimallySerializingSpaceSeparatedRectEdges<ScrollPaddingEdge>;

// MARK: - Conversion

ScrollPaddingEdge scrollPaddingEdgeFromCSSValue(const CSSValue&, BuilderState&);

// MARK: - Evaluation

template<> struct Evaluation<ScrollPaddingEdge> {
    auto operator()(const ScrollPaddingEdge&, LayoutUnit referenceLength) -> LayoutUnit;
    auto operator()(const ScrollPaddingEdge&, float referenceLength) -> float;
};

// MARK: - Extent

LayoutBoxExtent extentForRect(const ScrollPaddingBox&, const LayoutRect&);

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const ScrollPaddingEdge&);

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::ScrollPaddingEdge> = true;
