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

        friend constexpr bool operator==(const HSL&, const HSL&) = default;
    };
    struct HWB {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::HWB;
        using ColorType = WebCore::HWBA<float>;
        HueInterpolationMethod hueInterpolationMethod = HueInterpolationMethod::Shorter;

        friend constexpr bool operator==(const HWB&, const HWB&) = default;
    };
    struct LCH {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::LCH;
        using ColorType = WebCore::LCHA<float>;
        HueInterpolationMethod hueInterpolationMethod = HueInterpolationMethod::Shorter;

        friend constexpr bool operator==(const LCH&, const LCH&) = default;
    };
    struct Lab {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::Lab;
        using ColorType = WebCore::Lab<float>;
        friend constexpr bool operator==(const Lab&, const Lab&) = default;
    };
    struct OKLCH {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::OKLCH;
        using ColorType = WebCore::OKLCHA<float>;
        HueInterpolationMethod hueInterpolationMethod = HueInterpolationMethod::Shorter;
        friend constexpr bool operator==(const OKLCH&, const OKLCH&) = default;
    };
    struct OKLab {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::OKLab;
        using ColorType = WebCore::OKLab<float>;
        friend constexpr bool operator==(const OKLab&, const OKLab&) = default;
    };
    struct SRGB {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::SRGB;
        using ColorType = WebCore::ExtendedSRGBA<float>;
        friend constexpr bool operator==(const SRGB&, const SRGB&) = default;
    };
    struct SRGBLinear {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::SRGBLinear;
        using ColorType = WebCore::ExtendedLinearSRGBA<float>;
        friend constexpr bool operator==(const SRGBLinear&, const SRGBLinear&) = default;
    };
    struct XYZD50 {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::XYZD50;
        using ColorType = WebCore::XYZA<float, WhitePoint::D50>;
        friend constexpr bool operator==(const XYZD50&, const XYZD50&) = default;
    };
    struct XYZD65 {
        static constexpr auto interpolationColorSpace = ColorInterpolationColorSpace::XYZD65;
        using ColorType = WebCore::XYZA<float, WhitePoint::D65>;
        friend constexpr bool operator==(const XYZD65&, const XYZD65&) = default;
    };

    friend constexpr bool operator==(const ColorInterpolationMethod&, const ColorInterpolationMethod&) = default;

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

void serializationForCSS(StringBuilder&, const ColorInterpolationMethod&);
String serializationForCSS(const ColorInterpolationMethod&);

WTF::TextStream& operator<<(WTF::TextStream&, ColorInterpolationColorSpace);
WTF::TextStream& operator<<(WTF::TextStream&, HueInterpolationMethod);
WTF::TextStream& operator<<(WTF::TextStream&, const ColorInterpolationMethod&);

}
