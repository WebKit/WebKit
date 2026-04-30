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
#include "VideoToolboxSoftLink.h"

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

std::optional<ShareableGainMap> ShareableGainMap::create(RetainPtr<CFDataRef>&& metadata, Ref<ShareableCVPixelBuffer>&& gainMapShareablePixelBuffer, const DestinationColorSpace& colorSpace)
{
    if (!metadata)
        return std::nullopt;

    return ShareableGainMap { WTF::move(metadata), WTF::move(gainMapShareablePixelBuffer), colorSpace };
}

ShareableGainMap::ShareableGainMap(RetainPtr<CFDataRef>&& metadata, Ref<ShareableCVPixelBuffer>&& gainMapShareablePixelBuffer, const DestinationColorSpace& colorSpace)
    : m_metadata(WTF::move(metadata))
    , m_gainMapShareablePixelBuffer(WTF::move(gainMapShareablePixelBuffer))
    , m_colorSpace(colorSpace)
{
}

PlatformImagePtr ShareableGainMap::applyGainMapToBaseImage(PlatformImagePtr basePlatformImage) const
{
    if (!basePlatformImage)
        return nullptr;

    if (!canLoad_VideoToolbox_VTCreateCGImageFromCVPixelBuffer())
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

    RetainPtr outputColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceDisplayP3_PQ));
    RetainPtr outputImagePixelBuffer = createScratchMetalCompatibleCVPixelBuffer(baseImagePixelBuffer, outputColorSpace);
    if (!outputImagePixelBuffer) {
        RELEASE_LOG_ERROR(Images, "ShareableGainMap::%s: Failed to create outputImagePixelBuffer", __FUNCTION__);
        return nullptr;
    }

    RetainPtr metadata = adoptCF(CGImageMetadataCreateFromXMPData(m_metadata));

    RetainPtr options = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(options, kCGImageAuxiliaryDataInfoMetadata, metadata);
    CFDictionarySetValue(options, kCGImageAuxiliaryDataInfoColorSpace, m_colorSpace.platformColorSpace());

    auto status = CGImageApplyHDRGainMap(baseImagePixelBuffer, gainMapPixelBuffer, outputImagePixelBuffer, options);
    if (status != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(Images, "ShareableGainMap::%s: CGImageApplyHDRGainMap() failed with status=%d", __FUNCTION__, static_cast<int>(status));
        return nullptr;
    }

#if ENABLE(DUMP_GAIN_MAP_IMAGES)
    CVPixelBufferDumpToFile(baseImagePixelBuffer, "*/base-image.cvpb"_s);
    CVPixelBufferDumpToFile(gainMapPixelBuffer, "*/gainmap-image.cvpb"_s);
    CVPixelBufferDumpToFile(outputImagePixelBuffer, "*/output-image.cvpb"_s);
#endif

    CGImageRef outputPlatformImage = nullptr;
    status = VTCreateCGImageFromCVPixelBuffer(outputImagePixelBuffer, nullptr, &outputPlatformImage);
    if (status != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(Images, "ShareableGainMap::%s: VTCreateCGImageFromCVPixelBuffer() failed with status=%d", __FUNCTION__, static_cast<int>(status));
        return nullptr;
    }

    return adoptCF(outputPlatformImage);
}

} // namespace WebCore

#endif
