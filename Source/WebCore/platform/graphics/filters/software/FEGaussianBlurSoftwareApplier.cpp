/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Igalia, S.L.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2015-2022 Apple, Inc. All rights reserved.
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
#include "FEGaussianBlurSoftwareApplier.h"

#include "FEGaussianBlur.h"
#if HAVE(ARM_NEON_INTRINSICS)
#include "FEGaussianBlurNEON.h"
#endif
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PixelBuffer.h"
#include <JavaScriptCore/TypedArrayInlines.h>
#include <wtf/MathExtras.h>

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#else
#include <wtf/ParallelJobs.h>
#endif

namespace WebCore {

inline void FEGaussianBlurSoftwareApplier::kernelPosition(int blurIteration, unsigned& radius, int& deltaLeft, int& deltaRight)
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

// This function only operates on Alpha channel.
inline void FEGaussianBlurSoftwareApplier::boxBlurAlphaOnly(const PixelBuffer& srcPixelBuffer, PixelBuffer& dstPixelBuffer, unsigned dx, int& dxLeft, int& dxRight, int& stride, int& strideLine, int& effectWidth, int& effectHeight, const int& maxKernelSize)
{
    const uint8_t* srcData = srcPixelBuffer.bytes();
    uint8_t* dstData = dstPixelBuffer.bytes();
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

inline void FEGaussianBlurSoftwareApplier::boxBlur(const PixelBuffer& srcPixelBuffer, PixelBuffer& dstPixelBuffer, unsigned dx, int dxLeft, int dxRight, int stride, int strideLine, int effectWidth, int effectHeight, bool alphaImage, EdgeModeType edgeMode)
{
    const int maxKernelSize = std::min(dxRight, effectWidth);
    if (alphaImage)
        return boxBlurAlphaOnly(srcPixelBuffer, dstPixelBuffer, dx, dxLeft, dxRight, stride, strideLine,  effectWidth, effectHeight, maxKernelSize);

    const uint8_t* srcData = srcPixelBuffer.bytes();
    uint8_t* dstData = dstPixelBuffer.bytes();

    // Concerning the array width/length: it is Element size + Margin + Border. The number of pixels will be
    // P = width * height * channels.
    for (int y = 0; y < effectHeight; ++y) {
        int line = y * strideLine;
        int sumR = 0, sumG = 0, sumB = 0, sumA = 0;

        if (edgeMode == EdgeModeType::None) {
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
inline void FEGaussianBlurSoftwareApplier::boxBlurAccelerated(PixelBuffer& ioBuffer, PixelBuffer& tempBuffer, unsigned kernelSize, int stride, int effectWidth, int effectHeight)
{
    if (!ioBuffer.bytes() || !tempBuffer.bytes()) {
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
    effectInBuffer.data = ioBuffer.bytes();
    effectInBuffer.width = effectWidth;
    effectInBuffer.height = effectHeight;
    effectInBuffer.rowBytes = stride;

    vImage_Buffer effectOutBuffer;
    effectOutBuffer.data = tempBuffer.bytes();
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
    fastFree(tmpBuffer);

    // The final result should be stored in ioBuffer.
    ASSERT(ioBuffer.sizeInBytes() == tempBuffer.sizeInBytes());
    memcpy(ioBuffer.bytes(), tempBuffer.bytes(), ioBuffer.sizeInBytes());
}
#endif

inline void FEGaussianBlurSoftwareApplier::boxBlurUnaccelerated(PixelBuffer& ioBuffer, PixelBuffer& tempBuffer, unsigned kernelSizeX, unsigned kernelSizeY, int stride, IntSize& paintSize, bool isAlphaImage, EdgeModeType edgeMode)
{
    int dxLeft = 0;
    int dxRight = 0;
    int dyLeft = 0;
    int dyRight = 0;
    
    auto* fromBuffer = &ioBuffer;
    auto* toBuffer = &tempBuffer;

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
        ASSERT(ioBuffer.sizeInBytes() == fromBuffer->sizeInBytes());
        memcpy(ioBuffer.bytes(), fromBuffer->bytes(), ioBuffer.sizeInBytes());
    }
}

inline void FEGaussianBlurSoftwareApplier::boxBlurGeneric(PixelBuffer& ioBuffer, PixelBuffer& tempBuffer, unsigned kernelSizeX, unsigned kernelSizeY, IntSize& paintSize, bool isAlphaImage, EdgeModeType edgeMode)
{
    int stride = 4 * paintSize.width();

#if USE(ACCELERATE)
    if (kernelSizeX == kernelSizeY && (edgeMode == EdgeModeType::None || edgeMode == EdgeModeType::Duplicate)) {
        boxBlurAccelerated(ioBuffer, tempBuffer, kernelSizeX, stride, paintSize.width(), paintSize.height());
        return;
    }
#endif

    boxBlurUnaccelerated(ioBuffer, tempBuffer, kernelSizeX, kernelSizeY, stride, paintSize, isAlphaImage, edgeMode);
}

#if !USE(ACCELERATE)
inline void FEGaussianBlurSoftwareApplier::boxBlurWorker(ApplyParameters* parameters)
{
    IntSize paintSize(parameters->width, parameters->height);
    boxBlurGeneric(*parameters->ioBuffer, *parameters->tempBuffer, parameters->kernelSizeX, parameters->kernelSizeY, paintSize, parameters->isAlphaImage, parameters->edgeMode);
}
#endif

inline void FEGaussianBlurSoftwareApplier::applyPlatform(PixelBuffer& ioBuffer, PixelBuffer& tempBuffer, unsigned kernelSizeX, unsigned kernelSizeY, IntSize& paintSize, bool isAlphaImage, EdgeModeType edgeMode)
{
#if !USE(ACCELERATE)
    int scanline = 4 * paintSize.width();
    int extraHeight = 3 * kernelSizeY * 0.5f;

    static constexpr int minimalRectDimension = 100 * 100; // Empirical data limit for parallel jobs
    int optimalThreadNumber = (paintSize.width() * paintSize.height()) / (minimalRectDimension + extraHeight * paintSize.width());

    if (optimalThreadNumber > 1) {
        ParallelJobs<ApplyParameters> parallelJobs(&boxBlurWorker, optimalThreadNumber);

        int jobs = parallelJobs.numberOfJobs();
        if (jobs > 1) {
            // Split the job into "blockHeight"-sized jobs but there a few jobs that need to be slightly larger since
            // blockHeight * jobs < total size. These extras are handled by the remainder "jobsWithExtra".
            const int blockHeight = paintSize.height() / jobs;
            const int jobsWithExtra = paintSize.height() % jobs;

            int currentY = 0;
            for (int job = 0; job < jobs; job++) {
                ApplyParameters& params = parallelJobs.parameter(job);

                int startY = !job ? 0 : currentY - extraHeight;
                currentY += job < jobsWithExtra ? blockHeight + 1 : blockHeight;
                int endY = job == jobs - 1 ? currentY : currentY + extraHeight;

                IntSize blockSize = { paintSize.width(), endY - startY };

                if (!job) {
                    params.ioBuffer = &ioBuffer;
                    params.tempBuffer = &tempBuffer;
                } else {
                    params.ioBuffer = ioBuffer.createScratchPixelBuffer(blockSize);
                    params.tempBuffer = tempBuffer.createScratchPixelBuffer(blockSize);
                    memcpy(params.ioBuffer->bytes(), ioBuffer.bytes() + startY * scanline, params.ioBuffer->sizeInBytes());
                }

                params.width = paintSize.width();
                params.height = endY - startY;
                params.kernelSizeX = kernelSizeX;
                params.kernelSizeY = kernelSizeY;
                params.isAlphaImage = isAlphaImage;
                params.edgeMode = edgeMode;
            }

            parallelJobs.execute();

            // Copy together the parts of the image.
            currentY = 0;
            for (int job = 1; job < jobs; job++) {
                ApplyParameters& params = parallelJobs.parameter(job);
                int sourceOffset;
                int destinationOffset;
                int size;
                int adjustedBlockHeight = job < jobsWithExtra ? blockHeight + 1 : blockHeight;

                currentY += adjustedBlockHeight;
                sourceOffset = extraHeight * scanline;
                destinationOffset = currentY * scanline;
                size = adjustedBlockHeight * scanline;

                memcpy(ioBuffer.bytes() + destinationOffset, params.ioBuffer->bytes() + sourceOffset, size);
            }
            return;
        }
        // Fallback to single threaded mode.
    }
#endif

    // The selection here eventually should happen dynamically on some platforms.
    boxBlurGeneric(ioBuffer, tempBuffer, kernelSizeX, kernelSizeY, paintSize, isAlphaImage, edgeMode);
}

bool FEGaussianBlurSoftwareApplier::apply(const Filter& filter, const FilterImageVector& inputs, FilterImage& result) const
{
    auto& input = inputs[0].get();

    auto destinationPixelBuffer = result.pixelBuffer(AlphaPremultiplication::Premultiplied);
    if (!destinationPixelBuffer)
        return false;

    auto effectDrawingRect = result.absoluteImageRectRelativeTo(input);
    input.copyPixelBuffer(*destinationPixelBuffer, effectDrawingRect);
    if (!m_effect.stdDeviationX() && !m_effect.stdDeviationY())
        return true;

    auto kernelSize = m_effect.calculateKernelSize(filter, { m_effect.stdDeviationX(), m_effect.stdDeviationY() });

    IntSize paintSize = result.absoluteImageRect().size();
    auto tempBuffer = destinationPixelBuffer->createScratchPixelBuffer(paintSize);
    if (!tempBuffer)
        return false;

    applyPlatform(*destinationPixelBuffer, *tempBuffer, kernelSize.width(), kernelSize.height(), paintSize, result.isAlphaImage(), m_effect.edgeMode());
    return true;
}

} // namespace WebCore
