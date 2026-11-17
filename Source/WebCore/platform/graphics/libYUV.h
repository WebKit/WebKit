/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(LIBWEBRTC)

#include <cstdint>

// libyuv is built into libwebrtc, which exports these functions without their headers.
// The declarations match libyuv/planar_functions.h and libyuv/scale.h.
// Strides are in bytes for 8-bit planes and in samples for 16-bit planes.
namespace libyuv {
extern "C" {

void CopyPlane(const uint8_t* srcY, int srcStrideY, uint8_t* dstY, int dstStrideY, int width, int height);

void MergeUVPlane(const uint8_t* srcU, int srcStrideU, const uint8_t* srcV, int srcStrideV, uint8_t* dstUV, int dstStrideUV, int width, int height);
void SplitUVPlane(const uint8_t* srcUV, int srcStrideUV, uint8_t* dstU, int dstStrideU, uint8_t* dstV, int dstStrideV, int width, int height);

// The 16-bit variants also move depth-bit samples from the LSBs of the planar side to the MSBs of the interleaved side, and back.
void MergeUVPlane_16(const uint16_t* srcU, int srcStrideU, const uint16_t* srcV, int srcStrideV, uint16_t* dstUV, int dstStrideUV, int width, int height, int depth); // NOLINT
void SplitUVPlane_16(const uint16_t* srcUV, int srcStrideUV, uint16_t* dstU, int dstStrideU, uint16_t* dstV, int dstStrideV, int width, int height, int depth); // NOLINT
void ConvertToMSBPlane_16(const uint16_t* srcY, int srcStrideY, uint16_t* dstY, int dstStrideY, int width, int height, int depth); // NOLINT
void ConvertToLSBPlane_16(const uint16_t* srcY, int srcStrideY, uint16_t* dstY, int dstStrideY, int width, int height, int depth); // NOLINT

// dst = min((src * scale) >> 16, 255).
void Convert16To8Plane(const uint16_t* srcY, int srcStrideY, uint8_t* dstY, int dstStrideY, int scale, int width, int height);
// dst = (src * 0x0101) >> (16 - bits).
void Convert8To16Plane(const uint8_t* srcY, int srcStrideY, uint16_t* dstY, int dstStrideY, int bits, int width, int height);

// Writes byte 3 of each 4-byte pixel.
int ARGBCopyYToAlpha(const uint8_t* srcY, int srcStrideY, uint8_t* dstARGB, int dstStrideARGB, int width, int height);

enum FilterMode {
    kFilterNone = 0,
    kFilterLinear = 1,
    kFilterBilinear = 2,
    kFilterBox = 3
};
int ScalePlane(const uint8_t* src, int srcStride, int srcWidth, int srcHeight, uint8_t* dst, int dstStride, int dstWidth, int dstHeight, FilterMode);
int ScalePlane_16(const uint16_t* src, int srcStride, int srcWidth, int srcHeight, uint16_t* dst, int dstStride, int dstWidth, int dstHeight, FilterMode); // NOLINT

}
}

#endif // USE(LIBWEBRTC)
