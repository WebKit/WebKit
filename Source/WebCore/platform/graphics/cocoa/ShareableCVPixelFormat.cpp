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

#include "config.h"
#include "ShareableCVPixelFormat.h"

#if PLATFORM(COCOA)

#include <CoreVideo/CoreVideo.h>

namespace WebCore {

ShareableCVPixelFormat fromCVPixelFormat(OSType pixelFormat)
{
    switch (pixelFormat) {
    case kCVPixelFormatType_1Monochrome:
        return ShareableCVPixelFormat::Type_1Monochrome;
    case kCVPixelFormatType_2Indexed:
        return ShareableCVPixelFormat::Type_2Indexed;
    case kCVPixelFormatType_4Indexed:
        return ShareableCVPixelFormat::Type_4Indexed;
    case kCVPixelFormatType_8Indexed:
        return ShareableCVPixelFormat::Type_8Indexed;
    case kCVPixelFormatType_1IndexedGray_WhiteIsZero:
        return ShareableCVPixelFormat::Type_1IndexedGray_WhiteIsZero;
    case kCVPixelFormatType_2IndexedGray_WhiteIsZero:
        return ShareableCVPixelFormat::Type_2IndexedGray_WhiteIsZero;
    case kCVPixelFormatType_4IndexedGray_WhiteIsZero:
        return ShareableCVPixelFormat::Type_4IndexedGray_WhiteIsZero;
    case kCVPixelFormatType_8IndexedGray_WhiteIsZero:
        return ShareableCVPixelFormat::Type_8IndexedGray_WhiteIsZero;
    case kCVPixelFormatType_16BE555:
        return ShareableCVPixelFormat::Type_16BE555;
    case kCVPixelFormatType_16LE555:
        return ShareableCVPixelFormat::Type_16LE555;
    case kCVPixelFormatType_16LE5551:
        return ShareableCVPixelFormat::Type_16LE5551;
    case kCVPixelFormatType_16BE565:
        return ShareableCVPixelFormat::Type_16BE565;
    case kCVPixelFormatType_16LE565:
        return ShareableCVPixelFormat::Type_16LE565;
    case kCVPixelFormatType_24RGB:
        return ShareableCVPixelFormat::Type_24RGB;
    case kCVPixelFormatType_24BGR:
        return ShareableCVPixelFormat::Type_24BGR;
    case kCVPixelFormatType_32ARGB:
        return ShareableCVPixelFormat::Type_32ARGB;
    case kCVPixelFormatType_32BGRA:
        return ShareableCVPixelFormat::Type_32BGRA;
    case kCVPixelFormatType_32ABGR:
        return ShareableCVPixelFormat::Type_32ABGR;
    case kCVPixelFormatType_32RGBA:
        return ShareableCVPixelFormat::Type_32RGBA;
    case kCVPixelFormatType_64ARGB:
        return ShareableCVPixelFormat::Type_64ARGB;
    case kCVPixelFormatType_64RGBALE:
        return ShareableCVPixelFormat::Type_64RGBALE;
    case kCVPixelFormatType_48RGB:
        return ShareableCVPixelFormat::Type_48RGB;
    case kCVPixelFormatType_32AlphaGray:
        return ShareableCVPixelFormat::Type_32AlphaGray;
    case kCVPixelFormatType_16Gray:
        return ShareableCVPixelFormat::Type_16Gray;
    case kCVPixelFormatType_30RGB:
        return ShareableCVPixelFormat::Type_30RGB;
    case kCVPixelFormatType_30RGB_r210:
        return ShareableCVPixelFormat::Type_30RGB_r210;
    case kCVPixelFormatType_422YpCbCr8:
        return ShareableCVPixelFormat::Type_422YpCbCr8;
    case kCVPixelFormatType_4444YpCbCrA8:
        return ShareableCVPixelFormat::Type_4444YpCbCrA8;
    case kCVPixelFormatType_4444YpCbCrA8R:
        return ShareableCVPixelFormat::Type_4444YpCbCrA8R;
    case kCVPixelFormatType_4444AYpCbCr8:
        return ShareableCVPixelFormat::Type_4444AYpCbCr8;
    case kCVPixelFormatType_4444AYpCbCr16:
        return ShareableCVPixelFormat::Type_4444AYpCbCr16;
    case kCVPixelFormatType_4444AYpCbCrFloat:
        return ShareableCVPixelFormat::Type_4444AYpCbCrFloat;
    case kCVPixelFormatType_444YpCbCr8:
        return ShareableCVPixelFormat::Type_444YpCbCr8;
    case kCVPixelFormatType_422YpCbCr16:
        return ShareableCVPixelFormat::Type_422YpCbCr16;
    case kCVPixelFormatType_422YpCbCr10:
        return ShareableCVPixelFormat::Type_422YpCbCr10;
    case kCVPixelFormatType_444YpCbCr10:
        return ShareableCVPixelFormat::Type_444YpCbCr10;
    case kCVPixelFormatType_420YpCbCr8Planar:
        return ShareableCVPixelFormat::Type_420YpCbCr8Planar;
    case kCVPixelFormatType_420YpCbCr8PlanarFullRange:
        return ShareableCVPixelFormat::Type_420YpCbCr8PlanarFullRange;
    case kCVPixelFormatType_422YpCbCr_4A_8BiPlanar:
        return ShareableCVPixelFormat::Type_422YpCbCr_4A_8BiPlanar;
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        return ShareableCVPixelFormat::Type_420YpCbCr8BiPlanarVideoRange;
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
        return ShareableCVPixelFormat::Type_420YpCbCr8BiPlanarFullRange;
    case kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange:
        return ShareableCVPixelFormat::Type_422YpCbCr8BiPlanarVideoRange;
    case kCVPixelFormatType_422YpCbCr8BiPlanarFullRange:
        return ShareableCVPixelFormat::Type_422YpCbCr8BiPlanarFullRange;
    case kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange:
        return ShareableCVPixelFormat::Type_444YpCbCr8BiPlanarVideoRange;
    case kCVPixelFormatType_444YpCbCr8BiPlanarFullRange:
        return ShareableCVPixelFormat::Type_444YpCbCr8BiPlanarFullRange;
    case kCVPixelFormatType_422YpCbCr8_yuvs:
        return ShareableCVPixelFormat::Type_422YpCbCr8_yuvs;
    case kCVPixelFormatType_422YpCbCr8FullRange:
        return ShareableCVPixelFormat::Type_422YpCbCr8FullRange;
    case kCVPixelFormatType_OneComponent8:
        return ShareableCVPixelFormat::Type_OneComponent8;
    case kCVPixelFormatType_TwoComponent8:
        return ShareableCVPixelFormat::Type_TwoComponent8;
    case kCVPixelFormatType_30RGBLEPackedWideGamut:
        return ShareableCVPixelFormat::Type_30RGBLEPackedWideGamut;
    case kCVPixelFormatType_ARGB2101010LEPacked:
        return ShareableCVPixelFormat::Type_ARGB2101010LEPacked;
    case kCVPixelFormatType_40ARGBLEWideGamut:
        return ShareableCVPixelFormat::Type_40ARGBLEWideGamut;
    case kCVPixelFormatType_40ARGBLEWideGamutPremultiplied:
        return ShareableCVPixelFormat::Type_40ARGBLEWideGamutPremultiplied;
    case kCVPixelFormatType_OneComponent10:
        return ShareableCVPixelFormat::Type_OneComponent10;
    case kCVPixelFormatType_OneComponent12:
        return ShareableCVPixelFormat::Type_OneComponent12;
    case kCVPixelFormatType_OneComponent16:
        return ShareableCVPixelFormat::Type_OneComponent16;
    case kCVPixelFormatType_TwoComponent16:
        return ShareableCVPixelFormat::Type_TwoComponent16;
    case kCVPixelFormatType_OneComponent16Half:
        return ShareableCVPixelFormat::Type_OneComponent16Half;
    case kCVPixelFormatType_OneComponent32Float:
        return ShareableCVPixelFormat::Type_OneComponent32Float;
    case kCVPixelFormatType_TwoComponent16Half:
        return ShareableCVPixelFormat::Type_TwoComponent16Half;
    case kCVPixelFormatType_TwoComponent32Float:
        return ShareableCVPixelFormat::Type_TwoComponent32Float;
    case kCVPixelFormatType_64RGBAHalf:
        return ShareableCVPixelFormat::Type_64RGBAHalf;
    case kCVPixelFormatType_128RGBAFloat:
        return ShareableCVPixelFormat::Type_128RGBAFloat;
    case kCVPixelFormatType_14Bayer_GRBG:
        return ShareableCVPixelFormat::Type_14Bayer_GRBG;
    case kCVPixelFormatType_14Bayer_RGGB:
        return ShareableCVPixelFormat::Type_14Bayer_RGGB;
    case kCVPixelFormatType_14Bayer_BGGR:
        return ShareableCVPixelFormat::Type_14Bayer_BGGR;
    case kCVPixelFormatType_14Bayer_GBRG:
        return ShareableCVPixelFormat::Type_14Bayer_GBRG;
    case kCVPixelFormatType_DisparityFloat16:
        return ShareableCVPixelFormat::Type_DisparityFloat16;
    case kCVPixelFormatType_DisparityFloat32:
        return ShareableCVPixelFormat::Type_DisparityFloat32;
    case kCVPixelFormatType_DepthFloat16:
        return ShareableCVPixelFormat::Type_DepthFloat16;
    case kCVPixelFormatType_DepthFloat32:
        return ShareableCVPixelFormat::Type_DepthFloat32;
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
        return ShareableCVPixelFormat::Type_420YpCbCr10BiPlanarVideoRange;
    case kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange:
        return ShareableCVPixelFormat::Type_422YpCbCr10BiPlanarVideoRange;
    case kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange:
        return ShareableCVPixelFormat::Type_444YpCbCr10BiPlanarVideoRange;
    case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
        return ShareableCVPixelFormat::Type_420YpCbCr10BiPlanarFullRange;
    case kCVPixelFormatType_422YpCbCr10BiPlanarFullRange:
        return ShareableCVPixelFormat::Type_422YpCbCr10BiPlanarFullRange;
    case kCVPixelFormatType_444YpCbCr10BiPlanarFullRange:
        return ShareableCVPixelFormat::Type_444YpCbCr10BiPlanarFullRange;
    case kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar:
        return ShareableCVPixelFormat::Type_420YpCbCr8VideoRange_8A_TriPlanar;
    case kCVPixelFormatType_16VersatileBayer:
        return ShareableCVPixelFormat::Type_16VersatileBayer;
    case kCVPixelFormatType_96VersatileBayerPacked12:
        return ShareableCVPixelFormat::Type_96VersatileBayerPacked12;
    case kCVPixelFormatType_64RGBA_DownscaledProResRAW:
        return ShareableCVPixelFormat::Type_64RGBA_DownscaledProResRAW;
    case kCVPixelFormatType_422YpCbCr16BiPlanarVideoRange:
        return ShareableCVPixelFormat::Type_422YpCbCr16BiPlanarVideoRange;
    case kCVPixelFormatType_444YpCbCr16BiPlanarVideoRange:
        return ShareableCVPixelFormat::Type_444YpCbCr16BiPlanarVideoRange;
    case kCVPixelFormatType_444YpCbCr16VideoRange_16A_TriPlanar:
        return ShareableCVPixelFormat::Type_444YpCbCr16VideoRange_16A_TriPlanar;
    case kCVPixelFormatType_30RGBLE_8A_BiPlanar:
        return ShareableCVPixelFormat::Type_30RGBLE_8A_BiPlanar;
    case kCVPixelFormatType_Lossless_32BGRA:
        return ShareableCVPixelFormat::Type_Lossless_32BGRA;
    case kCVPixelFormatType_Lossless_64RGBAHalf:
        return ShareableCVPixelFormat::Type_Lossless_64RGBAHalf;
    case kCVPixelFormatType_Lossless_420YpCbCr8BiPlanarVideoRange:
        return ShareableCVPixelFormat::Type_Lossless_420YpCbCr8BiPlanarVideoRange;
    case kCVPixelFormatType_Lossless_420YpCbCr8BiPlanarFullRange:
        return ShareableCVPixelFormat::Type_Lossless_420YpCbCr8BiPlanarFullRange;
    case kCVPixelFormatType_Lossless_420YpCbCr10PackedBiPlanarVideoRange:
        return ShareableCVPixelFormat::Type_Lossless_420YpCbCr10PackedBiPlanarVideoRange;
    case kCVPixelFormatType_Lossless_422YpCbCr10PackedBiPlanarVideoRange:
        return ShareableCVPixelFormat::Type_Lossless_422YpCbCr10PackedBiPlanarVideoRange;
    case kCVPixelFormatType_Lossless_420YpCbCr10PackedBiPlanarFullRange:
        return ShareableCVPixelFormat::Type_Lossless_420YpCbCr10PackedBiPlanarFullRange;
    case kCVPixelFormatType_Lossless_30RGBLE_8A_BiPlanar:
        return ShareableCVPixelFormat::Type_Lossless_30RGBLE_8A_BiPlanar;
    case kCVPixelFormatType_Lossless_30RGBLEPackedWideGamut:
        return ShareableCVPixelFormat::Type_Lossless_30RGBLEPackedWideGamut;
    case kCVPixelFormatType_Lossy_32BGRA:
        return ShareableCVPixelFormat::Type_Lossy_32BGRA;
    case kCVPixelFormatType_Lossy_420YpCbCr8BiPlanarVideoRange:
        return ShareableCVPixelFormat::Type_Lossy_420YpCbCr8BiPlanarVideoRange;
    case kCVPixelFormatType_Lossy_420YpCbCr8BiPlanarFullRange:
        return ShareableCVPixelFormat::Type_Lossy_420YpCbCr8BiPlanarFullRange;
    case kCVPixelFormatType_Lossy_420YpCbCr10PackedBiPlanarVideoRange:
        return ShareableCVPixelFormat::Type_Lossy_420YpCbCr10PackedBiPlanarVideoRange;
    case kCVPixelFormatType_Lossy_422YpCbCr10PackedBiPlanarVideoRange:
        return ShareableCVPixelFormat::Type_Lossy_422YpCbCr10PackedBiPlanarVideoRange;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

OSType toCVPixelFormat(ShareableCVPixelFormat pixelFormat)
{
    switch (pixelFormat) {
    case ShareableCVPixelFormat::Type_1Monochrome:
        return kCVPixelFormatType_1Monochrome;
    case ShareableCVPixelFormat::Type_2Indexed:
        return kCVPixelFormatType_2Indexed;
    case ShareableCVPixelFormat::Type_4Indexed:
        return kCVPixelFormatType_4Indexed;
    case ShareableCVPixelFormat::Type_8Indexed:
        return kCVPixelFormatType_8Indexed;
    case ShareableCVPixelFormat::Type_1IndexedGray_WhiteIsZero:
        return kCVPixelFormatType_1IndexedGray_WhiteIsZero;
    case ShareableCVPixelFormat::Type_2IndexedGray_WhiteIsZero:
        return kCVPixelFormatType_2IndexedGray_WhiteIsZero;
    case ShareableCVPixelFormat::Type_4IndexedGray_WhiteIsZero:
        return kCVPixelFormatType_4IndexedGray_WhiteIsZero;
    case ShareableCVPixelFormat::Type_8IndexedGray_WhiteIsZero:
        return kCVPixelFormatType_8IndexedGray_WhiteIsZero;
    case ShareableCVPixelFormat::Type_16BE555:
        return kCVPixelFormatType_16BE555;
    case ShareableCVPixelFormat::Type_16LE555:
        return kCVPixelFormatType_16LE555;
    case ShareableCVPixelFormat::Type_16LE5551:
        return kCVPixelFormatType_16LE5551;
    case ShareableCVPixelFormat::Type_16BE565:
        return kCVPixelFormatType_16BE565;
    case ShareableCVPixelFormat::Type_16LE565:
        return kCVPixelFormatType_16LE565;
    case ShareableCVPixelFormat::Type_24RGB:
        return kCVPixelFormatType_24RGB;
    case ShareableCVPixelFormat::Type_24BGR:
        return kCVPixelFormatType_24BGR;
    case ShareableCVPixelFormat::Type_32ARGB:
        return kCVPixelFormatType_32ARGB;
    case ShareableCVPixelFormat::Type_32BGRA:
        return kCVPixelFormatType_32BGRA;
    case ShareableCVPixelFormat::Type_32ABGR:
        return kCVPixelFormatType_32ABGR;
    case ShareableCVPixelFormat::Type_32RGBA:
        return kCVPixelFormatType_32RGBA;
    case ShareableCVPixelFormat::Type_64ARGB:
        return kCVPixelFormatType_64ARGB;
    case ShareableCVPixelFormat::Type_64RGBALE:
        return kCVPixelFormatType_64RGBALE;
    case ShareableCVPixelFormat::Type_48RGB:
        return kCVPixelFormatType_48RGB;
    case ShareableCVPixelFormat::Type_32AlphaGray:
        return kCVPixelFormatType_32AlphaGray;
    case ShareableCVPixelFormat::Type_16Gray:
        return kCVPixelFormatType_16Gray;
    case ShareableCVPixelFormat::Type_30RGB:
        return kCVPixelFormatType_30RGB;
    case ShareableCVPixelFormat::Type_30RGB_r210:
        return kCVPixelFormatType_30RGB_r210;
    case ShareableCVPixelFormat::Type_422YpCbCr8:
        return kCVPixelFormatType_422YpCbCr8;
    case ShareableCVPixelFormat::Type_4444YpCbCrA8:
        return kCVPixelFormatType_4444YpCbCrA8;
    case ShareableCVPixelFormat::Type_4444YpCbCrA8R:
        return kCVPixelFormatType_4444YpCbCrA8R;
    case ShareableCVPixelFormat::Type_4444AYpCbCr8:
        return kCVPixelFormatType_4444AYpCbCr8;
    case ShareableCVPixelFormat::Type_4444AYpCbCr16:
        return kCVPixelFormatType_4444AYpCbCr16;
    case ShareableCVPixelFormat::Type_4444AYpCbCrFloat:
        return kCVPixelFormatType_4444AYpCbCrFloat;
    case ShareableCVPixelFormat::Type_444YpCbCr8:
        return kCVPixelFormatType_444YpCbCr8;
    case ShareableCVPixelFormat::Type_422YpCbCr16:
        return kCVPixelFormatType_422YpCbCr16;
    case ShareableCVPixelFormat::Type_422YpCbCr10:
        return kCVPixelFormatType_422YpCbCr10;
    case ShareableCVPixelFormat::Type_444YpCbCr10:
        return kCVPixelFormatType_444YpCbCr10;
    case ShareableCVPixelFormat::Type_420YpCbCr8Planar:
        return kCVPixelFormatType_420YpCbCr8Planar;
    case ShareableCVPixelFormat::Type_420YpCbCr8PlanarFullRange:
        return kCVPixelFormatType_420YpCbCr8PlanarFullRange;
    case ShareableCVPixelFormat::Type_422YpCbCr_4A_8BiPlanar:
        return kCVPixelFormatType_422YpCbCr_4A_8BiPlanar;
    case ShareableCVPixelFormat::Type_420YpCbCr8BiPlanarVideoRange:
        return kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    case ShareableCVPixelFormat::Type_420YpCbCr8BiPlanarFullRange:
        return kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
    case ShareableCVPixelFormat::Type_422YpCbCr8BiPlanarVideoRange:
        return kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange;
    case ShareableCVPixelFormat::Type_422YpCbCr8BiPlanarFullRange:
        return kCVPixelFormatType_422YpCbCr8BiPlanarFullRange;
    case ShareableCVPixelFormat::Type_444YpCbCr8BiPlanarVideoRange:
        return kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange;
    case ShareableCVPixelFormat::Type_444YpCbCr8BiPlanarFullRange:
        return kCVPixelFormatType_444YpCbCr8BiPlanarFullRange;
    case ShareableCVPixelFormat::Type_422YpCbCr8_yuvs:
        return kCVPixelFormatType_422YpCbCr8_yuvs;
    case ShareableCVPixelFormat::Type_422YpCbCr8FullRange:
        return kCVPixelFormatType_422YpCbCr8FullRange;
    case ShareableCVPixelFormat::Type_OneComponent8:
        return kCVPixelFormatType_OneComponent8;
    case ShareableCVPixelFormat::Type_TwoComponent8:
        return kCVPixelFormatType_TwoComponent8;
    case ShareableCVPixelFormat::Type_30RGBLEPackedWideGamut:
        return kCVPixelFormatType_30RGBLEPackedWideGamut;
    case ShareableCVPixelFormat::Type_ARGB2101010LEPacked:
        return kCVPixelFormatType_ARGB2101010LEPacked;
    case ShareableCVPixelFormat::Type_40ARGBLEWideGamut:
        return kCVPixelFormatType_40ARGBLEWideGamut;
    case ShareableCVPixelFormat::Type_40ARGBLEWideGamutPremultiplied:
        return kCVPixelFormatType_40ARGBLEWideGamutPremultiplied;
    case ShareableCVPixelFormat::Type_OneComponent10:
        return kCVPixelFormatType_OneComponent10;
    case ShareableCVPixelFormat::Type_OneComponent12:
        return kCVPixelFormatType_OneComponent12;
    case ShareableCVPixelFormat::Type_OneComponent16:
        return kCVPixelFormatType_OneComponent16;
    case ShareableCVPixelFormat::Type_TwoComponent16:
        return kCVPixelFormatType_TwoComponent16;
    case ShareableCVPixelFormat::Type_OneComponent16Half:
        return kCVPixelFormatType_OneComponent16Half;
    case ShareableCVPixelFormat::Type_OneComponent32Float:
        return kCVPixelFormatType_OneComponent32Float;
    case ShareableCVPixelFormat::Type_TwoComponent16Half:
        return kCVPixelFormatType_TwoComponent16Half;
    case ShareableCVPixelFormat::Type_TwoComponent32Float:
        return kCVPixelFormatType_TwoComponent32Float;
    case ShareableCVPixelFormat::Type_64RGBAHalf:
        return kCVPixelFormatType_64RGBAHalf;
    case ShareableCVPixelFormat::Type_128RGBAFloat:
        return kCVPixelFormatType_128RGBAFloat;
    case ShareableCVPixelFormat::Type_14Bayer_GRBG:
        return kCVPixelFormatType_14Bayer_GRBG;
    case ShareableCVPixelFormat::Type_14Bayer_RGGB:
        return kCVPixelFormatType_14Bayer_RGGB;
    case ShareableCVPixelFormat::Type_14Bayer_BGGR:
        return kCVPixelFormatType_14Bayer_BGGR;
    case ShareableCVPixelFormat::Type_14Bayer_GBRG:
        return kCVPixelFormatType_14Bayer_GBRG;
    case ShareableCVPixelFormat::Type_DisparityFloat16:
        return kCVPixelFormatType_DisparityFloat16;
    case ShareableCVPixelFormat::Type_DisparityFloat32:
        return kCVPixelFormatType_DisparityFloat32;
    case ShareableCVPixelFormat::Type_DepthFloat16:
        return kCVPixelFormatType_DepthFloat16;
    case ShareableCVPixelFormat::Type_DepthFloat32:
        return kCVPixelFormatType_DepthFloat32;
    case ShareableCVPixelFormat::Type_420YpCbCr10BiPlanarVideoRange:
        return kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
    case ShareableCVPixelFormat::Type_422YpCbCr10BiPlanarVideoRange:
        return kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange;
    case ShareableCVPixelFormat::Type_444YpCbCr10BiPlanarVideoRange:
        return kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange;
    case ShareableCVPixelFormat::Type_420YpCbCr10BiPlanarFullRange:
        return kCVPixelFormatType_420YpCbCr10BiPlanarFullRange;
    case ShareableCVPixelFormat::Type_422YpCbCr10BiPlanarFullRange:
        return kCVPixelFormatType_422YpCbCr10BiPlanarFullRange;
    case ShareableCVPixelFormat::Type_444YpCbCr10BiPlanarFullRange:
        return kCVPixelFormatType_444YpCbCr10BiPlanarFullRange;
    case ShareableCVPixelFormat::Type_420YpCbCr8VideoRange_8A_TriPlanar:
        return kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar;
    case ShareableCVPixelFormat::Type_16VersatileBayer:
        return kCVPixelFormatType_16VersatileBayer;
    case ShareableCVPixelFormat::Type_96VersatileBayerPacked12:
        return kCVPixelFormatType_96VersatileBayerPacked12;
    case ShareableCVPixelFormat::Type_64RGBA_DownscaledProResRAW:
        return kCVPixelFormatType_64RGBA_DownscaledProResRAW;
    case ShareableCVPixelFormat::Type_422YpCbCr16BiPlanarVideoRange:
        return kCVPixelFormatType_422YpCbCr16BiPlanarVideoRange;
    case ShareableCVPixelFormat::Type_444YpCbCr16BiPlanarVideoRange:
        return kCVPixelFormatType_444YpCbCr16BiPlanarVideoRange;
    case ShareableCVPixelFormat::Type_444YpCbCr16VideoRange_16A_TriPlanar:
        return kCVPixelFormatType_444YpCbCr16VideoRange_16A_TriPlanar;
    case ShareableCVPixelFormat::Type_30RGBLE_8A_BiPlanar:
        return kCVPixelFormatType_30RGBLE_8A_BiPlanar;
    case ShareableCVPixelFormat::Type_Lossless_32BGRA:
        return kCVPixelFormatType_Lossless_32BGRA;
    case ShareableCVPixelFormat::Type_Lossless_64RGBAHalf:
        return kCVPixelFormatType_Lossless_64RGBAHalf;
    case ShareableCVPixelFormat::Type_Lossless_420YpCbCr8BiPlanarVideoRange:
        return kCVPixelFormatType_Lossless_420YpCbCr8BiPlanarVideoRange;
    case ShareableCVPixelFormat::Type_Lossless_420YpCbCr8BiPlanarFullRange:
        return kCVPixelFormatType_Lossless_420YpCbCr8BiPlanarFullRange;
    case ShareableCVPixelFormat::Type_Lossless_420YpCbCr10PackedBiPlanarVideoRange:
        return kCVPixelFormatType_Lossless_420YpCbCr10PackedBiPlanarVideoRange;
    case ShareableCVPixelFormat::Type_Lossless_422YpCbCr10PackedBiPlanarVideoRange:
        return kCVPixelFormatType_Lossless_422YpCbCr10PackedBiPlanarVideoRange;
    case ShareableCVPixelFormat::Type_Lossless_420YpCbCr10PackedBiPlanarFullRange:
        return kCVPixelFormatType_Lossless_420YpCbCr10PackedBiPlanarFullRange;
    case ShareableCVPixelFormat::Type_Lossless_30RGBLE_8A_BiPlanar:
        return kCVPixelFormatType_Lossless_30RGBLE_8A_BiPlanar;
    case ShareableCVPixelFormat::Type_Lossless_30RGBLEPackedWideGamut:
        return kCVPixelFormatType_Lossless_30RGBLEPackedWideGamut;
    case ShareableCVPixelFormat::Type_Lossy_32BGRA:
        return kCVPixelFormatType_Lossy_32BGRA;
    case ShareableCVPixelFormat::Type_Lossy_420YpCbCr8BiPlanarVideoRange:
        return kCVPixelFormatType_Lossy_420YpCbCr8BiPlanarVideoRange;
    case ShareableCVPixelFormat::Type_Lossy_420YpCbCr8BiPlanarFullRange:
        return kCVPixelFormatType_Lossy_420YpCbCr8BiPlanarFullRange;
    case ShareableCVPixelFormat::Type_Lossy_420YpCbCr10PackedBiPlanarVideoRange:
        return kCVPixelFormatType_Lossy_420YpCbCr10PackedBiPlanarVideoRange;
    case ShareableCVPixelFormat::Type_Lossy_422YpCbCr10PackedBiPlanarVideoRange:
        return kCVPixelFormatType_Lossy_422YpCbCr10PackedBiPlanarVideoRange;
    }
}

} // namespace WebCore

#endif
