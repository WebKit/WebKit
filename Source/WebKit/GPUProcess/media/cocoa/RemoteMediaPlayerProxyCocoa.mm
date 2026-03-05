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
#import "RemoteMediaPlayerProxy.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#import "LayerHostingContext.h"
#import "LayerHostingContextManager.h"
#import "MediaPlayerPrivateRemoteMessages.h"
#import "RemoteVideoFrameObjectHeap.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/DestinationColorSpace.h>
#import <WebCore/FloatRect.h>
#import <WebCore/FloatSize.h>
#import <WebCore/HostingContext.h>
#import <WebCore/IOSurface.h>
#import <WebCore/VideoFrameCV.h>
#import <WebCore/VideoFrameMetadata.h>
#import <wtf/MachSendRightAnnotated.h>

#if USE(EXTENSIONKIT)
#import <BrowserEngineKit/BELayerHierarchy.h>
#import <BrowserEngineKit/BELayerHierarchyHandle.h>
#import <BrowserEngineKit/BELayerHierarchyHostingTransactionCoordinator.h>
#endif

namespace WebKit {

void RemoteMediaPlayerProxy::mediaPlayerFirstVideoFrameAvailable()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_layerHostingContextManager->setVideoLayerSizeIfPossible();
    protect(m_webProcessConnection)->send(Messages::MediaPlayerPrivateRemote::FirstVideoFrameAvailable(), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerRenderingModeChanged()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    bool canShowWhileLocked =
#if PLATFORM(IOS_FAMILY)
        m_configuration.canShowWhileLocked;
#else
        false;
#endif

    if (auto hostingContext = m_layerHostingContextManager->createHostingContextIfNeeded(protect(m_player)->platformLayer(), canShowWhileLocked))
        protect(m_webProcessConnection)->send(Messages::MediaPlayerPrivateRemote::LayerHostingContextChanged(*hostingContext, m_layerHostingContextManager->videoLayerSize()), m_id);

    protect(m_webProcessConnection)->send(Messages::MediaPlayerPrivateRemote::RenderingModeChanged(), m_id);
}

void RemoteMediaPlayerProxy::requestHostingContext(CompletionHandler<void(WebCore::HostingContext)>&& completionHandler)
{
    m_layerHostingContextManager->requestHostingContext(WTF::move(completionHandler));
}

void RemoteMediaPlayerProxy::setVideoLayerSizeFenced(const WebCore::FloatSize& size, WTF::MachSendRightAnnotated&& sendRightAnnotated)
{
    RELEASE_LOG(Media, "RemoteMediaPlayerProxy::setVideoLayerSizeFenced: send right %d, fence data size %lu", sendRightAnnotated.sendRight.sendRight(), sendRightAnnotated.data.size());

    ALWAYS_LOG(LOGIDENTIFIER, size.width(), "x", size.height());
    m_layerHostingContextManager->setVideoLayerSizeFenced(size, WTF::MachSendRightAnnotated { sendRightAnnotated }, [&] {
        protect(m_player)->setVideoLayerSizeFenced(size, WTF::move(sendRightAnnotated));
    });
}

void RemoteMediaPlayerProxy::mediaPlayerOnNewVideoFrameMetadata(WebCore::VideoFrameMetadata&& metadata, RetainPtr<CVPixelBufferRef>&& buffer)
{
    auto properties = protect(m_videoFrameObjectHeap)->add(WebCore::VideoFrameCV::create({ }, false, WebCore::VideoFrame::Rotation::None, WTF::move(buffer)));
    protect(m_webProcessConnection)->send(Messages::MediaPlayerPrivateRemote::PushVideoFrameMetadata(metadata, properties), m_id);
}

WebCore::FloatSize RemoteMediaPlayerProxy::mediaPlayerVideoLayerSize() const
{
    return m_layerHostingContextManager->videoLayerSize();
}

void RemoteMediaPlayerProxy::nativeImageForCurrentTime(CompletionHandler<void(std::optional<WTF::MachSendRight>&&, WebCore::DestinationColorSpace)>&& completionHandler)
{
    using namespace WebCore;

    RefPtr player = m_player;
    if (!player) {
        completionHandler(std::nullopt, DestinationColorSpace::SRGB());
        return;
    }

    auto nativeImage = player->nativeImageForCurrentTime();
    if (!nativeImage) {
        completionHandler(std::nullopt, DestinationColorSpace::SRGB());
        return;
    }

    auto platformImage = nativeImage->platformImage();
    if (!platformImage) {
        completionHandler(std::nullopt, DestinationColorSpace::SRGB());
        return;
    }

    auto surface = WebCore::IOSurface::createFromImage(nullptr, platformImage.get());
    if (!surface) {
        completionHandler(std::nullopt, DestinationColorSpace::SRGB());
        return;
    }

    completionHandler(surface->createSendRight(), nativeImage->colorSpace());
}

void RemoteMediaPlayerProxy::colorSpace(CompletionHandler<void(WebCore::DestinationColorSpace)>&& completionHandler)
{
    RefPtr player = m_player;
    if (!player) {
        completionHandler(WebCore::DestinationColorSpace::SRGB());
        return;
    }

    completionHandler(player->colorSpace());
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
