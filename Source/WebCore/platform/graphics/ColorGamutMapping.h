/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/ColorChromaticAdaptation.h>
#include <WebCore/ColorComponents.h>
#include <WebCore/ColorMatrix.h>
#include <WebCore/ColorTypes.h>
#include <WebCore/PlatformExportMacros.h>

namespace WebCore {

// The gamut of an RGB color space, the cube [0, 1] of its linear-light components.
// XYZ D65 is used as it is what OKLab is defined from.
struct RGBGamut {
    ColorMatrix<3, 3> xyzD65ToLinearRGB;
    ColorMatrix<3, 3> linearRGBToXYZD65;

    template<typename ExtendedColorType> static constexpr RGBGamut of();
};

// Gamut mapping algorithms map the linear-light components of a color in an RGB color space into its gamut.

struct ClipGamutMapping {
    static constexpr ColorComponents<float, 3> map(const RGBGamut&, const ColorComponents<float, 3>& linearRGB)
    {
        return linearRGB.map([](float component) {
            return std::clamp(component, 0.0f, 1.0f);
        });
    }
};

struct RayTraceGamutMapping {
    // https://drafts.csswg.org/css-color-4/#GMA-Raytrace
    WEBCORE_EXPORT static ColorComponents<float, 3> map(const RGBGamut&, const ColorComponents<float, 3>& linearRGB);
};

template<typename ExtendedColorType> constexpr RGBGamut RGBGamut::of()
{
    static_assert(IsRGBExtendedType<ExtendedColorType>);

    if constexpr (IsRGBGammaEncodedType<ExtendedColorType>)
        return of<typename ExtendedColorType::LinearCounterpart>();
    else {
        constexpr auto whitePoint = ExtendedColorType::whitePoint;
        if constexpr (whitePoint == WhitePoint::D65)
            return { ExtendedColorType::xyzToLinear, ExtendedColorType::linearToXYZ };
        else {
            return {
                ExtendedColorType::xyzToLinear * ChromaticAdaptation<WhitePoint::D65, whitePoint>::matrix,
                ChromaticAdaptation<whitePoint, WhitePoint::D65>::matrix * ExtendedColorType::linearToXYZ
            };
        }
    }
}

} // namespace WebCore
