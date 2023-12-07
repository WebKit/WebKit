/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "BindGroup.h"

#import "APIConversions.h"
#import "BindGroupLayout.h"
#import "Buffer.h"
#import "Device.h"
#import "Sampler.h"
#import "TextureView.h"
#import <wtf/EnumeratedArray.h>

#if USE(APPLE_INTERNAL_SDK)
#include <CoreVideo/CVPixelBufferPrivate.h>
#else

#if HAVE(COREVIDEO_COMPRESSED_PIXEL_FORMAT_TYPES)
enum {
    kCVPixelFormatType_AGX_420YpCbCr8BiPlanarVideoRange = '&8v0',
    kCVPixelFormatType_AGX_420YpCbCr8BiPlanarFullRange = '&8f0',
};
#endif
#endif

namespace WebGPU {

static bool bufferIsPresent(const WGPUBindGroupEntry& entry)
{
    return entry.buffer;
}

static bool samplerIsPresent(const WGPUBindGroupEntry& entry)
{
    return entry.sampler;
}

static bool textureViewIsPresent(const WGPUBindGroupEntry& entry)
{
    return entry.textureView;
}

static MTLRenderStages metalRenderStage(ShaderStage shaderStage)
{
    switch (shaderStage) {
    case ShaderStage::Vertex:
        return MTLRenderStageVertex;
    case ShaderStage::Fragment:
        return MTLRenderStageFragment;
    case ShaderStage::Compute:
        return BindGroup::MTLRenderStageCompute;
    }
}

template <typename T>
using ShaderStageArray = EnumeratedArray<ShaderStage, T, ShaderStage::Compute>;

#if HAVE(COREVIDEO_METAL_SUPPORT)

enum class TransferFunctionCV {
    kITU_R_709_2,
    kITU_R_601_4,
    kITU_R_2020,
};

enum class PixelRange {
    Video,
    Full
};

static PixelRange pixelRangeFromPixelFormat(OSType pixelFormat)
{
    switch (pixelFormat) {
    case kCVPixelFormatType_4444AYpCbCr8:
    case kCVPixelFormatType_4444AYpCbCr16:
    case kCVPixelFormatType_422YpCbCr_4A_8BiPlanar:
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
    case kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange:
    case kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange:
#if HAVE(COREVIDEO_COMPRESSED_PIXEL_FORMAT_TYPES)
    case kCVPixelFormatType_AGX_420YpCbCr8BiPlanarVideoRange:
#endif
        return PixelRange::Video;
    case kCVPixelFormatType_420YpCbCr8PlanarFullRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
    case kCVPixelFormatType_422YpCbCr8FullRange:
    case kCVPixelFormatType_ARGB2101010LEPacked:
    case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
    case kCVPixelFormatType_422YpCbCr10BiPlanarFullRange:
    case kCVPixelFormatType_444YpCbCr10BiPlanarFullRange:
#if HAVE(COREVIDEO_COMPRESSED_PIXEL_FORMAT_TYPES)
    case kCVPixelFormatType_AGX_420YpCbCr8BiPlanarFullRange:
#endif
        return PixelRange::Full;
    default:
        return PixelRange::Video;
    }
}

static TransferFunctionCV transferFunctionFromString(RetainPtr<CFStringRef> string)
{
    CFStringRef cfString = string.get();
    if (!string || CFEqual(cfString, kCVImageBufferYCbCrMatrix_ITU_R_709_2))
        return TransferFunctionCV::kITU_R_709_2;
    if (CFEqual(cfString, kCVImageBufferYCbCrMatrix_ITU_R_601_4))
        return TransferFunctionCV::kITU_R_601_4;
    if (CFEqual(cfString, kCVImageBufferYCbCrMatrix_ITU_R_2020))
        return TransferFunctionCV::kITU_R_2020;

    ASSERT_NOT_REACHED("Unexpected transfer function format");
    return TransferFunctionCV::kITU_R_709_2;
}

static simd::float4x3 colorSpaceConversionMatrixForPixelBuffer(CVPixelBufferRef pixelBuffer)
{
    auto format = CVPixelBufferGetPixelFormatType(pixelBuffer);
    auto range = pixelRangeFromPixelFormat(format);
    auto transferFunction = transferFunctionFromString(adoptCF((CFStringRef)CVBufferCopyAttachment(pixelBuffer, kCVImageBufferYCbCrMatrixKey, nil)));

    switch (transferFunction) {
    case TransferFunctionCV::kITU_R_709_2: {
        switch (range) {
        case PixelRange::Full:
            return simd::float4x3(simd::make_float3(+1.00000f, +1.00000f, +1.00000f),
                simd::make_float3(-0.00012f, -0.18726f, +1.85559f),
                simd::make_float3(+1.57471f, -0.46814f, +0.00012f),
                simd::make_float3(-0.78729f, +0.32770f, -0.92786f));
        case PixelRange::Video:
            return simd::float4x3(simd::make_float3(+1.16895f, +1.16895f, +1.16895f),
                simd::make_float3(-0.00012f, -0.21399f, +2.12073f),
                simd::make_float3(+1.79968f, -0.53503f, +0.00012f),
                simd::make_float3(-0.97284f, +0.30145f, -1.13348f));
        }
    }

    case TransferFunctionCV::kITU_R_601_4: {
        switch (range) {
        case PixelRange::Full:
            return simd::float4x3(simd::make_float3(+1.00000f, +1.00000f, +1.00000f),
                simd::make_float3(-0.00100f, -0.34375f, +1.77221f),
                simd::make_float3(+1.40173f, -0.71411f, +0.00100f),
                simd::make_float3(-0.70038f, +0.51672f, -0.88660f));

        case PixelRange::Video:
            return simd::float4x3(simd::make_float3(+1.16895f, +1.16895f, +1.16895f),
                simd::make_float3(-0.00110f, -0.39282f, +2.02527f),
                simd::make_float3(+1.60193f, -0.81616f, +0.00110f),
                simd::make_float3(-0.87347f, +0.53143f, -1.08624f));
        }
    }

    case TransferFunctionCV::kITU_R_2020: {
        switch (range) {
        case PixelRange::Full:
            return simd::float4x3(simd::make_float3(+1.00000f, +1.00000f, +1.00000f),
                simd::make_float3(+0.00000f, -0.16455f, +1.88135f),
                simd::make_float3(+1.47461f, -0.57129f, -0.00012f),
                simd::make_float3(-0.73730f, +0.36792f, -0.94061f));
        case PixelRange::Video:
            return simd::float4x3(simd::make_float3(+1.16895f, +1.16895f, +1.16895f),
                simd::make_float3(+0.00000f, -0.18799f, +2.15015f),
                simd::make_float3(+1.68530f, -0.65295f, +0.00012f),
                simd::make_float3(-0.91571f, +0.34741f, -1.14807f));
        }
    } }
}

static MTLPixelFormat metalPixelFormat(CVPixelBufferRef pixelBuffer, size_t plane)
{
    auto pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);
    auto biplanarFormat = [](int plane) {
        return plane ? MTLPixelFormatRG8Unorm : MTLPixelFormatR8Unorm;
    };

    switch (pixelFormat) {
    case kCVPixelFormatType_1Monochrome: /* 1 bit indexed */
    case kCVPixelFormatType_2Indexed: /* 2 bit indexed */
    case kCVPixelFormatType_4Indexed: /* 4 bit indexed */
    case kCVPixelFormatType_8Indexed: /* 8 bit indexed */
    case kCVPixelFormatType_1IndexedGray_WhiteIsZero: /* 1 bit indexed gray, white is zero */
    case kCVPixelFormatType_2IndexedGray_WhiteIsZero: /* 2 bit indexed gray, white is zero */
    case kCVPixelFormatType_4IndexedGray_WhiteIsZero: /* 4 bit indexed gray, white is zero */
    case kCVPixelFormatType_8IndexedGray_WhiteIsZero: /* 8 bit indexed gray, white is zero */
        return MTLPixelFormatA8Unorm;

    case kCVPixelFormatType_16BE555: /* 16 bit BE RGB 555 */
    case kCVPixelFormatType_16LE555:     /* 16 bit LE RGB 555 */
    case kCVPixelFormatType_16LE5551:     /* 16 bit LE RGB 5551 */
        return MTLPixelFormatBGR5A1Unorm;

    case kCVPixelFormatType_16BE565:     /* 16 bit BE RGB 565 */
    case kCVPixelFormatType_16LE565:     /* 16 bit LE RGB 565 */
        return MTLPixelFormatB5G6R5Unorm;

    case kCVPixelFormatType_24RGB: /* 24 bit RGB */
    case kCVPixelFormatType_24BGR:     /* 24 bit BGR */
    case kCVPixelFormatType_32ARGB: /* 32 bit ARGB */
    case kCVPixelFormatType_32BGRA:     /* 32 bit BGRA */
        return MTLPixelFormatBGRA8Unorm;
    case kCVPixelFormatType_32ABGR:     /* 32 bit ABGR */
    case kCVPixelFormatType_32RGBA:     /* 32 bit RGBA */
        return MTLPixelFormatRGBA8Unorm;
    case kCVPixelFormatType_64ARGB:     /* 64 bit ARGB, 16-bit big-endian samples */
    case kCVPixelFormatType_64RGBALE:     /* 64 bit RGBA, 16-bit little-endian full-range (0-65535) samples */
        return MTLPixelFormatRGBA16Unorm;

    case kCVPixelFormatType_48RGB:     /* 48 bit RGB, 16-bit big-endian samples */
        ASSERT_NOT_REACHED();
        return MTLPixelFormatBGRA8Unorm;

    case kCVPixelFormatType_32AlphaGray:     /* 32 bit AlphaGray, 16-bit big-endian samples, black is zero */
        return MTLPixelFormatR32Float;

    case kCVPixelFormatType_16Gray:     /* 16 bit Grayscale, 16-bit big-endian samples, black is zero */
        return MTLPixelFormatR16Float;

    case kCVPixelFormatType_30RGB:     /* 30 bit RGB, 10-bit big-endian samples, 2 unused padding bits (at least significant end). */
        return MTLPixelFormatBGR10A2Unorm;

    case kCVPixelFormatType_422YpCbCr8:     /* Component Y'CbCr 8-bit 4:2:2, ordered Cb Y'0 Cr Y'1 */
        return biplanarFormat(plane);

    case kCVPixelFormatType_4444YpCbCrA8:     /* Component Y'CbCrA 8-bit 4:4:4:4, ordered Cb Y' Cr A */
    case kCVPixelFormatType_4444YpCbCrA8R:     /* Component Y'CbCrA 8-bit 4:4:4:4, rendering format. full range alpha, zero biased YUV, ordered A Y' Cb Cr */
    case kCVPixelFormatType_4444AYpCbCr8:     /* Component Y'CbCrA 8-bit 4:4:4:4, ordered A Y' Cb Cr, full range alpha, video range Y'CbCr. */
    case kCVPixelFormatType_4444AYpCbCr16:     /* Component Y'CbCrA 16-bit 4:4:4:4, ordered A Y' Cb Cr, full range alpha, video range Y'CbCr, 16-bit little-endian samples. */
        ASSERT_NOT_REACHED();
        return MTLPixelFormatBGRA8Unorm;

    case kCVPixelFormatType_444YpCbCr8:     /* Component Y'CbCr 8-bit 4:4:4, ordered Cr Y' Cb, video range Y'CbCr */
        return biplanarFormat(plane);

    case kCVPixelFormatType_422YpCbCr16:     /* Component Y'CbCr 10,12,14,16-bit 4:2:2 */
        ASSERT_NOT_REACHED();
        return biplanarFormat(plane);

    case kCVPixelFormatType_422YpCbCr10:     /* Component Y'CbCr 10-bit 4:2:2 */
        return biplanarFormat(plane);

    case kCVPixelFormatType_444YpCbCr10:     /* Component Y'CbCr 10-bit 4:4:4 */
        return biplanarFormat(plane);

    case kCVPixelFormatType_420YpCbCr8Planar:   /* Planar Component Y'CbCr 8-bit 4:2:0.  baseAddr points to a big-endian CVPlanarPixelBufferInfo_YCbCrPlanar struct */
        return biplanarFormat(plane);

    case kCVPixelFormatType_420YpCbCr8PlanarFullRange:   /* Planar Component Y'CbCr 8-bit 4:2:0, full range.  baseAddr points to a big-endian CVPlanarPixelBufferInfo_YCbCrPlanar struct */
    case kCVPixelFormatType_422YpCbCr_4A_8BiPlanar: /* First plane: Video-range Component Y'CbCr 8-bit 4:2:2, ordered Cb Y'0 Cr Y'1; second plane: alpha 8-bit 0-255 */
        return biplanarFormat(plane);

    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: /* Bi-Planar Component Y'CbCr 8-bit 4:2:0, video-range (luma=[16,235] chroma=[16,240]).  baseAddr points to a big-endian CVPlanarPixelBufferInfo_YCbCrBiPlanar struct */
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange: /* Bi-Planar Component Y'CbCr 8-bit 4:2:0, full-range (luma=[0,255] chroma=[1,255]).  baseAddr points to a big-endian CVPlanarPixelBufferInfo_YCbCrBiPlanar struct */
        return biplanarFormat(plane);

    case kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange: /* Bi-Planar Component Y'CbCr 8-bit 4:2:2, video-range (luma=[16,235] chroma=[16,240]).  baseAddr points to a big-endian CVPlanarPixelBufferInfo_YCbCrBiPlanar struct */
    case kCVPixelFormatType_422YpCbCr8BiPlanarFullRange: /* Bi-Planar Component Y'CbCr 8-bit 4:2:2, full-range (luma=[0,255] chroma=[1,255]).  baseAddr points to a big-endian CVPlanarPixelBufferInfo_YCbCrBiPlanar struct */
        return biplanarFormat(plane);

    case kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange: /* Bi-Planar Component Y'CbCr 8-bit 4:4:4, video-range (luma=[16,235] chroma=[16,240]).  baseAddr points to a big-endian CVPlanarPixelBufferInfo_YCbCrBiPlanar struct */
    case kCVPixelFormatType_444YpCbCr8BiPlanarFullRange: /* Bi-Planar Component Y'CbCr 8-bit 4:4:4, full-range (luma=[0,255] chroma=[1,255]).  baseAddr points to a big-endian CVPlanarPixelBufferInfo_YCbCrBiPlanar struct */
        return biplanarFormat(plane);

    case kCVPixelFormatType_422YpCbCr8_yuvs:     /* Component Y'CbCr 8-bit 4:2:2, ordered Y'0 Cb Y'1 Cr */
    case kCVPixelFormatType_422YpCbCr8FullRange: /* Component Y'CbCr 8-bit 4:2:2, full range, ordered Y'0 Cb Y'1 Cr */
        return biplanarFormat(plane);

    case kCVPixelFormatType_OneComponent8:     /* 8 bit one component, black is zero */
        return MTLPixelFormatR8Unorm;

    case kCVPixelFormatType_TwoComponent8:     /* 8 bit two component, black is zero */
        return MTLPixelFormatRG8Unorm;

    case kCVPixelFormatType_30RGBLEPackedWideGamut: /* little-endian RGB101010, 2 MSB are zero, wide-gamut (384-895) */
    case kCVPixelFormatType_ARGB2101010LEPacked:     /* little-endian ARGB2101010 full-range ARGB */
        return MTLPixelFormatRGB10A2Unorm;

    case kCVPixelFormatType_40ARGBLEWideGamut: /* little-endian ARGB10101010, each 10 bits in the MSBs of 16bits, wide-gamut (384-895, including alpha) */
    case kCVPixelFormatType_40ARGBLEWideGamutPremultiplied: /* little-endian ARGB10101010, each 10 bits in the MSBs of 16bits, wide-gamut (384-895, including alpha). Alpha premultiplied */
        return MTLPixelFormatInvalid;

    case kCVPixelFormatType_OneComponent10:     /* 10 bit little-endian one component, stored as 10 MSBs of 16 bits, black is zero */
        return MTLPixelFormatInvalid;

    case kCVPixelFormatType_OneComponent12:     /* 12 bit little-endian one component, stored as 12 MSBs of 16 bits, black is zero */
        return MTLPixelFormatInvalid;

    case kCVPixelFormatType_OneComponent16:     /* 16 bit little-endian one component, black is zero */
        return MTLPixelFormatR16Unorm;

    case kCVPixelFormatType_TwoComponent16:     /* 16 bit little-endian two component, black is zero */
        return MTLPixelFormatRG16Unorm;

    case kCVPixelFormatType_OneComponent16Half:     /* 16 bit one component IEEE half-precision float, 16-bit little-endian samples */
        return MTLPixelFormatR16Float;

    case kCVPixelFormatType_OneComponent32Float:     /* 32 bit one component IEEE float, 32-bit little-endian samples */
        return MTLPixelFormatR32Float;

    case kCVPixelFormatType_TwoComponent16Half:     /* 16 bit two component IEEE half-precision float, 16-bit little-endian samples */
        return MTLPixelFormatRG16Float;

    case kCVPixelFormatType_TwoComponent32Float:     /* 32 bit two component IEEE float, 32-bit little-endian samples */
        return MTLPixelFormatRG32Float;

    case kCVPixelFormatType_64RGBAHalf:     /* 64 bit RGBA IEEE half-precision float, 16-bit little-endian samples */
        return MTLPixelFormatRGBA16Float;

    case kCVPixelFormatType_128RGBAFloat:     /* 128 bit RGBA IEEE float, 32-bit little-endian samples */
        return MTLPixelFormatRGBA32Float;

    case kCVPixelFormatType_14Bayer_GRBG:     /* Bayer 14-bit Little-Endian, packed in 16-bits, ordered G R G R... alternating with B G B G... */
    case kCVPixelFormatType_14Bayer_RGGB:     /* Bayer 14-bit Little-Endian, packed in 16-bits, ordered R G R G... alternating with G B G B... */
    case kCVPixelFormatType_14Bayer_BGGR:     /* Bayer 14-bit Little-Endian, packed in 16-bits, ordered B G B G... alternating with G R G R... */
    case kCVPixelFormatType_14Bayer_GBRG:     /* Bayer 14-bit Little-Endian, packed in 16-bits, ordered G B G B... alternating with R G R G... */
    case kCVPixelFormatType_DisparityFloat16:     /* IEEE754-2008 binary16 (half float), describing the normalized shift when comparing two images. Units are 1/meters: ( pixelShift / (pixelFocalLength * baselineInMeters) ) */
    case kCVPixelFormatType_DisparityFloat32:     /* IEEE754-2008 binary32 float, describing the normalized shift when comparing two images. Units are 1/meters: ( pixelShift / (pixelFocalLength * baselineInMeters) ) */
        ASSERT_NOT_REACHED();
        return MTLPixelFormatBGRA8Unorm;

    case kCVPixelFormatType_DepthFloat16:     /* IEEE754-2008 binary16 (half float), describing the depth (distance to an object) in meters */
        return MTLPixelFormatDepth16Unorm;
    case kCVPixelFormatType_DepthFloat32:     /* IEEE754-2008 binary32 float, describing the depth (distance to an object) in meters */
        return MTLPixelFormatDepth32Float;

    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange: /* 2 plane YCbCr10 4:2:0, each 10 bits in the MSBs of 16bits, video-range (luma=[64,940] chroma=[64,960]) */
        return biplanarFormat(plane);
    case kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange: /* 2 plane YCbCr10 4:2:2, each 10 bits in the MSBs of 16bits, video-range (luma=[64,940] chroma=[64,960]) */
        return biplanarFormat(plane);
    case kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange: /* 2 plane YCbCr10 4:4:4, each 10 bits in the MSBs of 16bits, video-range (luma=[64,940] chroma=[64,960]) */
        return biplanarFormat(plane);
    case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange: /* 2 plane YCbCr10 4:2:0, each 10 bits in the MSBs of 16bits, full-range (Y range 0-1023) */
        return biplanarFormat(plane);
    case kCVPixelFormatType_422YpCbCr10BiPlanarFullRange: /* 2 plane YCbCr10 4:2:2, each 10 bits in the MSBs of 16bits, full-range (Y range 0-1023) */
        return biplanarFormat(plane);
    case kCVPixelFormatType_444YpCbCr10BiPlanarFullRange: /* 2 plane YCbCr10 4:4:4, each 10 bits in the MSBs of 16bits, full-range (Y range 0-1023) */
        return biplanarFormat(plane);
    case kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar: /* first and second planes as per 420YpCbCr8BiPlanarVideoRange (420v), alpha 8 bits in third plane full-range.  No CVPlanarPixelBufferInfo struct. */
        return biplanarFormat(plane);

    case kCVPixelFormatType_16VersatileBayer:   /* Single plane Bayer 16-bit little-endian sensor element ("sensel") samples from full-size decoding of ProRes RAW images; Bayer pattern (sensel ordering) and other raw conversion information is described via buffer attachments */
    case kCVPixelFormatType_64RGBA_DownscaledProResRAW:   /* Single plane 64-bit RGBA (16-bit little-endian samples) from downscaled decoding of ProRes RAW images; components--which may not be co-sited with one another--are sensel values and require raw conversion, information for which is described via buffer attachments */
        ASSERT_NOT_REACHED();
        return MTLPixelFormatBGRA8Unorm;

    case kCVPixelFormatType_422YpCbCr16BiPlanarVideoRange: /* 2 plane YCbCr16 4:2:2, video-range (luma=[4096,60160] chroma=[4096,61440]) */
        ASSERT_NOT_REACHED();
        return MTLPixelFormatInvalid;

    case kCVPixelFormatType_444YpCbCr16BiPlanarVideoRange: /* 2 plane YCbCr16 4:4:4, video-range (luma=[4096,60160] chroma=[4096,61440]) */
    case kCVPixelFormatType_444YpCbCr16VideoRange_16A_TriPlanar: /* 3 plane video-range YCbCr16 4:4:4 with 16-bit full-range alpha (luma=[4096,60160] chroma=[4096,61440] alpha=[0,65535]).  No CVPlanarPixelBufferInfo struct. */
        ASSERT_NOT_REACHED();
        return MTLPixelFormatInvalid;

    case kCVPixelFormatType_Lossless_32BGRA: /* Lossless-compressed form of case kCVPixelFormatType_32BGRA. */
        return MTLPixelFormatBGRA8Unorm;

        // Lossless-compressed Bi-planar YCbCr pixel format types
    case kCVPixelFormatType_Lossless_420YpCbCr8BiPlanarVideoRange: /* Lossless-compressed form of case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange.  No CVPlanarPixelBufferInfo struct. */
    case kCVPixelFormatType_Lossless_420YpCbCr8BiPlanarFullRange: /* Lossless-compressed form of case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange.  No CVPlanarPixelBufferInfo struct. */
        return biplanarFormat(plane);

    case kCVPixelFormatType_Lossless_420YpCbCr10PackedBiPlanarVideoRange: /* Lossless-compressed-packed form of case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange.  No CVPlanarPixelBufferInfo struct. Format is compressed-packed with no padding bits between pixels. */
        return biplanarFormat(plane);

    case kCVPixelFormatType_Lossless_422YpCbCr10PackedBiPlanarVideoRange: /* Lossless-compressed form of case kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange.  No CVPlanarPixelBufferInfo struct. Format is compressed-packed with no padding bits between pixels. */
        return biplanarFormat(plane);

    case kCVPixelFormatType_Lossy_32BGRA: /* Lossy-compressed form of case kCVPixelFormatType_32BGRA. No CVPlanarPixelBufferInfo struct.  */
        return MTLPixelFormatBGRA8Unorm;

    case kCVPixelFormatType_Lossy_420YpCbCr8BiPlanarVideoRange: /* Lossy-compressed form of case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange.  No CVPlanarPixelBufferInfo struct. */
    case kCVPixelFormatType_Lossy_420YpCbCr8BiPlanarFullRange: /* Lossy-compressed form of case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange.  No CVPlanarPixelBufferInfo struct. */
        return biplanarFormat(plane);

    case kCVPixelFormatType_Lossy_420YpCbCr10PackedBiPlanarVideoRange: /* Lossy-compressed form of case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange.  No CVPlanarPixelBufferInfo struct. Format is compressed-packed with no padding bits between pixels. */
        return biplanarFormat(plane);

    case kCVPixelFormatType_Lossy_422YpCbCr10PackedBiPlanarVideoRange: /* Lossy-compressed form of kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange.  No CVPlanarPixelBufferInfo struct. Format is compressed-packed with no padding bits between pixels. */
        return biplanarFormat(plane);
    }

    return MTLPixelFormatInvalid;
}
#endif

Device::ExternalTextureData Device::createExternalTextureFromPixelBuffer(CVPixelBufferRef pixelBuffer, WGPUColorSpace colorSpace) const
{
#if HAVE(COREVIDEO_METAL_SUPPORT)
    UNUSED_PARAM(colorSpace);

    if (!CVPixelBufferGetIOSurface(pixelBuffer)) {
        auto planeCount = std::max<size_t>(CVPixelBufferGetPlaneCount(pixelBuffer), 1);
        if (planeCount > 2) {
            ASSERT_NOT_REACHED("non-IOSurface CVPixelBuffer instances with more than two planes are not supported");
            return { };
        }

        CVReturn status = CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
        if (status != kCVReturnSuccess)
            return { };

        id<MTLTexture> mtlTextures[2];

        for (size_t plane = 0; plane < planeCount; ++plane) {
            int width = CVPixelBufferGetWidthOfPlane(pixelBuffer, plane);
            int height = CVPixelBufferGetHeightOfPlane(pixelBuffer, plane);

            MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor new];
            textureDescriptor.usage = MTLTextureUsageShaderRead;
            textureDescriptor.textureType = MTLTextureType2D;
            textureDescriptor.width = width;
            textureDescriptor.height = height;
            textureDescriptor.pixelFormat = metalPixelFormat(pixelBuffer, plane);
            textureDescriptor.mipmapLevelCount = 1;
            textureDescriptor.sampleCount = 1;
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
            textureDescriptor.storageMode = hasUnifiedMemory() ? MTLStorageModeShared : MTLStorageModeManaged;
#else
            textureDescriptor.storageMode = hasUnifiedMemory() ? MTLStorageModeShared : MTLStorageModePrivate;
#endif

            id<MTLTexture> mtlTexture = [m_device newTextureWithDescriptor:textureDescriptor];
            if (!mtlTexture)
                return { };

            uint8_t *imageBytes = reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, plane));
            int bytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, plane);

            [mtlTexture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0 withBytes:imageBytes bytesPerRow:bytesPerRow];
            mtlTextures[plane] = mtlTexture;
        }

        CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);

        simd::float4x3 colorSpaceConversionMatrix;
        if (planeCount > 1)
            colorSpaceConversionMatrix = colorSpaceConversionMatrixForPixelBuffer(pixelBuffer);
        else {
            colorSpaceConversionMatrix = simd::float4x3(1.f);
            mtlTextures[1] = mtlTextures[0];
        }

        return { mtlTextures[0], mtlTextures[1], simd::float3x2(1.f), colorSpaceConversionMatrix };
    }

    id<MTLTexture> mtlTexture0 = nil;
    id<MTLTexture> mtlTexture1 = nil;

    CVMetalTextureRef plane0 = nullptr;
    CVMetalTextureRef plane1 = nullptr;

    CVReturn status1 = CVMetalTextureCacheCreateTextureFromImage(nullptr, m_coreVideoTextureCache.get(), pixelBuffer, nullptr, MTLPixelFormatR8Unorm, CVPixelBufferGetWidthOfPlane(pixelBuffer, 0), CVPixelBufferGetHeightOfPlane(pixelBuffer, 0), 0, &plane0);
    CVReturn status2 = CVMetalTextureCacheCreateTextureFromImage(nullptr, m_coreVideoTextureCache.get(), pixelBuffer, nullptr, MTLPixelFormatRG8Unorm, CVPixelBufferGetWidthOfPlane(pixelBuffer, 1), CVPixelBufferGetHeightOfPlane(pixelBuffer, 1), 1, &plane1);

    float lowerLeft[2];
    float lowerRight[2];
    float upperRight[2];
    float upperLeft[2];

    if (status1 == kCVReturnSuccess) {
        mtlTexture0 = CVMetalTextureGetTexture(plane0);
        CVMetalTextureGetCleanTexCoords(plane0, lowerLeft, lowerRight, upperRight, upperLeft);
    } else {
        if (plane1)
            CFRelease(plane1);
        return { };
    }

    if (status2 == kCVReturnSuccess)
        mtlTexture1 = CVMetalTextureGetTexture(plane1);

    m_defaultQueue->onSubmittedWorkDone([plane0, plane1](WGPUQueueWorkDoneStatus) {
        if (plane0)
            CFRelease(plane0);

        if (plane1)
            CFRelease(plane1);
    });

    float Ax = 1.f / (upperRight[0] - lowerLeft[0]);
    float Bx = -Ax * lowerLeft[0];
    float Ay = 1.f / (lowerRight[1] - upperLeft[1]);
    float By = -Ay * upperLeft[1];
    simd::float3x2 uvRemappingMatrix = simd::float3x2(simd::make_float2(Ax, 0.f), simd::make_float2(0.f, Ay), simd::make_float2(Bx, By));
    simd::float4x3 colorSpaceConversionMatrix = colorSpaceConversionMatrixForPixelBuffer(pixelBuffer);

    return { mtlTexture0, mtlTexture1, uvRemappingMatrix, colorSpaceConversionMatrix };
#else
    UNUSED_PARAM(pixelBuffer);
    UNUSED_PARAM(colorSpace);
    return { };
#endif
}

static MTLResourceUsage resourceUsageForBindingAcccess(BindGroupLayout::BindingAccess bindingAccess)
{
    switch (bindingAccess) {
    case BindGroupLayout::BindingAccessReadOnly:
        return MTLResourceUsageRead;
    case BindGroupLayout::BindingAccessWriteOnly:
        return MTLResourceUsageWrite;
    case BindGroupLayout::BindingAccessReadWrite:
        return MTLResourceUsageRead | MTLResourceUsageWrite;
    }
}

template <typename ExpectedType>
static bool hasBinding(const BindGroupLayout::EntriesContainer& bindGroupLayoutEntries, auto bindingIndex)
{
    auto it = bindGroupLayoutEntries.find(bindingIndex);
    RELEASE_ASSERT(it != bindGroupLayoutEntries.end());
    return std::get_if<ExpectedType>(&it->value.bindingLayout);
}

static bool is32bppFloatFormat(id<MTLTexture> t)
{
    return t.pixelFormat == MTLPixelFormatR32Float || t.pixelFormat == MTLPixelFormatRG32Float || t.pixelFormat == MTLPixelFormatRGBA32Float;
}

static bool valid32bppFloatSampleType(WGPUTextureSampleType sampleType)
{
    return sampleType == WGPUTextureSampleType_Float || sampleType == WGPUTextureSampleType_UnfilterableFloat;
}

Ref<BindGroup> Device::createBindGroup(const WGPUBindGroupDescriptor& descriptor)
{
    if (descriptor.nextInChain || !descriptor.layout || !isValid())
        return BindGroup::createInvalid(*this);

    constexpr ShaderStage stages[] = { ShaderStage::Vertex, ShaderStage::Fragment, ShaderStage::Compute };
    constexpr size_t stageCount = std::size(stages);
    const auto& bindGroupLayout = WebGPU::fromAPI(descriptor.layout);
    if (!bindGroupLayout.isValid()) {
        generateAValidationError("invalid BindGroupLayout createBindGroup"_s);
        return BindGroup::createInvalid(*this);
    }

    ShaderStageArray<id<MTLArgumentEncoder>> argumentEncoder = std::array<id<MTLArgumentEncoder>, stageCount>({ bindGroupLayout.vertexArgumentEncoder(), bindGroupLayout.fragmentArgumentEncoder(), bindGroupLayout.computeArgumentEncoder() });
    ShaderStageArray<id<MTLBuffer>> argumentBuffer;
    for (ShaderStage stage : stages) {
        auto encodedLength = bindGroupLayout.encodedLength(stage);
        argumentBuffer[stage] = encodedLength ? safeCreateBuffer(encodedLength, MTLStorageModeShared) : nil;
        [argumentEncoder[stage] setArgumentBuffer:argumentBuffer[stage] offset:0];
    }

    constexpr auto maxResourceUsageValue = MTLResourceUsageRead | MTLResourceUsageWrite;
    static_assert(maxResourceUsageValue == 3, "Code path assumes MTLResourceUsageRead | MTLResourceUsageWrite == 3");
    Vector<id<MTLResource>> stageResources[stageCount][maxResourceUsageValue];
    auto& bindGroupLayoutEntries = bindGroupLayout.entries();
    for (uint32_t i = 0, entryCount = descriptor.entryCount; i < entryCount; ++i) {
        const WGPUBindGroupEntry& entry = descriptor.entries[i];

        WGPUExternalTexture wgpuExternalTexture = nullptr;
        if (entry.nextInChain) {
            if (entry.nextInChain->sType != static_cast<WGPUSType>(WGPUSTypeExtended_BindGroupEntryExternalTexture)) {
                generateAValidationError("Unknown chain object in WGPUBindGroupEntry"_s);
                return BindGroup::createInvalid(*this);
            }
            if (entry.nextInChain->next) {
                generateAValidationError("Unknown chain object in WGPUBindGroupEntry"_s);
                return BindGroup::createInvalid(*this);
            }

            wgpuExternalTexture = reinterpret_cast<const WGPUBindGroupExternalTextureEntry*>(entry.nextInChain)->externalTexture;
        }

        bool bufferIsPresent = WebGPU::bufferIsPresent(entry);
        bool samplerIsPresent = WebGPU::samplerIsPresent(entry);
        bool textureViewIsPresent = WebGPU::textureViewIsPresent(entry);
        bool externalTextureIsPresent = static_cast<bool>(wgpuExternalTexture);
        if (bufferIsPresent + samplerIsPresent + textureViewIsPresent + externalTextureIsPresent != 1)
            return BindGroup::createInvalid(*this);

        for (ShaderStage stage : stages) {
            auto bindingIndex = entry.binding;
            auto index = bindGroupLayout.argumentBufferIndexForEntryIndex(bindingIndex, stage);
            if (index == NSNotFound)
                continue;

            auto optionalAccess = bindGroupLayout.bindingAccessForBindingIndex(bindingIndex, stage);
            RELEASE_ASSERT(optionalAccess);
            auto bufferSizeArgumentBufferIndex = bindGroupLayout.bufferSizeIndexForEntryIndex(bindingIndex, stage);
            MTLResourceUsage resourceUsage = resourceUsageForBindingAcccess(*optionalAccess);

            if (bufferIsPresent) {
                if (!hasBinding<WGPUBufferBindingLayout>(bindGroupLayoutEntries, bindingIndex)) {
                    generateAValidationError("Expected buffer but it was not present in the bind group layout"_s);
                    return BindGroup::createInvalid(*this);
                }
                id<MTLBuffer> buffer = WebGPU::fromAPI(entry.buffer).buffer();
                if (entry.offset >= buffer.length)
                    return BindGroup::createInvalid(*this);

                [argumentEncoder[stage] setBuffer:buffer offset:entry.offset atIndex:index];
                stageResources[metalRenderStage(stage)][resourceUsage - 1].append(buffer);
                if (bufferSizeArgumentBufferIndex)
                    *(uint32_t*)[argumentEncoder[stage] constantDataAtIndex:*bufferSizeArgumentBufferIndex] = static_cast<uint32_t>(entry.size == WGPU_WHOLE_MAP_SIZE ? buffer.length : entry.size);
            } else if (samplerIsPresent) {
                if (!hasBinding<WGPUSamplerBindingLayout>(bindGroupLayoutEntries, bindingIndex)) {
                    generateAValidationError("Expected sampler but it was not present in the bind group layout"_s);
                    return BindGroup::createInvalid(*this);
                }
                id<MTLSamplerState> sampler = WebGPU::fromAPI(entry.sampler).samplerState();
                [argumentEncoder[stage] setSamplerState:sampler atIndex:index];
            } else if (textureViewIsPresent) {
                auto it = bindGroupLayoutEntries.find(bindingIndex);
                RELEASE_ASSERT(it != bindGroupLayoutEntries.end());
                auto* textureEntry = std::get_if<WGPUTextureBindingLayout>(&it->value.bindingLayout);
                if (!textureEntry && !std::get_if<WGPUStorageTextureBindingLayout>(&it->value.bindingLayout)) {
                    generateAValidationError("Expected texture or storage texture but it was not present in the bind group layout"_s);
                    return BindGroup::createInvalid(*this);
                }
                id<MTLTexture> texture = WebGPU::fromAPI(entry.textureView).texture();
                if (textureEntry && is32bppFloatFormat(texture) && (!valid32bppFloatSampleType(textureEntry->sampleType) || (textureEntry->sampleType == WGPUTextureSampleType_Float && !hasFeature(WGPUFeatureName_Float32Filterable)))) {
                    generateAValidationError("Can not create bind group with filterable 32bpp floating point texture as float32-filterable feature is not enabled"_s);
                    return BindGroup::createInvalid(*this);
                }
                [argumentEncoder[stage] setTexture:texture atIndex:index];
                stageResources[metalRenderStage(stage)][resourceUsage - 1].append(texture);
            } else if (externalTextureIsPresent) {
                if (!hasBinding<WGPUExternalTextureBindingLayout>(bindGroupLayoutEntries, bindingIndex)) {
                    generateAValidationError("Expected external texture but it was not present in the bind group layout"_s);
                    return BindGroup::createInvalid(*this);
                }
                auto& externalTexture = WebGPU::fromAPI(wgpuExternalTexture);
                auto textureData = createExternalTextureFromPixelBuffer(externalTexture.pixelBuffer(), externalTexture.colorSpace());
                [argumentEncoder[stage] setTexture:textureData.texture0 atIndex:index++];
                [argumentEncoder[stage] setTexture:textureData.texture1 atIndex:index++];
                ASSERT(textureData.texture0);
                ASSERT(textureData.texture1);
                if (textureData.texture0)
                    stageResources[metalRenderStage(stage)][resourceUsage - 1].append(textureData.texture0);
                if (textureData.texture1)
                    stageResources[metalRenderStage(stage)][resourceUsage - 1].append(textureData.texture1);

                auto* uvRemapAddress = static_cast<simd::float3x2*>([argumentEncoder[stage] constantDataAtIndex:index++]);
                *uvRemapAddress = textureData.uvRemappingMatrix;

                auto* cscMatrixAddress = static_cast<simd::float4x3*>([argumentEncoder[stage] constantDataAtIndex:index++]);
                *cscMatrixAddress = textureData.colorSpaceConversionMatrix;
            }
        }
    }

    Vector<BindableResources> resources;
    for (ShaderStage stage : stages) {
        for (size_t i = 0; i < maxResourceUsageValue; ++i) {
            auto renderStage = metalRenderStage(stage);
            Vector<id<MTLResource>> &v = stageResources[renderStage][i];
            if (v.size()) {
                resources.append(BindableResources {
                    .mtlResources = WTFMove(v),
                    .usage = static_cast<MTLResourceUsage>(i + 1),
                    .renderStages = renderStage
                });
            }
        }
    }

    argumentBuffer[ShaderStage::Vertex].label = bindGroupLayout.vertexArgumentEncoder().label;
    argumentBuffer[ShaderStage::Fragment].label = bindGroupLayout.fragmentArgumentEncoder().label;
    argumentBuffer[ShaderStage::Compute].label = bindGroupLayout.computeArgumentEncoder().label;

    return BindGroup::create(argumentBuffer[ShaderStage::Vertex], argumentBuffer[ShaderStage::Fragment], argumentBuffer[ShaderStage::Compute], WTFMove(resources), *this);
}

BindGroup::BindGroup(id<MTLBuffer> vertexArgumentBuffer, id<MTLBuffer> fragmentArgumentBuffer, id<MTLBuffer> computeArgumentBuffer, Vector<BindableResources>&& resources, Device& device)
    : m_vertexArgumentBuffer(vertexArgumentBuffer)
    , m_fragmentArgumentBuffer(fragmentArgumentBuffer)
    , m_computeArgumentBuffer(computeArgumentBuffer)
    , m_device(device)
    , m_resources(WTFMove(resources))
{
}

BindGroup::BindGroup(Device& device)
    : m_device(device)
{
}

BindGroup::~BindGroup() = default;

void BindGroup::setLabel(String&& label)
{
    auto labelString = label;
    m_vertexArgumentBuffer.label = labelString;
    m_fragmentArgumentBuffer.label = labelString;
    m_computeArgumentBuffer.label = labelString;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuBindGroupReference(WGPUBindGroup bindGroup)
{
    WebGPU::fromAPI(bindGroup).ref();
}

void wgpuBindGroupRelease(WGPUBindGroup bindGroup)
{
    WebGPU::fromAPI(bindGroup).deref();
}

void wgpuBindGroupSetLabel(WGPUBindGroup bindGroup, const char* label)
{
    WebGPU::fromAPI(bindGroup).setLabel(WebGPU::fromAPI(label));
}
