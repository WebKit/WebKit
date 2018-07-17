/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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
#include "FEColorMatrix.h"

#include "Filter.h"
#include "GraphicsContext.h"
#include <JavaScriptCore/Uint8ClampedArray.h>
#include <wtf/MathExtras.h>
#include <wtf/text/TextStream.h>

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

namespace WebCore {

FEColorMatrix::FEColorMatrix(Filter& filter, ColorMatrixType type, const Vector<float>& values)
    : FilterEffect(filter)
    , m_type(type)
    , m_values(values)
{
}

Ref<FEColorMatrix> FEColorMatrix::create(Filter& filter, ColorMatrixType type, const Vector<float>& values)
{
    return adoptRef(*new FEColorMatrix(filter, type, values));
}

bool FEColorMatrix::setType(ColorMatrixType type)
{
    if (m_type == type)
        return false;
    m_type = type;
    return true;
}

bool FEColorMatrix::setValues(const Vector<float> &values)
{
    if (m_values == values)
        return false;
    m_values = values;
    return true;
}

inline void matrix(float& red, float& green, float& blue, float& alpha, const Vector<float>& values)
{
    float r = red;
    float g = green;
    float b = blue;
    float a = alpha;

    red     = values[ 0] * r + values[ 1] * g + values[ 2] * b + values[ 3] * a + values[ 4] * 255;
    green   = values[ 5] * r + values[ 6] * g + values [7] * b + values[ 8] * a + values[ 9] * 255;
    blue    = values[10] * r + values[11] * g + values[12] * b + values[13] * a + values[14] * 255;
    alpha   = values[15] * r + values[16] * g + values[17] * b + values[18] * a + values[19] * 255;
}

inline void saturateAndHueRotate(float& red, float& green, float& blue, const float* components)
{
    float r = red;
    float g = green;
    float b = blue;

    red     = r * components[0] + g * components[1] + b * components[2];
    green   = r * components[3] + g * components[4] + b * components[5];
    blue    = r * components[6] + g * components[7] + b * components[8];
}

// FIXME: this should use luminance(const FloatComponents& sRGBCompontents).
inline void luminance(float& red, float& green, float& blue, float& alpha)
{
    alpha = 0.2125 * red + 0.7154 * green + 0.0721 * blue;
    red = 0;
    green = 0;
    blue = 0;
}

#if USE(ACCELERATE)
template<ColorMatrixType filterType>
bool effectApplyAccelerated(Uint8ClampedArray& pixelArray, const Vector<float>& values, float components[9], IntSize bufferSize)
{
    ASSERT(pixelArray.length() == bufferSize.area().unsafeGet() * 4);
    
    if (filterType == FECOLORMATRIX_TYPE_MATRIX) {
        // vImageMatrixMultiply_ARGB8888 takes a 4x4 matrix, if any value in the last column of the FEColorMatrix 5x4 matrix
        // is not zero, fall back to non-vImage code.
        if (values[4] != 0 || values[9] != 0 || values[14] != 0 || values[19] != 0)
            return false;
    }

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

    switch (filterType) {
    case FECOLORMATRIX_TYPE_MATRIX: {
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
            static_cast<int16_t>(roundf(components[0] * divisor)),
            static_cast<int16_t>(roundf(components[3] * divisor)),
            static_cast<int16_t>(roundf(components[6] * divisor)),
            0,

            static_cast<int16_t>(roundf(components[1] * divisor)),
            static_cast<int16_t>(roundf(components[4] * divisor)),
            static_cast<int16_t>(roundf(components[7] * divisor)),
            0,

            static_cast<int16_t>(roundf(components[2] * divisor)),
            static_cast<int16_t>(roundf(components[5] * divisor)),
            static_cast<int16_t>(roundf(components[8] * divisor)),
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
    
    return true;
}
#endif

template<ColorMatrixType filterType>
void effectType(Uint8ClampedArray& pixelArray, const Vector<float>& values, IntSize bufferSize)
{
    float components[9];

    if (filterType == FECOLORMATRIX_TYPE_SATURATE)
        FEColorMatrix::calculateSaturateComponents(components, values[0]);
    else if (filterType == FECOLORMATRIX_TYPE_HUEROTATE)
        FEColorMatrix::calculateHueRotateComponents(components, values[0]);

    unsigned pixelArrayLength = pixelArray.length();

#if USE(ACCELERATE)
    if (effectApplyAccelerated<filterType>(pixelArray, values, components, bufferSize))
        return;
#else
    UNUSED_PARAM(bufferSize);
#endif

    switch (filterType) {
    case FECOLORMATRIX_TYPE_MATRIX:
        for (unsigned pixelByteOffset = 0; pixelByteOffset < pixelArrayLength; pixelByteOffset += 4) {
            float red = pixelArray.item(pixelByteOffset);
            float green = pixelArray.item(pixelByteOffset + 1);
            float blue = pixelArray.item(pixelByteOffset + 2);
            float alpha = pixelArray.item(pixelByteOffset + 3);
            matrix(red, green, blue, alpha, values);
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
            saturateAndHueRotate(red, green, blue, components);
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

void FEColorMatrix::platformApplySoftware()
{
    FilterEffect* in = inputEffect(0);

    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return;

    ImageBuffer* inBuffer = in->imageBufferResult();
    if (inBuffer)
        resultImage->context().drawImageBuffer(*inBuffer, drawingRegionOfInputImage(in->absolutePaintRect()));

    IntRect imageRect(IntPoint(), resultImage->logicalSize());
    IntSize pixelArrayDimensions;
    auto pixelArray = resultImage->getUnmultipliedImageData(imageRect, &pixelArrayDimensions);
    if (!pixelArray)
        return;

    Vector<float> values = normalizedFloats(m_values);

    switch (m_type) {
    case FECOLORMATRIX_TYPE_UNKNOWN:
        break;
    case FECOLORMATRIX_TYPE_MATRIX:
        effectType<FECOLORMATRIX_TYPE_MATRIX>(*pixelArray, values, pixelArrayDimensions);
        break;
    case FECOLORMATRIX_TYPE_SATURATE: 
        effectType<FECOLORMATRIX_TYPE_SATURATE>(*pixelArray, values, pixelArrayDimensions);
        break;
    case FECOLORMATRIX_TYPE_HUEROTATE:
        effectType<FECOLORMATRIX_TYPE_HUEROTATE>(*pixelArray, values, pixelArrayDimensions);
        break;
    case FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
        effectType<FECOLORMATRIX_TYPE_LUMINANCETOALPHA>(*pixelArray, values, pixelArrayDimensions);
        setIsAlphaImage(true);
        break;
    }

    resultImage->putByteArray(*pixelArray, AlphaPremultiplication::Unpremultiplied, imageRect.size(), imageRect, IntPoint());
}

static TextStream& operator<<(TextStream& ts, const ColorMatrixType& type)
{
    switch (type) {
    case FECOLORMATRIX_TYPE_UNKNOWN:
        ts << "UNKNOWN";
        break;
    case FECOLORMATRIX_TYPE_MATRIX:
        ts << "MATRIX";
        break;
    case FECOLORMATRIX_TYPE_SATURATE:
        ts << "SATURATE";
        break;
    case FECOLORMATRIX_TYPE_HUEROTATE:
        ts << "HUEROTATE";
        break;
    case FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
        ts << "LUMINANCETOALPHA";
        break;
    }
    return ts;
}

TextStream& FEColorMatrix::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    ts << indent << "[feColorMatrix";
    FilterEffect::externalRepresentation(ts, representation);
    ts << " type=\"" << m_type << "\"";
    if (!m_values.isEmpty()) {
        ts << " values=\"";
        Vector<float>::const_iterator ptr = m_values.begin();
        const Vector<float>::const_iterator end = m_values.end();
        while (ptr < end) {
            ts << *ptr;
            ++ptr;
            if (ptr < end) 
                ts << " ";
        }
        ts << "\"";
    }
    ts << "]\n";
    
    TextStream::IndentScope indentScope(ts);
    inputEffect(0)->externalRepresentation(ts, representation);
    return ts;
}

} // namespace WebCore
