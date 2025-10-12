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

#include <WebCore/StyleLengthWrapper.h>

namespace WebCore {
namespace Style {

struct SVGBaselineShiftLength : LengthWrapperBase<LengthPercentage<>> {
    using Base::Base;
};

// <'baseline-shift'> = baseline | sub | super | <length-percentage>
struct SVGBaselineShift {
    using Length = SVGBaselineShiftLength;

    SVGBaselineShift(CSS::Keyword::Baseline keyword)
        : m_value { keyword }
    {
    }

    SVGBaselineShift(CSS::Keyword::Sub keyword)
        : m_value { keyword }
    {
    }

    SVGBaselineShift(CSS::Keyword::Super keyword)
        : m_value { keyword }
    {
    }

    SVGBaselineShift(Length&& length)
        : m_value { WTFMove(length) }
    {
    }

    bool isBaseline() const { return WTF::holdsAlternative<CSS::Keyword::Baseline>(m_value); }
    bool isSub() const { return WTF::holdsAlternative<CSS::Keyword::Sub>(m_value); }
    bool isSuper() const { return WTF::holdsAlternative<CSS::Keyword::Super>(m_value); }
    bool isLength() const { return WTF::holdsAlternative<Length>(m_value); }
    std::optional<Length> tryLength() const { return isLength() ? std::make_optional(std::get<Length>(m_value)) : std::nullopt; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const SVGBaselineShift&) const = default;

private:
    friend struct Blending<SVGBaselineShift>;

    Variant<CSS::Keyword::Baseline, CSS::Keyword::Sub, CSS::Keyword::Super, SVGBaselineShiftLength> m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<SVGBaselineShift> { auto operator()(BuilderState&, const CSSValue&) -> SVGBaselineShift; };

// MARK: - Blending

template<> struct Blending<SVGBaselineShift> {
    auto canBlend(const SVGBaselineShift&, const SVGBaselineShift&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const SVGBaselineShift&, const SVGBaselineShift&) -> bool;
    auto blend(const SVGBaselineShift&, const SVGBaselineShift&, const BlendingContext&) -> SVGBaselineShift;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SVGBaselineShiftLength)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SVGBaselineShift)
