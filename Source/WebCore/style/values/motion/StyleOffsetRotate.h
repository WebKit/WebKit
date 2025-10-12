/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/AcceleratedEffectOffsetRotate.h>
#include <WebCore/StylePrimitiveNumericTypes.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'offset-rotate'> = [ auto | reverse ] || <angle>
// https://drafts.fxtf.org/motion/#propdef-offset-rotate
struct OffsetRotate {
    using Angle = Style::Angle<CSS::All, float>;

    constexpr OffsetRotate(CSS::Keyword::Auto keyword) : m_angle { 0 }, m_autoKeyword { keyword } { }
    constexpr OffsetRotate(std::optional<CSS::Keyword::Auto> autoKeyword, Angle angle) : m_angle(angle), m_autoKeyword(autoKeyword) { }

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    constexpr explicit OffsetRotate(AcceleratedEffectOffsetRotate rotate) : m_angle(rotate.angle), m_autoKeyword(rotate.hasAuto ? std::make_optional(CSS::Keyword::Auto { }) : std::nullopt) { }
#endif

    bool hasAuto() const { return m_autoKeyword.has_value(); }
    std::optional<CSS::Keyword::Auto> autoKeyword() const { return m_autoKeyword; }

    Angle angle() const { return m_angle; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (m_autoKeyword)
            return visitor(SpaceSeparatedTuple { *m_autoKeyword, m_angle });
        return visitor(m_angle);
    }

    constexpr bool operator==(const OffsetRotate&) const = default;

private:
    Angle m_angle;
    std::optional<CSS::Keyword::Auto> m_autoKeyword;
};

// MARK: - Conversion

template<> struct CSSValueConversion<OffsetRotate> { auto operator()(BuilderState&, const CSSValue&) -> OffsetRotate; };

// MARK: - Blending

template<> struct Blending<OffsetRotate> {
    auto canBlend(const OffsetRotate&, const OffsetRotate&) -> bool;
    auto blend(const OffsetRotate&, const OffsetRotate&, const BlendingContext&) -> OffsetRotate;
};

// MARK: - Evaluation

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

template<> struct Evaluation<OffsetRotate, AcceleratedEffectOffsetRotate> {
    auto operator()(const OffsetRotate&) -> AcceleratedEffectOffsetRotate;
};

#endif

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const OffsetRotate&);

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::OffsetRotate)
