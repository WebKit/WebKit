/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "ColorUtilities.h"

#include "Color.h"
#include <wtf/MathExtras.h>

namespace WebCore {

ColorComponents::ColorComponents(const FloatComponents& floatComponents)
{
    components[0] = clampedColorComponent(floatComponents.components[0]);
    components[1] = clampedColorComponent(floatComponents.components[1]);
    components[2] = clampedColorComponent(floatComponents.components[2]);
    components[3] = clampedColorComponent(floatComponents.components[3]);
}

// These are the standard sRGB <-> linearRGB conversion functions (https://en.wikipedia.org/wiki/SRGB).
float linearToSRGBColorComponent(float c)
{
    if (c < 0.0031308)
        return 12.92 * c;

    return clampTo<float>(1.055 * powf(c, 1.0 / 2.4) - 0.055, 0, 1);
}

float sRGBToLinearColorComponent(float c)
{
    if (c <= 0.04045)
        return c / 12.92;

    return clampTo<float>(powf((c + 0.055) / 1.055, 2.4), 0, 1);
}

Color linearToSRGBColor(const Color& color)
{
    float r, g, b, a;
    color.getRGBA(r, g, b, a);
    r = linearToSRGBColorComponent(r);
    g = linearToSRGBColorComponent(g);
    b = linearToSRGBColorComponent(b);

    return Color(r, g, b, a);
}

Color sRGBToLinearColor(const Color& color)
{
    float r, g, b, a;
    color.getRGBA(r, g, b, a);
    r = sRGBToLinearColorComponent(r);
    g = sRGBToLinearColorComponent(g);
    b = sRGBToLinearColorComponent(b);

    return Color(r, g, b, a);
}

} // namespace WebCore
