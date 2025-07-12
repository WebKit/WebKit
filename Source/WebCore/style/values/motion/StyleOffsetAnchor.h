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

#include "LengthPoint.h"
#include "StylePosition.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

// <'offset-anchor'> = auto | <position>
// https://drafts.fxtf.org/motion/#propdef-offset-anchor
struct OffsetAnchor {
    OffsetAnchor(CSS::Keyword::Auto) : value { WebCore::LengthType::Auto, WebCore::LengthType::Auto } { }
    OffsetAnchor(Position&& position) : value { toPlatform(position) } { }
    OffsetAnchor(const Position& position) : value { toPlatform(position) } { }
    explicit OffsetAnchor(WebCore::LengthPoint&& point) : value { WTFMove(point) } { RELEASE_ASSERT(isValid(value)); }
    explicit OffsetAnchor(const WebCore::LengthPoint& point) : value { point } { RELEASE_ASSERT(isValid(value)); }

    ALWAYS_INLINE bool isAuto() const { return value.x.isAuto(); }
    ALWAYS_INLINE bool isPosition() const { return value.x.isSpecified(); }

    template<typename> bool holdsAlternative() const;
    template<typename... F> decltype(auto) switchOn(F&&...) const;

    bool operator==(const OffsetAnchor&) const = default;

private:
    friend struct Blending<OffsetAnchor>;
    friend struct ToPlatform<OffsetAnchor>;

    static bool isValid(const WebCore::LengthPoint& point)
    {
        return (point.x.isAuto() && point.y.isAuto())
            || (point.x.isSpecified() && point.y.isSpecified());
    }

    WebCore::LengthPoint value;
};

template<typename T> bool OffsetAnchor::holdsAlternative() const
{
         if constexpr (std::same_as<T, CSS::Keyword::Auto>)     return isAuto();
    else if constexpr (std::same_as<T, Position>)               return isPosition();
}

template<typename... F> decltype(auto) OffsetAnchor::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

    if (isAuto())
        return visitor(CSS::Keyword::Auto { });
    return visitor(Position { value });
}

// MARK: - Conversion

template<> struct CSSValueConversion<OffsetAnchor> { auto operator()(BuilderState&, const CSSValue&) -> OffsetAnchor; };

// MARK: - Blending

template<> struct Blending<OffsetAnchor> {
    auto canBlend(const OffsetAnchor&, const OffsetAnchor&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const OffsetAnchor&, const OffsetAnchor&) -> bool;
    auto blend(const OffsetAnchor&, const OffsetAnchor&, const BlendingContext&) -> OffsetAnchor;
};

// MARK: - Platform

template<> struct ToPlatform<OffsetAnchor> { auto operator()(const OffsetAnchor&) -> WebCore::LengthPoint; };

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::OffsetAnchor> = true;
