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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleColor.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'accent-color'> = auto | <color>
// https://drafts.csswg.org/css-ui-4/#propdef-accent-color
struct AccentColor : ValueOrKeyword<Color, CSS::Keyword::Auto> {
    using Base::Base;

    bool isAuto() const { return isKeyword(); }
    bool isColor() const { return isValue(); }
    std::optional<Color> tryColor() const { return tryValue(); }

    // Returns the color or a `currentColor` singleton if `auto`.
    const Color& colorOrCurrentColor() const;
};

// MARK: - Blending

template<> struct Blending<AccentColor> {
    auto equals(const AccentColor&, const AccentColor&, const RenderStyle&, const RenderStyle&) -> bool;
    auto canBlend(const AccentColor&, const AccentColor&) -> bool;
    constexpr auto requiresInterpolationForAccumulativeIteration(const AccentColor&, const AccentColor&) -> bool { return true; }
    auto blend(const AccentColor&, const AccentColor&, const RenderStyle&, const RenderStyle&, const BlendingContext&) -> AccentColor;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::AccentColor);
