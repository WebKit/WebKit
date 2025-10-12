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

struct VerticalAlignLength : LengthWrapperBase<LengthPercentage<>> {
    using Base::Base;
};

// <'vertical-align'> = baseline | sub | super | top | text-top | middle | bottom | text-bottom | -webkit-baseline-middle | <length-percentage>
// https://www.w3.org/TR/CSS22/visudet.html#propdef-vertical-align

// FIXME: We implement the version of `vertical-align` from CSS 2. In CSS Inline 3, it has been changed to a shorthand property with grammar `[ first | last] || <'alignment-baseline'> || <'baseline-shift'>`.
// https://drafts.csswg.org/css-inline/#propdef-vertical-align

struct VerticalAlign {
    using Length = VerticalAlignLength;

    VerticalAlign(CSS::Keyword::Baseline keyword) : m_value { keyword } { }
    VerticalAlign(CSS::Keyword::Sub keyword) : m_value { keyword } { }
    VerticalAlign(CSS::Keyword::Super keyword) : m_value { keyword } { }
    VerticalAlign(CSS::Keyword::Top keyword) : m_value { keyword } { }
    VerticalAlign(CSS::Keyword::TextTop keyword) : m_value { keyword } { }
    VerticalAlign(CSS::Keyword::Middle keyword) : m_value { keyword } { }
    VerticalAlign(CSS::Keyword::Bottom keyword) : m_value { keyword } { }
    VerticalAlign(CSS::Keyword::TextBottom keyword) : m_value { keyword } { }
    VerticalAlign(CSS::Keyword::WebkitBaselineMiddle keyword) : m_value { keyword } { }
    VerticalAlign(Length&& length) : m_value { WTFMove(length) } { }

    bool isBaseline() const { return WTF::holdsAlternative<CSS::Keyword::Baseline>(m_value); }
    bool isSub() const { return WTF::holdsAlternative<CSS::Keyword::Sub>(m_value); }
    bool isSuper() const { return WTF::holdsAlternative<CSS::Keyword::Super>(m_value); }
    bool isTop() const { return WTF::holdsAlternative<CSS::Keyword::Top>(m_value); }
    bool isTextTop() const { return WTF::holdsAlternative<CSS::Keyword::TextTop>(m_value); }
    bool isMiddle() const { return WTF::holdsAlternative<CSS::Keyword::Middle>(m_value); }
    bool isBottom() const { return WTF::holdsAlternative<CSS::Keyword::Bottom>(m_value); }
    bool isTextBottom() const { return WTF::holdsAlternative<CSS::Keyword::TextBottom>(m_value); }
    bool isWebkitBaselineMiddle() const { return WTF::holdsAlternative<CSS::Keyword::WebkitBaselineMiddle>(m_value); }
    bool isLength() const { return WTF::holdsAlternative<Length>(m_value); }
    std::optional<Length> tryLength() const { return isLength() ? std::make_optional(std::get<Length>(m_value)) : std::nullopt; }

    template<typename U> bool holdsAlternative() const
    {
        return WTF::holdsAlternative<U>(m_value);
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const VerticalAlign&) const = default;

private:
    friend struct Blending<VerticalAlign>;

    Variant<CSS::Keyword::Baseline, CSS::Keyword::Sub, CSS::Keyword::Super, CSS::Keyword::Top, CSS::Keyword::TextTop, CSS::Keyword::Middle, CSS::Keyword::Bottom, CSS::Keyword::TextBottom, CSS::Keyword::WebkitBaselineMiddle, VerticalAlignLength> m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<VerticalAlign> { auto operator()(BuilderState&, const CSSValue&) -> VerticalAlign; };

// MARK: - Blending

template<> struct Blending<VerticalAlign> {
    auto canBlend(const VerticalAlign&, const VerticalAlign&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const VerticalAlign&, const VerticalAlign&) -> bool;
    auto blend(const VerticalAlign&, const VerticalAlign&, const BlendingContext&) -> VerticalAlign;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::VerticalAlignLength)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::VerticalAlign)
