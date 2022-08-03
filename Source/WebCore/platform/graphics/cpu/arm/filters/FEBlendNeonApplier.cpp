/*
 * Copyright (C) 2012 University of Szeged
 * Copyright (C) 2012 Gabor Rapcsanyi
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
#include "FEBlendNeonApplier.h"

#if HAVE(ARM_NEON_INTRINSICS)

#include "FEBlend.h"
#include "PixelBuffer.h"
#include <arm_neon.h>

namespace WebCore {

class FEBlendUtilitiesNEON {
public:
    static inline uint16x8_t div255(uint16x8_t num, uint16x8_t sixteenConst255, uint16x8_t sixteenConstOne)
    {
        uint16x8_t quotient = vshrq_n_u16(num, 8);
        uint16x8_t remainder = vaddq_u16(vsubq_u16(num, vmulq_u16(sixteenConst255, quotient)), sixteenConstOne);
        return vaddq_u16(quotient, vshrq_n_u16(remainder, 8));
    }

    static inline uint16x8_t normal(uint16x8_t pixelA, uint16x8_t pixelB, uint16x8_t alphaA, uint16x8_t,
                                    uint16x8_t sixteenConst255, uint16x8_t sixteenConstOne)
    {
        uint16x8_t tmp1 = vsubq_u16(sixteenConst255, alphaA);
        uint16x8_t tmp2 = vmulq_u16(tmp1, pixelB);
        uint16x8_t tmp3 = div255(tmp2, sixteenConst255, sixteenConstOne);
        return vaddq_u16(tmp3, pixelA);
    }

    static inline uint16x8_t multiply(uint16x8_t pixelA, uint16x8_t pixelB, uint16x8_t alphaA, uint16x8_t alphaB,
                                      uint16x8_t sixteenConst255, uint16x8_t sixteenConstOne)
    {
        uint16x8_t tmp1 = vsubq_u16(sixteenConst255, alphaA);
        uint16x8_t tmp2 = vmulq_u16(tmp1, pixelB);
        uint16x8_t tmp3 = vaddq_u16(vsubq_u16(sixteenConst255, alphaB), pixelB);
        uint16x8_t tmp4 = vmulq_u16(tmp3, pixelA);
        uint16x8_t tmp5 = vaddq_u16(tmp2, tmp4);
        return div255(tmp5, sixteenConst255, sixteenConstOne);
    }

    static inline uint16x8_t screen(uint16x8_t pixelA, uint16x8_t pixelB, uint16x8_t, uint16x8_t,
                                    uint16x8_t sixteenConst255, uint16x8_t sixteenConstOne)
    {
        uint16x8_t tmp1 = vaddq_u16(pixelA, pixelB);
        uint16x8_t tmp2 = vmulq_u16(pixelA, pixelB);
        uint16x8_t tmp3 = div255(tmp2, sixteenConst255, sixteenConstOne);
        return vsubq_u16(tmp1, tmp3);
    }

    static inline uint16x8_t darken(uint16x8_t pixelA, uint16x8_t pixelB, uint16x8_t alphaA, uint16x8_t alphaB,
                                    uint16x8_t sixteenConst255, uint16x8_t sixteenConstOne)
    {
        uint16x8_t tmp1 = vsubq_u16(sixteenConst255, alphaA);
        uint16x8_t tmp2 = vmulq_u16(tmp1, pixelB);
        uint16x8_t tmp3 = div255(tmp2, sixteenConst255, sixteenConstOne);
        uint16x8_t tmp4 = vaddq_u16(tmp3, pixelA);

        uint16x8_t tmp5 = vsubq_u16(sixteenConst255, alphaB);
        uint16x8_t tmp6 = vmulq_u16(tmp5, pixelA);
        uint16x8_t tmp7 = div255(tmp6, sixteenConst255, sixteenConstOne);
        uint16x8_t tmp8 = vaddq_u16(tmp7, pixelB);

        return vminq_u16(tmp4, tmp8);
    }

    static inline uint16x8_t lighten(uint16x8_t pixelA, uint16x8_t pixelB, uint16x8_t alphaA, uint16x8_t alphaB,
                                     uint16x8_t sixteenConst255, uint16x8_t sixteenConstOne)
    {
        uint16x8_t tmp1 = vsubq_u16(sixteenConst255, alphaA);
        uint16x8_t tmp2 = vmulq_u16(tmp1, pixelB);
        uint16x8_t tmp3 = div255(tmp2, sixteenConst255, sixteenConstOne);
        uint16x8_t tmp4 = vaddq_u16(tmp3, pixelA);

        uint16x8_t tmp5 = vsubq_u16(sixteenConst255, alphaB);
        uint16x8_t tmp6 = vmulq_u16(tmp5, pixelA);
        uint16x8_t tmp7 = div255(tmp6, sixteenConst255, sixteenConstOne);
        uint16x8_t tmp8 = vaddq_u16(tmp7, pixelB);

        return vmaxq_u16(tmp4, tmp8);
    }
};

void FEBlendNeonApplier::applyPlatform(unsigned char* srcPixelArrayA, unsigned char* srcPixelArrayB, unsigned char* dstPixelArray, unsigned colorArrayLength) const
{
    uint8_t* sourcePixelA = reinterpret_cast<uint8_t*>(srcPixelArrayA);
    uint8_t* sourcePixelB = reinterpret_cast<uint8_t*>(srcPixelArrayB);
    uint8_t* destinationPixel = reinterpret_cast<uint8_t*>(dstPixelArray);

    uint16x8_t sixteenConst255 = vdupq_n_u16(255);
    uint16x8_t sixteenConstOne = vdupq_n_u16(1);

    unsigned colorOffset = 0;
    while (colorOffset < colorArrayLength) {
        unsigned char alphaA1 = srcPixelArrayA[colorOffset + 3];
        unsigned char alphaB1 = srcPixelArrayB[colorOffset + 3];
        unsigned char alphaA2 = srcPixelArrayA[colorOffset + 7];
        unsigned char alphaB2 = srcPixelArrayB[colorOffset + 7];

        uint16x8_t doubblePixelA = vmovl_u8(vld1_u8(sourcePixelA + colorOffset));
        uint16x8_t doubblePixelB = vmovl_u8(vld1_u8(sourcePixelB + colorOffset));
        uint16x8_t alphaA = vcombine_u16(vdup_n_u16(alphaA1), vdup_n_u16(alphaA2));
        uint16x8_t alphaB = vcombine_u16(vdup_n_u16(alphaB1), vdup_n_u16(alphaB2));

        uint16x8_t result;
        switch (m_effect.blendMode()) {
        case BlendMode::Normal:
            result = FEBlendUtilitiesNEON::normal(doubblePixelA, doubblePixelB, alphaA, alphaB, sixteenConst255, sixteenConstOne);
            break;
        case BlendMode::Multiply:
            result = FEBlendUtilitiesNEON::multiply(doubblePixelA, doubblePixelB, alphaA, alphaB, sixteenConst255, sixteenConstOne);
            break;
        case BlendMode::Screen:
            result = FEBlendUtilitiesNEON::screen(doubblePixelA, doubblePixelB, alphaA, alphaB, sixteenConst255, sixteenConstOne);
            break;
        case BlendMode::Darken:
            result = FEBlendUtilitiesNEON::darken(doubblePixelA, doubblePixelB, alphaA, alphaB, sixteenConst255, sixteenConstOne);
            break;
        case BlendMode::Lighten:
            result = FEBlendUtilitiesNEON::lighten(doubblePixelA, doubblePixelB, alphaA, alphaB, sixteenConst255, sixteenConstOne);
            break;
        default:
            result = vdupq_n_u16(0);
            break;
        }

        vst1_u8(destinationPixel + colorOffset, vmovn_u16(result));

        unsigned char alphaR1 = 255 - ((255 - alphaA1) * (255 - alphaB1)) / 255;
        unsigned char alphaR2 = 255 - ((255 - alphaA2) * (255 - alphaB2)) / 255;

        dstPixelArray[colorOffset + 3] = alphaR1;
        dstPixelArray[colorOffset + 7] = alphaR2;

        colorOffset += 8;
        if (colorOffset > colorArrayLength) {
            ASSERT(colorOffset - 4 == colorArrayLength);
            colorOffset = colorArrayLength - 8;
        }
    }
}

bool FEBlendNeonApplier::apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const
{
    auto& input = inputs[0].get();
    auto& input2 = inputs[1].get();

    auto* destinationPixelBuffer = result.pixelBuffer(AlphaPremultiplication::Premultiplied);
    if (!destinationPixelBuffer)
        return false;

    auto* destinationPixelArray = destinationPixelBuffer->bytes();

    auto effectADrawingRect = result.absoluteImageRectRelativeTo(input);
    auto sourcePixelArrayA = input.getPixelBuffer(AlphaPremultiplication::Premultiplied, effectADrawingRect);

    auto effectBDrawingRect = result.absoluteImageRectRelativeTo(input2);
    auto sourcePixelArrayB = input2.getPixelBuffer(AlphaPremultiplication::Premultiplied, effectBDrawingRect);

    unsigned sourcePixelArrayLength = sourcePixelArrayA->sizeInBytes();
    ASSERT(sourcePixelArrayLength == sourcePixelArrayB->sizeInBytes());

    if (sourcePixelArrayLength >= 8) {
        applyPlatform(sourcePixelArrayA->bytes(), sourcePixelArrayB->bytes(), destinationPixelArray, sourcePixelArrayLength);
        return true;
    }

    // If there is just one pixel we expand it to two.
    ASSERT(sourcePixelArrayLength > 0);
    uint32_t sourceA[2] = { 0, 0 };
    uint32_t sourceBAndDest[2] = { 0, 0 };

    sourceA[0] = reinterpret_cast<uint32_t*>(sourcePixelArrayA->bytes())[0];
    sourceBAndDest[0] = reinterpret_cast<uint32_t*>(sourcePixelArrayB->bytes())[0];
    applyPlatform(reinterpret_cast<uint8_t*>(sourceA), reinterpret_cast<uint8_t*>(sourceBAndDest), reinterpret_cast<uint8_t*>(sourceBAndDest), 8);
    reinterpret_cast<uint32_t*>(destinationPixelArray)[0] = sourceBAndDest[0];
    return true;
}

} // namespace WebCore

#endif // HAVE(ARM_NEON_INTRINSICS)
