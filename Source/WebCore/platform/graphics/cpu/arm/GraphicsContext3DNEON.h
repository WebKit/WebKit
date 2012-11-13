/*
 * Copyright (C) 2012 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
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

#ifndef GraphicsContext3DNEON_h
#define GraphicsContext3DNEON_h

#if HAVE(ARM_NEON_INTRINSICS)

#include <arm_neon.h>

namespace WebCore {

namespace ARM {

ALWAYS_INLINE void unpackOneRowOfRGBA4444ToRGBA8NEON(const uint16_t* source, uint8_t* destination, unsigned pixelSize)
{
    uint16x8_t constant = vdupq_n_u16(0x0F);
    for (unsigned i = 0; i < pixelSize; i += 8) {
        uint16x8_t eightPixels = vld1q_u16(source + i);

        uint8x8_t componentR = vqmovn_u16(vshrq_n_u16(eightPixels, 12));
        uint8x8_t componentG = vqmovn_u16(vandq_u16(vshrq_n_u16(eightPixels, 8), constant));
        uint8x8_t componentB = vqmovn_u16(vandq_u16(vshrq_n_u16(eightPixels, 4), constant));
        uint8x8_t componentA = vqmovn_u16(vandq_u16(eightPixels, constant));

        componentR = vorr_u8(vshl_n_u8(componentR, 4), componentR);
        componentG = vorr_u8(vshl_n_u8(componentG, 4), componentG);
        componentB = vorr_u8(vshl_n_u8(componentB, 4), componentB);
        componentA = vorr_u8(vshl_n_u8(componentA, 4), componentA);

        uint8x8x4_t destComponents = {componentR, componentG, componentB, componentA};
        vst4_u8(destination, destComponents);
        destination += 32;
    }
}

ALWAYS_INLINE void packOneRowOfRGBA8ToUnsignedShort4444NEON(const uint8_t* source, uint16_t* destination, unsigned componentsSize)
{
    uint8_t* dst = reinterpret_cast<uint8_t*>(destination);
    uint8x8_t constant = vdup_n_u8(0xF0);
    for (unsigned i = 0; i < componentsSize; i += 32) {
        uint8x8x4_t RGBA8 = vld4_u8(source + i);

        uint8x8_t componentR = vand_u8(RGBA8.val[0], constant);
        uint8x8_t componentG = vshr_n_u8(vand_u8(RGBA8.val[1], constant), 4);
        uint8x8_t componentB = vand_u8(RGBA8.val[2], constant);
        uint8x8_t componentA = vshr_n_u8(vand_u8(RGBA8.val[3], constant), 4);

        uint8x8x2_t RGBA4;
        RGBA4.val[0] = vorr_u8(componentB, componentA);
        RGBA4.val[1] = vorr_u8(componentR, componentG);
        vst2_u8(dst, RGBA4);
        dst += 16;
    }
}

} // namespace ARM

} // namespace WebCore

#endif // HAVE(ARM_NEON_INTRINSICS)

#endif // GraphicsContext3DNEON_h
