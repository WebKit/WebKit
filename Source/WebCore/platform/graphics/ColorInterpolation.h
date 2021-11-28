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

#include "ColorTypes.h"
#include <variant>

namespace WebCore {

enum class HueInterpolationMethod : uint8_t {
    Shorter,
    Longer,
    Increasing,
    Decreasing,
    Specified
};

struct ColorInterpolationMethod {
    struct HSL {
        using ColorType = WebCore::HSLA<float>;
        HueInterpolationMethod hueInterpolationMethod = HueInterpolationMethod::Shorter;
    };
    struct HWB {
        using ColorType = WebCore::HWBA<float>;
        HueInterpolationMethod hueInterpolationMethod = HueInterpolationMethod::Shorter;
    };
    struct LCH {
        using ColorType = WebCore::LCHA<float>;
        HueInterpolationMethod hueInterpolationMethod = HueInterpolationMethod::Shorter;
    };
    struct Lab {
        using ColorType = WebCore::Lab<float>;
    };
    struct OKLCH {
        using ColorType = WebCore::OKLCHA<float>;
        HueInterpolationMethod hueInterpolationMethod = HueInterpolationMethod::Shorter;
    };
    struct OKLab {
        using ColorType = WebCore::OKLab<float>;
    };
    struct SRGB {
        using ColorType = WebCore::SRGBA<float>;
    };
    struct XYZD50 {
        using ColorType = WebCore::XYZA<float, WhitePoint::D50>;
    };
    struct XYZD65 {
        using ColorType = WebCore::XYZA<float, WhitePoint::D65>;
    };

    std::variant<HSL, HWB, LCH, Lab, OKLCH, OKLab, SRGB, XYZD50, XYZD65> value;
};

std::pair<float, float> fixupHueComponentsPriorToInterpolation(HueInterpolationMethod, float, float);

template<size_t I, typename InterpolationMethod>
std::pair<float, float> preInterpolationNormalizationForComponent(InterpolationMethod interpolationMethod, ColorComponents<float, 4> colorComponents1, ColorComponents<float, 4> colorComponents2)
{
    using ColorType = typename InterpolationMethod::ColorType;
    constexpr auto componentInfo = ColorType::Model::componentInfo;

    if constexpr (componentInfo[I].type == ColorComponentType::Angle)
        return fixupHueComponentsPriorToInterpolation(interpolationMethod.hueInterpolationMethod, colorComponents1[I], colorComponents2[I]);
    else
        return { colorComponents1[I], colorComponents2[I] };
}

template<typename InterpolationMethod>
std::pair<ColorComponents<float, 4>, ColorComponents<float, 4>> preInterpolationNormalization(InterpolationMethod interpolationMethod, ColorComponents<float, 4> colorComponents1, ColorComponents<float, 4> colorComponents2)
{
    auto [colorA0, colorB0] = preInterpolationNormalizationForComponent<0>(interpolationMethod, colorComponents1, colorComponents2);
    auto [colorA1, colorB1] = preInterpolationNormalizationForComponent<1>(interpolationMethod, colorComponents1, colorComponents2);
    auto [colorA2, colorB2] = preInterpolationNormalizationForComponent<2>(interpolationMethod, colorComponents1, colorComponents2);

    return {
        ColorComponents<float, 4> { colorA0, colorA1, colorA2, colorComponents1[3] },
        ColorComponents<float, 4> { colorB0, colorB1, colorB2, colorComponents2[3] }
    };
}

}
