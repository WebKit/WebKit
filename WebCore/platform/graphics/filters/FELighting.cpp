/*
 * Copyright (C) 2010 University of Szeged
 * Copyright (C) 2010 Zoltan Herczeg
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(FILTERS)
#include "FELighting.h"

#include "CanvasPixelArray.h"
#include "ImageData.h"
#include "LightSource.h"

namespace WebCore {

FELighting::FELighting(LightingType lightingType, const Color& lightingColor, float surfaceScale,
    float diffuseConstant, float specularConstant, float specularExponent,
    float kernelUnitLengthX, float kernelUnitLengthY, PassRefPtr<LightSource> lightSource)
    : FilterEffect()
    , m_lightingType(lightingType)
    , m_lightSource(lightSource)
    , m_lightingColor(lightingColor)
    , m_surfaceScale(surfaceScale)
    , m_diffuseConstant(diffuseConstant)
    , m_specularConstant(specularConstant)
    , m_specularExponent(specularExponent)
    , m_kernelUnitLengthX(kernelUnitLengthX)
    , m_kernelUnitLengthY(kernelUnitLengthY)
{
}

const static int cPixelSize = 4;
const static int cAlphaChannelOffset = 3;
const static unsigned char cOpaqueAlpha = static_cast<unsigned char>(0xff);

ALWAYS_INLINE int FELighting::LightingData::upLeftPixelValue()
{
    return static_cast<int>(pixels->get(offset - widthMultipliedByPixelSize - cPixelSize + cAlphaChannelOffset));
}

ALWAYS_INLINE int FELighting::LightingData::upPixelValue()
{
    return static_cast<int>(pixels->get(offset - widthMultipliedByPixelSize + cAlphaChannelOffset));
}

ALWAYS_INLINE int FELighting::LightingData::upRightPixelValue()
{
    return static_cast<int>(pixels->get(offset - widthMultipliedByPixelSize + cPixelSize + cAlphaChannelOffset));
}

ALWAYS_INLINE int FELighting::LightingData::leftPixelValue()
{
    return static_cast<int>(pixels->get(offset - cPixelSize + cAlphaChannelOffset));
}

ALWAYS_INLINE int FELighting::LightingData::centerPixelValue()
{
    return static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
}

ALWAYS_INLINE int FELighting::LightingData::rightPixelValue()
{
    return static_cast<int>(pixels->get(offset + cPixelSize + cAlphaChannelOffset));
}

ALWAYS_INLINE int FELighting::LightingData::downLeftPixelValue()
{
    return static_cast<int>(pixels->get(offset + widthMultipliedByPixelSize - cPixelSize + cAlphaChannelOffset));
}

ALWAYS_INLINE int FELighting::LightingData::downPixelValue()
{
    return static_cast<int>(pixels->get(offset + widthMultipliedByPixelSize + cAlphaChannelOffset));
}

ALWAYS_INLINE int FELighting::LightingData::downRightPixelValue()
{
    return static_cast<int>(pixels->get(offset + widthMultipliedByPixelSize + cPixelSize + cAlphaChannelOffset));
}

ALWAYS_INLINE void FELighting::setPixel(LightingData& data, LightSource::PaintingData& paintingData,
    int lightX, int lightY, float factorX, int normalX, float factorY, int normalY)
{
    m_lightSource->updatePaintingData(paintingData, lightX, lightY, static_cast<float>(data.pixels->get(data.offset + 3)) * data.surfaceScale);

    data.normalVector.setX(factorX * static_cast<float>(normalX) * data.surfaceScale);
    data.normalVector.setY(factorY * static_cast<float>(normalY) * data.surfaceScale);
    data.normalVector.setZ(1.0f);
    data.normalVector.normalize();

    if (m_lightingType == FELighting::DiffuseLighting)
        data.lightStrength = m_diffuseConstant * (data.normalVector * paintingData.lightVector);
    else {
        FloatPoint3D halfwayVector = paintingData.lightVector;
        halfwayVector.setZ(halfwayVector.z() + 1.0f);
        halfwayVector.normalize();
        if (m_specularExponent == 1.0f)
            data.lightStrength = m_specularConstant * (data.normalVector * halfwayVector);
        else
            data.lightStrength = m_specularConstant * powf(data.normalVector * halfwayVector, m_specularExponent);
    }

    if (data.lightStrength > 1.0f)
        data.lightStrength = 1.0f;
    if (data.lightStrength < 0.0f)
        data.lightStrength = 0.0f;

    data.pixels->set(data.offset, static_cast<unsigned char>(data.lightStrength * paintingData.colorVector.x()));
    data.pixels->set(data.offset + 1, static_cast<unsigned char>(data.lightStrength * paintingData.colorVector.y()));
    data.pixels->set(data.offset + 2, static_cast<unsigned char>(data.lightStrength * paintingData.colorVector.z()));
}

bool FELighting::drawLighting(CanvasPixelArray* pixels, int width, int height)
{
    LightSource::PaintingData paintingData;
    LightingData data;

    if (!m_lightSource)
        return false;

    // FIXME: do something if width or height (or both) is 1 pixel.
    // The W3 spec does not define this case. Now the filter just returns.
    if (width <= 2 || height <= 2)
        return false;

    data.pixels = pixels;
    data.surfaceScale = m_surfaceScale / 255.0f;
    data.widthMultipliedByPixelSize = width * cPixelSize;
    data.widthDecreasedByOne = width - 1;
    data.heightDecreasedByOne = height - 1;
    paintingData.colorVector = FloatPoint3D(m_lightingColor.red(), m_lightingColor.green(), m_lightingColor.blue());
    m_lightSource->initPaintingData(paintingData);

    // Top/Left corner
    data.offset = 0;
    setPixel(data, paintingData, 0, 0,
        -2.0f / 3.0f, -2 * data.centerPixelValue() + 2 * data.rightPixelValue() - data.downPixelValue() + data.downRightPixelValue(),
        -2.0f / 3.0f, -2 * data.centerPixelValue() - data.rightPixelValue() + 2 * data.downPixelValue() + data.downRightPixelValue());

    // Top/Right pixel
    data.offset = data.widthMultipliedByPixelSize - cPixelSize;
    setPixel(data, paintingData, data.widthDecreasedByOne, 0,
        -2.0f / 3.0f, -2 * data.leftPixelValue() + 2 * data.centerPixelValue() - data.downLeftPixelValue() + data.downPixelValue(),
        -2.0f / 3.0f, -data.leftPixelValue() - 2 * data.centerPixelValue() + data.downLeftPixelValue() + 2 * data.downPixelValue());

    // Bottom/Left pixel
    data.offset = data.heightDecreasedByOne * data.widthMultipliedByPixelSize;
    setPixel(data, paintingData, 0, data.heightDecreasedByOne,
        -2.0f / 3.0f, -data.upPixelValue() + data.upRightPixelValue() - 2 * data.centerPixelValue() + 2 * data.rightPixelValue(),
        -2.0f / 3.0f, -2 * data.upPixelValue() - data.upRightPixelValue() + 2 * data.centerPixelValue() + data.rightPixelValue());

    // Bottom/Right pixel
    data.offset = height * data.widthMultipliedByPixelSize - cPixelSize;
    setPixel(data, paintingData, data.widthDecreasedByOne, data.heightDecreasedByOne,
        -2.0f / 3.0f, -data.upLeftPixelValue() + data.upPixelValue() - 2 * data.leftPixelValue() + 2 * data.centerPixelValue(),
        -2.0f / 3.0f, -data.upLeftPixelValue() - 2 * data.upPixelValue() + data.leftPixelValue() + 2 * data.centerPixelValue());

    if (width >= 3) {
        // Top line
        data.offset = cPixelSize;
        for (int x = 1; x < data.widthDecreasedByOne; ++x, data.offset += cPixelSize) {
            setPixel(data, paintingData, x, 0,
                -1.0f / 3.0f, -2 * data.leftPixelValue() + 2 * data.rightPixelValue() - data.downLeftPixelValue() + data.downRightPixelValue(),
                -1.0f / 2.0f, -data.leftPixelValue() - 2 * data.centerPixelValue() - data.rightPixelValue() + data.downLeftPixelValue() + 2 * data.downPixelValue() + data.downRightPixelValue());
        }
        // Bottom line
        data.offset = data.heightDecreasedByOne * data.widthMultipliedByPixelSize + cPixelSize;
        for (int x = 1; x < data.widthDecreasedByOne; ++x, data.offset += cPixelSize) {
            setPixel(data, paintingData, x, data.heightDecreasedByOne,
                -1.0f / 3.0f, -data.upLeftPixelValue() + data.upRightPixelValue() - 2 * data.leftPixelValue() + 2 * data.rightPixelValue(),
                -1.0f / 2.0f, -data.upLeftPixelValue() - 2 * data.upPixelValue() - data.upRightPixelValue() + data.leftPixelValue() + 2 * data.centerPixelValue() + data.rightPixelValue());
        }
    }

    if (height >= 3) {
        // Left line
        data.offset = data.widthMultipliedByPixelSize;
        for (int y = 1; y < data.heightDecreasedByOne; ++y, data.offset += data.widthMultipliedByPixelSize) {
            setPixel(data, paintingData, 0, y,
                -1.0f / 2.0f, -data.upPixelValue() + data.upRightPixelValue() - 2 * data.centerPixelValue() + 2 * data.rightPixelValue() - data.downPixelValue() + data.downRightPixelValue(),
                -1.0f / 3.0f, -2 * data.upPixelValue() - data.upRightPixelValue() + 2 * data.downPixelValue() + data.downRightPixelValue());
        }
        // Right line
        data.offset = data.widthMultipliedByPixelSize + data.widthMultipliedByPixelSize - cPixelSize;
        for (int y = 1; y < data.heightDecreasedByOne; ++y, data.offset += data.widthMultipliedByPixelSize) {
            setPixel(data, paintingData, data.widthDecreasedByOne, y,
                -1.0f / 2.0f, -data.upLeftPixelValue() + data.upPixelValue() -2 * data.leftPixelValue() + 2 * data.centerPixelValue() - data.downLeftPixelValue() + data.downPixelValue(),
                -1.0f / 3.0f, -data.upLeftPixelValue() - 2 * data.upPixelValue() + data.downLeftPixelValue() + 2 * data.downPixelValue());
        }
    }

    if (width >= 3 && height >= 3) {
        // Interior pixels
        for (int y = 1; y < data.heightDecreasedByOne; ++y) {
            data.offset = y * data.widthMultipliedByPixelSize + cPixelSize;
            for (int x = 1; x < data.widthDecreasedByOne; ++x, data.offset += cPixelSize) {
                setPixel(data, paintingData, x, y,
                    -1.0f / 4.0f, -data.upLeftPixelValue() + data.upRightPixelValue() - 2 * data.leftPixelValue() + 2 * data.rightPixelValue() - data.downLeftPixelValue() + data.downRightPixelValue(),
                    -1.0f / 4.0f, -data.upLeftPixelValue() - 2 * data.upPixelValue() - data.upRightPixelValue() + data.downLeftPixelValue() + 2 * data.downPixelValue() + data.downRightPixelValue());
            }
        }
    }

    int totalSize = data.widthMultipliedByPixelSize * height;
    if (m_lightingType == DiffuseLighting) {
        for (int i = 3; i < totalSize; i += 4)
            data.pixels->set(i, cOpaqueAlpha);
    } else {
        for (int i = 0; i < totalSize; i += 4) {
            unsigned char a1 = data.pixels->get(i);
            unsigned char a2 = data.pixels->get(i + 1);
            unsigned char a3 = data.pixels->get(i + 2);
            // alpha set to set to max(a1, a2, a3)
            data.pixels->set(i + 3, a1 >= a2 ? (a1 >= a3 ? a1 : a3) : (a2 >= a3 ? a2 : a3));
        }
    }

    return true;
}

void FELighting::apply(Filter* filter)
{
    FilterEffect* in = inputEffect(0);
    in->apply(filter);
    if (!in->resultImage())
        return;

    if (!effectContext())
        return;

    setIsAlphaImage(false);

    IntRect effectDrawingRect = requestedRegionOfInputImageData(in->repaintRectInLocalCoordinates());
    RefPtr<ImageData> srcImageData(in->resultImage()->getUnmultipliedImageData(effectDrawingRect));
    CanvasPixelArray* srcPixelArray(srcImageData->data());

    // FIXME: support kernelUnitLengths other than (1,1). The issue here is that the W3
    // standard has no test case for them, and other browsers (like Firefox) has strange
    // output for various kernelUnitLengths, and I am not sure they are reliable.
    // Anyway, feConvolveMatrix should also use the implementation

    if (drawLighting(srcPixelArray, effectDrawingRect.width(), effectDrawingRect.height()))
        resultImage()->putUnmultipliedImageData(srcImageData.get(), IntRect(IntPoint(), resultImage()->size()), IntPoint());
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
