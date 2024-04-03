/*
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Felician Marton
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
#include "FECompositeNeonArithmeticApplier.h"

#if HAVE(ARM_NEON_INTRINSICS)

#include "FEComposite.h"
#include "NEONHelpers.h"
#include <arm_neon.h>

namespace WebCore {

FECompositeNeonArithmeticApplier::FECompositeNeonArithmeticApplier(const FEComposite& effect)
    : Base(effect)
{
    ASSERT(m_effect.operation() == CompositeOperationType::FECOMPOSITE_OPERATOR_ARITHMETIC);
}

template <int b1, int b4>
inline void FECompositeNeonArithmeticApplier::computePixels(const uint8_t* source, uint8_t* destination, unsigned pixelArrayLength, float k1, float k2, float k3, float k4)
{
    float32x4_t k1x4 = vdupq_n_f32(k1 / 255);
    float32x4_t k2x4 = vdupq_n_f32(k2);
    float32x4_t k3x4 = vdupq_n_f32(k3);
    float32x4_t k4x4 = vdupq_n_f32(k4 * 255);
    uint32x4_t max255 = vdupq_n_u32(255);

    const uint32_t* sourcePixel = reinterpret_cast<const uint32_t*>(source);
    uint32_t* destinationPixel = reinterpret_cast<uint32_t*>(destination);
    uint32_t* destinationEndPixel = destinationPixel + (pixelArrayLength >> 2);

    while (destinationPixel < destinationEndPixel) {
        float32x4_t sourcePixelAsFloat = loadRGBA8AsFloat(sourcePixel);
        float32x4_t destinationPixelAsFloat = loadRGBA8AsFloat(destinationPixel);

        float32x4_t result = vmulq_f32(sourcePixelAsFloat, k2x4);
        result = vmlaq_f32(result, destinationPixelAsFloat, k3x4);
        if (b1)
            result = vmlaq_f32(result, vmulq_f32(sourcePixelAsFloat, destinationPixelAsFloat), k1x4);
        if (b4)
            result = vaddq_f32(result, k4x4);

        // Convert result to uint so negative values are converted to zero.
        uint16x4_t temporary3 = vmovn_u32(vminq_u32(vcvtq_u32_f32(result), max255));
        uint8x8_t temporary4 = vmovn_u16(vcombine_u16(temporary3, temporary3));
        *destinationPixel++ = vget_lane_u32(vreinterpret_u32_u8(temporary4), 0);
        ++sourcePixel;
    }
}

inline void FECompositeNeonArithmeticApplier::applyPlatform(const uint8_t* source, uint8_t* destination, unsigned pixelArrayLength, float k1, float k2, float k3, float k4)
{
    if (!k4) {
        if (!k1) {
            computePixels<0, 0>(source, destination, pixelArrayLength, k1, k2, k3, k4);
            return;
        }

        computePixels<1, 0>(source, destination, pixelArrayLength, k1, k2, k3, k4);
        return;
    }

    if (!k1) {
        computePixels<0, 1>(source, destination, pixelArrayLength, k1, k2, k3, k4);
        return;
    }
    computePixels<1, 1>(source, destination, pixelArrayLength, k1, k2, k3, k4);
}

bool FECompositeNeonArithmeticApplier::apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const
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

#endif // HAVE(ARM_NEON_INTRINSICS)
