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

WEBCORE_EXPORT LinearSRGBA<float> toLinearSRGBA(const SRGBA<float>&);
WEBCORE_EXPORT SRGBA<float> toSRGBA(const LinearSRGBA<float>&);

WEBCORE_EXPORT LinearDisplayP3<float> toLinearDisplayP3(const DisplayP3<float>&);
WEBCORE_EXPORT DisplayP3<float> toDisplayP3(const LinearDisplayP3<float>&);

WEBCORE_EXPORT SRGBA<float> toSRGBA(const DisplayP3<float>&);
WEBCORE_EXPORT DisplayP3<float> toDisplayP3(const SRGBA<float>&);

WEBCORE_EXPORT HSLA<float> toHSLA(const SRGBA<float>&);
WEBCORE_EXPORT SRGBA<float> toSRGBA(const HSLA<float>&);

SRGBA<float> toSRGBA(const CMYKA<float>&);


// Identity conversions (useful for generic contexts)

constexpr SRGBA<float> toSRGBA(const SRGBA<float>& color) { return color; }
constexpr LinearSRGBA<float> toLinearSRGBA(const LinearSRGBA<float>& color) { return color; }
constexpr DisplayP3<float> toDisplayP3(const DisplayP3<float>& color) { return color; }
constexpr LinearDisplayP3<float> toLinearDisplayP3(const LinearDisplayP3<float>& color) { return color; }
constexpr HSLA<float> toHSLA(const HSLA<float>& color) { return color; }

} // namespace WebCore
