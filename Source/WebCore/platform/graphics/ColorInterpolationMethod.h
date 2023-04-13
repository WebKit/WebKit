/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include <optional>
#include <variant>
#include <wtf/EnumTraits.h>
#include <wtf/Hasher.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

enum class HueInterpolationMethod : uint8_t {
    Shorter,
    Longer,
    Increasing,
    Decreasing
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

template <typename T, typename = void>
struct HasHueInterpolationMethod : std::false_type { };

template <typename T>
struct HasHueInterpolationMethod<T, std::void_t<decltype(std::declval<T>().hueInterpolationMethod)>> : std::true_type { };

template <typename T>
inline constexpr bool hasHueInterpolationMethod = HasHueInterpolationMethod<T>::value;

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
        using ColorType = WebCore::ExtendedSRGBA<float>;
    };
    struct SRGBLinear {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::SRGBLinear;
        using ColorType = WebCore::ExtendedLinearSRGBA<float>;
    };
    struct XYZD50 {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::XYZD50;
        using ColorType = WebCore::XYZA<float, WhitePoint::D50>;
    };
    struct XYZD65 {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::XYZD65;
        using ColorType = WebCore::XYZA<float, WhitePoint::D65>;
    };

    std::variant<HSL, HWB, LCH, Lab, OKLCH, OKLab, SRGB, SRGBLinear, XYZD50, XYZD65> colorSpace;
    AlphaPremultiplication alphaPremultiplication;
};

inline void add(Hasher& hasher, const ColorInterpolationMethod& colorInterpolationMethod)
{
    add(hasher, colorInterpolationMethod.alphaPremultiplication);
    WTF::switchOn(colorInterpolationMethod.colorSpace,
        [&] (auto& type) {
            add(hasher, type.interpolationColorSpace);
            if constexpr (std::remove_reference_t<decltype(type)>::ColorType::Model::coordinateSystem == ColorSpaceCoordinateSystem::CylindricalPolar) {
                add(hasher, type.hueInterpolationMethod);
            }
        }
    );
}

inline constexpr bool operator==(const ColorInterpolationMethod::HSL& a, const ColorInterpolationMethod::HSL& b)
{
    return a.hueInterpolationMethod == b.hueInterpolationMethod;
}

inline constexpr bool operator==(const ColorInterpolationMethod::HWB& a, const ColorInterpolationMethod::HWB& b)
{
    return a.hueInterpolationMethod == b.hueInterpolationMethod;
}

inline constexpr bool operator==(const ColorInterpolationMethod::LCH& a, const ColorInterpolationMethod::LCH& b)
{
    return a.hueInterpolationMethod == b.hueInterpolationMethod;
}

inline constexpr bool operator==(const ColorInterpolationMethod::OKLCH& a, const ColorInterpolationMethod::OKLCH& b)
{
    return a.hueInterpolationMethod == b.hueInterpolationMethod;
}

inline constexpr bool operator==(const ColorInterpolationMethod::Lab&, const ColorInterpolationMethod::Lab&)
{
    return true;
}

inline constexpr bool operator==(const ColorInterpolationMethod::OKLab&, const ColorInterpolationMethod::OKLab&)
{
    return true;
}

inline constexpr bool operator==(const ColorInterpolationMethod::SRGB&, const ColorInterpolationMethod::SRGB&)
{
    return true;
}

inline constexpr bool operator==(const ColorInterpolationMethod::SRGBLinear&, const ColorInterpolationMethod::SRGBLinear&)
{
    return true;
}

inline constexpr bool operator==(const ColorInterpolationMethod::XYZD50&, const ColorInterpolationMethod::XYZD50&)
{
    return true;
}

inline constexpr bool operator==(const ColorInterpolationMethod::XYZD65&, const ColorInterpolationMethod::XYZD65&)
{
    return true;
}

inline constexpr bool operator==(const ColorInterpolationMethod& a, const ColorInterpolationMethod& b)
{
    return a.alphaPremultiplication == b.alphaPremultiplication && a.colorSpace == b.colorSpace;
}

void serializationForCSS(StringBuilder&, const ColorInterpolationMethod&);
String serializationForCSS(const ColorInterpolationMethod&);

WTF::TextStream& operator<<(WTF::TextStream&, ColorInterpolationColorSpace);
WTF::TextStream& operator<<(WTF::TextStream&, HueInterpolationMethod);
WTF::TextStream& operator<<(WTF::TextStream&, const ColorInterpolationMethod&);

}

namespace WTF {

template<> struct EnumTraits<WebCore::HueInterpolationMethod> {
    using values = EnumValues<
        WebCore::HueInterpolationMethod,
        WebCore::HueInterpolationMethod::Shorter,
        WebCore::HueInterpolationMethod::Longer,
        WebCore::HueInterpolationMethod::Increasing,
        WebCore::HueInterpolationMethod::Decreasing
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
