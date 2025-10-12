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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StylePrimitiveNumeric.h>

namespace WebCore {
namespace Style {

// <mask-border-slice-value> = <number [0,∞]> | <percentage [0,∞]>
struct MaskBorderSliceValue {
    using Number = Style::Number<CSS::Nonnegative, float>;
    using Percentage = Style::Percentage<CSS::Nonnegative, float>;

    MaskBorderSliceValue(Number number)
        : m_value { number }
    {
    }


    MaskBorderSliceValue(Percentage percentage)
        : m_value { percentage }
    {
    }

    bool isNumber() const { return WTF::holdsAlternative<Number>(m_value); }
    bool isPercentage() const { return WTF::holdsAlternative<Percentage>(m_value); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const MaskBorderSliceValue&) const = default;

    bool hasSameType(const MaskBorderSliceValue& other) const { return m_value.index() == other.m_value.index(); }

private:
    friend struct Blending<MaskBorderSliceValue>;

    Variant<Number, Percentage> m_value { Number { 0 } };
};

// <'mask-border-slice'> = [<number [0,∞]> | <percentage [0,∞]>]{1,4} && fill?
// https://drafts.fxtf.org/css-masking-1/#propdef-mask-border-slice
struct MaskBorderSlice {
    MinimallySerializingSpaceSeparatedRectEdges<MaskBorderSliceValue> values { MaskBorderSliceValue::Number { 0 } };
    std::optional<CSS::Keyword::Fill> fill { };

    bool operator==(const MaskBorderSlice&) const = default;
};
template<size_t I> const auto& get(const MaskBorderSlice& value)
{
    if constexpr (!I)
        return value.values;
    else if constexpr (I == 1)
        return value.fill;
}

// MARK: - Conversion

template<> struct CSSValueConversion<MaskBorderSlice> { auto operator()(BuilderState&, const CSSValue&) -> MaskBorderSlice; };
template<> struct CSSValueCreation<MaskBorderSlice> { auto operator()(CSSValuePool&, const RenderStyle&, const MaskBorderSlice&) -> Ref<CSSValue>; };

// MARK: - Blending

template<> struct Blending<MaskBorderSliceValue> {
    auto canBlend(const MaskBorderSliceValue&, const MaskBorderSliceValue&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const MaskBorderSliceValue&, const MaskBorderSliceValue&) -> bool;
    auto blend(const MaskBorderSliceValue&, const MaskBorderSliceValue&, const BlendingContext&) -> MaskBorderSliceValue;
};

template<> struct Blending<MaskBorderSlice> {
    auto canBlend(const MaskBorderSlice&, const MaskBorderSlice&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const MaskBorderSlice&, const MaskBorderSlice&) -> bool;
    auto blend(const MaskBorderSlice&, const MaskBorderSlice&, const BlendingContext&) -> MaskBorderSlice;
};

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::MaskBorderSlice, 2)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::MaskBorderSliceValue)
