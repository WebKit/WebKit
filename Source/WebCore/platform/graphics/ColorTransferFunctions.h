/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

template<typename T, TransferFunctionMode mode>
struct A98RGBTransferFunction {
    static T toGammaEncoded(T);
    static T toLinear(T);
};

template<typename T, TransferFunctionMode mode>
struct SRGBTransferFunction {
    static T toGammaEncoded(T);
    static T toLinear(T);
};

template<typename T, TransferFunctionMode mode> T A98RGBTransferFunction<T, mode>::toGammaEncoded(T c)
{
    auto sign = std::signbit(c) ? -1.0f : 1.0f;
    auto result = std::pow(std::abs(c), 256.0f / 563.0f) * sign;
    if constexpr (mode == TransferFunctionMode::Clamped)
        return clampTo<T>(result, 0, 1);
    return result;
}

template<typename T, TransferFunctionMode mode> T A98RGBTransferFunction<T, mode>::toLinear(T c)
{
    auto sign = std::signbit(c) ? -1.0f : 1.0f;
    auto result = std::pow(std::abs(c), 563.0f / 256.0f) * sign;
    if constexpr (mode == TransferFunctionMode::Clamped)
        return clampTo<T>(result, 0, 1);
    return result;
}

template<typename T, TransferFunctionMode mode> T SRGBTransferFunction<T, mode>::toGammaEncoded(T c)
{
    if constexpr (mode == TransferFunctionMode::Clamped) {
        if (c < 0.0031308f)
            return std::max<T>(12.92f * c, 0);
        return clampTo<T>(1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f, 0, 1);
    } else {
        auto sign = std::signbit(c) ? -1.0f : 1.0f;
        c = std::abs(c);

        if (c < 0.0031308f)
            return 12.92f * c * sign;
        return (1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f) * sign;
    }
}

template<typename T, TransferFunctionMode mode> T SRGBTransferFunction<T, mode>::toLinear(T c)
{
    if constexpr (mode == TransferFunctionMode::Clamped) {
        if (c <= 0.04045f)
            return std::max<float>(c / 12.92f, 0);
        return clampTo<float>(std::pow((c + 0.055f) / 1.055f, 2.4f), 0, 1);
    } else {
        auto sign = std::signbit(c) ? -1.0f : 1.0f;
        c = std::abs(c);

        if (c <= 0.04045f)
            return c / 12.92f * sign;
        return std::pow((c + 0.055f) / 1.055f, 2.4f) * sign;
    }
}

}
