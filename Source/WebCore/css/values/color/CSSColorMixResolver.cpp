/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "CSSColorMixResolver.h"

#include "CSSNormalizedMixPercentages.h"
#include "ColorInterpolation.h"
#include <wtf/ZippedRange.h>

namespace WebCore {
namespace CSS {

template<typename InterpolationMethod>
static WebCore::Color mixColorComponentsUsingColorInterpolationMethod(InterpolationMethod interpolationMethod, double progress, const WebCore::Color& color1, const WebCore::Color& color2)
{
    using ColorType = typename InterpolationMethod::ColorType;

    auto convertedColor1 = color1.template toColorTypeLossyCarryingForwardMissing<ColorType>();
    auto convertedColor2 = color2.template toColorTypeLossyCarryingForwardMissing<ColorType>();

    auto mixedColor = interpolateColorComponents<AlphaPremultiplication::Premultiplied>(interpolationMethod, convertedColor1, 1.0 - progress, convertedColor2, progress).unresolved();

    // `UseColorFunctionSerialization` is set unconditionally due to `color-mix()` serialization
    // always using the modern serialization formats.
    auto flags = OptionSet { WebCore::Color::Flags::UseColorFunctionSerialization };
    if (color1.isSemantic() || color2.isSemantic())
        flags.add(WebCore::Color::Flags::Semantic);

    return { mixedColor, flags };
}

static WebCore::Color convertToColorMixResultRepresentation(const ColorInterpolationMethod& method, const WebCore::Color& color)
{
    // `UseColorFunctionSerialization` is set unconditionally due to `color-mix()` serialization
    // always using the modern serialization formats.
    auto flags = OptionSet { WebCore::Color::Flags::UseColorFunctionSerialization };
    if (color.isSemantic())
        flags.add(WebCore::Color::Flags::Semantic);

    return WTF::switchOn(method.value.colorSpace,
        [&]<typename MethodColorSpace>(const MethodColorSpace&) -> WebCore::Color {
            using ColorType = typename MethodColorSpace::ColorType;

            return { color.template toColorTypeLossyCarryingForwardMissing<ColorType>(), flags };
        }
    );
}

WebCore::Color mix(const ColorMixResolver& colorMix)
{
    // https://drafts.csswg.org/css-color-5/#color-mix-result

    // 1. Normalize mix percentages from the list of mix items passed to the function, with the "forced normalization" flag set to true, letting items and leftover be the result.

    auto [percentages, leftover] = normalizedMixPercentages<ForceNormalization::Yes>(colorMix.components);

    // 2. Let alpha mult be 1 - leftover, interpreting leftover as a number between 0 and 1.
    // NOTE: Not calculated until it is used below.

    auto color = [&] -> WebCore::Color {
        // 3. If items is length 1, set color to the color of that sole item, converted to the specified interpolation <color-space>.
        if (colorMix.components.size() == 1)
            return convertToColorMixResultRepresentation(colorMix.colorInterpolationMethod, colorMix.components[0].color);

        // Otherwise:
        //
        // 1. Let item stack be a stack made by reversing items. (Thus, with the first item at the top of the stack.)

        // FIXME: When std::ranges::fold_left_first is available to use, this can be implemented a bit more clearly using that.

        struct Item {
            WebCore::Color color;
            double percentage;
        };

        auto result = Item { colorMix.components[0].color, percentages[0] };
        for (auto [b, bPercentage] : zippedRange(colorMix.components.subspan(1), percentages.subspan(1))) {
            // 1. Pop from item stack twice, letting a and b be the two results in order. Let combined percentage be the sum of a and b’s percentages.

            auto& a = result;

            auto combinedPercentage = a.percentage + bPercentage;

            // 2. Interpolate a and b’s colors as described in CSS Color 4 §  13. Color Interpolation, with a progress percentage equal to (b’s percentage) / combined percentage), if combined percentage is greater than 0, and 0.5 otherwise. If the specified color space is a cylindrical polar color space, then the <hue-interpolation-method> controls the interpolation of hue, as described in CSS Color 4 § 13.4 Hue Interpolation. If no <hue-interpolation-method> is specified, assume shorter.

            auto progressPercentage = combinedPercentage > 0 ? bPercentage / combinedPercentage : 0.5;
            auto mixedColor = WTF::switchOn(colorMix.colorInterpolationMethod.value.colorSpace,
                [&](const auto& methodColorSpace) {
                    return mixColorComponentsUsingColorInterpolationMethod(
                        methodColorSpace,
                        progressPercentage,
                        a.color,
                        b.color
                    );
                }
            );

            // 3. Create a new mix item with the resulting color and a percentage of combined percentage, and push it onto item stack.
            result = Item { WTF::move(mixedColor), combinedPercentage };
        }

        // 3. Set color to the color of the sole remaining item in item stack.
        return result.color;
    }();

    // 4. Multiply the alpha component of color by alpha mult.

    if (leftover != 0)
        color = color.colorWithUnresolvedAlphaMultipliedBy(1.0 - (leftover / 100.0));

    // 5. Return color.

    return color;
}

} // namespace CSS
} // namespace WebCore
