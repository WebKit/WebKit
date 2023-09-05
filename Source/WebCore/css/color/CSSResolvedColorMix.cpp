/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "CSSResolvedColorMix.h"

#include "ColorInterpolation.h"

namespace WebCore {

namespace {

struct ColorMixPercentages {
    double p1;
    double p2;
    std::optional<double> alphaMultiplier = std::nullopt;
};

}

static std::optional<ColorMixPercentages> normalizedMixPercentages(std::optional<double> mixComponents1Percentage, std::optional<double> mixComponents2Percentage)
{
    // The percentages are normalized as follows:

    // 1. Let p1 be the first percentage and p2 the second one.

    // 2. If both percentages are omitted, they each default to 50% (an equal mix of the two colors).
    if (!mixComponents1Percentage && !mixComponents2Percentage)
        return {{ 50.0, 50.0 }};

    ColorMixPercentages result;

    if (!mixComponents2Percentage) {
        // 3. Otherwise, if p2 is omitted, it becomes 100% - p1
        result.p1 = *mixComponents1Percentage;
        result.p2 = 100.0 - result.p1;
    } else if (!mixComponents1Percentage) {
        // 4. Otherwise, if p1 is omitted, it becomes 100% - p2
        result.p2 = *mixComponents2Percentage;
        result.p1 = 100.0 - result.p2;
    } else {
        result.p1 = *mixComponents1Percentage;
        result.p2 = *mixComponents2Percentage;
    }

    auto sum = result.p1 + result.p2;

    // 5.If the percentages sum to zero, the function is invalid.
    if (sum == 0)
        return { };

    if (sum > 100.0) {
        // 6. Otherwise, if both are provided but do not add up to 100%, they are scaled accordingly so that they
        //    add up to 100%.
        result.p1 *= 100.0 / sum;
        result.p2 *= 100.0 / sum;
    } else if (sum < 100.0) {
        // 7. Otherwise, if both are provided and add up to less than 100%, the sum is saved as an alpha multiplier.
        //    They are then scaled accordingly so that they add up to 100%.
        result.p1 *= 100.0 / sum;
        result.p2 *= 100.0 / sum;
        result.alphaMultiplier = sum;
    }

    return result;
}

template<typename InterpolationMethod> static Color mixColorComponentsUsingColorInterpolationMethod(InterpolationMethod interpolationMethod, ColorMixPercentages mixPercentages, const Color& color1, const Color& color2)
{
    using ColorType = typename InterpolationMethod::ColorType;

    // 1. Both colors are converted to the specified <color-space>. If the specified color space has a smaller gamut than
    //    the one in which the color to be adjusted is specified, gamut mapping will occur.
    auto convertedColor1 = color1.template toColorTypeLossy<ColorType>();
    auto convertedColor2 = color2.template toColorTypeLossy<ColorType>();

    // 2. Colors are then interpolated in the specified color space, as described in CSS Color 4 § 13 Interpolation. [...]
    auto mixedColor = interpolateColorComponents<AlphaPremultiplication::Premultiplied>(interpolationMethod, convertedColor1, mixPercentages.p1 / 100.0, convertedColor2, mixPercentages.p2 / 100.0).unresolved();

    // 3. If an alpha multiplier was produced during percentage normalization, the alpha component of the interpolated result
    //    is multiplied by the alpha multiplier.
    if (mixPercentages.alphaMultiplier && !std::isnan(mixedColor.alpha))
        mixedColor.alpha *= (*mixPercentages.alphaMultiplier / 100.0);

    return makeCanonicalColor(mixedColor, { Color::Flags::UseColorFunctionSerialization });
}

Color mix(const CSSResolvedColorMix& colorMix)
{
    auto mixPercentages = normalizedMixPercentages(colorMix.mixComponents1.percentage, colorMix.mixComponents2.percentage);
    if (!mixPercentages)
        return { };

    return WTF::switchOn(colorMix.colorInterpolationMethod.colorSpace,
        [&] (auto colorSpace) {
            return mixColorComponentsUsingColorInterpolationMethod<decltype(colorSpace)>(
                colorSpace,
                *mixPercentages,
                colorMix.mixComponents1.color,
                colorMix.mixComponents2.color
            );
        }
    );
}

} // namespace WebCore
