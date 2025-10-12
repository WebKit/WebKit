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

struct MaskBorderWidthValueLength : LengthWrapperBase<LengthPercentage<CSS::Nonnegative>> {
    using Base::Base;
};

// <mask-border-width-value> = <length-percentage [0,∞]> | <number [0,∞]> | auto
struct MaskBorderWidthValue {
    using LengthPercentage = MaskBorderWidthValueLength;
    using Number = Style::Number<CSS::Nonnegative, float>;

    MaskBorderWidthValue(CSS::Keyword::Auto keyword)
        : m_value { keyword }
    {
    }

    MaskBorderWidthValue(LengthPercentage&& length)
        : m_value { WTFMove(length) }
    {
    }

    MaskBorderWidthValue(Number number)
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

    bool operator==(const MaskBorderWidthValue&) const = default;

    bool hasSameType(const MaskBorderWidthValue& other) const { return m_value.index() == other.m_value.index(); }

private:
    friend struct Blending<MaskBorderWidthValue>;

    Variant<CSS::Keyword::Auto, LengthPercentage, Number> m_value { CSS::Keyword::Auto { } };
};

// <'mask-border-width'> = [ <length-percentage [0,∞]> | <number [0,∞]> | auto ]{1,4}
// https://drafts.fxtf.org/css-masking-1/#propdef-mask-border-width
struct MaskBorderWidth {
    MinimallySerializingSpaceSeparatedRectEdges<MaskBorderWidthValue> values { CSS::Keyword::Auto { } };

    bool operator==(const MaskBorderWidth&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(MaskBorderWidth, values);

// MARK: - Conversion

template<> struct CSSValueConversion<MaskBorderWidth> { auto operator()(BuilderState&, const CSSValue&) -> MaskBorderWidth; };
template<> struct CSSValueCreation<MaskBorderWidth> { auto operator()(CSSValuePool&, const RenderStyle&, const MaskBorderWidth&) -> Ref<CSSValue>; };

// MARK: - Blending

template<> struct Blending<MaskBorderWidthValue> {
    auto canBlend(const MaskBorderWidthValue&, const MaskBorderWidthValue&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const MaskBorderWidthValue&, const MaskBorderWidthValue&) -> bool;
    auto blend(const MaskBorderWidthValue&, const MaskBorderWidthValue&, const BlendingContext&) -> MaskBorderWidthValue;
};

template<> struct Blending<MaskBorderWidth> {
    auto canBlend(const MaskBorderWidth&, const MaskBorderWidth&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const MaskBorderWidth&, const MaskBorderWidth&) -> bool;
    auto blend(const MaskBorderWidth&, const MaskBorderWidth&, const BlendingContext&) -> MaskBorderWidth;
};

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::MaskBorderWidth)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::MaskBorderWidthValueLength)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::MaskBorderWidthValue)
