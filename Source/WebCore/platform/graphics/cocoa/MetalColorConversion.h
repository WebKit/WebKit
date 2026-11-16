/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <optional>
#include <simd/simd.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

class ColorSpace;

enum class ColorSpaceName : uint8_t;

// Matches `TransferFunction` in `metalColorConversionShaderSource`.
struct MetalTransferFunction {
    float threshold;
    float linearThreshold;
    float slope;
    float scale;
    float offset;
    float exponent;
};

// Matches `ColorConversionUniforms` in `metalColorConversionShaderSource`.
struct MetalColorConversionUniforms {
    simd_float3x3 toXYZD65;
    simd_float3x3 fromXYZD65;
    simd_float3x3 okLabToNonLinearLMS;
    simd_float3 minimum;
    simd_float3 maximum;
    simd_float3 whitePoint;
    MetalTransferFunction sourceTransferFunction;
    MetalTransferFunction destinationTransferFunction;
    float labE;
    float labK;
    int32_t hueIndex;
    uint32_t model;
};

// Metal source to put before a shader that converts colors. For components in the source color space, of a color without
// premultiplied alpha, `normalizedColorComponents()` does what makeColorTypeByNormalizingComponents() does, and
// `convertColorComponents()` then gives the components in the destination color space. Those can be out of the range
// of the destination, which the caller has to clamp if it needs to.
extern const ASCIILiteral metalColorConversionShaderSource;

// Returns nullopt if the shader can't convert to `destination`. Pass the result in as a `constant ColorConversionUniforms&`.
std::optional<MetalColorConversionUniforms> makeMetalColorConversionUniforms(ColorSpaceName source, const ColorSpace& destination);

} // namespace WebCore
