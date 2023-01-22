/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

static constexpr ASCIILiteral serializationForCSS(ColorInterpolationColorSpace interpolationColorSpace)
{
    switch (interpolationColorSpace) {
    case ColorInterpolationColorSpace::HSL:
        return "hsl"_s;
    case ColorInterpolationColorSpace::HWB:
        return "hwb"_s;
    case ColorInterpolationColorSpace::LCH:
        return "lch"_s;
    case ColorInterpolationColorSpace::Lab:
        return "lab"_s;
    case ColorInterpolationColorSpace::OKLCH:
        return "oklch"_s;
    case ColorInterpolationColorSpace::OKLab:
        return "oklab"_s;
    case ColorInterpolationColorSpace::SRGB:
        return "srgb"_s;
    case ColorInterpolationColorSpace::SRGBLinear:
        return "srgb-linear"_s;
    case ColorInterpolationColorSpace::XYZD50:
        return "xyz-d50"_s;
    case ColorInterpolationColorSpace::XYZD65:
        return "xyz-d65"_s;
    }

    ASSERT_NOT_REACHED();
    return ""_s;
}

static void serializationForCSS(StringBuilder& builder, ColorInterpolationColorSpace interpolationColorSpace)
{
    builder.append(serializationForCSS(interpolationColorSpace));
}

static void serializationForCSS(StringBuilder& builder, HueInterpolationMethod hueInterpolationMethod)
{
    switch (hueInterpolationMethod) {
    case HueInterpolationMethod::Shorter:
        break;
    case HueInterpolationMethod::Longer:
        builder.append(" longer hue");
        break;
    case HueInterpolationMethod::Increasing:
        builder.append(" increasing hue");
        break;
    case HueInterpolationMethod::Decreasing:
        builder.append(" decreasing hue");
        break;
    }
}

void serializationForCSS(StringBuilder& builder, const ColorInterpolationMethod& method)
{
    WTF::switchOn(method.colorSpace,
        [&] (auto& type) {
            serializationForCSS(builder, type.interpolationColorSpace);
            if constexpr (hasHueInterpolationMethod<decltype(type)>)
                serializationForCSS(builder, type.hueInterpolationMethod);
        }
    );
}

String serializationForCSS(const ColorInterpolationMethod& method)
{
    StringBuilder builder;
    serializationForCSS(builder, method);
    return builder.toString();
}

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
