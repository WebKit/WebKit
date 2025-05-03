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

#include <math.h>
#include <wtf/MathExtras.h>

namespace WebCore {

template<typename, size_t> struct ColorComponents;

template<size_t ColumnCount, size_t RowCount, typename MatrixValueType = float>
class ColorMatrix {
public:
    template<typename ...Ts, typename = std::enable_if_t<(std::is_convertible_v<Ts, MatrixValueType> && ...)>>
    explicit constexpr ColorMatrix(Ts ...input)
        : m_matrix { { static_cast<MatrixValueType>(input) ... } }
    {
        static_assert(sizeof...(Ts) == RowCount * ColumnCount);
    }

    template<typename ColorValueType, size_t NumberOfComponents>
    constexpr ColorComponents<ColorValueType, NumberOfComponents> transformedColorComponents(const ColorComponents<ColorValueType, NumberOfComponents>&) const;

    constexpr MatrixValueType at(size_t row, size_t column) const
    {
        return m_matrix[(row * ColumnCount) + column];
    }

    friend bool operator==(const ColorMatrix&, const ColorMatrix&) = default;

    template <typename TargetValueType>
    constexpr ColorMatrix<ColumnCount, RowCount, TargetValueType> toValueType() const
    {
        if constexpr(std::is_same_v<TargetValueType, MatrixValueType>)
            return *this;
        else
            return ColorMatrix<ColumnCount, RowCount, TargetValueType> { *this };
    }

private:
    template <size_t OtherColumnCount, size_t OtherRowCount, typename OtherMatrixValueType>
    friend class ColorMatrix;

    template <typename FromValueType>
    constexpr ColorMatrix(const ColorMatrix<ColumnCount, RowCount, FromValueType>& input)
    {
        for (size_t row = 0; row < RowCount; ++row) {
            for (size_t column = 0; column < ColumnCount; ++column)
                m_matrix[(row * ColumnCount) + column] = input.at(row, column);
        }
    }

    std::array<MatrixValueType, RowCount * ColumnCount> m_matrix;
};

constexpr ColorMatrix<3, 3> brightnessColorMatrix(float amount)
{
    // Brightness is specified as a component transfer function: https://www.w3.org/TR/filter-effects-1/#brightnessEquivalent
    // which is equivalent to the following matrix.
    amount = std::max(amount, 0.0f);
    return ColorMatrix<3, 3> {
        amount, 0.0f, 0.0f,
        0.0f, amount, 0.0f,
        0.0f, 0.0f, amount,
    };
}

constexpr ColorMatrix<5, 4> contrastColorMatrix(float amount)
{
    // Contrast is specified as a component transfer function: https://www.w3.org/TR/filter-effects-1/#contrastEquivalent
    // which is equivalent to the following matrix.
    amount = std::max(amount, 0.0f);
    float intercept = -0.5f * amount + 0.5f;

    return ColorMatrix<5, 4> {
        amount, 0.0f, 0.0f, 0.0f, intercept,
        0.0f, amount, 0.0f, 0.0f, intercept,
        0.0f, 0.0f, amount, 0.0f, intercept,
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f
    };
}

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

constexpr ColorMatrix<5, 4> invertColorMatrix(float amount)
{
    // Invert is specified as a component transfer function: https://www.w3.org/TR/filter-effects-1/#invertEquivalent
    // which is equivalent to the following matrix.
    amount = std::clamp(amount, 0.0f, 1.0f);
    float multiplier = 1.0f - amount * 2.0f;
    return ColorMatrix<5, 4> {
        multiplier, 0.0f, 0.0f, 0.0f, amount,
        0.0f, multiplier, 0.0f, 0.0f, amount,
        0.0f, 0.0f, multiplier, 0.0f, amount,
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f
    };
}

constexpr ColorMatrix<5, 4> opacityColorMatrix(float amount)
{
    // Opacity is specified as a component transfer function: https://www.w3.org/TR/filter-effects-1/#opacityEquivalent
    // which is equivalent to the following matrix.
    amount = std::clamp(amount, 0.0f, 1.0f);
    return ColorMatrix<5, 4> {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, amount, 0.0f
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

template<size_t ColumnCount, size_t RowCount, typename MatrixValueType>
template<typename ColorValueType, size_t NumberOfComponents>
constexpr auto ColorMatrix<ColumnCount, RowCount, MatrixValueType>::transformedColorComponents(const ColorComponents<ColorValueType, NumberOfComponents>& inputVector) const -> ColorComponents<ColorValueType, NumberOfComponents>
{
    using ValueType = std::conditional_t<sizeof(MatrixValueType) >= sizeof(ColorValueType), MatrixValueType, ColorValueType>;
    static_assert(ColorComponents<ValueType, NumberOfComponents>::Size >= RowCount);
    
    ColorComponents<ValueType, NumberOfComponents> result;
    for (size_t row = 0; row < RowCount; ++row) {
        if constexpr (ColumnCount <= ColorComponents<ColorValueType, NumberOfComponents>::Size) {
            for (size_t column = 0; column < ColumnCount; ++column)
                result[row] += at(row, column) * inputVector[column];
        } else if constexpr (ColumnCount > ColorComponents<ColorValueType, NumberOfComponents>::Size) {
            for (size_t column = 0; column < ColorComponents<ColorValueType, NumberOfComponents>::Size; ++column)
                result[row] += at(row, column) * inputVector[column];
            for (size_t additionalColumn = ColorComponents<ColorValueType, NumberOfComponents>::Size; additionalColumn < ColumnCount; ++additionalColumn)
                result[row] += at(row, additionalColumn);
        }
    }
    if constexpr (ColorComponents<ColorValueType, NumberOfComponents>::Size > RowCount) {
        for (size_t additionalRow = RowCount; additionalRow < ColorComponents<ColorValueType, NumberOfComponents>::Size; ++additionalRow)
            result[additionalRow] = inputVector[additionalRow];
    }

    return result;
}

template<typename T, typename M> inline constexpr auto applyMatricesToColorComponents(const ColorComponents<T, 4>& components, M matrix) -> ColorComponents<T, 4>
{
    return matrix.transformedColorComponents(components);
}

template<typename T, typename M, typename... Matrices> inline constexpr auto applyMatricesToColorComponents(const ColorComponents<T, 4>& components, M matrix, Matrices... matrices) -> ColorComponents<T, 4>
{
    return applyMatricesToColorComponents(matrix.transformedColorComponents(components), matrices...);
}

template<size_t ColumnCount, size_t RowCount, typename MatrixValueType>
consteval ColorMatrix<ColumnCount, RowCount, MatrixValueType> premultiply(const ColorMatrix<ColumnCount, RowCount, MatrixValueType>& m1, const ColorMatrix<ColumnCount, RowCount, MatrixValueType>& m2)
{
    static_assert(ColumnCount == 3);
    static_assert(ColumnCount == 3);

    float a = m1.at(0, 0), b = m1.at(0, 1), c = m1.at(0, 2);
    float d = m1.at(1, 0), e = m1.at(1, 1), f = m1.at(1, 2);
    float g = m1.at(2, 0), h = m1.at(2, 1), i = m1.at(2, 2);

    float j = m2.at(0, 0), k = m2.at(0, 1), l = m2.at(0, 2);
    float m = m2.at(1, 0), n = m2.at(1, 1), o = m2.at(1, 2);
    float p = m2.at(2, 0), q = m2.at(2, 1), r = m2.at(2, 2);

    return ColorMatrix<ColumnCount, RowCount, MatrixValueType> {
        a*j + d*k + g*l, b*j + e*k + h*l, c*j + f*k + i*l,
        a*m + d*n + g*o, b*m + e*n + h*o, c*m + f*n + i*o,
        a*p + d*q + g*r, b*p + e*q + h*r, c*p + f*q + i*r
    };
}

} // namespace WebCore
