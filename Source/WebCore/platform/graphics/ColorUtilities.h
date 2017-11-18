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

#pragma once

#include <algorithm>
#include <math.h>

namespace WebCore {

struct FloatComponents {
    FloatComponents(float a = 0, float b = 0, float c = 0, float d = 0)
    {
        components[0] = a;
        components[1] = b;
        components[2] = c;
        components[3] = d;
    }

    FloatComponents& operator+=(const FloatComponents& rhs)
    {
        components[0] += rhs.components[0];
        components[1] += rhs.components[1];
        components[2] += rhs.components[2];
        components[3] += rhs.components[3];
        return *this;
    }

    FloatComponents operator+(float rhs) const
    {
        FloatComponents result;
        result.components[0] = components[0] + rhs;
        result.components[1] = components[1] + rhs;
        result.components[2] = components[2] + rhs;
        result.components[3] = components[3] + rhs;
        return result;
    }

    FloatComponents operator/(float denominator) const
    {
        FloatComponents result;
        result.components[0] = components[0] / denominator;
        result.components[1] = components[1] / denominator;
        result.components[2] = components[2] / denominator;
        result.components[3] = components[3] / denominator;
        return result;
    }

    FloatComponents operator*(float factor) const
    {
        FloatComponents result;
        result.components[0] = components[0] * factor;
        result.components[1] = components[1] * factor;
        result.components[2] = components[2] * factor;
        result.components[3] = components[3] * factor;
        return result;
    }

    FloatComponents abs() const
    {
        FloatComponents result;
        result.components[0] = fabs(components[0]);
        result.components[1] = fabs(components[1]);
        result.components[2] = fabs(components[2]);
        result.components[3] = fabs(components[3]);
        return result;
    }

    float components[4];
};

struct ColorComponents {
    ColorComponents() = default;
    ColorComponents(const FloatComponents&);
    uint8_t components[4] { };
};

inline uint8_t clampedColorComponent(float f)
{
    // See also colorFloatToRGBAByte().
    return std::max(0, std::min(static_cast<int>(lroundf(255.0f * f)), 255));
}

} // namespace WebCore

