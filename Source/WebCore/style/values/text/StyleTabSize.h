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
#include <WebCore/StyleValueTypes.h>

namespace WebCore {

struct TabSize;

namespace Style {

// <'tab-size'> = <number [0,∞]> | <length [0,∞]>
// https://drafts.csswg.org/css-text-3/#propdef-tab-size
struct TabSize {
    using Spaces = Style::Number<CSS::Nonnegative, float>;
    using Length = Style::Length<CSS::Nonnegative, float>;

    constexpr TabSize(CSS::ValueLiteral<CSS::NumberUnit::Number> literal)
        : m_value { Spaces { literal } }
    {
    }

    constexpr TabSize(CSS::ValueLiteral<CSS::LengthUnit::Px> literal)
        : m_value { Length { literal } }
    {
    }

    constexpr TabSize(Spaces spaces)
        : m_value { spaces }
    {
    }

    constexpr TabSize(Length length)
        : m_value { length }
    {
    }

    constexpr bool isSpaces() const { return holdsAlternative<Spaces>(); }
    constexpr bool isLength() const { return holdsAlternative<Length>(); }
    constexpr std::optional<Spaces> trySpaces() const { return isSpaces() ? std::make_optional(std::get<Spaces>(m_value)) : std::nullopt; }
    constexpr std::optional<Length> tryLength() const { return isLength() ? std::make_optional(std::get<Length>(m_value)) : std::nullopt; }

    constexpr bool isZero() const { return WTF::switchOn(m_value, [](auto value) { return value.isZero(); }); }

    template<typename U> constexpr bool holdsAlternative() const
    {
        return WTF::holdsAlternative<U>(m_value);
    }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    constexpr bool operator==(const TabSize&) const = default;
    constexpr bool hasSameType(const TabSize& other) const { return m_value.index() == other.m_value.index(); }

private:
    friend struct Blending<TabSize>;

    Variant<Spaces, Length> m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<TabSize> { auto operator()(BuilderState&, const CSSValue&) -> TabSize; };

// MARK: - Blending

template<> struct Blending<TabSize> {
    auto canBlend(const TabSize&, const TabSize&) -> bool;
    auto blend(const TabSize&, const TabSize&, const BlendingContext&) -> TabSize;
};

// MARK: - Platform

template<> struct ToPlatform<TabSize> { auto operator()(const TabSize&) -> WebCore::TabSize; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TabSize);
