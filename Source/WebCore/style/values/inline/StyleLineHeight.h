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

#include <WebCore/StylePrimitiveNumeric.h>
#include <wtf/Hasher.h>

namespace WebCore {
namespace Style {

// <'line-height'> = normal | <number [0,∞]> | <length-percentage [0,∞]>
// NOTE: <length-percentage [0,∞]> gets converted to <length [0,∞]> at style building time by resolving any percentages against `font-size`.
// https://drafts.csswg.org/css-inline/#propdef-line-height
struct LineHeight {
    using Number = Style::Number<CSS::Nonnegative, float>;
    using Length = Style::Length<CSS::Nonnegative, float>;

    constexpr LineHeight(CSS::Keyword::Normal keyword) : m_value { keyword } { }
    constexpr LineHeight(Number number) : m_value { number } { }
    constexpr LineHeight(Length length) : m_value { length } { }

    constexpr bool isNormal() const { return holdsAlternative<CSS::Keyword::Normal>(); }
    constexpr bool isNumeric() const { return !holdsAlternative<CSS::Keyword::Normal>(); }

    constexpr std::optional<Length> tryLength() const { return holdsAlternative<Length>() ? std::optional { get<Length>(m_value) } : std::nullopt; }
    constexpr std::optional<Number> tryNumber() const { return holdsAlternative<Number>() ? std::optional { get<Number>(m_value) } : std::nullopt; }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    template<typename U> constexpr bool holdsAlternative() const
    {
        return WTF::holdsAlternative<U>(m_value);
    }

    constexpr unsigned valueForHash() const
    {
        return switchOn(
            [&](const CSS::Keyword::Normal&) -> unsigned { return computeHash(0); },
            [&](const Number& number) -> unsigned { return computeHash(1, number.value); },
            [&](const Length& length) -> unsigned { return computeHash(2, length.unresolvedValue()); }
        );
    }

    constexpr bool hasSameType(const LineHeight& other) const
    {
        return m_value.index() == other.m_value.index();
    }

    constexpr bool operator==(const LineHeight&) const = default;

    // Legacy name support
    using Fixed = Length;
    constexpr std::optional<Length> tryFixed() const { return tryLength(); }
    constexpr bool isFixed() const { return holdsAlternative<Length>(); }

private:
    friend struct Blending<LineHeight>;

    Variant<CSS::Keyword::Normal, Number, Length> m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<LineHeight> {
    auto operator()(BuilderState&, const CSSValue&, float multiplier = 1.0f) -> LineHeight;
};

// MARK: - Blending

template<> struct Blending<LineHeight> {
    bool NODELETE canBlend(const LineHeight&, const LineHeight&);
    bool NODELETE requiresInterpolationForAccumulativeIteration(const LineHeight&, const LineHeight&);
    auto blend(const LineHeight&, const LineHeight&, const BlendingContext&) -> LineHeight;
};

// MARK: - Evaluation

struct LineHeightEvaluationContext {
    float computedFontSize;
    float lineSpacing;
};

template<> struct Evaluation<LineHeight, float> {
    auto operator()(const LineHeight&, LineHeightEvaluationContext, ZoomFactor) -> float;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::LineHeight)
