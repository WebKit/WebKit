/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

DECLARE_SYSTEM_HEADER

#include <CoreVideo/CoreVideo.h>

#if USE(APPLE_INTERNAL_SDK)
#include <CoreVideo/CVPixelBufferPrivate.h>
#else

#if HAVE(COREVIDEO_COMPRESSED_PIXEL_FORMAT_TYPES)
enum {
    kCVPixelFormatType_AGX_420YpCbCr8BiPlanarVideoRange = '&8v0', // FIXME: Use kCVPixelFormatType_Lossless_420YpCbCr8BiPlanarVideoRange.
    kCVPixelFormatType_AGX_420YpCbCr8BiPlanarFullRange  = '&8f0', // FIXME: Use kCVPixelFormatType_Lossless_420YpCbCr8BiPlanarFullRange.

    kCVPixelFormatType_AGX_30RGBLEPackedWideGamut       = '&w3r', // FIXME: Use kCVPixelFormatType_Lossless_30RGBLEPackedWideGamut
    kCVPixelFormatType_AGX_30RGBLE_8A_BiPlanar          = '&b38', // FIXME: Use kCVPixelFormatType_Lossless_30RGBLE_8A_BiPlanar
};
#endif

#if !HAVE(CVPIXELFORMATTYPE_30RGBLE_8A_BIPLANAR)
enum {
    kCVPixelFormatType_30RGBLE_8A_BiPlanar = 'b3a8',
};
#endif

#endif // USE(APPLE_INTERNAL_SDK)
