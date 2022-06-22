/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ColorInterpolationMethod.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, ColorInterpolationColorSpace interpolationColorSpace)
{
    switch (interpolationColorSpace) {
    case ColorInterpolationColorSpace::HSL:
        ts << "HSL";
        break;
    case ColorInterpolationColorSpace::HWB:
        ts << "HWB";
        break;
    case ColorInterpolationColorSpace::LCH:
        ts << "LCH";
        break;
    case ColorInterpolationColorSpace::Lab:
        ts << "Lab";
        break;
    case ColorInterpolationColorSpace::OKLCH:
        ts << "OKLCH";
        break;
    case ColorInterpolationColorSpace::OKLab:
        ts << "OKLab";
        break;
    case ColorInterpolationColorSpace::SRGB:
        ts << "sRGB";
        break;
    case ColorInterpolationColorSpace::SRGBLinear:
        ts << "sRGB linear";
        break;
    case ColorInterpolationColorSpace::XYZD50:
        ts << "XYZ D50";
        break;
    case ColorInterpolationColorSpace::XYZD65:
        ts << "XYZ D65";
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, HueInterpolationMethod hueInterpolationMethod)
{
    switch (hueInterpolationMethod) {
    case HueInterpolationMethod::Shorter:
        ts << "shorter";
        break;
    case HueInterpolationMethod::Longer:
        ts << "longer";
        break;
    case HueInterpolationMethod::Increasing:
        ts << "increasing";
        break;
    case HueInterpolationMethod::Decreasing:
        ts << "decreasing";
        break;
    case HueInterpolationMethod::Specified:
        ts << "specified";
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const ColorInterpolationMethod& method)
{
    WTF::switchOn(method.colorSpace,
        [&] (auto& type) {
            ts << type.interpolationColorSpace;
            if constexpr (hasHueInterpolationMethod<decltype(type)>)
                ts << ' ' << type.hueInterpolationMethod;
            ts << ' ' << method.alphaPremultiplication;
        }
    );
    return ts;
}

} // namespace WebCore
