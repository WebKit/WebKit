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
#include "FECompositeSoftwareArithmeticApplier.h"

#if !HAVE(ARM_NEON_INTRINSICS)

#include "FEComposite.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PixelBuffer.h"
#include <wtf/MathExtras.h>

namespace WebCore {

FECompositeSoftwareArithmeticApplier::FECompositeSoftwareArithmeticApplier(const FEComposite& effect)
    : Base(effect)
{
    ASSERT(m_effect.operation() == CompositeOperationType::FECOMPOSITE_OPERATOR_ARITHMETIC);
}

uint8_t FECompositeSoftwareArithmeticApplier::clampByte(int c)
{
    uint8_t buff[] = { static_cast<uint8_t>(c), 255, 0 };
    unsigned uc = static_cast<unsigned>(c);
    return buff[!!(uc & ~0xff) + !!(uc & ~(~0u >> 1))];
}

template <int b1, int b4>
inline void FECompositeSoftwareArithmeticApplier::computePixels(unsigned char* source, unsigned char* destination, int pixelArrayLength, float k1, float k2, float k3, float k4)
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

// computePixelsUnclamped is a faster version of computePixels for the common case where clamping
// is not necessary. This enables aggresive compiler optimizations such as auto-vectorization.
template <int b1, int b4>
inline void FECompositeSoftwareArithmeticApplier::computePixelsUnclamped(unsigned char* source, unsigned char* destination, int pixelArrayLength, float k1, float k2, float k3, float k4)
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

inline void FECompositeSoftwareArithmeticApplier::applyPlatform(unsigned char* source, unsigned char* destination, int pixelArrayLength, float k1, float k2, float k3, float k4)
{
    float upperLimit = std::max(0.0f, k1) + std::max(0.0f, k2) + std::max(0.0f, k3) + k4;
    float lowerLimit = std::min(0.0f, k1) + std::min(0.0f, k2) + std::min(0.0f, k3) + k4;
    if ((k4 >= 0.0f && k4 <= 1.0f) && (upperLimit >= 0.0f && upperLimit <= 1.0f) && (lowerLimit >= 0.0f && lowerLimit <= 1.0f)) {
        if (k4) {
            if (k1)
                computePixelsUnclamped<1, 1>(source, destination, pixelArrayLength, k1, k2, k3, k4);
            else
                computePixelsUnclamped<0, 1>(source, destination, pixelArrayLength, k1, k2, k3, k4);
        } else {
            if (k1)
                computePixelsUnclamped<1, 0>(source, destination, pixelArrayLength, k1, k2, k3, k4);
            else
                computePixelsUnclamped<0, 0>(source, destination, pixelArrayLength, k1, k2, k3, k4);
        }
        return;
    }

    if (k4) {
        if (k1)
            computePixels<1, 1>(source, destination, pixelArrayLength, k1, k2, k3, k4);
        else
            computePixels<0, 1>(source, destination, pixelArrayLength, k1, k2, k3, k4);
    } else {
        if (k1)
            computePixels<1, 0>(source, destination, pixelArrayLength, k1, k2, k3, k4);
        else
            computePixels<0, 0>(source, destination, pixelArrayLength, k1, k2, k3, k4);
    }
}

bool FECompositeSoftwareArithmeticApplier::apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const
{
    auto& input = inputs[0].get();
    auto& input2 = inputs[1].get();

    auto destinationPixelBuffer = result.pixelBuffer(AlphaPremultiplication::Premultiplied);
    if (!destinationPixelBuffer)
        return false;

    IntRect effectADrawingRect = result.absoluteImageRectRelativeTo(input);
    auto sourcePixelBuffer = input.getPixelBuffer(AlphaPremultiplication::Premultiplied, effectADrawingRect, m_effect.operatingColorSpace());
    if (!sourcePixelBuffer)
        return false;

    IntRect effectBDrawingRect = result.absoluteImageRectRelativeTo(input2);
    input2.copyPixelBuffer(*destinationPixelBuffer, effectBDrawingRect);

    auto* sourcePixelBytes = sourcePixelBuffer->bytes();
    auto* destinationPixelBytes = destinationPixelBuffer->bytes();

    auto length = sourcePixelBuffer->sizeInBytes();
    ASSERT(length == destinationPixelBuffer->sizeInBytes());

    applyPlatform(sourcePixelBytes, destinationPixelBytes, length, m_effect.k1(), m_effect.k2(), m_effect.k3(), m_effect.k4());
    return true;
}

} // namespace WebCore

#endif // !HAVE(ARM_NEON_INTRINSICS)
