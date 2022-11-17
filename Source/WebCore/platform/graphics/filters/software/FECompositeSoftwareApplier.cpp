/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "FECompositeSoftwareApplier.h"

#include "FEComposite.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PixelBuffer.h"
#include <wtf/MathExtras.h>

namespace WebCore {

uint8_t FECompositeSoftwareApplier::clampByte(int c)
{
    uint8_t buff[] = { static_cast<uint8_t>(c), 255, 0 };
    unsigned uc = static_cast<unsigned>(c);
    return buff[!!(uc & ~0xff) + !!(uc & ~(~0u >> 1))];
}

template <int b1, int b4>
inline void FECompositeSoftwareApplier::computeArithmeticPixels(unsigned char* source, unsigned char* destination, int pixelArrayLength, float k1, float k2, float k3, float k4)
{
    float scaledK1;
    float scaledK4;
    if (b1)
        scaledK1 = k1 / 255.0f;
    if (b4)
        scaledK4 = k4 * 255.0f;

    while (--pixelArrayLength >= 0) {
        unsigned char i1 = *source;
        unsigned char i2 = *destination;
        float result = k2 * i1 + k3 * i2;
        if (b1)
            result += scaledK1 * i1 * i2;
        if (b4)
            result += scaledK4;

        *destination = clampByte(result);
        ++source;
        ++destination;
    }
}

// computeArithmeticPixelsUnclamped is a faster version of computeArithmeticPixels for the common case where clamping
// is not necessary. This enables aggresive compiler optimizations such as auto-vectorization.
template <int b1, int b4>
inline void FECompositeSoftwareApplier::computeArithmeticPixelsUnclamped(unsigned char* source, unsigned char* destination, int pixelArrayLength, float k1, float k2, float k3, float k4)
{
    float scaledK1;
    float scaledK4;
    if (b1)
        scaledK1 = k1 / 255.0f;
    if (b4)
        scaledK4 = k4 * 255.0f;

    while (--pixelArrayLength >= 0) {
        unsigned char i1 = *source;
        unsigned char i2 = *destination;
        float result = k2 * i1 + k3 * i2;
        if (b1)
            result += scaledK1 * i1 * i2;
        if (b4)
            result += scaledK4;

        *destination = result;
        ++source;
        ++destination;
    }
}

#if !HAVE(ARM_NEON_INTRINSICS)
inline void FECompositeSoftwareApplier::applyPlatformArithmetic(unsigned char* source, unsigned char* destination, int pixelArrayLength, float k1, float k2, float k3, float k4)
{
    float upperLimit = std::max(0.0f, k1) + std::max(0.0f, k2) + std::max(0.0f, k3) + k4;
    float lowerLimit = std::min(0.0f, k1) + std::min(0.0f, k2) + std::min(0.0f, k3) + k4;
    if ((k4 >= 0.0f && k4 <= 1.0f) && (upperLimit >= 0.0f && upperLimit <= 1.0f) && (lowerLimit >= 0.0f && lowerLimit <= 1.0f)) {
        if (k4) {
            if (k1)
                computeArithmeticPixelsUnclamped<1, 1>(source, destination, pixelArrayLength, k1, k2, k3, k4);
            else
                computeArithmeticPixelsUnclamped<0, 1>(source, destination, pixelArrayLength, k1, k2, k3, k4);
        } else {
            if (k1)
                computeArithmeticPixelsUnclamped<1, 0>(source, destination, pixelArrayLength, k1, k2, k3, k4);
            else
                computeArithmeticPixelsUnclamped<0, 0>(source, destination, pixelArrayLength, k1, k2, k3, k4);
        }
        return;
    }

    if (k4) {
        if (k1)
            computeArithmeticPixels<1, 1>(source, destination, pixelArrayLength, k1, k2, k3, k4);
        else
            computeArithmeticPixels<0, 1>(source, destination, pixelArrayLength, k1, k2, k3, k4);
    } else {
        if (k1)
            computeArithmeticPixels<1, 0>(source, destination, pixelArrayLength, k1, k2, k3, k4);
        else
            computeArithmeticPixels<0, 0>(source, destination, pixelArrayLength, k1, k2, k3, k4);
    }
}
#endif

bool FECompositeSoftwareApplier::applyArithmetic(FilterImage& input, FilterImage& input2, FilterImage& result) const
{
    auto destinationPixelBuffer = result.pixelBuffer(AlphaPremultiplication::Premultiplied);
    if (!destinationPixelBuffer)
        return false;

    IntRect effectADrawingRect = result.absoluteImageRectRelativeTo(input);
    auto sourcePixelBuffer = input.getPixelBuffer(AlphaPremultiplication::Premultiplied, effectADrawingRect, m_effect.operatingColorSpace());
    if (!sourcePixelBuffer)
        return false;

    IntRect effectBDrawingRect = result.absoluteImageRectRelativeTo(input2);
    input2.copyPixelBuffer(*destinationPixelBuffer, effectBDrawingRect);

#if !HAVE(ARM_NEON_INTRINSICS)
    auto* sourcePixelBytes = sourcePixelBuffer->bytes();
    auto* destinationPixelBytes = destinationPixelBuffer->bytes();

    auto length = sourcePixelBuffer->sizeInBytes();
    ASSERT(length == destinationPixelBuffer->sizeInBytes());
    applyPlatformArithmetic(sourcePixelBytes, destinationPixelBytes, length, m_effect.k1(), m_effect.k2(), m_effect.k3(), m_effect.k4());
#endif
    return true;
}

bool FECompositeSoftwareApplier::applyNonArithmetic(FilterImage& input, FilterImage& input2, FilterImage& result) const
{
    auto resultImage = result.imageBuffer();
    if (!resultImage)
        return false;

    auto inputImage = input.imageBuffer();
    auto inputImage2 = input2.imageBuffer();
    if (!inputImage || !inputImage2)
        return false;

    auto& filterContext = resultImage->context();
    auto inputImageRect = input.absoluteImageRectRelativeTo(result);
    auto inputImageRect2 = input2.absoluteImageRectRelativeTo(result);

    switch (m_effect.operation()) {
    case FECOMPOSITE_OPERATOR_UNKNOWN:
        return false;

    case FECOMPOSITE_OPERATOR_OVER:
        filterContext.drawImageBuffer(*inputImage2, inputImageRect2);
        filterContext.drawImageBuffer(*inputImage, inputImageRect);
        break;

    case FECOMPOSITE_OPERATOR_IN: {
        // Applies only to the intersected region.
        IntRect destinationRect = input.absoluteImageRect();
        destinationRect.intersect(input2.absoluteImageRect());
        destinationRect.intersect(result.absoluteImageRect());
        if (destinationRect.isEmpty())
            break;
        IntRect adjustedDestinationRect = destinationRect - result.absoluteImageRect().location();
        IntRect sourceRect = destinationRect - input.absoluteImageRect().location();
        IntRect source2Rect = destinationRect - input2.absoluteImageRect().location();
        filterContext.drawImageBuffer(*inputImage2, FloatRect(adjustedDestinationRect), FloatRect(source2Rect));
        filterContext.drawImageBuffer(*inputImage, FloatRect(adjustedDestinationRect), FloatRect(sourceRect), { CompositeOperator::SourceIn });
        break;
    }

    case FECOMPOSITE_OPERATOR_OUT:
        filterContext.drawImageBuffer(*inputImage, inputImageRect);
        filterContext.drawImageBuffer(*inputImage2, inputImageRect2, { { }, inputImage2->logicalSize() }, CompositeOperator::DestinationOut);
        break;

    case FECOMPOSITE_OPERATOR_ATOP:
        filterContext.drawImageBuffer(*inputImage2, inputImageRect2);
        filterContext.drawImageBuffer(*inputImage, inputImageRect, { { }, inputImage->logicalSize() }, CompositeOperator::SourceAtop);
        break;

    case FECOMPOSITE_OPERATOR_XOR:
        filterContext.drawImageBuffer(*inputImage2, inputImageRect2);
        filterContext.drawImageBuffer(*inputImage, inputImageRect, { { }, inputImage->logicalSize() }, CompositeOperator::XOR);
        break;

    case FECOMPOSITE_OPERATOR_ARITHMETIC:
        ASSERT_NOT_REACHED();
        return false;

    case FECOMPOSITE_OPERATOR_LIGHTER:
        filterContext.drawImageBuffer(*inputImage2, inputImageRect2);
        filterContext.drawImageBuffer(*inputImage, inputImageRect, { { }, inputImage->logicalSize() }, CompositeOperator::PlusLighter);
        break;
    }

    return true;
}

bool FECompositeSoftwareApplier::apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const
{
    auto& input = inputs[0].get();
    auto& input2 = inputs[1].get();

    if (m_effect.operation() == FECOMPOSITE_OPERATOR_ARITHMETIC)
        return applyArithmetic(input, input2, result);
    return applyNonArithmetic(input, input2, result);
}

} // namespace WebCore
