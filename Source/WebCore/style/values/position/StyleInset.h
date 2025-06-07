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

// <'top'>/<'right'>/<'bottom'>/<'left'> = auto | <length-percentage>
// https://drafts.csswg.org/css-position/#insets
struct InsetEdge {
    using Specified = LengthPercentage<>;
    using Fixed = typename Specified::Dimension;
    using Percentage = typename Specified::Percentage;
    using Calc = typename Specified::Calc;

    enum class Type : uint8_t { Auto, Fixed, Percent, Calculated };

    explicit InsetEdge(WebCore::Length&& value)
        : m_value { WTFMove(value) }
    {
        RELEASE_ASSERT(m_value.isSpecified() || m_value.isAuto());
    }

    InsetEdge(CSS::Keyword::Auto)
        : m_value { WebCore::LengthType::Auto }
    {
    }

    InsetEdge(CSS::ValueLiteral<CSS::LengthUnit::Px> pixels)
        : m_value { pixels.value, WebCore::LengthType::Fixed }
    {
    }

    InsetEdge(CSS::ValueLiteral<CSS::PercentageUnit::Percentage> percentage)
        : m_value { percentage.value, WebCore::LengthType::Percent }
    {
    }

    InsetEdge(Fixed pixels)
        : m_value { pixels.value, WebCore::LengthType::Fixed }
    {
    }

    InsetEdge(Percentage percentage)
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

    Type type() const
    {
        switch (m_value.type()) {
        case WebCore::LengthType::Auto:
            return Type::Auto;
        case WebCore::LengthType::Fixed:
            return Type::Fixed;
        case WebCore::LengthType::Percent:
            return Type::Percent;
        case WebCore::LengthType::Calculated:
            return Type::Calculated;
        default:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

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

    bool operator==(const InsetEdge&) const = default;

private:
    friend struct Blending<InsetEdge>;
    friend struct Evaluation<InsetEdge>;
    friend LayoutUnit evaluateMinimum(const InsetEdge&, NOESCAPE const Invocable<LayoutUnit()> auto&);
    friend LayoutUnit evaluateMinimum(const InsetEdge&, LayoutUnit);
    friend WTF::TextStream& operator<<(WTF::TextStream&, const InsetEdge&);

    WebCore::Length m_value;
};

// <'inset'> = <'top'>{1,4}
// https://drafts.csswg.org/css-position/#propdef-inset
using InsetBox = MinimallySerializingSpaceSeparatedRectEdges<InsetEdge>;

// MARK: - Conversion

InsetEdge insetEdgeFromCSSValue(const CSSValue&, BuilderState&);

// MARK: - Evaluation

template<> struct Evaluation<InsetEdge> {
    auto operator()(const InsetEdge& edge, LayoutUnit referenceLength) -> LayoutUnit
    {
        return valueForLength(edge.m_value, referenceLength);
    }

    auto operator()(const InsetEdge& edge, int referenceLength) -> int
    {
        return static_cast<int>(valueForLength(edge.m_value, referenceLength));
    }

    auto operator()(const InsetEdge& edge, float referenceLength) -> float
    {
        return floatValueForLength(edge.m_value, referenceLength);
    }
};

inline LayoutUnit evaluateMinimum(const InsetEdge& edge, NOESCAPE const Invocable<LayoutUnit()> auto& lazyMaximumValueFunctor)
{
    return minimumValueForLengthWithLazyMaximum<LayoutUnit, LayoutUnit>(edge.m_value, lazyMaximumValueFunctor);
}

inline LayoutUnit evaluateMinimum(const InsetEdge& edge, LayoutUnit maximumValue)
{
    return minimumValueForLength(edge.m_value, maximumValue);
}

// MARK: - Blending

template<> struct Blending<InsetEdge> {
    auto canBlend(const InsetEdge&, const InsetEdge&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const InsetEdge&, const InsetEdge&) -> bool;
    auto blend(const InsetEdge&, const InsetEdge&, const BlendingContext&) -> InsetEdge;
};

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const InsetEdge&);

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::InsetEdge> = true;
