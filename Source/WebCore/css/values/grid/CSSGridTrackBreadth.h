/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/CSSPrimitiveNumericTypes.h>

namespace WebCore {
namespace CSS {

// <track-breadth> = <length-percentage [0,∞]> | <flex [0,∞]> | min-content | max-content | auto
// https://drafts.csswg.org/css-grid/#typedef-track-breadth
struct GridTrackBreadth {
    GridTrackBreadth(LengthPercentage<Nonnegative>&& lengthPercentage) : m_value { WTF::move(lengthPercentage) } { }
    GridTrackBreadth(Flex<Nonnegative>&& flex) : m_value { WTF::move(flex) } { }
    GridTrackBreadth(Keyword::MinContent keyword) : m_value { keyword } { }
    GridTrackBreadth(Keyword::MaxContent keyword) : m_value { keyword } { }
    GridTrackBreadth(Keyword::Auto keyword) : m_value { keyword } { }

    bool isLengthPercentage() const { return WTF::holdsAlternative<LengthPercentage<Nonnegative>>(m_value); }
    bool isFlex() const { return WTF::holdsAlternative<Flex<Nonnegative>>(m_value); }
    bool isMinContent() const { return WTF::holdsAlternative<Keyword::MinContent>(m_value); }
    bool isMaxContent() const { return WTF::holdsAlternative<Keyword::MaxContent>(m_value); }
    bool isAuto() const { return WTF::holdsAlternative<Keyword::Auto>(m_value); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const GridTrackBreadth&) const = default;

private:
    Variant<LengthPercentage<Nonnegative>, Flex<Nonnegative>, Keyword::MinContent, Keyword::MaxContent, Keyword::Auto> m_value;
};

} // namespace CSS
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::GridTrackBreadth)
