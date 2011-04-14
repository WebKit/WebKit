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

#include "LightSource.h"
#include "PointLightSource.h"
#include "SpotLightSource.h"

#if CPU(ARM_NEON) && COMPILER(GCC)
#include "FELightingNEON.h"
#include <wtf/Vector.h>
#endif

namespace WebCore {

FELighting::FELighting(Filter* filter, LightingType lightingType, const Color& lightingColor, float surfaceScale,
    float diffuseConstant, float specularConstant, float specularExponent,
    float kernelUnitLengthX, float kernelUnitLengthY, PassRefPtr<LightSource> lightSource)
    : FilterEffect(filter)
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
const static float cFactor1div2 = -1 / 2.f;
const static float cFactor1div3 = -1 / 3.f;
const static float cFactor1div4 = -1 / 4.f;
const static float cFactor2div3 = -2 / 3.f;

// << 1 is signed multiply by 2
inline void FELighting::LightingData::topLeft(int offset, IntPoint& normalVector)
{
    int center = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    int right = static_cast<int>(pixels->get(offset + cPixelSize + cAlphaChannelOffset));
    offset += widthMultipliedByPixelSize;
    int bottom = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    int bottomRight = static_cast<int>(pixels->get(offset + cPixelSize + cAlphaChannelOffset));
    normalVector.setX(-(center << 1) + (right << 1) - bottom + bottomRight);
    normalVector.setY(-(center << 1) - right + (bottom << 1) + bottomRight);
}

inline void FELighting::LightingData::topRow(int offset, IntPoint& normalVector)
{
    int left = static_cast<int>(pixels->get(offset - cPixelSize + cAlphaChannelOffset));
    int center = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    int right = static_cast<int>(pixels->get(offset + cPixelSize + cAlphaChannelOffset));
    offset += widthMultipliedByPixelSize;
    int bottomLeft = static_cast<int>(pixels->get(offset - cPixelSize + cAlphaChannelOffset));
    int bottom = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    int bottomRight = static_cast<int>(pixels->get(offset + cPixelSize + cAlphaChannelOffset));
    normalVector.setX(-(left << 1) + (right << 1) - bottomLeft + bottomRight);
    normalVector.setY(-left - (center << 1) - right + bottomLeft + (bottom << 1) + bottomRight);
}

inline void FELighting::LightingData::topRight(int offset, IntPoint& normalVector)
{
    int left = static_cast<int>(pixels->get(offset - cPixelSize + cAlphaChannelOffset));
    int center = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    offset += widthMultipliedByPixelSize;
    int bottomLeft = static_cast<int>(pixels->get(offset - cPixelSize + cAlphaChannelOffset));
    int bottom = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    normalVector.setX(-(left << 1) + (center << 1) - bottomLeft + bottom);
    normalVector.setY(-left - (center << 1) + bottomLeft + (bottom << 1));
}

inline void FELighting::LightingData::leftColumn(int offset, IntPoint& normalVector)
{
    int center = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    int right = static_cast<int>(pixels->get(offset + cPixelSize + cAlphaChannelOffset));
    offset -= widthMultipliedByPixelSize;
    int top = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    int topRight = static_cast<int>(pixels->get(offset + cPixelSize + cAlphaChannelOffset));
    offset += widthMultipliedByPixelSize << 1;
    int bottom = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    int bottomRight = static_cast<int>(pixels->get(offset + cPixelSize + cAlphaChannelOffset));
    normalVector.setX(-top + topRight - (center << 1) + (right << 1) - bottom + bottomRight);
    normalVector.setY(-(top << 1) - topRight + (bottom << 1) + bottomRight);
}

inline void FELighting::LightingData::interior(int offset, IntPoint& normalVector)
{
    int left = static_cast<int>(pixels->get(offset - cPixelSize + cAlphaChannelOffset));
    int right = static_cast<int>(pixels->get(offset + cPixelSize + cAlphaChannelOffset));
    offset -= widthMultipliedByPixelSize;
    int topLeft = static_cast<int>(pixels->get(offset - cPixelSize + cAlphaChannelOffset));
    int top = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    int topRight = static_cast<int>(pixels->get(offset + cPixelSize + cAlphaChannelOffset));
    offset += widthMultipliedByPixelSize << 1;
    int bottomLeft = static_cast<int>(pixels->get(offset - cPixelSize + cAlphaChannelOffset));
    int bottom = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    int bottomRight = static_cast<int>(pixels->get(offset + cPixelSize + cAlphaChannelOffset));
    normalVector.setX(-topLeft + topRight - (left << 1) + (right << 1) - bottomLeft + bottomRight);
    normalVector.setY(-topLeft - (top << 1) - topRight + bottomLeft + (bottom << 1) + bottomRight);
}

inline void FELighting::LightingData::rightColumn(int offset, IntPoint& normalVector)
{
    int left = static_cast<int>(pixels->get(offset - cPixelSize + cAlphaChannelOffset));
    int center = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    offset -= widthMultipliedByPixelSize;
    int topLeft = static_cast<int>(pixels->get(offset - cPixelSize + cAlphaChannelOffset));
    int top = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    offset += widthMultipliedByPixelSize << 1;
    int bottomLeft = static_cast<int>(pixels->get(offset - cPixelSize + cAlphaChannelOffset));
    int bottom = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    normalVector.setX(-topLeft + top - (left << 1) + (center << 1) - bottomLeft + bottom);
    normalVector.setY(-topLeft - (top << 1) + bottomLeft + (bottom << 1));
}

inline void FELighting::LightingData::bottomLeft(int offset, IntPoint& normalVector)
{
    int center = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    int right = static_cast<int>(pixels->get(offset + cPixelSize + cAlphaChannelOffset));
    offset -= widthMultipliedByPixelSize;
    int top = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    int topRight = static_cast<int>(pixels->get(offset + cPixelSize + cAlphaChannelOffset));
    normalVector.setX(-top + topRight - (center << 1) + (right << 1));
    normalVector.setY(-(top << 1) - topRight + (center << 1) + right);
}

inline void FELighting::LightingData::bottomRow(int offset, IntPoint& normalVector)
{
    int left = static_cast<int>(pixels->get(offset - cPixelSize + cAlphaChannelOffset));
    int center = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    int right = static_cast<int>(pixels->get(offset + cPixelSize + cAlphaChannelOffset));
    offset -= widthMultipliedByPixelSize;
    int topLeft = static_cast<int>(pixels->get(offset - cPixelSize + cAlphaChannelOffset));
    int top = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    int topRight = static_cast<int>(pixels->get(offset + cPixelSize + cAlphaChannelOffset));
    normalVector.setX(-topLeft + topRight - (left << 1) + (right << 1));
    normalVector.setY(-topLeft - (top << 1) - topRight + left + (center << 1) + right);
}

inline void FELighting::LightingData::bottomRight(int offset, IntPoint& normalVector)
{
    int left = static_cast<int>(pixels->get(offset - cPixelSize + cAlphaChannelOffset));
    int center = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    offset -= widthMultipliedByPixelSize;
    int topLeft = static_cast<int>(pixels->get(offset - cPixelSize + cAlphaChannelOffset));
    int top = static_cast<int>(pixels->get(offset + cAlphaChannelOffset));
    normalVector.setX(-topLeft + top - (left << 1) + (center << 1));
    normalVector.setY(-topLeft - (top << 1) + left + (center << 1));
}

inline void FELighting::inlineSetPixel(int offset, LightingData& data, LightSource::PaintingData& paintingData,
                                       int lightX, int lightY, float factorX, float factorY, IntPoint& normal2DVector)
{
    m_lightSource->updatePaintingData(paintingData, lightX, lightY, static_cast<float>(data.pixels->get(offset + cAlphaChannelOffset)) * data.surfaceScale);

    float lightStrength;
    if (!normal2DVector.x() && !normal2DVector.y()) {
        // Normal vector is (0, 0, 1). This is a quite frequent case.
        if (m_lightingType == FELighting::DiffuseLighting)
            lightStrength = m_diffuseConstant * paintingData.lightVector.z() / paintingData.lightVectorLength;
        else {
            FloatPoint3D halfwayVector = paintingData.lightVector;
            halfwayVector.setZ(halfwayVector.z() + paintingData.lightVectorLength);
            float halfwayVectorLength = halfwayVector.length();
            if (m_specularExponent == 1)
                lightStrength = m_specularConstant * halfwayVector.z() / halfwayVectorLength;
            else
                lightStrength = m_specularConstant * powf(halfwayVector.z() / halfwayVectorLength, m_specularExponent);
        }
    } else {
        FloatPoint3D normalVector;
        normalVector.setX(factorX * static_cast<float>(normal2DVector.x()) * data.surfaceScale);
        normalVector.setY(factorY * static_cast<float>(normal2DVector.y()) * data.surfaceScale);
        normalVector.setZ(1);
        float normalVectorLength = normalVector.length();

        if (m_lightingType == FELighting::DiffuseLighting)
            lightStrength = m_diffuseConstant * (normalVector * paintingData.lightVector) / (normalVectorLength * paintingData.lightVectorLength);
        else {
            FloatPoint3D halfwayVector = paintingData.lightVector;
            halfwayVector.setZ(halfwayVector.z() + paintingData.lightVectorLength);
            float halfwayVectorLength = halfwayVector.length();
            if (m_specularExponent == 1)
                lightStrength = m_specularConstant * (normalVector * halfwayVector) / (normalVectorLength * halfwayVectorLength);
            else
                lightStrength = m_specularConstant * powf((normalVector * halfwayVector) / (normalVectorLength * halfwayVectorLength), m_specularExponent);
        }
    }

    if (lightStrength > 1)
        lightStrength = 1;
    if (lightStrength < 0)
        lightStrength = 0;

    data.pixels->set(offset, static_cast<unsigned char>(lightStrength * paintingData.colorVector.x()));
    data.pixels->set(offset + 1, static_cast<unsigned char>(lightStrength * paintingData.colorVector.y()));
    data.pixels->set(offset + 2, static_cast<unsigned char>(lightStrength * paintingData.colorVector.z()));
}

void FELighting::setPixel(int offset, LightingData& data, LightSource::PaintingData& paintingData,
                          int lightX, int lightY, float factorX, float factorY, IntPoint& normalVector)
{
    inlineSetPixel(offset, data, paintingData, lightX, lightY, factorX, factorY, normalVector);
}

bool FELighting::drawLighting(ByteArray* pixels, int width, int height)
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
    IntPoint normalVector;
    int offset = 0;
    data.topLeft(offset, normalVector);
    setPixel(offset, data, paintingData, 0, 0, cFactor2div3, cFactor2div3, normalVector);

    // Top/Right pixel
    offset = data.widthMultipliedByPixelSize - cPixelSize;
    data.topRight(offset, normalVector);
    setPixel(offset, data, paintingData, data.widthDecreasedByOne, 0, cFactor2div3, cFactor2div3, normalVector);

    // Bottom/Left pixel
    offset = data.heightDecreasedByOne * data.widthMultipliedByPixelSize;
    data.bottomLeft(offset, normalVector);
    setPixel(offset, data, paintingData, 0, data.heightDecreasedByOne, cFactor2div3, cFactor2div3, normalVector);

    // Bottom/Right pixel
    offset = height * data.widthMultipliedByPixelSize - cPixelSize;
    data.bottomRight(offset, normalVector);
    setPixel(offset, data, paintingData, data.widthDecreasedByOne, data.heightDecreasedByOne, cFactor2div3, cFactor2div3, normalVector);

    if (width >= 3) {
        // Top row
        offset = cPixelSize;
        for (int x = 1; x < data.widthDecreasedByOne; ++x, offset += cPixelSize) {
            data.topRow(offset, normalVector);
            inlineSetPixel(offset, data, paintingData, x, 0, cFactor1div3, cFactor1div2, normalVector);
        }
        // Bottom row
        offset = data.heightDecreasedByOne * data.widthMultipliedByPixelSize + cPixelSize;
        for (int x = 1; x < data.widthDecreasedByOne; ++x, offset += cPixelSize) {
            data.bottomRow(offset, normalVector);
            inlineSetPixel(offset, data, paintingData, x, data.heightDecreasedByOne, cFactor1div3, cFactor1div2, normalVector);
        }
    }

    if (height >= 3) {
        // Left column
        offset = data.widthMultipliedByPixelSize;
        for (int y = 1; y < data.heightDecreasedByOne; ++y, offset += data.widthMultipliedByPixelSize) {
            data.leftColumn(offset, normalVector);
            inlineSetPixel(offset, data, paintingData, 0, y, cFactor1div2, cFactor1div3, normalVector);
        }
        // Right column
        offset = (data.widthMultipliedByPixelSize << 1) - cPixelSize;
        for (int y = 1; y < data.heightDecreasedByOne; ++y, offset += data.widthMultipliedByPixelSize) {
            data.rightColumn(offset, normalVector);
            inlineSetPixel(offset, data, paintingData, data.widthDecreasedByOne, y, cFactor1div2, cFactor1div3, normalVector);
        }
    }

    if (width >= 3 && height >= 3) {
        // Interior pixels
#if CPU(ARM_NEON) && COMPILER(GCC)
        drawInteriorPixels(data, paintingData);
#else
        for (int y = 1; y < data.heightDecreasedByOne; ++y) {
            offset = y * data.widthMultipliedByPixelSize + cPixelSize;
            for (int x = 1; x < data.widthDecreasedByOne; ++x, offset += cPixelSize) {
                data.interior(offset, normalVector);
                inlineSetPixel(offset, data, paintingData, x, y, cFactor1div4, cFactor1div4, normalVector);
            }
        }
#endif
    }

    int lastPixel = data.widthMultipliedByPixelSize * height;
    if (m_lightingType == DiffuseLighting) {
        for (int i = cAlphaChannelOffset; i < lastPixel; i += cPixelSize)
            data.pixels->set(i, cOpaqueAlpha);
    } else {
        for (int i = 0; i < lastPixel; i += cPixelSize) {
            unsigned char a1 = data.pixels->get(i);
            unsigned char a2 = data.pixels->get(i + 1);
            unsigned char a3 = data.pixels->get(i + 2);
            // alpha set to set to max(a1, a2, a3)
            data.pixels->set(i + 3, a1 >= a2 ? (a1 >= a3 ? a1 : a3) : (a2 >= a3 ? a2 : a3));
        }
    }

    return true;
}

void FELighting::apply()
{
    if (hasResult())
        return;
    FilterEffect* in = inputEffect(0);
    in->apply();
    if (!in->hasResult())
        return;

    ByteArray* srcPixelArray = createUnmultipliedImageResult();
    if (!srcPixelArray)
        return;

    setIsAlphaImage(false);

    IntRect effectDrawingRect = requestedRegionOfInputImageData(in->absolutePaintRect());
    in->copyUnmultipliedImage(srcPixelArray, effectDrawingRect);

    // FIXME: support kernelUnitLengths other than (1,1). The issue here is that the W3
    // standard has no test case for them, and other browsers (like Firefox) has strange
    // output for various kernelUnitLengths, and I am not sure they are reliable.
    // Anyway, feConvolveMatrix should also use the implementation

    IntSize absolutePaintSize = absolutePaintRect().size();
    drawLighting(srcPixelArray, absolutePaintSize.width(), absolutePaintSize.height());
}

#if CPU(ARM_NEON) && COMPILER(GCC)

static int getPowerCoefficients(float exponent)
{
    // Calling a powf function from the assembly code would require to save
    // and reload a lot of NEON registers. Since the base is in range [0..1]
    // and only 8 bit precision is required, we use our own powf function.
    // This is probably not the best, but it uses only a few registers and
    // gives us enough precision (modifying the exponent field directly would
    // also be possible).

    // First, we limit the exponent to maximum of 64, which gives us enough
    // precision. We split the exponent to an integer and fraction part,
    // since a^x = (a^y)*(a^z) where x = y+z. The integer exponent of the
    // power is estimated by square, and the fraction exponent of the power
    // is estimated by square root assembly instructions.
    int i, result;

    if (exponent < 0)
        exponent = 1 / (-exponent);

    if (exponent > 63.99)
        exponent = 63.99;

    exponent /= 64;
    result = 0;
    for (i = 11; i >= 0; --i) {
        exponent *= 2;
        if (exponent >= 1) {
            result |= 1 << i;
            exponent -= 1;
        }
    }
    return result;
}

void FELighting::drawInteriorPixels(LightingData& data, LightSource::PaintingData& paintingData)
{
    WTF_ALIGNED(FELightingFloatArgumentsForNeon, floatArguments, 16);

    FELightingPaintingDataForNeon neonData = {
        data.pixels->data(),
        data.widthDecreasedByOne - 1,
        data.heightDecreasedByOne - 1,
        0,
        0,
        0,
        &floatArguments,
        feLightingConstantsForNeon()
    };

    // Set light source arguments.
    floatArguments.constOne = 1;

    floatArguments.colorRed = m_lightingColor.red();
    floatArguments.colorGreen = m_lightingColor.green();
    floatArguments.colorBlue = m_lightingColor.blue();
    floatArguments.padding4 = 0;

    if (m_lightSource->type() == LS_POINT) {
        neonData.flags |= FLAG_POINT_LIGHT;
        PointLightSource* pointLightSource = static_cast<PointLightSource*>(m_lightSource.get());
        floatArguments.lightX = pointLightSource->position().x();
        floatArguments.lightY = pointLightSource->position().y();
        floatArguments.lightZ = pointLightSource->position().z();
        floatArguments.padding2 = 0;
    } else if (m_lightSource->type() == LS_SPOT) {
        neonData.flags |= FLAG_SPOT_LIGHT;
        SpotLightSource* spotLightSource = static_cast<SpotLightSource*>(m_lightSource.get());
        floatArguments.lightX = spotLightSource->position().x();
        floatArguments.lightY = spotLightSource->position().y();
        floatArguments.lightZ = spotLightSource->position().z();
        floatArguments.padding2 = 0;

        floatArguments.directionX = paintingData.directionVector.x();
        floatArguments.directionY = paintingData.directionVector.y();
        floatArguments.directionZ = paintingData.directionVector.z();
        floatArguments.padding3 = 0;

        floatArguments.coneCutOffLimit = paintingData.coneCutOffLimit;
        floatArguments.coneFullLight = paintingData.coneFullLight;
        floatArguments.coneCutOffRange = paintingData.coneCutOffLimit - paintingData.coneFullLight;
        neonData.coneExponent = getPowerCoefficients(spotLightSource->specularExponent());
        if (spotLightSource->specularExponent() == 1)
            neonData.flags |= FLAG_CONE_EXPONENT_IS_1;
    } else {
        ASSERT(m_lightSource.type == LS_DISTANT);
        floatArguments.lightX = paintingData.lightVector.x();
        floatArguments.lightY = paintingData.lightVector.y();
        floatArguments.lightZ = paintingData.lightVector.z();
        floatArguments.padding2 = 1;
    }

    // Set lighting arguments.
    floatArguments.surfaceScale = data.surfaceScale;
    floatArguments.minusSurfaceScaleDividedByFour = -data.surfaceScale / 4;
    if (m_lightingType == FELighting::DiffuseLighting)
        floatArguments.diffuseConstant = m_diffuseConstant;
    else {
        neonData.flags |= FLAG_SPECULAR_LIGHT;
        floatArguments.diffuseConstant = m_specularConstant;
        neonData.specularExponent = getPowerCoefficients(m_specularExponent);
        if (m_specularExponent == 1)
            neonData.flags |= FLAG_SPECULAR_EXPONENT_IS_1;
    }
    if (floatArguments.diffuseConstant == 1)
        neonData.flags |= FLAG_DIFFUSE_CONST_IS_1;

    neonDrawLighting(&neonData);
}
#endif // CPU(ARM_NEON) && COMPILER(GCC)

} // namespace WebCore

#endif // ENABLE(FILTERS)
