/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include "LayoutSize.h"
#include "RenderElement.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Rounding.h"
#include "StyleZoomPrimitives.h"
#include <concepts>

namespace WebCore {
namespace Style {

template<typename T>
T unapplyingZoom(T value, const ComputedStyle& style)
{
    auto zoom = style.usedZoom();

    if constexpr (std::integral<T>) {
        if (zoom == 1)
            return value;
        // Needed to match historical `CSSPrimitiveValue::resolveAsLength<int>` behavior which truncated (rather than rounding) when scaling up.
        if (zoom > 1) {
            if (value < 0)
                value--;
            else
                value++;
        }
        return roundForImpreciseConversion<int>(value / zoom);
    } else if constexpr (std::floating_point<T> || std::same_as<T, LayoutUnit>) {
        return T(value / zoom);
    } else if constexpr (std::same_as<T, LayoutSize>) {
        return T(value.width() / zoom, value.height() / zoom);
    }
}

template<typename T>
T unapplyingZoom(T value, const RenderElement& renderer)
{
    return Style::unapplyingZoom<T>(value, renderer.style());
}

template<typename T>
inline T applyingZoom(T value, const ComputedStyle& style)
{
    return value * style.usedZoom();
}

template<typename T>
inline T applyingZoom(T value, const RenderElement& renderer)
{
    return Style::applyingZoom<T>(value, renderer.style());
}

} // namespace Style
} // namespace WebCore
