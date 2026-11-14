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

#include <WebCore/WebRTCProvider.h>
#include <cstdint>

// libyuv is built into libwebrtc, which exports these functions without their headers.
// The declarations match libyuv/planar_functions.h.
// Strides are in bytes for 8-bit planes and in samples for 16-bit planes.
namespace libyuv {
extern "C" {

void CopyPlane(const uint8_t* srcY, int srcStrideY, uint8_t* dstY, int dstStrideY, int width, int height);

void MergeUVPlane(const uint8_t* srcU, int srcStrideU, const uint8_t* srcV, int srcStrideV, uint8_t* dstUV, int dstStrideUV, int width, int height);
void SplitUVPlane(const uint8_t* srcUV, int srcStrideUV, uint8_t* dstU, int dstStrideU, uint8_t* dstV, int dstStrideV, int width, int height);

// The 16-bit variants also move depth-bit samples from the LSBs of the planar source to the MSBs of the interleaved destination, and back.
void MergeUVPlane_16(const uint16_t* srcU, int srcStrideU, const uint16_t* srcV, int srcStrideV, uint16_t* dstUV, int dstStrideUV, int width, int height, int depth);
void SplitUVPlane_16(const uint16_t* srcUV, int srcStrideUV, uint16_t* dstU, int dstStrideU, uint16_t* dstV, int dstStrideV, int width, int height, int depth);
void ConvertToMSBPlane_16(const uint16_t* srcY, int srcStrideY, uint16_t* dstY, int dstStrideY, int width, int height, int depth);
void ConvertToLSBPlane_16(const uint16_t* srcY, int srcStrideY, uint16_t* dstY, int dstStrideY, int width, int height, int depth);

}
}

namespace WebCore {

// The libyuv functions must only be called when this returns true.
inline bool libYUVAvailable()
{
    return WebRTCProvider::webRTCAvailable();
}

}

#endif // USE(LIBWEBRTC)
