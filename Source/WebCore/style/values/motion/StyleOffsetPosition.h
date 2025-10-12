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

#include <WebCore/AcceleratedEffectOffsetPosition.h>
#include <WebCore/StylePosition.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'offset-position'> = auto | normal | <position>
// https://drafts.fxtf.org/motion/#propdef-offset-position
struct OffsetPosition {
    OffsetPosition(CSS::Keyword::Auto keyword) : m_value { keyword} { }
    OffsetPosition(CSS::Keyword::Normal keyword) : m_value { keyword} { }
    OffsetPosition(Position&& position) : m_value { WTFMove(position) } { }
    OffsetPosition(const Position& position) : m_value { position } { }

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    explicit OffsetPosition(AcceleratedEffectOffsetPosition&& point) : m_value { convert(point) } { }
    explicit OffsetPosition(const AcceleratedEffectOffsetPosition& point) : m_value { convert(point) } { }
#endif

    ALWAYS_INLINE bool isAuto() const { return holdsAlternative<CSS::Keyword::Auto>(); }
    ALWAYS_INLINE bool isNormal() const { return holdsAlternative<CSS::Keyword::Auto>(); }
    ALWAYS_INLINE bool isPosition() const { return holdsAlternative<Position>(); }
    std::optional<Position> tryPosition() const { return isPosition() ? std::make_optional(std::get<Position>(m_value)) : std::nullopt; }

    template<typename> bool holdsAlternative() const;
    template<typename... F> decltype(auto) switchOn(F&&...) const;

    bool operator==(const OffsetPosition&) const = default;

private:
#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    static auto convert(const AcceleratedEffectOffsetPosition&) -> Variant<CSS::Keyword::Auto, CSS::Keyword::Normal, Position>;
#endif

    Variant<CSS::Keyword::Auto, CSS::Keyword::Normal, Position> m_value;
};

template<typename T> bool OffsetPosition::holdsAlternative() const
{
    return std::holds_alternative<T>(m_value);
}

template<typename... F> decltype(auto) OffsetPosition::switchOn(F&&... f) const
{
    return WTF::switchOn(m_value, std::forward<F>(f)...);
}

// MARK: - Conversion

template<> struct CSSValueConversion<OffsetPosition> { auto operator()(BuilderState&, const CSSValue&) -> OffsetPosition; };

// MARK: - Blending

template<> struct Blending<OffsetPosition> {
    auto canBlend(const OffsetPosition&, const OffsetPosition&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const OffsetPosition&, const OffsetPosition&) -> bool;
    auto blend(const OffsetPosition&, const OffsetPosition&, const BlendingContext&) -> OffsetPosition;
};

// MARK: - Evaluation

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

template<> struct Evaluation<OffsetPosition, AcceleratedEffectOffsetPosition> {
    auto operator()(const OffsetPosition&, FloatSize, ZoomNeeded) -> AcceleratedEffectOffsetPosition;
};

#endif

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::OffsetPosition)
