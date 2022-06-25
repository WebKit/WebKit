/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Zoltan Herczeg <zherczeg@webkit.org>
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
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
#include "FEConvolveMatrixSoftwareApplier.h"

#include "FEColorMatrix.h"
#include "FEConvolveMatrix.h"
#include "ImageBuffer.h"
#include "PixelBuffer.h"
#include <wtf/ParallelJobs.h>
#include <wtf/WorkQueue.h>

namespace WebCore {

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

inline uint8_t FEConvolveMatrixSoftwareApplier::clampRGBAValue(float channel, uint8_t max)
{
    if (channel <= 0)
        return 0;
    if (channel >= max)
        return max;
    return channel;
}

inline void FEConvolveMatrixSoftwareApplier::setDestinationPixels(const PixelBuffer& sourcePixelBuffer, PixelBuffer& destinationPixelBuffer, int& pixel, float* totals, float divisor, float bias, bool preserveAlphaValues)
{
    uint8_t maxAlpha = preserveAlphaValues ? 255 : clampRGBAValue(totals[3] / divisor + bias);
    for (int i = 0; i < 3; ++i)
        destinationPixelBuffer.set(pixel++, clampRGBAValue(totals[i] / divisor + bias, maxAlpha));

    if (preserveAlphaValues) {
        destinationPixelBuffer.set(pixel, sourcePixelBuffer.item(pixel));
        ++pixel;
    } else
        destinationPixelBuffer.set(pixel++, maxAlpha);
}

inline int FEConvolveMatrixSoftwareApplier::getPixelValue(const PaintingData& paintingData, int x, int y)
{
    if (x >= 0 && x < paintingData.width && y >= 0 && y < paintingData.height)
        return (y * paintingData.width + x) << 2;

    switch (paintingData.edgeMode) {
    default: // EdgeModeType::None
        return -1;
    case EdgeModeType::Duplicate:
        if (x < 0)
            x = 0;
        else if (x >= paintingData.width)
            x = paintingData.width - 1;
        if (y < 0)
            y = 0;
        else if (y >= paintingData.height)
            y = paintingData.height - 1;
        return (y * paintingData.width + x) << 2;
    case EdgeModeType::Wrap:
        while (x < 0)
            x += paintingData.width;
        x %= paintingData.width;
        while (y < 0)
            y += paintingData.height;
        y %= paintingData.height;
        return (y * paintingData.width + x) << 2;
    }
}

// Only for region C
inline void FEConvolveMatrixSoftwareApplier::setInteriorPixels(PaintingData& paintingData, int clipRight, int clipBottom, int yStart, int yEnd)
{
    // edge mode does not affect these pixels
    int pixel = (paintingData.targetOffset.y() * paintingData.width + paintingData.targetOffset.x()) * 4;
    int kernelIncrease = clipRight * 4;
    int xIncrease = (paintingData.kernelSize.width() - 1) * 4;
    // Contains the sum of rgb(a) components
    float totals[4];

    // divisor cannot be 0, SVGFEConvolveMatrixElement ensures this
    ASSERT(paintingData.divisor);

    // Skip the first '(clipBottom - yEnd)' lines
    pixel += (clipBottom - yEnd) * (xIncrease + (clipRight + 1) * 4);
    int startKernelPixel = (clipBottom - yEnd) * (xIncrease + (clipRight + 1) * 4);

    for (int y = yEnd + 1; y > yStart; --y) {
        for (int x = clipRight + 1; x > 0; --x) {
            int kernelValue = paintingData.kernelMatrix.size() - 1;
            int kernelPixel = startKernelPixel;
            int width = paintingData.kernelSize.width();

            totals[0] = 0;
            totals[1] = 0;
            totals[2] = 0;
            totals[3] = 0;

            while (kernelValue >= 0) {
                totals[0] += paintingData.kernelMatrix[kernelValue] * static_cast<float>(paintingData.sourcePixelBuffer.item(kernelPixel++));
                totals[1] += paintingData.kernelMatrix[kernelValue] * static_cast<float>(paintingData.sourcePixelBuffer.item(kernelPixel++));
                totals[2] += paintingData.kernelMatrix[kernelValue] * static_cast<float>(paintingData.sourcePixelBuffer.item(kernelPixel++));
                if (!paintingData.preserveAlpha)
                    totals[3] += paintingData.kernelMatrix[kernelValue] * static_cast<float>(paintingData.sourcePixelBuffer.item(kernelPixel));
                ++kernelPixel;
                --kernelValue;
                if (!--width) {
                    kernelPixel += kernelIncrease;
                    width = paintingData.kernelSize.width();
                }
            }

            setDestinationPixels(paintingData.sourcePixelBuffer, paintingData.destinationPixelBuffer, pixel, totals, paintingData.divisor, paintingData.bias, paintingData.preserveAlpha);
            startKernelPixel += 4;
        }
        pixel += xIncrease;
        startKernelPixel += xIncrease;
    }
}

// For other regions than C
inline void FEConvolveMatrixSoftwareApplier::setOuterPixels(PaintingData& paintingData, int x1, int y1, int x2, int y2)
{
    int pixel = (y1 * paintingData.width + x1) * 4;
    int height = y2 - y1;
    int width = x2 - x1;
    int beginKernelPixelX = x1 - paintingData.targetOffset.x();
    int startKernelPixelX = beginKernelPixelX;
    int startKernelPixelY = y1 - paintingData.targetOffset.y();
    int xIncrease = (paintingData.width - width) * 4;
    // Contains the sum of rgb(a) components
    float totals[4];

    // paintingData.divisor cannot be 0, SVGFEConvolveMatrixElement ensures this
    ASSERT(paintingData.divisor);

    for (int y = height; y > 0; --y) {
        for (int x = width; x > 0; --x) {
            int kernelValue = paintingData.kernelMatrix.size() - 1;
            int kernelPixelX = startKernelPixelX;
            int kernelPixelY = startKernelPixelY;
            int width = paintingData.kernelSize.width();

            totals[0] = 0;
            totals[1] = 0;
            totals[2] = 0;
            totals[3] = 0;

            while (kernelValue >= 0) {
                int pixelIndex = getPixelValue(paintingData, kernelPixelX, kernelPixelY);
                if (pixelIndex >= 0) {
                    totals[0] += paintingData.kernelMatrix[kernelValue] * static_cast<float>(paintingData.sourcePixelBuffer.item(pixelIndex));
                    totals[1] += paintingData.kernelMatrix[kernelValue] * static_cast<float>(paintingData.sourcePixelBuffer.item(pixelIndex + 1));
                    totals[2] += paintingData.kernelMatrix[kernelValue] * static_cast<float>(paintingData.sourcePixelBuffer.item(pixelIndex + 2));
                }
                if (!paintingData.preserveAlpha && pixelIndex >= 0)
                    totals[3] += paintingData.kernelMatrix[kernelValue] * static_cast<float>(paintingData.sourcePixelBuffer.item(pixelIndex + 3));
                ++kernelPixelX;
                --kernelValue;
                if (!--width) {
                    kernelPixelX = startKernelPixelX;
                    ++kernelPixelY;
                    width = paintingData.kernelSize.width();
                }
            }

            setDestinationPixels(paintingData.sourcePixelBuffer, paintingData.destinationPixelBuffer, pixel, totals, paintingData.divisor, paintingData.bias, paintingData.preserveAlpha);
            ++startKernelPixelX;
        }
        pixel += xIncrease;
        startKernelPixelX = beginKernelPixelX;
        ++startKernelPixelY;
    }
}

void FEConvolveMatrixSoftwareApplier::applyPlatform(PaintingData& paintingData) const
{
    // Drawing fully covered pixels
    int clipRight = paintingData.width - paintingData.kernelSize.width();
    int clipBottom = paintingData.height - paintingData.kernelSize.height();

    if (clipRight < 0 || clipBottom < 0) {
        // Rare situation, not optimized for speed
        setOuterPixels(paintingData, 0, 0, paintingData.width, paintingData.height);
        return;
    }

    static constexpr int minimalRectDimension = (100 * 100); // Empirical data limit for parallel jobs

    if (int iterations = (paintingData.width * paintingData.height) / minimalRectDimension) {
        int stride = clipBottom / iterations;
        int chunkCount = (clipBottom + stride - 1) / stride;

        ConcurrentWorkQueue::apply(chunkCount, [&](size_t index) {
            int yStart = (stride * index);
            int yEnd = std::min<int>(stride * (index + 1), clipBottom);

            setInteriorPixels(paintingData, clipRight, clipBottom, yStart, yEnd);
        });
    } else
        setInteriorPixels(paintingData, clipRight, clipBottom, 0, clipBottom);

    clipRight += paintingData.targetOffset.x() + 1;
    clipBottom += paintingData.targetOffset.y() + 1;

    if (paintingData.targetOffset.y() > 0)
        setOuterPixels(paintingData, 0, 0, paintingData.width, paintingData.targetOffset.y());
    if (clipBottom < paintingData.height)
        setOuterPixels(paintingData, 0, clipBottom, paintingData.width, paintingData.height);
    if (paintingData.targetOffset.x() > 0)
        setOuterPixels(paintingData, 0, paintingData.targetOffset.y(), paintingData.targetOffset.x(), clipBottom);
    if (clipRight < paintingData.width)
        setOuterPixels(paintingData, clipRight, paintingData.targetOffset.y(), paintingData.width, clipBottom);
}

bool FEConvolveMatrixSoftwareApplier::apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const
{
    auto& input = inputs[0].get();

    auto alphaFormat = m_effect.preserveAlpha() ? AlphaPremultiplication::Unpremultiplied : AlphaPremultiplication::Premultiplied;
    auto destinationPixelBuffer = result.pixelBuffer(alphaFormat);
    if (!destinationPixelBuffer)
        return false;

    auto effectDrawingRect = result.absoluteImageRectRelativeTo(input);
    auto sourcePixelBuffer = input.getPixelBuffer(alphaFormat, effectDrawingRect, m_effect.operatingColorSpace());
    if (!sourcePixelBuffer)
        return false;

    auto paintSize = result.absoluteImageRect().size();

    PaintingData paintingData = {
        *sourcePixelBuffer,
        *destinationPixelBuffer,
        paintSize.width(),
        paintSize.height(),
        m_effect.kernelSize(),
        m_effect.divisor(),
        m_effect.bias() * 255,
        m_effect.targetOffset(),
        m_effect.edgeMode(),
        m_effect.preserveAlpha(),
        FEColorMatrix::normalizedFloats(m_effect.kernel())
    };

    applyPlatform(paintingData);
    return true;
}

} // namespace WebCore
