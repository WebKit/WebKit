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

#include <WebCore/StyleLengthWrapper.h>

namespace WebCore {
namespace Style {

struct BorderImageWidthValueLength : LengthWrapperBase<LengthPercentage<CSS::Nonnegative>> {
    using Base::Base;
};

// <border-image-width-value> = <length-percentage [0,∞]> | <number [0,∞]> | auto
struct BorderImageWidthValue {
    using LengthPercentage = BorderImageWidthValueLength;
    using Number = Style::Number<CSS::Nonnegative, float>;

    BorderImageWidthValue(CSS::Keyword::Auto keyword)
        : m_value { keyword }
    {
    }

    BorderImageWidthValue(LengthPercentage&& length)
        : m_value { WTFMove(length) }
    {
    }

    BorderImageWidthValue(Number number)
        : m_value { number }
    {
    }

    bool isAuto() const { return WTF::holdsAlternative<CSS::Keyword::Auto>(m_value); }
    bool isLengthPercentage() const { return WTF::holdsAlternative<LengthPercentage>(m_value); }
    bool isNumber() const { return WTF::holdsAlternative<Number>(m_value); }

    std::optional<LengthPercentage::Fixed> tryFixed() const
    {
        if (auto* length = std::get_if<LengthPercentage>(&m_value))
            return length->tryFixed();
        return { };
    }

    bool isFixed() const
    {
        if (auto* length = std::get_if<LengthPercentage>(&m_value))
            return length->isFixed();
        return false;
    }

    bool isCalculated() const
    {
        if (auto* length = std::get_if<LengthPercentage>(&m_value))
            return length->isCalculated();
        return false;
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const BorderImageWidthValue&) const = default;

    bool hasSameType(const BorderImageWidthValue& other) const { return m_value.index() == other.m_value.index(); }

private:
    friend struct Blending<BorderImageWidthValue>;

    Variant<CSS::Keyword::Auto, LengthPercentage, Number> m_value { Number { 1 } };
};

// <'border-image-width'> = [ <length-percentage [0,∞]> | <number [0,∞]> | auto ]{1,4}
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image-width
struct BorderImageWidth {
    MinimallySerializingSpaceSeparatedRectEdges<BorderImageWidthValue> values { BorderImageWidthValue::Number { 1 } };
    bool legacyWebkitBorderImage { false };

    bool operator==(const BorderImageWidth&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(BorderImageWidth, values);

// MARK: - Conversion

template<> struct CSSValueConversion<BorderImageWidth> { auto operator()(BuilderState&, const CSSValue&) -> BorderImageWidth; };
template<> struct CSSValueCreation<BorderImageWidth> { auto operator()(CSSValuePool&, const RenderStyle&, const BorderImageWidth&) -> Ref<CSSValue>; };

// MARK: - Blending

template<> struct Blending<BorderImageWidthValue> {
    auto canBlend(const BorderImageWidthValue&, const BorderImageWidthValue&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const BorderImageWidthValue&, const BorderImageWidthValue&) -> bool;
    auto blend(const BorderImageWidthValue&, const BorderImageWidthValue&, const BlendingContext&) -> BorderImageWidthValue;
};

template<> struct Blending<BorderImageWidth> {
    auto canBlend(const BorderImageWidth&, const BorderImageWidth&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const BorderImageWidth&, const BorderImageWidth&) -> bool;
    auto blend(const BorderImageWidth&, const BorderImageWidth&, const BlendingContext&) -> BorderImageWidth;
};

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::BorderImageWidth)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::BorderImageWidthValueLength)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::BorderImageWidthValue)
