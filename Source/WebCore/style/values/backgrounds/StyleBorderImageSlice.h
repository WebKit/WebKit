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

// <border-image-slice-value> = <number [0,∞]> | <percentage [0,∞]>
struct BorderImageSliceValue {
    using Number = Style::Number<CSS::Nonnegative, float>;
    using Percentage = Style::Percentage<CSS::Nonnegative, float>;

    BorderImageSliceValue(Number number)
        : m_value { number }
    {
    }

    BorderImageSliceValue(Percentage percentage)
        : m_value { percentage }
    {
    }

    bool isNumber() const { return WTF::holdsAlternative<Number>(m_value); }
    bool isPercentage() const { return WTF::holdsAlternative<Percentage>(m_value); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const BorderImageSliceValue&) const = default;

    bool hasSameType(const BorderImageSliceValue& other) const { return m_value.index() == other.m_value.index(); }

private:
    friend struct Blending<BorderImageSliceValue>;

    Variant<Number, Percentage> m_value { Percentage { 100 } };
};

// <'border-image-slice'> = [<number [0,∞]> | <percentage [0,∞]>]{1,4} && fill?
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image-slice
struct BorderImageSlice {
    MinimallySerializingSpaceSeparatedRectEdges<BorderImageSliceValue> values { BorderImageSliceValue::Percentage { 100 } };
    std::optional<CSS::Keyword::Fill> fill { };

    bool operator==(const BorderImageSlice&) const = default;
};
template<size_t I> const auto& get(const BorderImageSlice& value)
{
    if constexpr (!I)
        return value.values;
    else if constexpr (I == 1)
        return value.fill;
}

// MARK: - Conversion

template<> struct CSSValueConversion<BorderImageSlice> { auto operator()(BuilderState&, const CSSValue&) -> BorderImageSlice; };
template<> struct CSSValueCreation<BorderImageSlice> { auto operator()(CSSValuePool&, const RenderStyle&, const BorderImageSlice&) -> Ref<CSSValue>; };

// MARK: - Blending

template<> struct Blending<BorderImageSliceValue> {
    auto canBlend(const BorderImageSliceValue&, const BorderImageSliceValue&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const BorderImageSliceValue&, const BorderImageSliceValue&) -> bool;
    auto blend(const BorderImageSliceValue&, const BorderImageSliceValue&, const BlendingContext&) -> BorderImageSliceValue;
};

template<> struct Blending<BorderImageSlice> {
    auto canBlend(const BorderImageSlice&, const BorderImageSlice&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const BorderImageSlice&, const BorderImageSlice&) -> bool;
    auto blend(const BorderImageSlice&, const BorderImageSlice&, const BlendingContext&) -> BorderImageSlice;
};

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::BorderImageSlice, 2)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::BorderImageSliceValue)
