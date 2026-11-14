/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "config.h"
#include "StyleFreeformGradient.h"

#include "FreeformGradient.h"
#include "StyleAppleColorFilter.h"
#include "StyleColorResolver.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

Ref<WebCore::FreeformGradient> createPlatformFreeformGradient(const FreeformGradient& gradient, FloatSize size, const ComputedStyle& style)
{
    auto zoom = style.usedZoomForLength();
    bool hasColorFilter = !style.appleColorFilter().isNone();
    ColorResolver colorResolver { style };

    auto resolveColor = [&](const Color& color) {
        return hasColorFilter ? colorResolver.colorResolvingCurrentColorApplyingColorFilter(color) : colorResolver.colorResolvingCurrentColor(color);
    };

    auto stops = WTF::map(gradient.stops, [&](auto& stop) -> WebCore::FreeformGradient::Stop {
        return {
            .position = evaluate<FloatPoint>(stop.position, size, zoom),
            .color = resolveColor(stop.color),
            .weight = stop.weight ? narrowPrecisionToFloat(stop.weight->value) : 0,
        };
    });

    return WebCore::FreeformGradient::create(gradient.colorInterpolationMethod.method.value, WTF::move(stops));
}

bool containsCurrentColor(const FreeformGradient& gradient)
{
    return std::ranges::any_of(gradient.stops, [](auto& stop) {
        return containsCurrentColor(stop.color);
    });
}

} // namespace Style
} // namespace WebCore
