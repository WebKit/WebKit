/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/StylePrimitiveNumeric.h>

namespace WebCore {
namespace Style {

// Non-SVG (box) baseline-shift. Superset of the SVG `baseline-shift` grammar per
// CSS Inline 3: it adds the line-relative keywords `top`/`center`/`bottom`.
//
// <baseline-shift> = sub | super | top | center | bottom | <length-percentage>
// https://drafts.csswg.org/css-inline-3/#propdef-baseline-shift
//
// FIXME: The initial value is the legacy `baseline` keyword rather than a zero
// <length-percentage>, which the spec grammar above does not allow (it is the SVG
// spelling for "no shift"). It is retained as a sentinel so that `baseline` and a
// zero shift stay distinguishable, preserving the CSS 2 `vertical-align` computed
// value: without it, `vertical-align: 0px` / `0%` would collapse into and report as
// `baseline`. Removing the sentinel is an observable change that should be taken to
// the CSS Working Group first.
struct BaselineShift {
    using LengthPercentage = Style::LengthPercentage<>;

    BaselineShift(CSS::Keyword::Baseline keyword)
        : m_value { keyword }
    {
    }

    BaselineShift(CSS::Keyword::Sub keyword)
        : m_value { keyword }
    {
    }

    BaselineShift(CSS::Keyword::Super keyword)
        : m_value { keyword }
    {
    }

    BaselineShift(CSS::Keyword::Top keyword)
        : m_value { keyword }
    {
    }

    BaselineShift(CSS::Keyword::Center keyword)
        : m_value { keyword }
    {
    }

    BaselineShift(CSS::Keyword::Bottom keyword)
        : m_value { keyword }
    {
    }

    BaselineShift(LengthPercentage&& length)
        : m_value { WTF::move(length) }
    {
    }

    BaselineShift(const LengthPercentage& length)
        : m_value { length }
    {
    }

    bool isBaseline() const { return WTF::holdsAlternative<CSS::Keyword::Baseline>(m_value); }
    bool isSub() const { return WTF::holdsAlternative<CSS::Keyword::Sub>(m_value); }
    bool isSuper() const { return WTF::holdsAlternative<CSS::Keyword::Super>(m_value); }
    bool isTop() const { return WTF::holdsAlternative<CSS::Keyword::Top>(m_value); }
    bool isCenter() const { return WTF::holdsAlternative<CSS::Keyword::Center>(m_value); }
    bool isBottom() const { return WTF::holdsAlternative<CSS::Keyword::Bottom>(m_value); }
    bool isLengthPercentage() const { return WTF::holdsAlternative<LengthPercentage>(m_value); }
    std::optional<LengthPercentage> tryLengthPercentage() const { return isLengthPercentage() ? std::make_optional(std::get<LengthPercentage>(m_value)) : std::nullopt; }

    // The line-relative keywords (`top`/`center`/`bottom`) shift the inline box and its
    // contents relative to the line box rather than relative to the baseline.
    bool isLineRelative() const { return isTop() || isCenter() || isBottom(); }

    template<typename U> bool holdsAlternative() const
    {
        return WTF::holdsAlternative<U>(m_value);
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const BaselineShift&) const = default;

private:
    friend struct Blending<BaselineShift>;

    Variant<CSS::Keyword::Baseline, CSS::Keyword::Sub, CSS::Keyword::Super, CSS::Keyword::Top, CSS::Keyword::Center, CSS::Keyword::Bottom, LengthPercentage> m_value;
};

// MARK: - Blending

template<> struct Blending<BaselineShift> {
    bool NODELETE canBlend(const BaselineShift&, const BaselineShift&);
    auto requiresInterpolationForAccumulativeIteration(const BaselineShift&, const BaselineShift&) -> bool;
    auto blend(const BaselineShift&, const BaselineShift&, const BlendingContext&) -> BaselineShift;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::BaselineShift)
