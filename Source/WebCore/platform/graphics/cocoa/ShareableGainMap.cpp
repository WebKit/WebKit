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
#include "ShareableGainMap.h"

#if PLATFORM(COCOA)

#include "Logging.h"
#include <pal/spi/cg/ImageIOSPI.h>

#include "CoreVideoSoftLink.h"

namespace WebCore {

std::optional<ShareableGainMap> ShareableGainMap::create(const std::optional<GainMap>& gainMap)
{
    if (!gainMap || !gainMap->gainMapPixelBuffer)
        return std::nullopt;

    RetainPtr metadata = adoptCF(CGImageMetadataCreateXMPData(gainMap->metadata, nullptr));
    if (!metadata)
        return std::nullopt;

    RefPtr gainMapShareablePixelBuffer = ShareableCVPixelBuffer::create(gainMap->gainMapPixelBuffer);
    if (!gainMapShareablePixelBuffer)
        return std::nullopt;

    return ShareableGainMap { WTF::move(metadata), gainMapShareablePixelBuffer.releaseNonNull(), gainMap->colorSpace };
}

std::optional<ShareableGainMap> ShareableGainMap::create(RetainPtr<CFDataRef>&& metadata, Ref<ShareableCVPixelBuffer>&& gainMapShareablePixelBuffer, const std::optional<DestinationColorSpace> colorSpace)
{
    if (!metadata)
        return std::nullopt;

    return ShareableGainMap { WTF::move(metadata), WTF::move(gainMapShareablePixelBuffer), colorSpace };
}

ShareableGainMap::ShareableGainMap(RetainPtr<CFDataRef>&& metadata, Ref<ShareableCVPixelBuffer>&& gainMapShareablePixelBuffer, const std::optional<DestinationColorSpace> colorSpace)
    : m_metadata(WTF::move(metadata))
    , m_gainMapShareablePixelBuffer(WTF::move(gainMapShareablePixelBuffer))
    , m_colorSpace(colorSpace)
{
}

static void setDictionaryValue(CFMutableDictionaryRef theDict, const void *key, unsigned value)
{
    RetainPtr number = adoptCF(CFNumberCreate(nullptr,  kCFNumberIntType,  &value));
    CFDictionarySetValue(theDict, key, number);
}

static void setDictionaryValue(CFMutableDictionaryRef theDict, const void *key, float value)
{
    RetainPtr number = adoptCF(CFNumberCreate(nullptr,  kCFNumberFloatType,  &value));
    CFDictionarySetValue(theDict, key, number);
}

PlatformImagePtr ShareableGainMap::applyGainMapToBaseImage(PlatformImagePtr basePlatformImage) const
{
    if (!basePlatformImage)
        return nullptr;

    RetainPtr baseImagePixelBuffer = createMetalCompatibleCVPixelBufferFromImage(basePlatformImage);
    if (!baseImagePixelBuffer) {
        RELEASE_LOG_ERROR(Images, "ShareableGainMap::%s: Failed to create baseImagePixelBuffer", __FUNCTION__);
        return nullptr;
    }

    RetainPtr gainMapPixelBuffer = protect(m_gainMapShareablePixelBuffer)->createMetalCompatibleCVPixelBuffer();
    if (!gainMapPixelBuffer) {
        RELEASE_LOG_ERROR(Images, "ShareableGainMap::%s: Failed to create gainMapPixelBuffer", __FUNCTION__);
        return nullptr;
    }

    RetainPtr metadata = adoptCF(CGImageMetadataCreateFromXMPData(m_metadata));
    if (!metadata) {
        RELEASE_LOG_ERROR(Images, "ShareableGainMap::%s: CGImageMetadataCreateFromXMPData() failed", __FUNCTION__);
        return nullptr;
    }

    RetainPtr targetColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceDisplayP3_PQ));
    float targetHeadroom = CGImageGetHDRGainMapHeadroom(metadata, nullptr);

    unsigned width = CVPixelBufferGetWidth(baseImagePixelBuffer);
    unsigned height = CVPixelBufferGetHeight(baseImagePixelBuffer);
    unsigned inputPixelFormatType = CVPixelBufferGetPixelFormatType(baseImagePixelBuffer);
    RetainPtr inputColorSpace = CGImageGetColorSpace(basePlatformImage);

    // MARK: - Get the target CVPixelBuffer attributes.
    RetainPtr inputAttributes = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    setDictionaryValue(inputAttributes, kCVPixelBufferWidthKey, width);
    setDictionaryValue(inputAttributes, kCVPixelBufferHeightKey, height);
    setDictionaryValue(inputAttributes, kCVPixelBufferPixelFormatTypeKey, inputPixelFormatType);
    CFDictionarySetValue(inputAttributes, kCVImageBufferCGColorSpaceKey, inputColorSpace);

    RetainPtr attributesOptions = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    setDictionaryValue(attributesOptions, kCGTargetPixelFormat, static_cast<unsigned>(kCVPixelFormatType_64RGBAHalf));
    setDictionaryValue(attributesOptions, kCGTargetHeadroom, targetHeadroom);
    CFDictionarySetValue(attributesOptions, kCGFlexRangeAlternateColorSpace, targetColorSpace);
    CFDictionarySetValue(attributesOptions, kCGTargetColorSpace, kCGColorSpaceDisplayP3_PQ);

    CFDictionaryRef outputAttributesRef = nullptr;
    CGImageCreatePixelBufferAttributesForHDRTarget(kCGImageHDRTargetHDR, inputAttributes, attributesOptions, &outputAttributesRef);

    RetainPtr outputAttributes = outputAttributesRef;
    if (!outputAttributes) {
        RELEASE_LOG_ERROR(Images, "ShareableGainMap::%s: CGImageCreatePixelBufferAttributesForHDRTarget() failed", __FUNCTION__);
        return nullptr;
    }

    RetainPtr targetAttributes = adoptCF(CFDictionaryCreateMutableCopy(nullptr, 0, outputAttributes));
    RetainPtr surfaceProperties = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(targetAttributes, kCVPixelBufferIOSurfacePropertiesKey, surfaceProperties);
    CFDictionarySetValue(targetAttributes, kCVPixelBufferMetalCompatibilityKey, kCFBooleanTrue);

    RetainPtr pixelFormatNumber = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(targetAttributes, kCVPixelBufferPixelFormatTypeKey));
    OSType targtePixelFormat;
    CFNumberGetValue(pixelFormatNumber, kCFNumberSInt32Type, &targtePixelFormat);

    // MARK: - Create the target CVPixelBuffer.
    RetainPtr targetImagePixelBuffer = createScratchCVPixelBuffer(width, height, targtePixelFormat, targetAttributes, targetColorSpace, targetHeadroom);
    if (!targetImagePixelBuffer) {
        RELEASE_LOG_ERROR(Images, "ShareableGainMap::%s: Failed to create targetImagePixelBuffer", __FUNCTION__);
        return nullptr;
    }

    RetainPtr applyGainMapOptions = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(applyGainMapOptions, kCGImageAuxiliaryDataInfoMetadata, metadata);
    CFDictionarySetValue(applyGainMapOptions, kCGImageAuxiliaryDataInfoColorSpace, m_colorSpace ? m_colorSpace->platformColorSpace() : nullptr);

    auto status = CGImageApplyHDRGainMap(baseImagePixelBuffer, gainMapPixelBuffer, targetImagePixelBuffer, applyGainMapOptions);
    if (status != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(Images, "ShareableGainMap::%s: CGImageApplyHDRGainMap() failed with status=%d", __FUNCTION__, static_cast<int>(status));
        return nullptr;
    }

#if ENABLE(DUMP_GAIN_MAP_IMAGES)
    CVPixelBufferDumpToFile(baseImagePixelBuffer, "*/base-image.cvpb"_s);
    CVPixelBufferDumpToFile(gainMapPixelBuffer, "*/gainmap-image.cvpb"_s);
    CVPixelBufferDumpToFile(targetImagePixelBuffer, "*/output-image.cvpb"_s);
#endif

    // MARK: - Get a CGImage from the target CVPixelBuffer.
    RetainPtr surface = CVPixelBufferGetIOSurface(targetImagePixelBuffer);
    if (!surface) {
        RELEASE_LOG_ERROR(Images, "ShareableGainMap::%s: CVPixelBufferGetIOSurface() failed", __FUNCTION__);
        return nullptr;
    }

    RetainPtr targetPlatformImage = adoptCF(CGImageCreateFromIOSurface(surface, nullptr));
    if (!targetPlatformImage) {
        RELEASE_LOG_ERROR(Images, "ShareableGainMap::%s: CGImageCreateFromIOSurface() failed", __FUNCTION__);
        return nullptr;
    }

    return targetPlatformImage;
}

} // namespace WebCore

#endif
