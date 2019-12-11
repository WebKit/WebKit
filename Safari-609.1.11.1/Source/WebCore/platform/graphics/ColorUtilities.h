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

#include "Color.h"
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

    FloatComponents(const Color&);

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
    ColorComponents(const FloatComponents&);
    
    static ColorComponents fromRGBA(unsigned pixel)
    {
        return ColorComponents((pixel >> 24) & 0xFF, (pixel >> 16) & 0xFF, (pixel >> 8) & 0xFF, pixel & 0xFF);
    }
    
    ColorComponents(uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, uint8_t d = 0)
    {
        components[0] = a;
        components[1] = b;
        components[2] = c;
        components[3] = d;
    }
    
    unsigned toRGBA() const
    {
        return components[0] << 24 | components[1] << 16 | components[2] << 8 | components[3];
    }

    uint8_t components[4] { };
};

inline ColorComponents perComponentMax(const ColorComponents& a, const ColorComponents& b)
{
    return {
        std::max(a.components[0], b.components[0]),
        std::max(a.components[1], b.components[1]),
        std::max(a.components[2], b.components[2]),
        std::max(a.components[3], b.components[3])
    };
}

inline ColorComponents perComponentMin(const ColorComponents& a, const ColorComponents& b)
{
    return {
        std::min(a.components[0], b.components[0]),
        std::min(a.components[1], b.components[1]),
        std::min(a.components[2], b.components[2]),
        std::min(a.components[3], b.components[3])
    };
}

inline uint8_t clampedColorComponent(float f)
{
    // See also colorFloatToRGBAByte().
    return std::max(0, std::min(static_cast<int>(lroundf(255.0f * f)), 255));
}

inline unsigned byteOffsetOfPixel(unsigned x, unsigned y, unsigned rowBytes)
{
    const unsigned bytesPerPixel = 4;
    return x * bytesPerPixel + y * rowBytes;
}

// 0-1 components, result is clamped.
float linearToSRGBColorComponent(float);
float sRGBToLinearColorComponent(float);

FloatComponents sRGBColorToLinearComponents(const Color&);
FloatComponents sRGBToLinearComponents(const FloatComponents&);
FloatComponents linearToSRGBComponents(const FloatComponents&);

FloatComponents sRGBToHSL(const FloatComponents&);
FloatComponents HSLToSRGB(const FloatComponents&);

float luminance(const FloatComponents& sRGBCompontents);
float contrastRatio(const FloatComponents&, const FloatComponents&);

class ColorMatrix {
public:
    static ColorMatrix grayscaleMatrix(float);
    static ColorMatrix saturationMatrix(float);
    static ColorMatrix hueRotateMatrix(float angleInDegrees);
    static ColorMatrix sepiaMatrix(float);

    ColorMatrix();
    ColorMatrix(float[20]);
    
    void transformColorComponents(FloatComponents&) const;

private:
    void makeIdentity();

    float m_matrix[4][5];
};

} // namespace WebCore

