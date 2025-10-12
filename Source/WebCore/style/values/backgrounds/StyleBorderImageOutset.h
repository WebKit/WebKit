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

#include <WebCore/StylePrimitiveNumericTypes.h>

namespace WebCore {
namespace Style {

// <border-image-outset-value> = <length [0,∞]> | <number [0,∞]>
struct BorderImageOutsetValue {
    using Length = Style::Length<CSS::Nonnegative, float>;
    using Number = Style::Number<CSS::Nonnegative, float>;

    BorderImageOutsetValue(Length length)
        : m_value { length }
    {
    }

    BorderImageOutsetValue(Number number)
        : m_value { number }
    {
    }

    bool isLength() const { return WTF::holdsAlternative<Length>(m_value); }
    bool isNumber() const { return WTF::holdsAlternative<Number>(m_value); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const BorderImageOutsetValue&) const = default;

    bool hasSameType(const BorderImageOutsetValue& other) const { return m_value.index() == other.m_value.index(); }

private:
    friend struct Blending<BorderImageOutsetValue>;

    Variant<Length, Number> m_value { Number { 0 } };
};

// <'border-image-outset'> = [ <length [0,∞]> | <number [0,∞]> ]{1,4}
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image-outset
struct BorderImageOutset {
    MinimallySerializingSpaceSeparatedRectEdges<BorderImageOutsetValue> values { BorderImageOutsetValue { BorderImageOutsetValue::Number { 0 } } };

    bool isZero() const
    {
        return values.allOf([](auto& edge) {
            return WTF::switchOn(edge, [&](auto& value) { return value == 0; });
        });
    }

    bool operator==(const BorderImageOutset&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(BorderImageOutset, values);

// MARK: - Conversion

template<> struct CSSValueConversion<BorderImageOutset> { auto operator()(BuilderState&, const CSSValue&) -> BorderImageOutset; };
template<> struct CSSValueCreation<BorderImageOutset> { auto operator()(CSSValuePool&, const RenderStyle&, const BorderImageOutset&) -> Ref<CSSValue>; };

// MARK: - Blending

template<> struct Blending<BorderImageOutsetValue> {
    auto canBlend(const BorderImageOutsetValue&, const BorderImageOutsetValue&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const BorderImageOutsetValue&, const BorderImageOutsetValue&) -> bool;
    auto blend(const BorderImageOutsetValue&, const BorderImageOutsetValue&, const BlendingContext&) -> BorderImageOutsetValue;
};

template<> struct Blending<BorderImageOutset> {
    auto canBlend(const BorderImageOutset&, const BorderImageOutset&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const BorderImageOutset&, const BorderImageOutset&) -> bool;
    auto blend(const BorderImageOutset&, const BorderImageOutset&, const BlendingContext&) -> BorderImageOutset;
};

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::BorderImageOutset)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::BorderImageOutsetValue)
