/*
 * Copyright (C) 2010 University of Szeged
 * Copyright (C) 2010 Zoltan Herczeg
 * Copyright (C) 2018 Apple Inc.
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
#include "FELighting.h"

#include "ColorConversion.h"
#include "FELightingNEON.h"
#include "ImageData.h"
#include <wtf/ParallelJobs.h>

namespace WebCore {

FELighting::FELighting(Filter& filter, LightingType lightingType, const Color& lightingColor, float surfaceScale, float diffuseConstant, float specularConstant, float specularExponent, float kernelUnitLengthX, float kernelUnitLengthY, Ref<LightSource>&& lightSource, Type type)
    : FilterEffect(filter, type)
    , m_lightingType(lightingType)
    , m_lightSource(WTFMove(lightSource))
    , m_lightingColor(lightingColor)
    , m_surfaceScale(surfaceScale)
    , m_diffuseConstant(diffuseConstant)
    , m_specularConstant(specularConstant)
    , m_specularExponent(specularExponent)
    , m_kernelUnitLengthX(kernelUnitLengthX)
    , m_kernelUnitLengthY(kernelUnitLengthY)
{
}

bool FELighting::setSurfaceScale(float surfaceScale)
{
    if (m_surfaceScale == surfaceScale)
        return false;

    m_surfaceScale = surfaceScale;
    return true;
}

bool FELighting::setLightingColor(const Color& lightingColor)
{
    if (m_lightingColor == lightingColor)
        return false;

    m_lightingColor = lightingColor;
    return true;
}

bool FELighting::setKernelUnitLengthX(float kernelUnitLengthX)
{
    if (m_kernelUnitLengthX == kernelUnitLengthX)
        return false;

    m_kernelUnitLengthX = kernelUnitLengthX;
    return true;
}

bool FELighting::setKernelUnitLengthY(float kernelUnitLengthY)
{
    if (m_kernelUnitLengthY == kernelUnitLengthY)
        return false;

    m_kernelUnitLengthY = kernelUnitLengthY;
    return true;
}

static constexpr int cPixelSize = 4;
static constexpr int cAlphaChannelOffset = 3;
static constexpr uint8_t cOpaqueAlpha = static_cast<uint8_t>(0xFF);

// These factors and the normal coefficients come from the table under https://www.w3.org/TR/SVG/filters.html#feDiffuseLightingElement.
static constexpr float cFactor1div2 = -1 / 2.f;
static constexpr float cFactor1div3 = -1 / 3.f;
static constexpr float cFactor1div4 = -1 / 4.f;
static constexpr float cFactor2div3 = -2 / 3.f;

inline IntSize FELighting::LightingData::topLeftNormal(int offset) const
{
    int center = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int right = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    offset += widthMultipliedByPixelSize;
    int bottom = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int bottomRight = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    return {
        -2 * center + 2 * right - bottom + bottomRight,
        -2 * center - right + 2 * bottom + bottomRight
    };
}

inline IntSize FELighting::LightingData::topRowNormal(int offset) const
{
    int left = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int center = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int right = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    offset += widthMultipliedByPixelSize;
    int bottomLeft = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int bottom = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int bottomRight = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    return {
        -2 * left + 2 * right - bottomLeft + bottomRight,
        -left - 2 * center - right + bottomLeft + 2 * bottom + bottomRight
    };
}

inline IntSize FELighting::LightingData::topRightNormal(int offset) const
{
    int left = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int center = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    offset += widthMultipliedByPixelSize;
    int bottomLeft = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int bottom = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    return {
        -2 * left + 2 * center - bottomLeft + bottom,
        -left - 2 * center + bottomLeft + 2 * bottom
    };
}

inline IntSize FELighting::LightingData::leftColumnNormal(int offset) const
{
    int center = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int right = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    offset -= widthMultipliedByPixelSize;
    int top = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int topRight = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    offset += 2 * widthMultipliedByPixelSize;
    int bottom = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int bottomRight = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    return {
        -top + topRight - 2 * center + 2 * right - bottom + bottomRight,
        -2 * top - topRight + 2 * bottom + bottomRight
    };
}

inline IntSize FELighting::LightingData::interiorNormal(int offset, AlphaWindow& alphaWindow) const
{
    int rightAlphaOffset = offset + cPixelSize + cAlphaChannelOffset;
    
    int right = static_cast<int>(pixels->item(rightAlphaOffset));
    int topRight = static_cast<int>(pixels->item(rightAlphaOffset - widthMultipliedByPixelSize));
    int bottomRight = static_cast<int>(pixels->item(rightAlphaOffset + widthMultipliedByPixelSize));

    int left = alphaWindow.left();
    int topLeft = alphaWindow.topLeft();
    int top = alphaWindow.top();

    int bottomLeft = alphaWindow.bottomLeft();
    int bottom = alphaWindow.bottom();

    // The alphaWindow has been shifted, and here we fill in the right column.
    alphaWindow.alpha[0][2] = topRight;
    alphaWindow.alpha[1][2] = right;
    alphaWindow.alpha[2][2] = bottomRight;
    
    // Check that the alphaWindow is working with some spot-checks.
    ASSERT(alphaWindow.topLeft() == pixels->item(offset - cPixelSize - widthMultipliedByPixelSize + cAlphaChannelOffset)); // topLeft
    ASSERT(alphaWindow.top() == pixels->item(offset - widthMultipliedByPixelSize + cAlphaChannelOffset)); // top

    return {
        -topLeft + topRight - 2 * left + 2 * right - bottomLeft + bottomRight,
        -topLeft - 2 * top - topRight + bottomLeft + 2 * bottom + bottomRight
    };
}

inline IntSize FELighting::LightingData::rightColumnNormal(int offset) const
{
    int left = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int center = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    offset -= widthMultipliedByPixelSize;
    int topLeft = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int top = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    offset += 2 * widthMultipliedByPixelSize;
    int bottomLeft = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int bottom = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    return {
        -topLeft + top - 2 * left + 2 * center - bottomLeft + bottom,
        -topLeft - 2 * top + bottomLeft + 2 * bottom
    };
}

inline IntSize FELighting::LightingData::bottomLeftNormal(int offset) const
{
    int center = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int right = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    offset -= widthMultipliedByPixelSize;
    int top = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int topRight = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    return {
        -top + topRight - 2 * center + 2 * right,
        -2 * top - topRight + 2 * center + right
    };
}

inline IntSize FELighting::LightingData::bottomRowNormal(int offset) const
{
    int left = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int center = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int right = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    offset -= widthMultipliedByPixelSize;
    int topLeft = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int top = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    int topRight = static_cast<int>(pixels->item(offset + cPixelSize + cAlphaChannelOffset));
    return {
        -topLeft + topRight - 2 * left + 2 * right,
        -topLeft - 2 * top - topRight + left + 2 * center + right
    };
}

inline IntSize FELighting::LightingData::bottomRightNormal(int offset) const
{
    int left = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int center = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    offset -= widthMultipliedByPixelSize;
    int topLeft = static_cast<int>(pixels->item(offset - cPixelSize + cAlphaChannelOffset));
    int top = static_cast<int>(pixels->item(offset + cAlphaChannelOffset));
    return {
        -topLeft + top - 2 * left + 2 * center,
        -topLeft - 2 * top + left + 2 * center
    };
}

void FELighting::setPixel(int offset, const LightingData& data, const LightSource::PaintingData& paintingData, int x, int y, float factorX, float factorY, IntSize normal2DVector)
{
    setPixelInternal(offset, data, paintingData, x, y, factorX, factorY, normal2DVector, data.pixels->item(offset + cAlphaChannelOffset));
}

void FELighting::setPixelInternal(int offset, const LightingData& data, const LightSource::PaintingData& paintingData, int x, int y, float factorX, float factorY, IntSize normal2DVector, float alpha)
{
    float z = alpha * data.surfaceScale;
    LightSource::ComputedLightingData lightingData = m_lightSource->computePixelLightingData(paintingData, x, y, z);

    float lightStrength;
    if (normal2DVector.isZero()) {
        // Normal vector is (0, 0, 1). This is a quite frequent case.
        if (m_lightingType == FELighting::DiffuseLighting)
            lightStrength = m_diffuseConstant * lightingData.lightVector.z() / lightingData.lightVectorLength;
        else {
            FloatPoint3D halfwayVector = {
                lightingData.lightVector.x(),
                lightingData.lightVector.y(),
                lightingData.lightVector.z() + lightingData.lightVectorLength
            };
            float halfwayVectorLength = halfwayVector.length();
            if (m_specularExponent == 1)
                lightStrength = m_specularConstant * halfwayVector.z() / halfwayVectorLength;
            else
                lightStrength = m_specularConstant * powf(halfwayVector.z() / halfwayVectorLength, m_specularExponent);
        }
    } else {
        FloatPoint3D normalVector = {
            factorX * normal2DVector.width() * data.surfaceScale,
            factorY * normal2DVector.height() * data.surfaceScale,
            1.0f
        };
        float normalVectorLength = normalVector.length();

        if (m_lightingType == FELighting::DiffuseLighting)
            lightStrength = m_diffuseConstant * (normalVector * lightingData.lightVector) / (normalVectorLength * lightingData.lightVectorLength);
        else {
            FloatPoint3D halfwayVector = {
                lightingData.lightVector.x(),
                lightingData.lightVector.y(),
                lightingData.lightVector.z() + lightingData.lightVectorLength
            };
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

    uint8_t pixelValue[3] = {
        static_cast<uint8_t>(lightStrength * lightingData.colorVector.x() * 255.0f),
        static_cast<uint8_t>(lightStrength * lightingData.colorVector.y() * 255.0f),
        static_cast<uint8_t>(lightStrength * lightingData.colorVector.z() * 255.0f)
    };
    
    data.pixels->setRange(pixelValue, 3, offset);
}

// This appears to read from and write to the same pixel buffer, but it only reads the alpha channel, and writes the non-alpha channels.
void FELighting::platformApplyGenericPaint(const LightingData& data, const LightSource::PaintingData& paintingData, int startY, int endY)
{
    // Make sure startY is > 0 since we read from the previous row in the loop.
    ASSERT(startY);
    ASSERT(endY > startY);

    for (int y = startY; y < endY; ++y) {
        int rowStartOffset = y * data.widthMultipliedByPixelSize;
        int previousRowStart = rowStartOffset - data.widthMultipliedByPixelSize;
        int nextRowStart = rowStartOffset + data.widthMultipliedByPixelSize;

        // alphaWindow is a local cache of alpha values.
        // Fill the two right columns putting the left edge value in the center column.
        // For each pixel, we shift each row left then fill the right column.
        AlphaWindow alphaWindow;
        alphaWindow.setTop(data.pixels->item(previousRowStart + cAlphaChannelOffset));
        alphaWindow.setTopRight(data.pixels->item(previousRowStart + cPixelSize + cAlphaChannelOffset));

        alphaWindow.setCenter(data.pixels->item(rowStartOffset + cAlphaChannelOffset));
        alphaWindow.setRight(data.pixels->item(rowStartOffset + cPixelSize + cAlphaChannelOffset));

        alphaWindow.setBottom(data.pixels->item(nextRowStart + cAlphaChannelOffset));
        alphaWindow.setBottomRight(data.pixels->item(nextRowStart + cPixelSize + cAlphaChannelOffset));

        int offset = rowStartOffset + cPixelSize;
        for (int x = 1; x < data.widthDecreasedByOne; ++x, offset += cPixelSize) {
            alphaWindow.shift();
            setPixelInternal(offset, data, paintingData, x, y, cFactor1div4, cFactor1div4, data.interiorNormal(offset, alphaWindow), alphaWindow.center());
        }
    }
}

void FELighting::platformApplyGenericWorker(PlatformApplyGenericParameters* parameters)
{
    parameters->filter->platformApplyGenericPaint(parameters->data, parameters->paintingData, parameters->yStart, parameters->yEnd);
}

void FELighting::platformApplyGeneric(const LightingData& data, const LightSource::PaintingData& paintingData)
{
    unsigned rowsToProcess = data.heightDecreasedByOne - 1;
    unsigned maxNumThreads = rowsToProcess / 8;
    unsigned optimalThreadNumber = std::min<unsigned>(((data.widthDecreasedByOne - 1) * rowsToProcess) / s_minimalRectDimension, maxNumThreads);
    if (optimalThreadNumber > 1) {
        // Initialize parallel jobs
        WTF::ParallelJobs<PlatformApplyGenericParameters> parallelJobs(&platformApplyGenericWorker, optimalThreadNumber);

        // Fill the parameter array
        int job = parallelJobs.numberOfJobs();
        if (job > 1) {
            // Split the job into "yStep"-sized jobs but there a few jobs that need to be slightly larger since
            // yStep * jobs < total size. These extras are handled by the remainder "jobsWithExtra".
            const int yStep = rowsToProcess / job;
            const int jobsWithExtra = rowsToProcess % job;

            int yStart = 1;
            for (--job; job >= 0; --job) {
                PlatformApplyGenericParameters& params = parallelJobs.parameter(job);
                params.filter = this;
                params.data = data;
                params.paintingData = paintingData;
                params.yStart = yStart;
                yStart += job < jobsWithExtra ? yStep + 1 : yStep;
                params.yEnd = yStart;
            }
            parallelJobs.execute();
            return;
        }
        // Fallback to single threaded mode.
    }

    platformApplyGenericPaint(data, paintingData, 1, data.heightDecreasedByOne);
}

inline void FELighting::platformApply(const LightingData& data, const LightSource::PaintingData& paintingData)
{
    // The selection here eventually should happen dynamically on some platforms.
#if CPU(ARM_NEON) && CPU(ARM_TRADITIONAL) && COMPILER(GCC_COMPATIBLE)
    platformApplyNeon(data, paintingData);
#else
    platformApplyGeneric(data, paintingData);
#endif
}

bool FELighting::drawLighting(Uint8ClampedArray& pixels, int width, int height)
{
    LightSource::PaintingData paintingData;
    LightingData data;

    // FIXME: do something if width or height (or both) is 1 pixel.
    // The W3 spec does not define this case. Now the filter just returns.
    if (width <= 2 || height <= 2)
        return false;

    data.pixels = &pixels;
    data.surfaceScale = m_surfaceScale / 255.0f;
    data.widthMultipliedByPixelSize = width * cPixelSize;
    data.widthDecreasedByOne = width - 1;
    data.heightDecreasedByOne = height - 1;
    
    if (operatingColorSpace() == ColorSpace::LinearRGB) {
        auto [r, g, b, a] = toLinearSRGBA(m_lightingColor.toSRGBALossy<float>());
        paintingData.initialLightingData.colorVector = FloatPoint3D(r, g, b);
    } else {
        auto [r, g, b, a] = m_lightingColor.toSRGBALossy<float>();
        paintingData.initialLightingData.colorVector = FloatPoint3D(r, g, b);
    }
    m_lightSource->initPaintingData(*this, paintingData);

    // Top left.
    int offset = 0;
    setPixel(offset, data, paintingData, 0, 0, cFactor2div3, cFactor2div3, data.topLeftNormal(offset));

    // Top right.
    offset = data.widthMultipliedByPixelSize - cPixelSize;
    setPixel(offset, data, paintingData, data.widthDecreasedByOne, 0, cFactor2div3, cFactor2div3, data.topRightNormal(offset));

    // Bottom left.
    offset = data.heightDecreasedByOne * data.widthMultipliedByPixelSize;
    setPixel(offset, data, paintingData, 0, data.heightDecreasedByOne, cFactor2div3, cFactor2div3, data.bottomLeftNormal(offset));

    // Bottom right.
    offset = height * data.widthMultipliedByPixelSize - cPixelSize;
    setPixel(offset, data, paintingData, data.widthDecreasedByOne, data.heightDecreasedByOne, cFactor2div3, cFactor2div3, data.bottomRightNormal(offset));

    if (width >= 3) {
        // Top row.
        offset = cPixelSize;
        for (int x = 1; x < data.widthDecreasedByOne; ++x, offset += cPixelSize)
            setPixel(offset, data, paintingData, x, 0, cFactor1div3, cFactor1div2, data.topRowNormal(offset));

        // Bottom row.
        offset = data.heightDecreasedByOne * data.widthMultipliedByPixelSize + cPixelSize;
        for (int x = 1; x < data.widthDecreasedByOne; ++x, offset += cPixelSize)
            setPixel(offset, data, paintingData, x, data.heightDecreasedByOne, cFactor1div3, cFactor1div2, data.bottomRowNormal(offset));
    }

    if (height >= 3) {
        // Left column.
        offset = data.widthMultipliedByPixelSize;
        for (int y = 1; y < data.heightDecreasedByOne; ++y, offset += data.widthMultipliedByPixelSize)
            setPixel(offset, data, paintingData, 0, y, cFactor1div2, cFactor1div3, data.leftColumnNormal(offset));

        // Right column.
        offset = 2 * data.widthMultipliedByPixelSize - cPixelSize;
        for (int y = 1; y < data.heightDecreasedByOne; ++y, offset += data.widthMultipliedByPixelSize)
            setPixel(offset, data, paintingData, data.widthDecreasedByOne, y, cFactor1div2, cFactor1div3, data.rightColumnNormal(offset));
    }

    if (width >= 3 && height >= 3) {
        // Interior pixels.
        platformApply(data, paintingData);
    }

    int lastPixel = data.widthMultipliedByPixelSize * height;
    if (m_lightingType == DiffuseLighting) {
        for (int i = cAlphaChannelOffset; i < lastPixel; i += cPixelSize)
            data.pixels->set(i, cOpaqueAlpha);
    } else {
        for (int i = 0; i < lastPixel; i += cPixelSize) {
            uint8_t a1 = data.pixels->item(i);
            uint8_t a2 = data.pixels->item(i + 1);
            uint8_t a3 = data.pixels->item(i + 2);
            // alpha set to set to max(a1, a2, a3)
            data.pixels->set(i + 3, a1 >= a2 ? (a1 >= a3 ? a1 : a3) : (a2 >= a3 ? a2 : a3));
        }
    }

    return true;
}

void FELighting::platformApplySoftware()
{
    FilterEffect* in = inputEffect(0);

    auto* resultImage = createPremultipliedImageResult();
    auto* resutPixelArray = resultImage ? resultImage->data() : nullptr;
    if (!resutPixelArray)
        return;

    setIsAlphaImage(false);

    IntRect effectDrawingRect = requestedRegionOfInputImageData(in->absolutePaintRect());
    in->copyPremultipliedResult(*resutPixelArray, effectDrawingRect, operatingColorSpace());
    // FIXME: support kernelUnitLengths other than (1,1). The issue here is that the W3
    // standard has no test case for them, and other browsers (like Firefox) has strange
    // output for various kernelUnitLengths, and I am not sure they are reliable.
    // Anyway, feConvolveMatrix should also use the implementation

    IntSize absolutePaintSize = absolutePaintRect().size();
    drawLighting(*resutPixelArray, absolutePaintSize.width(), absolutePaintSize.height());
}

} // namespace WebCore
