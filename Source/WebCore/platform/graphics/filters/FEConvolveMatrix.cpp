/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Zoltan Herczeg <zherczeg@webkit.org>
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
#include "FEConvolveMatrix.h"

#include "Filter.h"
#include "PixelBuffer.h"
#include <wtf/ParallelJobs.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

FEConvolveMatrix::FEConvolveMatrix(Filter& filter, const IntSize& kernelSize,
    float divisor, float bias, const IntPoint& targetOffset, EdgeModeType edgeMode,
    const FloatPoint& kernelUnitLength, bool preserveAlpha, const Vector<float>& kernelMatrix)
    : FilterEffect(filter, Type::ConvolveMatrix)
    , m_kernelSize(kernelSize)
    , m_divisor(divisor)
    , m_bias(bias)
    , m_targetOffset(targetOffset)
    , m_edgeMode(edgeMode)
    , m_kernelUnitLength(kernelUnitLength)
    , m_preserveAlpha(preserveAlpha)
    , m_kernelMatrix(kernelMatrix)
{
    ASSERT(m_kernelSize.width() > 0);
    ASSERT(m_kernelSize.height() > 0);
}

Ref<FEConvolveMatrix> FEConvolveMatrix::create(Filter& filter, const IntSize& kernelSize,
    float divisor, float bias, const IntPoint& targetOffset, EdgeModeType edgeMode,
    const FloatPoint& kernelUnitLength, bool preserveAlpha, const Vector<float>& kernelMatrix)
{
    return adoptRef(*new FEConvolveMatrix(filter, kernelSize, divisor, bias, targetOffset, edgeMode, kernelUnitLength,
        preserveAlpha, kernelMatrix));
}

void FEConvolveMatrix::setKernelSize(const IntSize& kernelSize)
{
    ASSERT(kernelSize.width() > 0);
    ASSERT(kernelSize.height() > 0);
    m_kernelSize = kernelSize;
}

void FEConvolveMatrix::setKernel(const Vector<float>& kernel)
{
    m_kernelMatrix = kernel; 
}

bool FEConvolveMatrix::setDivisor(float divisor)
{
    ASSERT(divisor);
    if (m_divisor == divisor)
        return false;
    m_divisor = divisor;
    return true;
}

bool FEConvolveMatrix::setBias(float bias)
{
    if (m_bias == bias)
        return false;
    m_bias = bias;
    return true;
}

bool FEConvolveMatrix::setTargetOffset(const IntPoint& targetOffset)
{
    if (m_targetOffset == targetOffset)
        return false;
    m_targetOffset = targetOffset;
    return true;
}

bool FEConvolveMatrix::setEdgeMode(EdgeModeType edgeMode)
{
    if (m_edgeMode == edgeMode)
        return false;
    m_edgeMode = edgeMode;
    return true;
}

bool FEConvolveMatrix::setKernelUnitLength(const FloatPoint& kernelUnitLength)
{
    ASSERT(kernelUnitLength.x() > 0);
    ASSERT(kernelUnitLength.y() > 0);
    if (m_kernelUnitLength == kernelUnitLength)
        return false;
    m_kernelUnitLength = kernelUnitLength;
    return true;
}

bool FEConvolveMatrix::setPreserveAlpha(bool preserveAlpha)
{
    if (m_preserveAlpha == preserveAlpha)
        return false;
    m_preserveAlpha = preserveAlpha;
    return true;
}

/*
   -----------------------------------
      ConvolveMatrix implementation
   -----------------------------------

   The image rectangle is split in the following way:

      +---------------------+
      |          A          |
      +---------------------+
      |   |             |   |
      | B |      C      | D |
      |   |             |   |
      +---------------------+
      |          E          |
      +---------------------+

   Where region C contains those pixels, whose values
   can be calculated without crossing the edge of the rectangle.

   Example:
      Image size: width: 10, height: 10

      Order (kernel matrix size): width: 3, height 4
      Target: x:1, y:3

      The following figure shows the target inside the kernel matrix:

        ...
        ...
        ...
        .X.

   The regions in this case are the following:
      Note: (x1, y1) top-left and (x2, y2) is the bottom-right corner
      Note: row x2 and column y2 is not part of the region
            only those (x, y) pixels, where x1 <= x < x2 and y1 <= y < y2

      Region A: x1: 0, y1: 0, x2: 10, y2: 3
      Region B: x1: 0, y1: 3, x2: 1, y2: 10
      Region C: x1: 1, y1: 3, x2: 9, y2: 10
      Region D: x1: 9, y1: 3, x2: 10, y2: 10
      Region E: x1: 0, y1: 10, x2: 10, y2: 10 (empty region)

   Since region C (often) contains most of the pixels, we implemented
   a fast algoritm to calculate these values, called fastSetInteriorPixels.
   For other regions, fastSetOuterPixels is used, which calls getPixelValue,
   to handle pixels outside of the image. In a rare situations, when
   kernel matrix is bigger than the image, all pixels are calculated by this
   function.

   Although these two functions have lot in common, I decided not to make
   common a template for them, since there are key differences as well,
   and would make it really hard to understand.
*/

static ALWAYS_INLINE unsigned char clampRGBAValue(float channel, unsigned char max = 255)
{
    if (channel <= 0)
        return 0;
    if (channel >= max)
        return max;
    return channel;
}

template<bool preserveAlphaValues>
ALWAYS_INLINE void setDestinationPixels(const Uint8ClampedArray& sourcePixels, Uint8ClampedArray& destPixels, int& pixel, float* totals, float divisor, float bias)
{
    unsigned char maxAlpha = preserveAlphaValues ? 255 : clampRGBAValue(totals[3] / divisor + bias);
    for (int i = 0; i < 3; ++i)
        destPixels.set(pixel++, clampRGBAValue(totals[i] / divisor + bias, maxAlpha));

    if (preserveAlphaValues) {
        destPixels.set(pixel, sourcePixels.item(pixel));
        ++pixel;
    } else
        destPixels.set(pixel++, maxAlpha);
}

#if COMPILER(MSVC)
// Incorrectly diagnosing overwrite of stack in |totals| due to |preserveAlphaValues|.
#pragma warning(push)
#pragma warning(disable: 4789)
#endif

// Only for region C
template<bool preserveAlphaValues>
ALWAYS_INLINE void FEConvolveMatrix::fastSetInteriorPixels(PaintingData& paintingData, int clipRight, int clipBottom, int yStart, int yEnd)
{
    // edge mode does not affect these pixels
    int pixel = (m_targetOffset.y() * paintingData.width + m_targetOffset.x()) * 4;
    int kernelIncrease = clipRight * 4;
    int xIncrease = (m_kernelSize.width() - 1) * 4;
    // Contains the sum of rgb(a) components
    float totals[3 + (preserveAlphaValues ? 0 : 1)];

    // m_divisor cannot be 0, SVGFEConvolveMatrixElement ensures this
    ASSERT(m_divisor);

    // Skip the first '(clipBottom - yEnd)' lines
    pixel += (clipBottom - yEnd) * (xIncrease + (clipRight + 1) * 4);
    int startKernelPixel = (clipBottom - yEnd) * (xIncrease + (clipRight + 1) * 4);

    for (int y = yEnd + 1; y > yStart; --y) {
        for (int x = clipRight + 1; x > 0; --x) {
            int kernelValue = paintingData.kernelMatrix.size() - 1;
            int kernelPixel = startKernelPixel;
            int width = m_kernelSize.width();

            totals[0] = 0;
            totals[1] = 0;
            totals[2] = 0;
            if (!preserveAlphaValues)
                totals[3] = 0;

            while (kernelValue >= 0) {
                totals[0] += paintingData.kernelMatrix[kernelValue] * static_cast<float>(paintingData.srcPixelArray.item(kernelPixel++));
                totals[1] += paintingData.kernelMatrix[kernelValue] * static_cast<float>(paintingData.srcPixelArray.item(kernelPixel++));
                totals[2] += paintingData.kernelMatrix[kernelValue] * static_cast<float>(paintingData.srcPixelArray.item(kernelPixel++));
                if (!preserveAlphaValues)
                    totals[3] += paintingData.kernelMatrix[kernelValue] * static_cast<float>(paintingData.srcPixelArray.item(kernelPixel));
                ++kernelPixel;
                --kernelValue;
                if (!--width) {
                    kernelPixel += kernelIncrease;
                    width = m_kernelSize.width();
                }
            }

            setDestinationPixels<preserveAlphaValues>(paintingData.srcPixelArray, paintingData.dstPixelArray, pixel, totals, m_divisor, paintingData.bias);
            startKernelPixel += 4;
        }
        pixel += xIncrease;
        startKernelPixel += xIncrease;
    }
}

ALWAYS_INLINE int FEConvolveMatrix::getPixelValue(PaintingData& paintingData, int x, int y)
{
    if (x >= 0 && x < paintingData.width && y >= 0 && y < paintingData.height)
        return (y * paintingData.width + x) << 2;

    switch (m_edgeMode) {
    default: // EDGEMODE_NONE
        return -1;
    case EDGEMODE_DUPLICATE:
        if (x < 0)
            x = 0;
        else if (x >= paintingData.width)
            x = paintingData.width - 1;
        if (y < 0)
            y = 0;
        else if (y >= paintingData.height)
            y = paintingData.height - 1;
        return (y * paintingData.width + x) << 2;
    case EDGEMODE_WRAP:
        while (x < 0)
            x += paintingData.width;
        x %= paintingData.width;
        while (y < 0)
            y += paintingData.height;
        y %= paintingData.height;
        return (y * paintingData.width + x) << 2;
    }
}

// For other regions than C
template<bool preserveAlphaValues>
void FEConvolveMatrix::fastSetOuterPixels(PaintingData& paintingData, int x1, int y1, int x2, int y2)
{
    int pixel = (y1 * paintingData.width + x1) * 4;
    int height = y2 - y1;
    int width = x2 - x1;
    int beginKernelPixelX = x1 - m_targetOffset.x();
    int startKernelPixelX = beginKernelPixelX;
    int startKernelPixelY = y1 - m_targetOffset.y();
    int xIncrease = (paintingData.width - width) * 4;
    // Contains the sum of rgb(a) components
    float totals[3 + (preserveAlphaValues ? 0 : 1)];

    // m_divisor cannot be 0, SVGFEConvolveMatrixElement ensures this
    ASSERT(m_divisor);

    for (int y = height; y > 0; --y) {
        for (int x = width; x > 0; --x) {
            int kernelValue = paintingData.kernelMatrix.size() - 1;
            int kernelPixelX = startKernelPixelX;
            int kernelPixelY = startKernelPixelY;
            int width = m_kernelSize.width();

            totals[0] = 0;
            totals[1] = 0;
            totals[2] = 0;
            if (!preserveAlphaValues)
                totals[3] = 0;

            while (kernelValue >= 0) {
                int pixelIndex = getPixelValue(paintingData, kernelPixelX, kernelPixelY);
                if (pixelIndex >= 0) {
                    totals[0] += paintingData.kernelMatrix[kernelValue] * static_cast<float>(paintingData.srcPixelArray.item(pixelIndex));
                    totals[1] += paintingData.kernelMatrix[kernelValue] * static_cast<float>(paintingData.srcPixelArray.item(pixelIndex + 1));
                    totals[2] += paintingData.kernelMatrix[kernelValue] * static_cast<float>(paintingData.srcPixelArray.item(pixelIndex + 2));
                }
                if (!preserveAlphaValues && pixelIndex >= 0)
                    totals[3] += paintingData.kernelMatrix[kernelValue] * static_cast<float>(paintingData.srcPixelArray.item(pixelIndex + 3));
                ++kernelPixelX;
                --kernelValue;
                if (!--width) {
                    kernelPixelX = startKernelPixelX;
                    ++kernelPixelY;
                    width = m_kernelSize.width();
                }
            }

            setDestinationPixels<preserveAlphaValues>(paintingData.srcPixelArray, paintingData.dstPixelArray, pixel, totals, m_divisor, paintingData.bias);
            ++startKernelPixelX;
        }
        pixel += xIncrease;
        startKernelPixelX = beginKernelPixelX;
        ++startKernelPixelY;
    }
}

#if COMPILER(MSVC)
#pragma warning(pop) // Disable of 4789
#endif

ALWAYS_INLINE void FEConvolveMatrix::setInteriorPixels(PaintingData& paintingData, int clipRight, int clipBottom, int yStart, int yEnd)
{
    // Must be implemented here, since it refers another ALWAYS_INLINE
    // function, which defined in this C++ source file as well
    if (m_preserveAlpha)
        fastSetInteriorPixels<true>(paintingData, clipRight, clipBottom, yStart, yEnd);
    else
        fastSetInteriorPixels<false>(paintingData, clipRight, clipBottom, yStart, yEnd);
}

ALWAYS_INLINE void FEConvolveMatrix::setOuterPixels(PaintingData& paintingData, int x1, int y1, int x2, int y2)
{
    // Although this function can be moved to the header, it is implemented here
    // because setInteriorPixels is also implemented here
    if (m_preserveAlpha)
        fastSetOuterPixels<true>(paintingData, x1, y1, x2, y2);
    else
        fastSetOuterPixels<false>(paintingData, x1, y1, x2, y2);
}

void FEConvolveMatrix::platformApplySoftware()
{
    FilterEffect* in = inputEffect(0);

    auto& destinationPixelBuffer = m_preserveAlpha ? createUnmultipliedImageResult() : createPremultipliedImageResult();
    if (!destinationPixelBuffer)
        return;

    auto& destinationPixelArray = destinationPixelBuffer->data();

    IntRect effectDrawingRect = requestedRegionOfInputPixelBuffer(in->absolutePaintRect());

    RefPtr<Uint8ClampedArray> sourcePixelArray;
    if (m_preserveAlpha)
        sourcePixelArray = in->unmultipliedResult(effectDrawingRect, operatingColorSpace());
    else
        sourcePixelArray = in->premultipliedResult(effectDrawingRect, operatingColorSpace());
    if (!sourcePixelArray)
        return;

    IntSize paintSize = absolutePaintRect().size();
    paintSize.scale(filter().filterScale());

    PaintingData paintingData = {
        *sourcePixelArray,
        destinationPixelArray,
        paintSize.width(),
        paintSize.height(),
        m_bias * 255,
        normalizedFloats(m_kernelMatrix)
    };

    // Drawing fully covered pixels
    int clipRight = paintSize.width() - m_kernelSize.width();
    int clipBottom = paintSize.height() - m_kernelSize.height();

    if (clipRight < 0 || clipBottom < 0) {
        // Rare situation, not optimizied for speed
        setOuterPixels(paintingData, 0, 0, paintSize.width(), paintSize.height());
        return;
    }

    if (int iterations = (absolutePaintRect().width() * absolutePaintRect().height()) / s_minimalRectDimension) {
        int stride = clipBottom / iterations;
        int chunkCount = (clipBottom + stride - 1) / stride;

        WorkQueue::concurrentApply(chunkCount, [&](size_t index) {
            int yStart = (stride * index);
            int yEnd = std::min<int>(stride * (index + 1), clipBottom);

            setInteriorPixels(paintingData, clipRight, clipBottom, yStart, yEnd);
        });
    } else
        setInteriorPixels(paintingData, clipRight, clipBottom, 0, clipBottom);

    clipRight += m_targetOffset.x() + 1;
    clipBottom += m_targetOffset.y() + 1;
    if (m_targetOffset.y() > 0)
        setOuterPixels(paintingData, 0, 0, paintSize.width(), m_targetOffset.y());
    if (clipBottom < paintSize.height())
        setOuterPixels(paintingData, 0, clipBottom, paintSize.width(), paintSize.height());
    if (m_targetOffset.x() > 0)
        setOuterPixels(paintingData, 0, m_targetOffset.y(), m_targetOffset.x(), clipBottom);
    if (clipRight < paintSize.width())
        setOuterPixels(paintingData, clipRight, m_targetOffset.y(), paintSize.width(), clipBottom);
}

static TextStream& operator<<(TextStream& ts, const EdgeModeType& type)
{
    switch (type) {
    case EDGEMODE_UNKNOWN:
        ts << "UNKNOWN";
        break;
    case EDGEMODE_DUPLICATE:
        ts << "DUPLICATE";
        break;
    case EDGEMODE_WRAP:
        ts << "WRAP";
        break;
    case EDGEMODE_NONE:
        ts << "NONE";
        break;
    }
    return ts;
}

TextStream& FEConvolveMatrix::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    ts << indent << "[feConvolveMatrix";
    FilterEffect::externalRepresentation(ts, representation);
    ts << " order=\"" << m_kernelSize << "\" "
       << "kernelMatrix=\"" << m_kernelMatrix  << "\" "
       << "divisor=\"" << m_divisor << "\" "
       << "bias=\"" << m_bias << "\" "
       << "target=\"" << m_targetOffset << "\" "
       << "edgeMode=\"" << m_edgeMode << "\" "
       << "kernelUnitLength=\"" << m_kernelUnitLength << "\" "
       << "preserveAlpha=\"" << m_preserveAlpha << "\"]\n";

    TextStream::IndentScope indentScope(ts);
    inputEffect(0)->externalRepresentation(ts, representation);
    return ts;
}

}; // namespace WebCore
