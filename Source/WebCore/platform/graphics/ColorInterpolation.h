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
#include "ColorTypes.h"
#include "ColorNormalization.h"
#include <variant>
#include <wtf/EnumTraits.h>

namespace WebCore {

enum class HueInterpolationMethod : uint8_t {
    Shorter,
    Longer,
    Increasing,
    Decreasing,
    Specified
};

enum class ColorInterpolationColorSpace : uint8_t {
    HSL,
    HWB,
    LCH,
    Lab,
    OKLCH,
    OKLab,
    SRGB,
    SRGBLinear,
    XYZD50,
    XYZD65
};
    
struct ColorInterpolationMethod {
    struct HSL {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::HSL;
        using ColorType = WebCore::HSLA<float>;
        HueInterpolationMethod hueInterpolationMethod = HueInterpolationMethod::Shorter;
    };
    struct HWB {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::HWB;
        using ColorType = WebCore::HWBA<float>;
        HueInterpolationMethod hueInterpolationMethod = HueInterpolationMethod::Shorter;
    };
    struct LCH {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::LCH;
        using ColorType = WebCore::LCHA<float>;
        HueInterpolationMethod hueInterpolationMethod = HueInterpolationMethod::Shorter;
    };
    struct Lab {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::Lab;
        using ColorType = WebCore::Lab<float>;
    };
    struct OKLCH {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::OKLCH;
        using ColorType = WebCore::OKLCHA<float>;
        HueInterpolationMethod hueInterpolationMethod = HueInterpolationMethod::Shorter;
    };
    struct OKLab {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::OKLab;
        using ColorType = WebCore::OKLab<float>;
    };
    struct SRGB {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::SRGB;
        using ColorType = WebCore::SRGBA<float>;
    };
    struct SRGBLinear {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::SRGBLinear;
        using ColorType = WebCore::LinearSRGBA<float>;
    };
    struct XYZD50 {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::XYZD50;
        using ColorType = WebCore::XYZA<float, WhitePoint::D50>;
    };
    struct XYZD65 {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::XYZD65;
        using ColorType = WebCore::XYZA<float, WhitePoint::D65>;
    };

    template<typename Encoder> void encode(Encoder&) const;
    template<typename Decoder> static std::optional<ColorInterpolationMethod> decode(Decoder&);

    std::variant<HSL, HWB, LCH, Lab, OKLCH, OKLab, SRGB, SRGBLinear, XYZD50, XYZD65> colorSpace;
    AlphaPremultiplication alphaPremultiplication;
};

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

template<AlphaPremultiplication alphaPremultiplication, typename InterpolationMethod>
typename InterpolationMethod::ColorType interpolateColorComponents(InterpolationMethod interpolationMethod, typename InterpolationMethod::ColorType color1, double color1Multiplier, typename InterpolationMethod::ColorType color2, double color2Multiplier)
{
    auto [normalizedColorComponents1, normalizedColorComponents2] = preInterpolationNormalization<alphaPremultiplication>(interpolationMethod, asColorComponents(color1), asColorComponents(color2));

    auto interpolatedColorComponents = mapColorComponents([&] (auto componentFromColor1, auto componentFromColor2) -> float {
        return (componentFromColor1 * color1Multiplier) + (componentFromColor2 * color2Multiplier);
    }, normalizedColorComponents1, normalizedColorComponents2);

    auto normalizedInterpolatedColorComponents = postInterpolationNormalization<alphaPremultiplication>(interpolationMethod, interpolatedColorComponents);

    return makeColorTypeByNormalizingComponents<typename InterpolationMethod::ColorType>(normalizedInterpolatedColorComponents);
}

template<typename Encoder> void ColorInterpolationMethod::encode(Encoder& encoder) const
{
    encoder << alphaPremultiplication;

    WTF::switchOn(colorSpace,
        [&] (auto& type) {
            encoder << type.interpolationColorSpace;
            if constexpr (decltype(type)::ColorType::Model::coordinateSystem == ColorSpaceCoordinateSystem::CylindricalPolar) {
                encoder << type.hueInterpolationMethod;
            }
        }
    );
}

template<typename Decoder> std::optional<ColorInterpolationMethod> ColorInterpolationMethod::decode(Decoder& decoder)
{
    std::optional<AlphaPremultiplication> alphaPremultiplication;
    decoder >> alphaPremultiplication;
    if (!alphaPremultiplication)
        return std::nullopt;

    std::optional<ColorInterpolationColorSpace> interpolationColorSpace;
    decoder >> interpolationColorSpace;
    if (!interpolationColorSpace)
        return std::nullopt;

    switch (*interpolationColorSpace) {
    case ColorInterpolationColorSpace::HSL: {
        std::optional<HueInterpolationMethod> hueInterpolationMethod;
        decoder >> hueInterpolationMethod;
        if (!hueInterpolationMethod)
            return std::nullopt;

        return ColorInterpolationMethod { ColorInterpolationMethod::HSL { *hueInterpolationMethod }, *alphaPremultiplication };
    }
    case ColorInterpolationColorSpace::HWB: {
        std::optional<HueInterpolationMethod> hueInterpolationMethod;
        decoder >> hueInterpolationMethod;
        if (!hueInterpolationMethod)
            return std::nullopt;

        return ColorInterpolationMethod { ColorInterpolationMethod::HWB { *hueInterpolationMethod }, *alphaPremultiplication };
    }
    case ColorInterpolationColorSpace::LCH: {
        std::optional<HueInterpolationMethod> hueInterpolationMethod;
        decoder >> hueInterpolationMethod;
        if (!hueInterpolationMethod)
            return std::nullopt;

        return ColorInterpolationMethod { ColorInterpolationMethod::LCH { *hueInterpolationMethod }, *alphaPremultiplication };
    }
    case ColorInterpolationColorSpace::OKLCH: {
        std::optional<HueInterpolationMethod> hueInterpolationMethod;
        decoder >> hueInterpolationMethod;
        if (!hueInterpolationMethod)
            return std::nullopt;

        return ColorInterpolationMethod { ColorInterpolationMethod::OKLCH { *hueInterpolationMethod }, *alphaPremultiplication };
    }
    case ColorInterpolationColorSpace::Lab:
        return ColorInterpolationMethod { ColorInterpolationMethod::Lab { }, *alphaPremultiplication };
    case ColorInterpolationColorSpace::OKLab:
        return ColorInterpolationMethod { ColorInterpolationMethod::OKLab { }, *alphaPremultiplication };
    case ColorInterpolationColorSpace::SRGB:
        return ColorInterpolationMethod { ColorInterpolationMethod::SRGB { }, *alphaPremultiplication };
    case ColorInterpolationColorSpace::SRGBLinear:
        return ColorInterpolationMethod { ColorInterpolationMethod::SRGBLinear { }, *alphaPremultiplication };
    case ColorInterpolationColorSpace::XYZD50:
        return ColorInterpolationMethod { ColorInterpolationMethod::XYZD50 { }, *alphaPremultiplication };
    case ColorInterpolationColorSpace::XYZD65:
        return ColorInterpolationMethod { ColorInterpolationMethod::XYZD65 { }, *alphaPremultiplication };
    }

    RELEASE_ASSERT_NOT_REACHED();
    return std::nullopt;
}

}

namespace WTF {

template<> struct EnumTraits<WebCore::HueInterpolationMethod> {
    using values = EnumValues<
        WebCore::HueInterpolationMethod,
        WebCore::HueInterpolationMethod::Shorter,
        WebCore::HueInterpolationMethod::Longer,
        WebCore::HueInterpolationMethod::Increasing,
        WebCore::HueInterpolationMethod::Decreasing,
        WebCore::HueInterpolationMethod::Specified
    >;
};

template<> struct EnumTraits<WebCore::ColorInterpolationColorSpace> {
    using values = EnumValues<
        WebCore::ColorInterpolationColorSpace,
        WebCore::ColorInterpolationColorSpace::HSL,
        WebCore::ColorInterpolationColorSpace::HWB,
        WebCore::ColorInterpolationColorSpace::LCH,
        WebCore::ColorInterpolationColorSpace::Lab,
        WebCore::ColorInterpolationColorSpace::OKLCH,
        WebCore::ColorInterpolationColorSpace::OKLab,
        WebCore::ColorInterpolationColorSpace::SRGB,
        WebCore::ColorInterpolationColorSpace::SRGBLinear,
        WebCore::ColorInterpolationColorSpace::XYZD50,
        WebCore::ColorInterpolationColorSpace::XYZD65
    >;
};

}
