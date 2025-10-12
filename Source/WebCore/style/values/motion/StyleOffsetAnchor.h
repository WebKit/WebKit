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

#include <WebCore/AcceleratedEffectOffsetAnchor.h>
#include <WebCore/StylePosition.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'offset-anchor'> = auto | <position>
// https://drafts.fxtf.org/motion/#propdef-offset-anchor
struct OffsetAnchor {
    OffsetAnchor(CSS::Keyword::Auto keyword) : m_value { keyword} { }
    OffsetAnchor(Position&& position) : m_value { WTFMove(position) } { }
    OffsetAnchor(const Position& position) : m_value { position } { }

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    explicit OffsetAnchor(AcceleratedEffectOffsetAnchor&& point) : m_value { convert(point) } { }
    explicit OffsetAnchor(const AcceleratedEffectOffsetAnchor& point) : m_value { convert(point) } { }
#endif

    ALWAYS_INLINE bool isAuto() const { return holdsAlternative<CSS::Keyword::Auto>(); }
    ALWAYS_INLINE bool isPosition() const { return holdsAlternative<Position>(); }
    std::optional<Position> tryPosition() const { return isPosition() ? std::make_optional(std::get<Position>(m_value)) : std::nullopt; }

    template<typename> bool holdsAlternative() const;
    template<typename... F> decltype(auto) switchOn(F&&...) const;

    bool operator==(const OffsetAnchor&) const = default;

private:
#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    static auto convert(const AcceleratedEffectOffsetAnchor&) -> Variant<CSS::Keyword::Auto, Position>;
#endif

    Variant<CSS::Keyword::Auto, Position> m_value;
};

template<typename T> bool OffsetAnchor::holdsAlternative() const
{
    return std::holds_alternative<T>(m_value);
}

template<typename... F> decltype(auto) OffsetAnchor::switchOn(F&&... f) const
{
    return WTF::switchOn(m_value, std::forward<F>(f)...);
}

// MARK: - Conversion

template<> struct CSSValueConversion<OffsetAnchor> { auto operator()(BuilderState&, const CSSValue&) -> OffsetAnchor; };

// MARK: - Blending

template<> struct Blending<OffsetAnchor> {
    auto canBlend(const OffsetAnchor&, const OffsetAnchor&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const OffsetAnchor&, const OffsetAnchor&) -> bool;
    auto blend(const OffsetAnchor&, const OffsetAnchor&, const BlendingContext&) -> OffsetAnchor;
};

// MARK: - Evaluation

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

template<> struct Evaluation<OffsetAnchor, AcceleratedEffectOffsetAnchor> {
    auto operator()(const OffsetAnchor&, FloatSize, ZoomNeeded) -> AcceleratedEffectOffsetAnchor;
};

#endif

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::OffsetAnchor)
