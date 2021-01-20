/*
 * Copyright (C) 2017, 2020 Apple Inc. All rights reserved.
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

namespace WebCore {

// These are the standard sRGB <-> LinearRGB / DisplayP3 <-> LinearDisplayP3 conversion functions (https://en.wikipedia.org/wiki/SRGB).
float linearToRGBColorComponent(float);
float rgbToLinearColorComponent(float);


// All color types must at least implement the following conversions to and from the XYZA color space:
//    XYZA<float> toXYZA(const ColorType<float>&);
//    ColorType<float> toColorType(const XYZA<float>&);
//
// Any additional conversions can be thought of as optimizations, shortcutting unnecessary steps, though
// some may be integral to the base conversion.


// SRGBA
WEBCORE_EXPORT XYZA<float> toXYZA(const SRGBA<float>&);
WEBCORE_EXPORT SRGBA<float> toSRGBA(const XYZA<float>&);
// Additions
WEBCORE_EXPORT LinearSRGBA<float> toLinearSRGBA(const SRGBA<float>&);
WEBCORE_EXPORT HSLA<float> toHSLA(const SRGBA<float>&);
WEBCORE_EXPORT CMYKA<float> toCMYKA(const SRGBA<float>&);

// LinearSRGBA
WEBCORE_EXPORT XYZA<float> toXYZA(const LinearSRGBA<float>&);
WEBCORE_EXPORT LinearSRGBA<float> toLinearSRGBA(const XYZA<float>&);
// Additions
WEBCORE_EXPORT SRGBA<float> toSRGBA(const LinearSRGBA<float>&);


// DisplayP3
WEBCORE_EXPORT XYZA<float> toXYZA(const DisplayP3<float>&);
WEBCORE_EXPORT DisplayP3<float> toDisplayP3(const XYZA<float>&);
// Additions
WEBCORE_EXPORT LinearDisplayP3<float> toLinearDisplayP3(const DisplayP3<float>&);


// LinearDisplayP3
WEBCORE_EXPORT XYZA<float> toXYZA(const LinearDisplayP3<float>&);
WEBCORE_EXPORT LinearDisplayP3<float> toLinearDisplayP3(const XYZA<float>&);
// Additions
WEBCORE_EXPORT DisplayP3<float> toDisplayP3(const LinearDisplayP3<float>&);


// HSLA
WEBCORE_EXPORT XYZA<float> toXYZA(const HSLA<float>&);
WEBCORE_EXPORT HSLA<float> toHSLA(const XYZA<float>&);
// Additions
WEBCORE_EXPORT SRGBA<float> toSRGBA(const HSLA<float>&);


// CMYKA
WEBCORE_EXPORT XYZA<float> toXYZA(const CMYKA<float>&);
WEBCORE_EXPORT CMYKA<float> toCMYKA(const XYZA<float>&);
// Additions
WEBCORE_EXPORT SRGBA<float> toSRGBA(const CMYKA<float>&);


// Identity conversions (useful for generic contexts).

constexpr SRGBA<float> toSRGBA(const SRGBA<float>& color) { return color; }
constexpr LinearSRGBA<float> toLinearSRGBA(const LinearSRGBA<float>& color) { return color; }
constexpr DisplayP3<float> toDisplayP3(const DisplayP3<float>& color) { return color; }
constexpr LinearDisplayP3<float> toLinearDisplayP3(const LinearDisplayP3<float>& color) { return color; }
constexpr HSLA<float> toHSLA(const HSLA<float>& color) { return color; }
constexpr CMYKA<float> toCMYKA(const CMYKA<float>& color) { return color; }
constexpr XYZA<float> toXYZA(const XYZA<float>& color) { return color; }


// Fallback conversions.

// All types are required to have a conversion to XYZA, so these are guaranteed to work if
// another overload is not already provided.

template<typename T> SRGBA<float> toSRGBA(const T& color)
{
    return toSRGBA(toXYZA(color));
}

template<typename T> LinearSRGBA<float> toLinearSRGBA(const T& color)
{
    return toLinearSRGBA(toXYZA(color));
}

template<typename T> DisplayP3<float> toDisplayP3(const T& color)
{
    return toDisplayP3(toXYZA(color));
}

template<typename T> LinearDisplayP3<float> toLinearDisplayP3(const T& color)
{
    return toLinearDisplayP3(toXYZA(color));
}

template<typename T> HSLA<float> toHSLA(const T& color)
{
    return toHSLA(toXYZA(color));
}

template<typename T> CMYKA<float> toCMYKA(const T& color)
{
    return toCMYKA(toXYZA(color));
}

} // namespace WebCore
