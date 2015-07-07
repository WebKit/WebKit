/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Igalia, S.L.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2015 Apple, Inc. All rights reserved.
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

#if ENABLE(FILTERS)
#include "FEGaussianBlur.h"

#include "FEGaussianBlurNEON.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "TextStream.h"

#if HAVE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

#include <runtime/JSCInlines.h>
#include <runtime/TypedArrayInlines.h>
#include <runtime/Uint8ClampedArray.h>
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

FEGaussianBlur::FEGaussianBlur(Filter* filter, float x, float y, EdgeModeType edgeMode)
    : FilterEffect(filter)
    , m_stdX(x)
    , m_stdY(y)
    , m_edgeMode(edgeMode)
{
}

PassRefPtr<FEGaussianBlur> FEGaussianBlur::create(Filter* filter, float x, float y, EdgeModeType edgeMode)
{
    return adoptRef(new FEGaussianBlur(filter, x, y, edgeMode));
}

float FEGaussianBlur::stdDeviationX() const
{
    return m_stdX;
}

void FEGaussianBlur::setStdDeviationX(float x)
{
    m_stdX = x;
}

float FEGaussianBlur::stdDeviationY() const
{
    return m_stdY;
}

void FEGaussianBlur::setStdDeviationY(float y)
{
    m_stdY = y;
}

EdgeModeType FEGaussianBlur::edgeMode() const
{
    return m_edgeMode;
}

void FEGaussianBlur::setEdgeMode(EdgeModeType edgeMode)
{
    m_edgeMode = edgeMode;
}

// This function only operates on Alpha channel.
inline void boxBlurAlphaOnly(const Uint8ClampedArray* srcPixelArray, Uint8ClampedArray* dstPixelArray,
    unsigned dx, int& dxLeft, int& dxRight, int& stride, int& strideLine, int& effectWidth, int& effectHeight, const int& maxKernelSize)
{
    unsigned char* srcData = srcPixelArray->data();
    unsigned char* dstData = dstPixelArray->data();
    // Memory alignment is: RGBA, zero-index based.
    const int channel = 3;

    for (int y = 0; y < effectHeight; ++y) {
        int line = y * strideLine;
        int sum = 0;

        // Fill the kernel.
        for (int i = 0; i < maxKernelSize; ++i) {
            unsigned offset = line + i * stride;
            unsigned char* srcPtr = srcData + offset;
            sum += srcPtr[channel];
        }

        // Blurring.
        for (int x = 0; x < effectWidth; ++x) {
            unsigned pixelByteOffset = line + x * stride + channel;
            unsigned char* dstPtr = dstData + pixelByteOffset;
            *dstPtr = static_cast<unsigned char>(sum / dx);

            // Shift kernel.
            if (x >= dxLeft) {
                unsigned leftOffset = pixelByteOffset - dxLeft * stride;
                unsigned char* srcPtr = srcData + leftOffset;
                sum -= *srcPtr;
            }

            if (x + dxRight < effectWidth) {
                unsigned rightOffset = pixelByteOffset + dxRight * stride;
                unsigned char* srcPtr = srcData + rightOffset;
                sum += *srcPtr;
            }
        }
    }
}

inline void boxBlur(const Uint8ClampedArray* srcPixelArray, Uint8ClampedArray* dstPixelArray,
    unsigned dx, int dxLeft, int dxRight, int stride, int strideLine, int effectWidth, int effectHeight, bool alphaImage, EdgeModeType edgeMode)
{
    const int maxKernelSize = std::min(dxRight, effectWidth);
    if (alphaImage) {
        return boxBlurAlphaOnly(srcPixelArray, dstPixelArray, dx, dxLeft, dxRight, stride, strideLine,
            effectWidth, effectHeight, maxKernelSize);
    }

    unsigned char* srcData = srcPixelArray->data();
    unsigned char* dstData = dstPixelArray->data();

    // Concerning the array width/length: it is Element size + Margin + Border. The number of pixels will be
    // P = width * height * channels.
    for (int y = 0; y < effectHeight; ++y) {
        int line = y * strideLine;
        int sumR = 0, sumG = 0, sumB = 0, sumA = 0;

        if (edgeMode == EDGEMODE_NONE) {
            // Fill the kernel.
            for (int i = 0; i < maxKernelSize; ++i) {
                unsigned offset = line + i * stride;
                unsigned char* srcPtr = srcData + offset;
                sumR += *srcPtr++;
                sumG += *srcPtr++;
                sumB += *srcPtr++;
                sumA += *srcPtr;
            }

            // Blurring.
            for (int x = 0; x < effectWidth; ++x) {
                unsigned pixelByteOffset = line + x * stride;
                unsigned char* dstPtr = dstData + pixelByteOffset;

                *dstPtr++ = static_cast<unsigned char>(sumR / dx);
                *dstPtr++ = static_cast<unsigned char>(sumG / dx);
                *dstPtr++ = static_cast<unsigned char>(sumB / dx);
                *dstPtr = static_cast<unsigned char>(sumA / dx);

                // Shift kernel.
                if (x >= dxLeft) {
                    unsigned leftOffset = pixelByteOffset - dxLeft * stride;
                    unsigned char* srcPtr = srcData + leftOffset;
                    sumR -= srcPtr[0];
                    sumG -= srcPtr[1];
                    sumB -= srcPtr[2];
                    sumA -= srcPtr[3];
                }

                if (x + dxRight < effectWidth) {
                    unsigned rightOffset = pixelByteOffset + dxRight * stride;
                    unsigned char* srcPtr = srcData + rightOffset;
                    sumR += srcPtr[0];
                    sumG += srcPtr[1];
                    sumB += srcPtr[2];
                    sumA += srcPtr[3];
                }
            }

        } else {
            // FIXME: Add support for 'wrap' here.
            // Get edge values for edgeMode 'duplicate'.
            unsigned char* edgeValueLeft = srcData + line;
            unsigned char* edgeValueRight  = srcData + (line + (effectWidth - 1) * stride);

            // Fill the kernel.
            for (int i = dxLeft * -1; i < dxRight; ++i) {
                // Is this right for negative values of 'i'?
                unsigned offset = line + i * stride;
                unsigned char* srcPtr = srcData + offset;

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
                unsigned char* dstPtr = dstData + pixelByteOffset;

                *dstPtr++ = static_cast<unsigned char>(sumR / dx);
                *dstPtr++ = static_cast<unsigned char>(sumG / dx);
                *dstPtr++ = static_cast<unsigned char>(sumB / dx);
                *dstPtr = static_cast<unsigned char>(sumA / dx);

                // Shift kernel.
                if (x < dxLeft) {
                    sumR -= edgeValueLeft[0];
                    sumG -= edgeValueLeft[1];
                    sumB -= edgeValueLeft[2];
                    sumA -= edgeValueLeft[3];
                } else {
                    unsigned leftOffset = pixelByteOffset - dxLeft * stride;
                    unsigned char* srcPtr = srcData + leftOffset;
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
                    unsigned char* srcPtr = srcData + rightOffset;
                    sumR += srcPtr[0];
                    sumG += srcPtr[1];
                    sumB += srcPtr[2];
                    sumA += srcPtr[3];
                }
            }
        }
    }
}

#if HAVE(ACCELERATE)
inline void accelerateBoxBlur(const Uint8ClampedArray* src, Uint8ClampedArray* dst, unsigned kernelSize, int stride, int effectWidth, int effectHeight)
{
    // We must always use an odd radius.
    if (kernelSize % 2 != 1)
        kernelSize += 1;

    vImage_Buffer effectInBuffer;
    effectInBuffer.data = src->data();
    effectInBuffer.width = effectWidth;
    effectInBuffer.height = effectHeight;
    effectInBuffer.rowBytes = stride;

    vImage_Buffer effectOutBuffer;
    effectOutBuffer.data = dst->data();
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

    // The final result should be stored in src.
    if (dst == src) {
        ASSERT(src->length() == dst->length());
        memcpy(dst->data(), src->data(), src->length());
    }
}
#endif

inline void standardBoxBlur(Uint8ClampedArray* src, Uint8ClampedArray* dst, unsigned kernelSizeX, unsigned kernelSizeY, int stride, IntSize& paintSize, bool isAlphaImage, EdgeModeType edgeMode)
{
    int dxLeft = 0;
    int dxRight = 0;
    int dyLeft = 0;
    int dyRight = 0;

    for (int i = 0; i < 3; ++i) {
        if (kernelSizeX) {
            kernelPosition(i, kernelSizeX, dxLeft, dxRight);
#if HAVE(ARM_NEON_INTRINSICS)
            if (!isAlphaImage())
                boxBlurNEON(src, dst, kernelSizeX, dxLeft, dxRight, 4, stride, paintSize.width(), paintSize.height());
            else
                boxBlur(src, dst, kernelSizeX, dxLeft, dxRight, 4, stride, paintSize.width(), paintSize.height(), true, edgeMode);
#else
            boxBlur(src, dst, kernelSizeX, dxLeft, dxRight, 4, stride, paintSize.width(), paintSize.height(), isAlphaImage, edgeMode);
#endif
            std::swap(src, dst);
        }

        if (kernelSizeY) {
            kernelPosition(i, kernelSizeY, dyLeft, dyRight);
#if HAVE(ARM_NEON_INTRINSICS)
            if (!isAlphaImage())
                boxBlurNEON(src, dst, kernelSizeY, dyLeft, dyRight, stride, 4, paintSize.height(), paintSize.width());
            else
                boxBlur(src, dst, kernelSizeY, dyLeft, dyRight, stride, 4, paintSize.height(), paintSize.width(), true, edgeMode);
#else
            boxBlur(src, dst, kernelSizeY, dyLeft, dyRight, stride, 4, paintSize.height(), paintSize.width(), isAlphaImage, edgeMode);
#endif
            std::swap(src, dst);
        }
    }

    // The final result should be stored in src.
    if (dst == src) {
        ASSERT(src->length() == dst->length());
        memcpy(dst->data(), src->data(), src->length());
    }
}

inline void FEGaussianBlur::platformApplyGeneric(Uint8ClampedArray* srcPixelArray, Uint8ClampedArray* tmpPixelArray, unsigned kernelSizeX, unsigned kernelSizeY, IntSize& paintSize)
{
    int stride = 4 * paintSize.width();

#if HAVE(ACCELERATE)
    if (kernelSizeX == kernelSizeY && (m_edgeMode == EDGEMODE_NONE || m_edgeMode == EDGEMODE_DUPLICATE)) {
        accelerateBoxBlur(srcPixelArray, tmpPixelArray, kernelSizeX, stride, paintSize.width(), paintSize.height());
        return;
    }
#endif

    standardBoxBlur(srcPixelArray, tmpPixelArray, kernelSizeX, kernelSizeY, stride, paintSize, isAlphaImage(), m_edgeMode);
}

void FEGaussianBlur::platformApplyWorker(PlatformApplyParameters* parameters)
{
    IntSize paintSize(parameters->width, parameters->height);
    parameters->filter->platformApplyGeneric(parameters->srcPixelArray.get(), parameters->dstPixelArray.get(),
        parameters->kernelSizeX, parameters->kernelSizeY, paintSize);
}

inline void FEGaussianBlur::platformApply(Uint8ClampedArray* srcPixelArray, Uint8ClampedArray* tmpPixelArray, unsigned kernelSizeX, unsigned kernelSizeY, IntSize& paintSize)
{
#if !HAVE(ACCELERATE)
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
                    params.srcPixelArray = srcPixelArray;
                    params.dstPixelArray = tmpPixelArray;
                } else {
                    params.srcPixelArray = Uint8ClampedArray::createUninitialized(blockSize);
                    params.dstPixelArray = Uint8ClampedArray::createUninitialized(blockSize);
                    memcpy(params.srcPixelArray->data(), srcPixelArray->data() + startY * scanline, blockSize);
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

                memcpy(srcPixelArray->data() + destinationOffset, params.srcPixelArray->data() + sourceOffset, size);
            }
            return;
        }
        // Fallback to single threaded mode.
    }
#endif

    // The selection here eventually should happen dynamically on some platforms.
    platformApplyGeneric(srcPixelArray, tmpPixelArray, kernelSizeX, kernelSizeY, paintSize);
}

static int clampedToKernelSize(float value)
{
    // Limit the kernel size to 500. A bigger radius won't make a big difference for the result image but
    // inflates the absolute paint rect too much. This is compatible with Firefox' behavior.
    unsigned size = std::max<unsigned>(2, static_cast<unsigned>(floorf(value * gaussianKernelFactor() + 0.5f)));
    return clampTo<int>(std::min(size, static_cast<unsigned>(gMaxKernelSize)));
}
    
IntSize FEGaussianBlur::calculateUnscaledKernelSize(const FloatPoint& stdDeviation)
{
    ASSERT(stdDeviation.x() >= 0 && stdDeviation.y() >= 0);
    IntSize kernelSize;

    if (stdDeviation.x())
        kernelSize.setWidth(clampedToKernelSize(stdDeviation.x()));

    if (stdDeviation.y())
        kernelSize.setHeight(clampedToKernelSize(stdDeviation.y()));

    return kernelSize;
}

IntSize FEGaussianBlur::calculateKernelSize(const Filter& filter, const FloatPoint& stdDeviation)
{
    FloatPoint stdFilterScaled(filter.applyHorizontalScale(stdDeviation.x()), filter.applyVerticalScale(stdDeviation.y()));
    return calculateUnscaledKernelSize(stdFilterScaled);
}

void FEGaussianBlur::determineAbsolutePaintRect()
{
    IntSize kernelSize = calculateKernelSize(filter(), FloatPoint(m_stdX, m_stdY));

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

    Uint8ClampedArray* srcPixelArray = createPremultipliedImageResult();
    if (!srcPixelArray)
        return;

    setIsAlphaImage(in->isAlphaImage());

    IntRect effectDrawingRect = requestedRegionOfInputImageData(in->absolutePaintRect());
    in->copyPremultipliedImage(srcPixelArray, effectDrawingRect);

    if (!m_stdX && !m_stdY)
        return;

    IntSize kernelSize = calculateKernelSize(filter(), FloatPoint(m_stdX, m_stdY));
    kernelSize.scale(filter().filterScale());

    IntSize paintSize = absolutePaintRect().size();
    paintSize.scale(filter().filterScale());
    RefPtr<Uint8ClampedArray> tmpImageData = Uint8ClampedArray::createUninitialized(paintSize.width() * paintSize.height() * 4);
    Uint8ClampedArray* tmpPixelArray = tmpImageData.get();

    platformApply(srcPixelArray, tmpPixelArray, kernelSize.width(), kernelSize.height(), paintSize);
}

void FEGaussianBlur::dump()
{
}

TextStream& FEGaussianBlur::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feGaussianBlur";
    FilterEffect::externalRepresentation(ts);
    ts << " stdDeviation=\"" << m_stdX << ", " << m_stdY << "\"]\n";
    inputEffect(0)->externalRepresentation(ts, indent + 1);
    return ts;
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
