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

#import "config.h"
#import "MediaPlayerPrivateRemote.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#import "RemoteAudioSourceProvider.h"
#import "RemoteMediaPlayerProxyMessages.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/ColorSpaceCG.h>
#import <WebCore/IOSurface.h>
#import <WebCore/PixelBufferConformerCV.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/MachSendRight.h>

#import <WebCore/CoreVideoSoftLink.h>

namespace WebKit {
using namespace WebCore;

#if ENABLE(VIDEO_PRESENTATION_MODE)
PlatformLayerContainer MediaPlayerPrivateRemote::createVideoFullscreenLayer()
{
    return adoptNS([[CALayer alloc] init]);
}
#endif

void MediaPlayerPrivateRemote::pushVideoFrameMetadata(WebCore::VideoFrameMetadata&& videoFrameMetadata, RetainPtr<CVPixelBufferRef>&& buffer)
{
    if (!m_isGatheringVideoFrameMetadata)
        return;
    m_videoFrameMetadata = WTFMove(videoFrameMetadata);
    m_pixelBufferGatheredWithVideoFrameMetadata = WTFMove(buffer);
}

RefPtr<NativeImage> MediaPlayerPrivateRemote::nativeImageForCurrentTime()
{
    if (m_pixelBufferGatheredWithVideoFrameMetadata) {
        if (!m_pixelBufferConformer)
            m_pixelBufferConformer = makeUnique<PixelBufferConformerCV>((__bridge CFDictionaryRef)@{ (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA) });
        ASSERT(m_pixelBufferConformer);
        if (!m_pixelBufferConformer)
            return nullptr;
        return NativeImage::create(m_pixelBufferConformer->createImageFromPixelBuffer(m_pixelBufferGatheredWithVideoFrameMetadata.get()));
    }

    std::optional<MachSendRight> sendRight;
    auto colorSpace = DestinationColorSpace::SRGB();
    if (!connection().sendSync(Messages::RemoteMediaPlayerProxy::NativeImageForCurrentTime(), Messages::RemoteMediaPlayerProxy::NativeImageForCurrentTime::Reply(sendRight, colorSpace), m_id))
        return nullptr;

    if (!sendRight)
        return nullptr;

    auto surface = WebCore::IOSurface::createFromSendRight(WTFMove(*sendRight), colorSpace);
    if (!surface)
        return nullptr;

    auto platformImage = WebCore::IOSurface::sinkIntoImage(WTFMove(surface));
    if (!platformImage)
        return nullptr;

    return NativeImage::create(WTFMove(platformImage));
}

WebCore::DestinationColorSpace MediaPlayerPrivateRemote::colorSpace()
{
    auto colorSpace = DestinationColorSpace::SRGB();
    connection().sendSync(Messages::RemoteMediaPlayerProxy::ColorSpace(), Messages::RemoteMediaPlayerProxy::ColorSpace::Reply(colorSpace), m_id);
    return colorSpace;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
