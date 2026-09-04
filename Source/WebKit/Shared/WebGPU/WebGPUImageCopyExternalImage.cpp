/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "WebGPUImageCopyExternalImage.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebCore/WebGPUImageCopyExternalImage.h>

namespace WebKit::WebGPU {

std::optional<ImageCopyExternalImage> ConvertToBackingContext::convertToBacking(const WebCore::WebGPU::ImageCopyExternalImage& imageCopyExternalImage)
{
    std::optional<Origin2D> origin;
    if (imageCopyExternalImage.origin) {
        origin = convertToBacking(*imageCopyExternalImage.origin);
        if (!origin)
            return std::nullopt;
    }

    return { { WTF::move(origin), imageCopyExternalImage.flipY, imageCopyExternalImage.imageBuffer ? std::optional { imageCopyExternalImage.imageBuffer->renderingResourceIdentifier() } : std::nullopt, imageCopyExternalImage.premultipliedAlpha } };
}

std::optional<WebCore::WebGPU::ImageCopyExternalImage> ConvertFromBackingContext::convertFromBacking(const ImageCopyExternalImage& imageCopyExternalImage)
{
    std::optional<WebCore::WebGPU::Origin2D> origin;
    if (imageCopyExternalImage.origin) {
        origin = convertFromBacking(*imageCopyExternalImage.origin);
        if (!origin)
            return std::nullopt;
    }

    // The source ImageBuffer cannot be resolved here; RemoteQueue looks it up through RemoteGPU
    // and fills it in, because only RemoteGPU can reach the RemoteRenderingBackend.
    return { { WTF::move(origin), imageCopyExternalImage.flipY, nullptr, imageCopyExternalImage.premultipliedAlpha, std::nullopt } };
}

#if PLATFORM(COCOA) && ENABLE(VIDEO)
std::optional<ImageCopyExternalImageVideoSource> ConvertToBackingContext::convertToBackingVideoSource(const WebCore::WebGPU::ImageCopyExternalImage& imageCopyExternalImage)
{
    ASSERT(imageCopyExternalImage.videoSource);
    if (!imageCopyExternalImage.videoSource)
        return std::nullopt;

    std::optional<Origin2D> origin;
    if (imageCopyExternalImage.origin) {
        origin = convertToBacking(*imageCopyExternalImage.origin);
        if (!origin)
            return std::nullopt;
    }

    std::optional<WebCore::MediaPlayerIdentifier> mediaIdentifier;
    if (auto* identifier = std::get_if<std::optional<WebCore::MediaPlayerIdentifier>>(&*imageCopyExternalImage.videoSource))
        mediaIdentifier = *identifier;

    // A frame which has no media player behind it has to travel through the shared video frame
    // memory instead, which only RemoteQueueProxy can write to, so it fills sharedFrame in.
    return { { WTF::move(origin), imageCopyExternalImage.flipY, mediaIdentifier, std::nullopt } };
}

std::optional<WebCore::WebGPU::ImageCopyExternalImage> ConvertFromBackingContext::convertFromBacking(const ImageCopyExternalImageVideoSource& imageCopyExternalImage, ConvertFromBackingContext::PixelBufferType pixelBuffer, WebCore::VideoFrameRotation rotation, bool isMirrored)
{
    std::optional<WebCore::WebGPU::Origin2D> origin;
    if (imageCopyExternalImage.origin) {
        origin = convertFromBacking(*imageCopyExternalImage.origin);
        if (!origin)
            return std::nullopt;
    }

    // RemoteQueue has already resolved the media player identifier, or read the shared frame, into
    // the pixel buffer the backing queue wraps, and read the display transform off the frame the
    // pixel buffer came from; a decoded frame is opaque, so its alpha needs neither premultiplying
    // nor undoing.
    return { { WTF::move(origin), imageCopyExternalImage.flipY, nullptr, true, WebCore::WebGPU::VideoSourceIdentifier { pixelBuffer }, rotation, isMirrored } };
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
