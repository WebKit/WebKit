/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2021 Apple Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "FEColorMatrixSoftwareApplier.h"

#include "FEColorMatrix.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PixelBuffer.h"
#include <wtf/MathExtras.h>

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

namespace WebCore {

FEColorMatrixSoftwareApplier::FEColorMatrixSoftwareApplier(const FEColorMatrix& effect)
    : Base(effect)
{
    if (m_effect.type() == FECOLORMATRIX_TYPE_SATURATE)
        FEColorMatrix::calculateSaturateComponents(m_components, m_effect.values()[0]);
    else if (m_effect.type() == FECOLORMATRIX_TYPE_HUEROTATE)
        FEColorMatrix::calculateHueRotateComponents(m_components, m_effect.values()[0]);
}

inline void FEColorMatrixSoftwareApplier::matrix(float& red, float& green, float& blue, float& alpha) const
{
    float r = red;
    float g = green;
    float b = blue;
    float a = alpha;

    const auto& values = m_effect.values();

    red   = values[ 0] * r + values[ 1] * g + values[ 2] * b + values[ 3] * a + values[ 4] * 255;
    green = values[ 5] * r + values[ 6] * g + values[ 7] * b + values[ 8] * a + values[ 9] * 255;
    blue  = values[10] * r + values[11] * g + values[12] * b + values[13] * a + values[14] * 255;
    alpha = values[15] * r + values[16] * g + values[17] * b + values[18] * a + values[19] * 255;
}

inline void FEColorMatrixSoftwareApplier::saturateAndHueRotate(float& red, float& green, float& blue) const
{
    float r = red;
    float g = green;
    float b = blue;

    red     = r * m_components[0] + g * m_components[1] + b * m_components[2];
    green   = r * m_components[3] + g * m_components[4] + b * m_components[5];
    blue    = r * m_components[6] + g * m_components[7] + b * m_components[8];
}

// FIXME: this should use the luminance(...) function in ColorLuminance.h.
inline void FEColorMatrixSoftwareApplier::luminance(float& red, float& green, float& blue, float& alpha) const
{
    alpha = 0.2125 * red + 0.7154 * green + 0.0721 * blue;
    red = 0;
    green = 0;
    blue = 0;
}

#if USE(ACCELERATE)
void FEColorMatrixSoftwareApplier::applyPlatformAccelerated(PixelBuffer& pixelBuffer) const
{
    auto& pixelArray = pixelBuffer.data();
    auto bufferSize = pixelBuffer.size();

    ASSERT(pixelArray.length() == bufferSize.area() * 4);
    
    const int32_t divisor = 256;
    uint8_t* data = pixelArray.data();

    vImage_Buffer src;
    src.width = bufferSize.width();
    src.height = bufferSize.height();
    src.rowBytes = bufferSize.width() * 4;
    src.data = data;
    
    vImage_Buffer dest;
    dest.width = bufferSize.width();
    dest.height = bufferSize.height();
    dest.rowBytes = bufferSize.width() * 4;
    dest.data = data;

    switch (m_effect.type()) {
    case FECOLORMATRIX_TYPE_UNKNOWN:
        break;

    case FECOLORMATRIX_TYPE_MATRIX: {
        const auto& values = m_effect.values();

        const int16_t matrix[4 * 4] = {
            static_cast<int16_t>(roundf(values[ 0] * divisor)),
            static_cast<int16_t>(roundf(values[ 5] * divisor)),
            static_cast<int16_t>(roundf(values[10] * divisor)),
            static_cast<int16_t>(roundf(values[15] * divisor)),

            static_cast<int16_t>(roundf(values[ 1] * divisor)),
            static_cast<int16_t>(roundf(values[ 6] * divisor)),
            static_cast<int16_t>(roundf(values[11] * divisor)),
            static_cast<int16_t>(roundf(values[16] * divisor)),

            static_cast<int16_t>(roundf(values[ 2] * divisor)),
            static_cast<int16_t>(roundf(values[ 7] * divisor)),
            static_cast<int16_t>(roundf(values[12] * divisor)),
            static_cast<int16_t>(roundf(values[17] * divisor)),

            static_cast<int16_t>(roundf(values[ 3] * divisor)),
            static_cast<int16_t>(roundf(values[ 8] * divisor)),
            static_cast<int16_t>(roundf(values[13] * divisor)),
            static_cast<int16_t>(roundf(values[18] * divisor)),
        };
        vImageMatrixMultiply_ARGB8888(&src, &dest, matrix, divisor, nullptr, nullptr, kvImageNoFlags);
        break;
    }

    case FECOLORMATRIX_TYPE_SATURATE:
    case FECOLORMATRIX_TYPE_HUEROTATE: {
        const int16_t matrix[4 * 4] = {
            static_cast<int16_t>(roundf(m_components[0] * divisor)),
            static_cast<int16_t>(roundf(m_components[3] * divisor)),
            static_cast<int16_t>(roundf(m_components[6] * divisor)),
            0,

            static_cast<int16_t>(roundf(m_components[1] * divisor)),
            static_cast<int16_t>(roundf(m_components[4] * divisor)),
            static_cast<int16_t>(roundf(m_components[7] * divisor)),
            0,

            static_cast<int16_t>(roundf(m_components[2] * divisor)),
            static_cast<int16_t>(roundf(m_components[5] * divisor)),
            static_cast<int16_t>(roundf(m_components[8] * divisor)),
            0,

            0,
            0,
            0,
            divisor,
        };
        vImageMatrixMultiply_ARGB8888(&src, &dest, matrix, divisor, nullptr, nullptr, kvImageNoFlags);
        break;
    }
    case FECOLORMATRIX_TYPE_LUMINANCETOALPHA: {
        const int16_t matrix[4 * 4] = {
            0,
            0,
            0,
            static_cast<int16_t>(roundf(0.2125 * divisor)),

            0,
            0,
            0,
            static_cast<int16_t>(roundf(0.7154 * divisor)),

            0,
            0,
            0,
            static_cast<int16_t>(roundf(0.0721 * divisor)),

            0,
            0,
            0,
            0,
        };
        vImageMatrixMultiply_ARGB8888(&src, &dest, matrix, divisor, nullptr, nullptr, kvImageNoFlags);
        break;
    }
    }
}
#endif

void FEColorMatrixSoftwareApplier::applyPlatformUnaccelerated(PixelBuffer& pixelBuffer) const
{
    auto& pixelArray = pixelBuffer.data();
    unsigned pixelArrayLength = pixelArray.length();

    switch (m_effect.type()) {
    case FECOLORMATRIX_TYPE_UNKNOWN:
        break;

    case FECOLORMATRIX_TYPE_MATRIX:
        for (unsigned pixelByteOffset = 0; pixelByteOffset < pixelArrayLength; pixelByteOffset += 4) {
            float red = pixelArray.item(pixelByteOffset);
            float green = pixelArray.item(pixelByteOffset + 1);
            float blue = pixelArray.item(pixelByteOffset + 2);
            float alpha = pixelArray.item(pixelByteOffset + 3);
            matrix(red, green, blue, alpha);
            pixelArray.set(pixelByteOffset, red);
            pixelArray.set(pixelByteOffset + 1, green);
            pixelArray.set(pixelByteOffset + 2, blue);
            pixelArray.set(pixelByteOffset + 3, alpha);
        }
        break;

    case FECOLORMATRIX_TYPE_SATURATE:
    case FECOLORMATRIX_TYPE_HUEROTATE:
        for (unsigned pixelByteOffset = 0; pixelByteOffset < pixelArrayLength; pixelByteOffset += 4) {
            float red = pixelArray.item(pixelByteOffset);
            float green = pixelArray.item(pixelByteOffset + 1);
            float blue = pixelArray.item(pixelByteOffset + 2);
            float alpha = pixelArray.item(pixelByteOffset + 3);
            saturateAndHueRotate(red, green, blue);
            pixelArray.set(pixelByteOffset, red);
            pixelArray.set(pixelByteOffset + 1, green);
            pixelArray.set(pixelByteOffset + 2, blue);
            pixelArray.set(pixelByteOffset + 3, alpha);
        }
        break;

    case FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
        for (unsigned pixelByteOffset = 0; pixelByteOffset < pixelArrayLength; pixelByteOffset += 4) {
            float red = pixelArray.item(pixelByteOffset);
            float green = pixelArray.item(pixelByteOffset + 1);
            float blue = pixelArray.item(pixelByteOffset + 2);
            float alpha = pixelArray.item(pixelByteOffset + 3);
            luminance(red, green, blue, alpha);
            pixelArray.set(pixelByteOffset, red);
            pixelArray.set(pixelByteOffset + 1, green);
            pixelArray.set(pixelByteOffset + 2, blue);
            pixelArray.set(pixelByteOffset + 3, alpha);
        }
        break;
    }
}

void FEColorMatrixSoftwareApplier::applyPlatform(PixelBuffer& pixelBuffer) const
{
#if USE(ACCELERATE)
    const auto& values = m_effect.values();

    // vImageMatrixMultiply_ARGB8888 takes a 4x4 matrix, if any value in the last column of the FEColorMatrix 5x4 matrix
    // is not zero, fall back to non-vImage code.
    if (m_effect.type() != FECOLORMATRIX_TYPE_MATRIX || (!values[4] && !values[9] && !values[14] && !values[19])) {
        applyPlatformAccelerated(pixelBuffer);
        return;
    }
#endif
    applyPlatformUnaccelerated(pixelBuffer);
}

bool FEColorMatrixSoftwareApplier::apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const
{
    auto& input = inputs[0].get();

    auto resultImage = result.imageBuffer();
    if (!resultImage)
        return false;

    auto inputImage = input.imageBuffer();
    if (inputImage) {
        auto inputImageRect = input.absoluteImageRectRelativeTo(result);
        resultImage->context().drawImageBuffer(*inputImage, inputImageRect);
    }

    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, result.colorSpace() };
    IntRect imageRect(IntPoint(), resultImage->truncatedLogicalSize());
    auto pixelBuffer = resultImage->getPixelBuffer(format, imageRect);
    if (!pixelBuffer)
        return false;

    applyPlatform(*pixelBuffer);
    resultImage->putPixelBuffer(*pixelBuffer, imageRect);
    return true;
}

} // namespace WebCore
