/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AlphaPremultiplication.h"
#include "ColorInterpolationMethod.h"
#include "ColorNormalization.h"
#include "ColorTypes.h"

namespace WebCore {

// MARK: - Pre-interpolation normalization/fixup

std::pair<float, float> fixupHueComponentsPriorToInterpolation(HueInterpolationMethod, float, float);

template<size_t I, AlphaPremultiplication alphaPremultiplication, typename InterpolationMethod>
std::pair<float, float> preInterpolationNormalizationForComponent(InterpolationMethod interpolationMethod, ColorComponents<float, 4> colorComponents1, ColorComponents<float, 4> colorComponents2)
{
    using ColorType = typename InterpolationMethod::ColorType;
    constexpr auto componentInfo = ColorType::Model::componentInfo;

    if constexpr (componentInfo[I].type == ColorComponentType::Angle)
        return fixupHueComponentsPriorToInterpolation(interpolationMethod.hueInterpolationMethod, colorComponents1[I], colorComponents2[I]);
    else {
        if constexpr (alphaPremultiplication == AlphaPremultiplication::Premultiplied)
            return { colorComponents1[I] * colorComponents1[3], colorComponents2[I] * colorComponents2[3] };
        else
            return { colorComponents1[I], colorComponents2[I] };
    }
}

template<AlphaPremultiplication alphaPremultiplication, typename InterpolationMethod>
std::pair<ColorComponents<float, 4>, ColorComponents<float, 4>> preInterpolationNormalization(InterpolationMethod interpolationMethod, ColorComponents<float, 4> colorComponents1, ColorComponents<float, 4> colorComponents2)
{
    auto [colorA0, colorB0] = preInterpolationNormalizationForComponent<0, alphaPremultiplication>(interpolationMethod, colorComponents1, colorComponents2);
    auto [colorA1, colorB1] = preInterpolationNormalizationForComponent<1, alphaPremultiplication>(interpolationMethod, colorComponents1, colorComponents2);
    auto [colorA2, colorB2] = preInterpolationNormalizationForComponent<2, alphaPremultiplication>(interpolationMethod, colorComponents1, colorComponents2);

    return {
        { colorA0, colorA1, colorA2, colorComponents1[3] },
        { colorB0, colorB1, colorB2, colorComponents2[3] }
    };
}


// MARK: - Post-interpolation normalization/fixup

template<size_t I, AlphaPremultiplication alphaPremultiplication, typename InterpolationMethod>
float postInterpolationNormalizationForComponent(InterpolationMethod, ColorComponents<float, 4> colorComponents)
{
    using ColorType = typename InterpolationMethod::ColorType;
    constexpr auto componentInfo = ColorType::Model::componentInfo;

    if constexpr (componentInfo[I].type != ColorComponentType::Angle && alphaPremultiplication == AlphaPremultiplication::Premultiplied) {
        if (colorComponents[3] == 0.0f)
            return 0;
        return colorComponents[I] / colorComponents[3];
    } else
        return colorComponents[I];
}

template<AlphaPremultiplication alphaPremultiplication, typename InterpolationMethod>
ColorComponents<float, 4> postInterpolationNormalization(InterpolationMethod interpolationMethod, ColorComponents<float, 4> colorComponents)
{
    return {
        postInterpolationNormalizationForComponent<0, alphaPremultiplication>(interpolationMethod, colorComponents),
        postInterpolationNormalizationForComponent<1, alphaPremultiplication>(interpolationMethod, colorComponents),
        postInterpolationNormalizationForComponent<2, alphaPremultiplication>(interpolationMethod, colorComponents),
        colorComponents[3]
    };
 }


// MARK: - Interpolation

template<AlphaPremultiplication alphaPremultiplication, typename InterpolationMethod>
typename InterpolationMethod::ColorType interpolateColorComponents(InterpolationMethod interpolationMethod, typename InterpolationMethod::ColorType color1, double color1Multiplier, typename InterpolationMethod::ColorType color2, double color2Multiplier)
{
    // 1. Apply pre-interpolation transforms (hue fixup for polar color spaces, alpha premultiplication if required).
    auto [normalizedColorComponents1, normalizedColorComponents2] = preInterpolationNormalization<alphaPremultiplication>(interpolationMethod, asColorComponents(color1), asColorComponents(color2));

    // 2. Interpolate using the normalized components.
    auto interpolatedColorComponents = mapColorComponents([&] (auto componentFromColor1, auto componentFromColor2) -> float {
        return (componentFromColor1 * color1Multiplier) + (componentFromColor2 * color2Multiplier);
    }, normalizedColorComponents1, normalizedColorComponents2);

    // 3. Apply post-interpolation trasforms (alpha un-premultiplication if required).
    auto normalizedInterpolatedColorComponents = postInterpolationNormalization<alphaPremultiplication>(interpolationMethod, interpolatedColorComponents);

    // 4. Create color type from components, normalizing any components that may be out of range.
    return makeColorTypeByNormalizingComponents<typename InterpolationMethod::ColorType>(normalizedInterpolatedColorComponents);
}

}
