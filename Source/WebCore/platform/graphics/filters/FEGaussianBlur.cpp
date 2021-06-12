/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Igalia, S.L.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2015-2016 Apple, Inc. All rights reserved.
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
#include "FEGaussianBlur.h"

#include "FEGaussianBlurNEON.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "PixelBuffer.h"
#include <wtf/text/TextStream.h>

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/Uint8ClampedArray.h>
#include <wtf/MathExtras.h>
#include <wtf/ParallelJobs.h>

static inline float gaussianKernelFactor()
{
    return 3 / 4.f * sqrtf(2 * piFloat);
}

static const int gMaxKernelSize = 500;

namespace WebCore {

inline void kernelPosition(int blurIteration, unsigned& radius, int& deltaLeft, int& deltaRight)
{
    // Check http://www.w3.org/TR/SVG/filters.html#feGaussianBlurElement for details.
    switch (blurIteration) {
    case 0:
        if (!(radius % 2)) {
            deltaLeft = radius / 2 - 1;
            deltaRight = radius - deltaLeft;
        } else {
            deltaLeft = radius / 2;
            deltaRight = radius - deltaLeft;
        }
        break;
    case 1:
        if (!(radius % 2)) {
            deltaLeft++;
            deltaRight--;
        }
        break;
    case 2:
        if (!(radius % 2)) {
            deltaRight++;
            radius++;
        }
        break;
    }
}

FEGaussianBlur::FEGaussianBlur(Filter& filter, float x, float y, EdgeModeType edgeMode)
    : FilterEffect(filter, Type::GaussianBlur)
    , m_stdX(x)
    , m_stdY(y)
    , m_edgeMode(edgeMode)
{
}

Ref<FEGaussianBlur> FEGaussianBlur::create(Filter& filter, float x, float y, EdgeModeType edgeMode)
{
    return adoptRef(*new FEGaussianBlur(filter, x, y, edgeMode));
}

void FEGaussianBlur::setStdDeviationX(float x)
{
    m_stdX = x;
}

void FEGaussianBlur::setStdDeviationY(float y)
{
    m_stdY = y;
}

void FEGaussianBlur::setEdgeMode(EdgeModeType edgeMode)
{
    m_edgeMode = edgeMode;
}

// This function only operates on Alpha channel.
inline void boxBlurAlphaOnly(const Uint8ClampedArray& srcPixelArray, Uint8ClampedArray& dstPixelArray,
    unsigned dx, int& dxLeft, int& dxRight, int& stride, int& strideLine, int& effectWidth, int& effectHeight, const int& maxKernelSize)
{
    const uint8_t* srcData = srcPixelArray.data();
    uint8_t* dstData = dstPixelArray.data();
    // Memory alignment is: RGBA, zero-index based.
    const int channel = 3;

    for (int y = 0; y < effectHeight; ++y) {
        int line = y * strideLine;
        int sum = 0;

        // Fill the kernel.
        for (int i = 0; i < maxKernelSize; ++i) {
            unsigned offset = line + i * stride;
            const uint8_t* srcPtr = srcData + offset;
            sum += srcPtr[channel];
        }

        // Blurring.
        for (int x = 0; x < effectWidth; ++x) {
            unsigned pixelByteOffset = line + x * stride + channel;
            uint8_t* dstPtr = dstData + pixelByteOffset;
            *dstPtr = static_cast<uint8_t>(sum / dx);

            // Shift kernel.
            if (x >= dxLeft) {
                unsigned leftOffset = pixelByteOffset - dxLeft * stride;
                const uint8_t* srcPtr = srcData + leftOffset;
                sum -= *srcPtr;
            }

            if (x + dxRight < effectWidth) {
                unsigned rightOffset = pixelByteOffset + dxRight * stride;
                const uint8_t* srcPtr = srcData + rightOffset;
                sum += *srcPtr;
            }
        }
    }
}

inline void boxBlur(const Uint8ClampedArray& srcPixelArray, Uint8ClampedArray& dstPixelArray,
    unsigned dx, int dxLeft, int dxRight, int stride, int strideLine, int effectWidth, int effectHeight, bool alphaImage, EdgeModeType edgeMode)
{
    const int maxKernelSize = std::min(dxRight, effectWidth);
    if (alphaImage)
        return boxBlurAlphaOnly(srcPixelArray, dstPixelArray, dx, dxLeft, dxRight, stride, strideLine,  effectWidth, effectHeight, maxKernelSize);

    const uint8_t* srcData = srcPixelArray.data();
    uint8_t* dstData = dstPixelArray.data();

    // Concerning the array width/length: it is Element size + Margin + Border. The number of pixels will be
    // P = width * height * channels.
    for (int y = 0; y < effectHeight; ++y) {
        int line = y * strideLine;
        int sumR = 0, sumG = 0, sumB = 0, sumA = 0;

        if (edgeMode == EDGEMODE_NONE) {
            // Fill the kernel.
            for (int i = 0; i < maxKernelSize; ++i) {
                unsigned offset = line + i * stride;
                const uint8_t* srcPtr = srcData + offset;
                sumR += *srcPtr++;
                sumG += *srcPtr++;
                sumB += *srcPtr++;
                sumA += *srcPtr;
            }

            // Blurring.
            for (int x = 0; x < effectWidth; ++x) {
                unsigned pixelByteOffset = line + x * stride;
                uint8_t* dstPtr = dstData + pixelByteOffset;

                *dstPtr++ = static_cast<uint8_t>(sumR / dx);
                *dstPtr++ = static_cast<uint8_t>(sumG / dx);
                *dstPtr++ = static_cast<uint8_t>(sumB / dx);
                *dstPtr = static_cast<uint8_t>(sumA / dx);

                // Shift kernel.
                if (x >= dxLeft) {
                    unsigned leftOffset = pixelByteOffset - dxLeft * stride;
                    const uint8_t* srcPtr = srcData + leftOffset;
                    sumR -= srcPtr[0];
                    sumG -= srcPtr[1];
                    sumB -= srcPtr[2];
                    sumA -= srcPtr[3];
                }

                if (x + dxRight < effectWidth) {
                    unsigned rightOffset = pixelByteOffset + dxRight * stride;
                    const uint8_t* srcPtr = srcData + rightOffset;
                    sumR += srcPtr[0];
                    sumG += srcPtr[1];
                    sumB += srcPtr[2];
                    sumA += srcPtr[3];
                }
            }

        } else {
            // FIXME: Add support for 'wrap' here.
            // Get edge values for edgeMode 'duplicate'.
            const uint8_t* edgeValueLeft = srcData + line;
            const uint8_t* edgeValueRight  = srcData + (line + (effectWidth - 1) * stride);

            // Fill the kernel.
            for (int i = dxLeft * -1; i < dxRight; ++i) {
                // Is this right for negative values of 'i'?
                unsigned offset = line + i * stride;
                const uint8_t* srcPtr = srcData + offset;

                if (i < 0) {
                    sumR += edgeValueLeft[0];
                    sumG += edgeValueLeft[1];
                    sumB += edgeValueLeft[2];
                    sumA += edgeValueLeft[3];
                } else if (i >= effectWidth) {
                    sumR += edgeValueRight[0];
                    sumG += edgeValueRight[1];
                    sumB += edgeValueRight[2];
                    sumA += edgeValueRight[3];
                } else {
                    sumR += *srcPtr++;
                    sumG += *srcPtr++;
                    sumB += *srcPtr++;
                    sumA += *srcPtr;
                }
            }

            // Blurring.
            for (int x = 0; x < effectWidth; ++x) {
                unsigned pixelByteOffset = line + x * stride;
                uint8_t* dstPtr = dstData + pixelByteOffset;

                *dstPtr++ = static_cast<uint8_t>(sumR / dx);
                *dstPtr++ = static_cast<uint8_t>(sumG / dx);
                *dstPtr++ = static_cast<uint8_t>(sumB / dx);
                *dstPtr = static_cast<uint8_t>(sumA / dx);

                // Shift kernel.
                if (x < dxLeft) {
                    sumR -= edgeValueLeft[0];
                    sumG -= edgeValueLeft[1];
                    sumB -= edgeValueLeft[2];
                    sumA -= edgeValueLeft[3];
                } else {
                    unsigned leftOffset = pixelByteOffset - dxLeft * stride;
                    const uint8_t* srcPtr = srcData + leftOffset;
                    sumR -= srcPtr[0];
                    sumG -= srcPtr[1];
                    sumB -= srcPtr[2];
                    sumA -= srcPtr[3];
                }

                if (x + dxRight >= effectWidth) {
                    sumR += edgeValueRight[0];
                    sumG += edgeValueRight[1];
                    sumB += edgeValueRight[2];
                    sumA += edgeValueRight[3];
                } else {
                    unsigned rightOffset = pixelByteOffset + dxRight * stride;
                    const uint8_t* srcPtr = srcData + rightOffset;
                    sumR += srcPtr[0];
                    sumG += srcPtr[1];
                    sumB += srcPtr[2];
                    sumA += srcPtr[3];
                }
            }
        }
    }
}

#if USE(ACCELERATE)
inline void accelerateBoxBlur(Uint8ClampedArray& ioBuffer, Uint8ClampedArray& tempBuffer, unsigned kernelSize, int stride, int effectWidth, int effectHeight)
{
    if (!ioBuffer.data() || !tempBuffer.data()) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (effectWidth <= 0 || effectHeight <= 0 || stride <= 0) {
        ASSERT_NOT_REACHED();
        return;
    }

    // We must always use an odd radius.
    if (kernelSize % 2 != 1)
        kernelSize += 1;

    vImage_Buffer effectInBuffer;
    effectInBuffer.data = static_cast<void*>(ioBuffer.data());
    effectInBuffer.width = effectWidth;
    effectInBuffer.height = effectHeight;
    effectInBuffer.rowBytes = stride;

    vImage_Buffer effectOutBuffer;
    effectOutBuffer.data = tempBuffer.data();
    effectOutBuffer.width = effectWidth;
    effectOutBuffer.height = effectHeight;
    effectOutBuffer.rowBytes = stride;

    // Determine the size of a temporary buffer by calling the function first with a special flag. vImage will return
    // the size needed, or an error (which are all negative).
    size_t tmpBufferSize = vImageBoxConvolve_ARGB8888(&effectInBuffer, &effectOutBuffer, 0, 0, 0, kernelSize, kernelSize, 0, kvImageEdgeExtend | kvImageGetTempBufferSize);
    if (tmpBufferSize <= 0)
        return;

    void* tmpBuffer = fastMalloc(tmpBufferSize);
    vImageBoxConvolve_ARGB8888(&effectInBuffer, &effectOutBuffer, tmpBuffer, 0, 0, kernelSize, kernelSize, 0, kvImageEdgeExtend);
    vImageBoxConvolve_ARGB8888(&effectOutBuffer, &effectInBuffer, tmpBuffer, 0, 0, kernelSize, kernelSize, 0, kvImageEdgeExtend);
    vImageBoxConvolve_ARGB8888(&effectInBuffer, &effectOutBuffer, tmpBuffer, 0, 0, kernelSize, kernelSize, 0, kvImageEdgeExtend);
    WTF::fastFree(tmpBuffer);

    // The final result should be stored in ioBuffer.
    ASSERT(ioBuffer.length() == tempBuffer.length());
    memcpy(ioBuffer.data(), tempBuffer.data(), ioBuffer.length());
}
#endif

inline void standardBoxBlur(Uint8ClampedArray& ioBuffer, Uint8ClampedArray& tempBuffer, unsigned kernelSizeX, unsigned kernelSizeY, int stride, IntSize& paintSize, bool isAlphaImage, EdgeModeType edgeMode)
{
    int dxLeft = 0;
    int dxRight = 0;
    int dyLeft = 0;
    int dyRight = 0;
    
    Uint8ClampedArray* fromBuffer = &ioBuffer;
    Uint8ClampedArray* toBuffer = &tempBuffer;

    for (int i = 0; i < 3; ++i) {
        if (kernelSizeX) {
            kernelPosition(i, kernelSizeX, dxLeft, dxRight);
#if HAVE(ARM_NEON_INTRINSICS)
            if (!isAlphaImage)
                boxBlurNEON(*fromBuffer, *toBuffer, kernelSizeX, dxLeft, dxRight, 4, stride, paintSize.width(), paintSize.height());
            else
                boxBlur(*fromBuffer, *toBuffer, kernelSizeX, dxLeft, dxRight, 4, stride, paintSize.width(), paintSize.height(), true, edgeMode);
#else
            boxBlur(*fromBuffer, *toBuffer, kernelSizeX, dxLeft, dxRight, 4, stride, paintSize.width(), paintSize.height(), isAlphaImage, edgeMode);
#endif
            std::swap(fromBuffer, toBuffer);
        }

        if (kernelSizeY) {
            kernelPosition(i, kernelSizeY, dyLeft, dyRight);
#if HAVE(ARM_NEON_INTRINSICS)
            if (!isAlphaImage)
                boxBlurNEON(*fromBuffer, *toBuffer, kernelSizeY, dyLeft, dyRight, stride, 4, paintSize.height(), paintSize.width());
            else
                boxBlur(*fromBuffer, *toBuffer, kernelSizeY, dyLeft, dyRight, stride, 4, paintSize.height(), paintSize.width(), true, edgeMode);
#else
            boxBlur(*fromBuffer, *toBuffer, kernelSizeY, dyLeft, dyRight, stride, 4, paintSize.height(), paintSize.width(), isAlphaImage, edgeMode);
#endif
            std::swap(fromBuffer, toBuffer);
        }
    }

    // The final result should be stored in ioBuffer.
    if (&ioBuffer != fromBuffer) {
        ASSERT(ioBuffer.length() == fromBuffer->length());
        memcpy(ioBuffer.data(), fromBuffer->data(), ioBuffer.length());
    }
}

inline void FEGaussianBlur::platformApplyGeneric(Uint8ClampedArray& ioBuffer, Uint8ClampedArray& tmpPixelArray, unsigned kernelSizeX, unsigned kernelSizeY, IntSize& paintSize)
{
    int stride = 4 * paintSize.width();

#if USE(ACCELERATE)
    if (kernelSizeX == kernelSizeY && (m_edgeMode == EDGEMODE_NONE || m_edgeMode == EDGEMODE_DUPLICATE)) {
        accelerateBoxBlur(ioBuffer, tmpPixelArray, kernelSizeX, stride, paintSize.width(), paintSize.height());
        return;
    }
#endif

    standardBoxBlur(ioBuffer, tmpPixelArray, kernelSizeX, kernelSizeY, stride, paintSize, isAlphaImage(), m_edgeMode);
}

void FEGaussianBlur::platformApplyWorker(PlatformApplyParameters* parameters)
{
    IntSize paintSize(parameters->width, parameters->height);
    parameters->filter->platformApplyGeneric(*parameters->ioPixelArray, *parameters->tmpPixelArray, parameters->kernelSizeX, parameters->kernelSizeY, paintSize);
}

inline void FEGaussianBlur::platformApply(Uint8ClampedArray& ioBuffer, Uint8ClampedArray& tmpPixelArray, unsigned kernelSizeX, unsigned kernelSizeY, IntSize& paintSize)
{
#if !USE(ACCELERATE)
    int scanline = 4 * paintSize.width();
    int extraHeight = 3 * kernelSizeY * 0.5f;
    int optimalThreadNumber = (paintSize.width() * paintSize.height()) / (s_minimalRectDimension + extraHeight * paintSize.width());

    if (optimalThreadNumber > 1) {
        WTF::ParallelJobs<PlatformApplyParameters> parallelJobs(&platformApplyWorker, optimalThreadNumber);

        int jobs = parallelJobs.numberOfJobs();
        if (jobs > 1) {
            // Split the job into "blockHeight"-sized jobs but there a few jobs that need to be slightly larger since
            // blockHeight * jobs < total size. These extras are handled by the remainder "jobsWithExtra".
            const int blockHeight = paintSize.height() / jobs;
            const int jobsWithExtra = paintSize.height() % jobs;

            int currentY = 0;
            for (int job = 0; job < jobs; job++) {
                PlatformApplyParameters& params = parallelJobs.parameter(job);
                params.filter = this;

                int startY = !job ? 0 : currentY - extraHeight;
                currentY += job < jobsWithExtra ? blockHeight + 1 : blockHeight;
                int endY = job == jobs - 1 ? currentY : currentY + extraHeight;

                int blockSize = (endY - startY) * scanline;
                if (!job) {
                    params.ioPixelArray = &ioBuffer;
                    params.tmpPixelArray = &tmpPixelArray;
                } else {
                    params.ioPixelArray = Uint8ClampedArray::createUninitialized(blockSize);
                    params.tmpPixelArray = Uint8ClampedArray::createUninitialized(blockSize);
                    memcpy(params.ioPixelArray->data(), ioBuffer.data() + startY * scanline, blockSize);
                }

                params.width = paintSize.width();
                params.height = endY - startY;
                params.kernelSizeX = kernelSizeX;
                params.kernelSizeY = kernelSizeY;
            }

            parallelJobs.execute();

            // Copy together the parts of the image.
            currentY = 0;
            for (int job = 1; job < jobs; job++) {
                PlatformApplyParameters& params = parallelJobs.parameter(job);
                int sourceOffset;
                int destinationOffset;
                int size;
                int adjustedBlockHeight = job < jobsWithExtra ? blockHeight + 1 : blockHeight;

                currentY += adjustedBlockHeight;
                sourceOffset = extraHeight * scanline;
                destinationOffset = currentY * scanline;
                size = adjustedBlockHeight * scanline;

                memcpy(ioBuffer.data() + destinationOffset, params.ioPixelArray->data() + sourceOffset, size);
            }
            return;
        }
        // Fallback to single threaded mode.
    }
#endif

    // The selection here eventually should happen dynamically on some platforms.
    platformApplyGeneric(ioBuffer, tmpPixelArray, kernelSizeX, kernelSizeY, paintSize);
}

static int clampedToKernelSize(float value)
{
    // Limit the kernel size to 500. A bigger radius won't make a big difference for the result image but
    // inflates the absolute paint rect too much. This is compatible with Firefox' behavior.
    unsigned size = std::max<unsigned>(2, static_cast<unsigned>(floorf(value * gaussianKernelFactor() + 0.5f)));
    return clampTo<int>(std::min(size, static_cast<unsigned>(gMaxKernelSize)));
}
    
IntSize FEGaussianBlur::calculateUnscaledKernelSize(FloatSize stdDeviation)
{
    ASSERT(stdDeviation.width() >= 0 && stdDeviation.height() >= 0);
    IntSize kernelSize;

    if (stdDeviation.width())
        kernelSize.setWidth(clampedToKernelSize(stdDeviation.width()));

    if (stdDeviation.height())
        kernelSize.setHeight(clampedToKernelSize(stdDeviation.height()));

    return kernelSize;
}

IntSize FEGaussianBlur::calculateKernelSize(const Filter& filter, FloatSize stdDeviation)
{
    return calculateUnscaledKernelSize(filter.scaledByFilterResolution(stdDeviation));
}

IntSize FEGaussianBlur::calculateOutsetSize(FloatSize stdDeviation)
{
    auto kernelSize = calculateUnscaledKernelSize(stdDeviation);

    // We take the half kernel size and multiply it with three, because we run box blur three times.
    return { 3 * kernelSize.width() / 2, 3 * kernelSize.height() / 2 };
}

void FEGaussianBlur::determineAbsolutePaintRect()
{
    IntSize kernelSize = calculateKernelSize(filter(), { m_stdX, m_stdY });

    FloatRect absolutePaintRect = inputEffect(0)->absolutePaintRect();
    // Edge modes other than 'none' do not inflate the affected paint rect.
    if (m_edgeMode != EDGEMODE_NONE) {
        setAbsolutePaintRect(enclosingIntRect(absolutePaintRect));
        return;
    }

    // We take the half kernel size and multiply it with three, because we run box blur three times.
    absolutePaintRect.inflateX(3 * kernelSize.width() * 0.5f);
    absolutePaintRect.inflateY(3 * kernelSize.height() * 0.5f);

    if (clipsToBounds())
        absolutePaintRect.intersect(maxEffectRect());
    else
        absolutePaintRect.unite(maxEffectRect());

    setAbsolutePaintRect(enclosingIntRect(absolutePaintRect));
}

void FEGaussianBlur::platformApplySoftware()
{
    FilterEffect* in = inputEffect(0);

    auto& destinationPixelBuffer = createPremultipliedImageResult();
    if (!destinationPixelBuffer)
        return;

    auto& destinationPixelArray = destinationPixelBuffer->data();

    setIsAlphaImage(in->isAlphaImage());

    IntRect effectDrawingRect = requestedRegionOfInputPixelBuffer(in->absolutePaintRect());
    in->copyPremultipliedResult(destinationPixelArray, effectDrawingRect, operatingColorSpace());
    if (!m_stdX && !m_stdY)
        return;

    IntSize kernelSize = calculateKernelSize(filter(), { m_stdX, m_stdY });
    kernelSize.scale(filter().filterScale());

    IntSize paintSize = absolutePaintRect().size();
    paintSize.scale(filter().filterScale());
    auto tmpImageData = Uint8ClampedArray::tryCreateUninitialized(paintSize.area() * 4);
    if (!tmpImageData)
        return;

    platformApply(destinationPixelArray, *tmpImageData, kernelSize.width(), kernelSize.height(), paintSize);
}

IntOutsets FEGaussianBlur::outsets() const
{
    IntSize outsetSize = calculateOutsetSize({ m_stdX, m_stdY });
    return { outsetSize.height(), outsetSize.width(), outsetSize.height(), outsetSize.width() };
}

TextStream& FEGaussianBlur::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    ts << indent << "[feGaussianBlur";
    FilterEffect::externalRepresentation(ts, representation);
    ts << " stdDeviation=\"" << m_stdX << ", " << m_stdY << "\"]\n";

    TextStream::IndentScope indentScope(ts);
    inputEffect(0)->externalRepresentation(ts, representation);
    return ts;
}

} // namespace WebCore
