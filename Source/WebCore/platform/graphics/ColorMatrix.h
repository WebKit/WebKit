/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "ColorComponents.h"
#include <math.h>
#include <wtf/MathExtras.h>

namespace WebCore {

template<size_t Columns, size_t Rows>
class ColorMatrix {
public:
    template<typename ...Ts>
    explicit constexpr ColorMatrix(Ts ...input)
        : m_matrix {{ input ... }}
    {
        static_assert(sizeof...(Ts) == Rows * Columns);
    }

    constexpr void transformColorComponents(ColorComponents<float>&) const;
    constexpr ColorComponents<float> transformedColorComponents(const ColorComponents<float>&) const;

    constexpr float at(size_t row, size_t column) const
    {
        return m_matrix[(row * Columns) + column];
    }

private:
    std::array<float, Rows * Columns> m_matrix;
};

// FIXME: These are only used in FilterOperations.cpp. Consider moving them there.
constexpr ColorMatrix<3, 3> grayscaleColorMatrix(float amount)
{
    // Values from https://www.w3.org/TR/filter-effects-1/#grayscaleEquivalent
    float oneMinusAmount = std::clamp(1.0f - amount, 0.0f, 1.0f);
    return ColorMatrix<3, 3> {
        0.2126f + 0.7874f * oneMinusAmount, 0.7152f - 0.7152f * oneMinusAmount, 0.0722f - 0.0722f * oneMinusAmount,
        0.2126f - 0.2126f * oneMinusAmount, 0.7152f + 0.2848f * oneMinusAmount, 0.0722f - 0.0722f * oneMinusAmount,
        0.2126f - 0.2126f * oneMinusAmount, 0.7152f - 0.7152f * oneMinusAmount, 0.0722f + 0.9278f * oneMinusAmount
    };
}

constexpr ColorMatrix<3, 3> sepiaColorMatrix(float amount)
{
    // Values from https://www.w3.org/TR/filter-effects-1/#sepiaEquivalent
    float oneMinusAmount = std::clamp(1.0f - amount, 0.0f, 1.0f);
    return ColorMatrix<3, 3> {
        0.393f + 0.607f * oneMinusAmount, 0.769f - 0.769f * oneMinusAmount, 0.189f - 0.189f * oneMinusAmount,
        0.349f - 0.349f * oneMinusAmount, 0.686f + 0.314f * oneMinusAmount, 0.168f - 0.168f * oneMinusAmount,
        0.272f - 0.272f * oneMinusAmount, 0.534f - 0.534f * oneMinusAmount, 0.131f + 0.869f * oneMinusAmount
    };
}

constexpr ColorMatrix<3, 3> saturationColorMatrix(float amount)
{
    // Values from https://www.w3.org/TR/filter-effects-1/#feColorMatrixElement
    return ColorMatrix<3, 3> {
        0.213f + 0.787f * amount,  0.715f - 0.715f * amount, 0.072f - 0.072f * amount,
        0.213f - 0.213f * amount,  0.715f + 0.285f * amount, 0.072f - 0.072f * amount,
        0.213f - 0.213f * amount,  0.715f - 0.715f * amount, 0.072f + 0.928f * amount
    };
}

// NOTE: hueRotateColorMatrix is not constexpr due to use of cos/sin which are not constexpr yet.
inline ColorMatrix<3, 3> hueRotateColorMatrix(float angleInDegrees)
{
    float cosHue = cos(deg2rad(angleInDegrees));
    float sinHue = sin(deg2rad(angleInDegrees));

    // Values from https://www.w3.org/TR/filter-effects-1/#feColorMatrixElement
    return ColorMatrix<3, 3> {
        0.213f + cosHue * 0.787f - sinHue * 0.213f, 0.715f - cosHue * 0.715f - sinHue * 0.715f, 0.072f - cosHue * 0.072f + sinHue * 0.928f,
        0.213f - cosHue * 0.213f + sinHue * 0.143f, 0.715f + cosHue * 0.285f + sinHue * 0.140f, 0.072f - cosHue * 0.072f - sinHue * 0.283f,
        0.213f - cosHue * 0.213f - sinHue * 0.787f, 0.715f - cosHue * 0.715f + sinHue * 0.715f, 0.072f + cosHue * 0.928f + sinHue * 0.072f
    };
}

template<size_t Columns, size_t Rows>
constexpr void ColorMatrix<Columns, Rows>::transformColorComponents(ColorComponents<float>& inputVector) const
{
    inputVector = transformedColorComponents(inputVector);
}

template<size_t Columns, size_t Rows>
constexpr ColorComponents<float> ColorMatrix<Columns, Rows>::transformedColorComponents(const ColorComponents<float>& inputVector) const
{
    static_assert(ColorComponents<float>::Size >= Rows);
    
    ColorComponents<float> result;
    for (size_t row = 0; row < Rows; ++row) {
        if constexpr (Columns <= ColorComponents<float>::Size) {
            for (size_t column = 0; column < Columns; ++column)
                result[row] += at(row, column) * inputVector[column];
        } else if constexpr (Columns > ColorComponents<float>::Size) {
            for (size_t column = 0; column < ColorComponents<float>::Size; ++column)
                result[row] += at(row, column) * inputVector[column];
            for (size_t additionalColumn = ColorComponents<float>::Size; additionalColumn < Columns; ++additionalColumn)
                result[row] += at(row, additionalColumn);
        }
    }
    if constexpr (ColorComponents<float>::Size > Rows) {
        for (size_t additionalRow = Rows; additionalRow < ColorComponents<float>::Size; ++additionalRow)
            result[additionalRow] = inputVector[additionalRow];
    }

    return result;
}

} // namespace WebCore
