/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/MathExtras.h>

namespace WebCore {

// Transfer functions for colors that can be gamma encoded.

enum class TransferFunctionMode : uint8_t {
    Clamped,
    Unclamped
};

// With `sign` as the sign of the component `c`, and `threshold` applying to the magnitude of `c`, a gamma encoded component goes to linear as:
//
//     c / slope                                        up to `threshold`
//     sign * ((|c| + offset) / scale) ^ exponent       past it
//
// and a linear component goes to gamma encoded as:
//
//     c * slope                                        up to `linearThreshold`
//     sign * (scale * |c| ^ (1 / exponent) - offset)   past it
template<typename T>
struct ParametricTransferFunction {
    T threshold;
    T linearThreshold;
    T slope;
    T scale;
    T offset;
    T exponent;
};

template<typename T, TransferFunctionMode mode, ParametricTransferFunction<T> parameters>
struct PiecewiseTransferFunction {
    static constexpr ParametricTransferFunction<T> parametric = parameters;

    static T toGammaEncoded(T);
    static T toLinear(T);
};

template<typename T, TransferFunctionMode mode>
struct A98RGBTransferFunction : PiecewiseTransferFunction<T, mode, ParametricTransferFunction<T> {
    .threshold = 0,
    .linearThreshold = 0,
    .slope = 1,
    .scale = 1,
    .offset = 0,
    .exponent = 563.0 / 256.0,
}> { };

template<typename T, TransferFunctionMode mode>
struct ProPhotoRGBTransferFunction : PiecewiseTransferFunction<T, mode, ParametricTransferFunction<T> {
    .threshold = 16.0 / 512.0,
    .linearThreshold = 1.0 / 512.0,
    .slope = 16,
    .scale = 1,
    .offset = 0,
    .exponent = 1.8,
}> { };

template<typename T, TransferFunctionMode mode>
struct Rec2020TransferFunction : PiecewiseTransferFunction<T, mode, ParametricTransferFunction<T> {
    .threshold = 0.018053968510807 * 4.5,
    .linearThreshold = 0.018053968510807,
    .slope = 4.5,
    .scale = 1.09929682680944,
    .offset = 0.09929682680944,
    .exponent = 1.0 / 0.45,
}> { };

template<typename T, TransferFunctionMode mode>
struct SRGBTransferFunction : PiecewiseTransferFunction<T, mode, ParametricTransferFunction<T> {
    .threshold = 0.04045,
    .linearThreshold = 0.0031308,
    .slope = 12.92,
    .scale = 1.055,
    .offset = 0.055,
    .exponent = 2.4,
}> { };

template<typename T, TransferFunctionMode mode, ParametricTransferFunction<T> parameters>
T PiecewiseTransferFunction<T, mode, parameters>::toGammaEncoded(T c)
{
    auto magnitude = std::abs(c);
    T result = magnitude <= parameters.linearThreshold ? c * parameters.slope : std::copysign(parameters.scale * std::pow(magnitude, 1 / parameters.exponent) - parameters.offset, c);
    if constexpr (mode == TransferFunctionMode::Clamped)
        return clampTo<T>(result, 0, 1);
    return result;
}

template<typename T, TransferFunctionMode mode, ParametricTransferFunction<T> parameters>
T PiecewiseTransferFunction<T, mode, parameters>::toLinear(T c)
{
    auto magnitude = std::abs(c);
    T result = magnitude <= parameters.threshold ? c / parameters.slope : std::copysign(std::pow((magnitude + parameters.offset) / parameters.scale, parameters.exponent), c);
    if constexpr (mode == TransferFunctionMode::Clamped)
        return clampTo<T>(result, 0, 1);
    return result;
}

} // namespace WebCore
