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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if PLATFORM(COCOA)

namespace WebCore {

enum class ShareableCVPixelFormat : uint8_t {
    Type_1Monochrome,
    Type_2Indexed,
    Type_4Indexed,
    Type_8Indexed,
    Type_1IndexedGray_WhiteIsZero,
    Type_2IndexedGray_WhiteIsZero,
    Type_4IndexedGray_WhiteIsZero,
    Type_8IndexedGray_WhiteIsZero,
    Type_16BE555,
    Type_16LE555,
    Type_16LE5551,
    Type_16BE565,
    Type_16LE565,
    Type_24RGB,
    Type_24BGR,
    Type_32ARGB,
    Type_32BGRA,
    Type_32ABGR,
    Type_32RGBA,
    Type_64ARGB,
    Type_64RGBALE,
    Type_48RGB,
    Type_32AlphaGray,
    Type_16Gray,
    Type_30RGB,
    Type_30RGB_r210,
    Type_422YpCbCr8,
    Type_4444YpCbCrA8,
    Type_4444YpCbCrA8R,
    Type_4444AYpCbCr8,
    Type_4444AYpCbCr16,
    Type_4444AYpCbCrFloat,
    Type_444YpCbCr8,
    Type_422YpCbCr16,
    Type_422YpCbCr10,
    Type_444YpCbCr10,
    Type_420YpCbCr8Planar,
    Type_420YpCbCr8PlanarFullRange,
    Type_422YpCbCr_4A_8BiPlanar,
    Type_420YpCbCr8BiPlanarVideoRange,
    Type_420YpCbCr8BiPlanarFullRange,
    Type_422YpCbCr8BiPlanarVideoRange,
    Type_422YpCbCr8BiPlanarFullRange,
    Type_444YpCbCr8BiPlanarVideoRange,
    Type_444YpCbCr8BiPlanarFullRange,
    Type_422YpCbCr8_yuvs,
    Type_422YpCbCr8FullRange,
    Type_OneComponent8,
    Type_TwoComponent8,
    Type_30RGBLEPackedWideGamut,
    Type_ARGB2101010LEPacked,
    Type_40ARGBLEWideGamut,
    Type_40ARGBLEWideGamutPremultiplied,
    Type_OneComponent10,
    Type_OneComponent12,
    Type_OneComponent16,
    Type_TwoComponent16,
    Type_OneComponent16Half,
    Type_OneComponent32Float,
    Type_TwoComponent16Half,
    Type_TwoComponent32Float,
    Type_64RGBAHalf,
    Type_128RGBAFloat,
    Type_14Bayer_GRBG,
    Type_14Bayer_RGGB,
    Type_14Bayer_BGGR,
    Type_14Bayer_GBRG,
    Type_DisparityFloat16,
    Type_DisparityFloat32,
    Type_DepthFloat16,
    Type_DepthFloat32,
    Type_420YpCbCr10BiPlanarVideoRange,
    Type_422YpCbCr10BiPlanarVideoRange,
    Type_444YpCbCr10BiPlanarVideoRange,
    Type_420YpCbCr10BiPlanarFullRange,
    Type_422YpCbCr10BiPlanarFullRange,
    Type_444YpCbCr10BiPlanarFullRange,
    Type_420YpCbCr8VideoRange_8A_TriPlanar,
    Type_16VersatileBayer,
    Type_96VersatileBayerPacked12,
    Type_64RGBA_DownscaledProResRAW,
    Type_422YpCbCr16BiPlanarVideoRange,
    Type_444YpCbCr16BiPlanarVideoRange,
    Type_444YpCbCr16VideoRange_16A_TriPlanar,
    Type_30RGBLE_8A_BiPlanar,
    Type_Lossless_32BGRA,
    Type_Lossless_64RGBAHalf,
    Type_Lossless_420YpCbCr8BiPlanarVideoRange,
    Type_Lossless_420YpCbCr8BiPlanarFullRange,
    Type_Lossless_420YpCbCr10PackedBiPlanarVideoRange,
    Type_Lossless_422YpCbCr10PackedBiPlanarVideoRange,
    Type_Lossless_420YpCbCr10PackedBiPlanarFullRange,
    Type_Lossless_30RGBLE_8A_BiPlanar,
    Type_Lossless_30RGBLEPackedWideGamut,
    Type_Lossy_32BGRA,
    Type_Lossy_420YpCbCr8BiPlanarVideoRange,
    Type_Lossy_420YpCbCr8BiPlanarFullRange,
    Type_Lossy_420YpCbCr10PackedBiPlanarVideoRange,
    Type_Lossy_422YpCbCr10PackedBiPlanarVideoRange,
};

ShareableCVPixelFormat fromCVPixelFormat(OSType);
OSType toCVPixelFormat(ShareableCVPixelFormat);

} // namespace WebCore

#endif
