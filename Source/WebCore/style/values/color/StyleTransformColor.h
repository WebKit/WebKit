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

#include "StyleColor.h"
#include "StyleColorLayers.h"
#include "StyleColorMix.h"
#include "StyleContrastColor.h"
#include "StyleCurrentAccentColor.h"
#include "StyleCurrentColor.h"
#include "StyleRelativeAlphaColor.h"
#include "StyleRelativeColor.h"
#include "StyleResolvedColor.h"

namespace WebCore {
namespace Style {

template<class... F> Color transformInnerColor(const ColorLayers& colorLayers, F&&... f)
{
    return ColorLayers {
        .blendMode = colorLayers.blendMode,
        .colors = colorLayers.colors.map([&] (const Color& color) { return transformColor(color, std::forward<F>(f)...); })
    };
}

template<class... F> Color transformInnerColor(const ColorMix& colorMix, F&&... f)
{
    return ColorMix {
        .colorInterpolationMethod = colorMix.colorInterpolationMethod,
        .components = colorMix.components.map([&] (const ColorMix::Component& component) {
            return ColorMix::Component {
                .color = transformColor(component.color, std::forward<F>(f)...),
                .percentage = component.percentage
            };
        })
    };
}

template<class... F> Color transformInnerColor(const ContrastColor& contrastColor, F&&... f)
{
    return ContrastColor {
        .color = transformColor(contrastColor.color, std::forward<F>(f)...)
    };
}

template<class... F> Color transformInnerColor(const RelativeAlphaColor& relativeAlphaColor, F&&... f)
{
    return RelativeAlphaColor {
        .origin = transformColor(relativeAlphaColor.origin, std::forward<F>(f)...),
        .alpha = relativeAlphaColor.alpha
    };
}

template<class D, class... F> Color transformInnerColor(const RelativeColor<D>& relativeColor, F&&... f)
{
    return RelativeColor<D> {
        .origin = transformColor(relativeColor.origin, std::forward<F>(f)...),
        .components = relativeColor.components
    };
}

template<class... F> Color transformInnerColor(const ResolvedColor& color, F&&...)
{
    return ResolvedColor { color };
}

template<class... F> Color transformInnerColor(const CurrentColor& color, F&&...)
{
    return CurrentColor { color };
}

template<class... F> Color transformInnerColor(const CurrentAccentColor& color, F&&...)
{
    return CurrentAccentColor { color };
}

// Transform a color and its inner colors.
//
// Following `color` should be a list of functions that accept each different
// Style::Color types as the argument, and returns a std::optional<Color>:
// * If it returns a color, then that color is returned.
// * If it returns std::nullopt, the color is unchanged. If the color contains
//   inner colors (e.g RelativeColor, ContrastColor, ...), then the color is
//   transformed by calling transformColor on each inner color, and the new color is returned.
//
// For example, transformColor can be used to replace all CurrentAccentColor with another
// color, like so:
//
//     Color color = ContrastColor { CurrentAccentColor { } };
//     Color newColor = transformColor(color,
//         [&](const auto&) -> std::optional<Color> { return std::nullopt; },
//         [&](const CurrentAccentColor&) -> std::optional<Color> { return ResolvedColor { WebCore::Color { ... } }; }
//     );
//     // newColor is ContrastColor { ResolvedColor { WebCore::Color { ... } } }
//
template<class... F> Color transformColor(const Color& color, F&&... f)
{
    std::optional<Color> maybeTransformed = color.switchOn(f...);
    if (maybeTransformed)
        return *maybeTransformed;

    return color.switchOn([&](const auto& kind) { return transformInnerColor(kind, std::forward<F>(f)...); });
}

} // namespace Style
} // namespace WebCore
