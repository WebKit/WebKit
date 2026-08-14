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
#include "StyleRelativeAlphaColor.h"
#include "StyleRelativeColor.h"
#include "StyleResolvedColor.h"

namespace WebCore {
namespace Style {

namespace {

template<class F> Color transformCurrentColorInternal(const ColorLayers& colorLayers, F&& f)
{
    return ColorLayers {
        .blendMode = colorLayers.blendMode,
        .colors = colorLayers.colors.map([&] (const Color& color) { return transformCurrentColor(color, std::forward<F>(f)); })
    };
}


template<class F> Color transformCurrentColorInternal(const ColorMix& colorMix, F&& f)
{
    return ColorMix {
        .colorInterpolationMethod = colorMix.colorInterpolationMethod,
        .components = colorMix.components.map([&] (const ColorMix::Component& component) {
            return ColorMix::Component {
                .color = transformCurrentColor(component.color, std::forward<F>(f)),
                .percentage = component.percentage
            };
        })
    };
}

template<class F> Color transformCurrentColorInternal(const ContrastColor& contrastColor, F&& f)
{
    return ContrastColor {
        .color = transformCurrentColor(contrastColor.color, std::forward<F>(f))
    };
}

template<class F> Color transformCurrentColorInternal(const RelativeAlphaColor& relativeAlphaColor, F&& f)
{
    return RelativeAlphaColor {
        .origin = transformCurrentColor(relativeAlphaColor.origin, std::forward<F>(f)),
        .alpha = relativeAlphaColor.alpha
    };
}

template<class F, class D> Color transformCurrentColorInternal(const RelativeColor<D>& relativeColor, F&& f)
{
    return RelativeColor<D> {
        .origin = transformCurrentColor(relativeColor.origin, std::forward<F>(f)),
        .components = relativeColor.components
    };
}


template<class F> Color transformCurrentColorInternal(const ResolvedColor& color, F&&)
{
    return ResolvedColor { color };
}

template<class F> Color transformCurrentColorInternal(const CurrentColor& color, F&& f)
{
    return f(color);
}

} // namespace

// Returns a new Color with the same structure as the old color, except with any embedded
// CurrentColor being replaced with `f(currentColor)`. This is useful for e.g replacing
// any embedded CurrentColor with another color.
// E.g: a ContrastColor { CurrentColor { ... } } becomes ContrastColor { f(CurrentColor { ... }) }
template<class F> Color transformCurrentColor(const Color& color, F&& f)
{
    return color.switchOn([&](const auto& kind) { return transformCurrentColorInternal(kind, std::forward<F>(f)); });
}


} // namespace Style
} // namespace WebCore
