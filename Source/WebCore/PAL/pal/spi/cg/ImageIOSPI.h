/*
 * Copyright (C) 2017-2026 Apple Inc. All rights reserved.
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

DECLARE_SYSTEM_HEADER

#include <ImageIO/CGImageMetadata.h>
#include <ImageIO/CGImageSource.h>
#include <ImageIO/ImageIOBase.h>

#if USE(APPLE_INTERNAL_SDK)

#include <ImageIO/CGImageHDRFunctionsPrivate.h>
#include <ImageIO/CGImageMetadataPrivate.h>
#include <ImageIO/CGImagePropertiesPriv.h>
#include <ImageIO/CGImageSourcePrivate.h>

#else

typedef CF_CLOSED_ENUM(uint32_t, CGImageHDRTarget)
{
    kCGImageHDRTargetUndefined = 0,
    kCGImageHDRTargetSDR, // e.g. HDR -> SDR
    kCGImageHDRTargetHDR, // e.g. M+ -> HDR
    kCGImageHDRTargetGainMap, // e.g. HDR -> M+
};

typedef struct CF_BRIDGED_TYPE(id) __CVBuffer* CVPixelBufferRef;

IMAGEIO_EXTERN const CFStringRef kCGImageAuxiliaryDataInfoColorSpace;
IMAGEIO_EXTERN const CFStringRef kCGImageAuxiliaryDataInfoMetadata;
IMAGEIO_EXTERN const CFStringRef kCGImageAuxiliaryDataTypeISOGainMap;
IMAGEIO_EXTERN const CFStringRef kCGImageAuxiliaryDataInfoPixelBuffer;
IMAGEIO_EXTERN const CFStringRef kCGImageAuxiliaryDataRepresentation;
IMAGEIO_EXTERN const CFStringRef kCGImageAuxiliaryDataRepresentationPixelBuffer;
IMAGEIO_EXTERN const CFStringRef kCGImageSourceShouldCacheImmediately;
IMAGEIO_EXTERN const CFStringRef kCGImageSourceShouldPreferRGB32;
IMAGEIO_EXTERN const CFStringRef kCGImageSourceSkipMetadata;
IMAGEIO_EXTERN const CFStringRef kCGImageSourceSubsampleFactor;
IMAGEIO_EXTERN const CFStringRef kCGImageSourceUseHardwareAcceleration;
IMAGEIO_EXTERN const CFStringRef kCGTargetPixelFormat;
IMAGEIO_EXTERN const CFStringRef kCGTargetHeadroom;
IMAGEIO_EXTERN const CFStringRef kCGTargetColorSpace;
IMAGEIO_EXTERN const CFStringRef kCGFlexRangeAlternateColorSpace;

WTF_EXTERN_C_BEGIN

CFStringRef CGImageSourceGetTypeWithData(CFDataRef, CFStringRef, bool*);
OSStatus CGImageSourceSetAllowableTypes(CFArrayRef allowableTypes);

IMAGEIO_EXTERN OSStatus CGImageSourceDisableHardwareDecoding();
IMAGEIO_EXTERN OSStatus CGImageSourceEnableRestrictedDecoding();

IMAGEIO_EXTERN uint16_t CGImageGetContentAverageLightLevelNits(CGImageRef);

IMAGEIO_EXTERN CFDictionaryRef CGImageSourceCopyAuxiliaryDataInfoAtIndexWithOptions(CGImageSourceRef, size_t index, CFStringRef auxiliaryDataType, CFDictionaryRef options);

IMAGEIO_EXTERN OSStatus CGImageApplyHDRGainMap(CVPixelBufferRef inputImage, CVPixelBufferRef inputGainmap, CVPixelBufferRef outputImage, CFDictionaryRef options);

IMAGEIO_EXTERN CGFloat CGImageGetHDRGainMapHeadroom(CGImageMetadataRef gainMapMetadata, CFDictionaryRef options);

IMAGEIO_EXTERN CGImageRef CGImageCreateFromIOSurface(IOSurfaceRef, CFDictionaryRef options);

IMAGEIO_EXTERN OSStatus CGImageCreatePixelBufferAttributesForHDRTarget(CGImageHDRTarget hdrType, CFDictionaryRef attributes, CFDictionaryRef options, CFDictionaryRef* attributesOut);

WTF_EXTERN_C_END

#endif
