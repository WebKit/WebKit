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
#import "MediaPlayerPrivateRemoteMessages.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/FloatSize.h>
#import <WebCore/IOSurface.h>
#import <wtf/MachSendRight.h>

namespace WebKit {

static void setVideoInlineSizeIfPossible(LayerHostingContext& context, const WebCore::FloatSize& size)
{
    if (!context.rootLayer() || size.isEmpty())
        return;

    // We do not want animations here.
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [context.rootLayer() setFrame:CGRectMake(0, 0, size.width(), size.height())];
    [CATransaction commit];
}

void RemoteMediaPlayerProxy::prepareForPlayback(bool privateMode, WebCore::MediaPlayerEnums::Preload preload, bool preservesPitch, bool prepareForRendering, float videoContentScale, WebCore::DynamicRangeMode preferredDynamicRangeMode, CompletionHandler<void(std::optional<LayerHostingContextID>&& inlineLayerHostingContextId)>&& completionHandler)
{
    m_player->setPrivateBrowsingMode(privateMode);
    m_player->setPreload(preload);
    m_player->setPreservesPitch(preservesPitch);
    m_player->setPreferredDynamicRangeMode(preferredDynamicRangeMode);
    if (prepareForRendering)
        m_player->prepareForRendering();
    m_videoContentScale = videoContentScale;
    if (!m_inlineLayerHostingContext)
        m_inlineLayerHostingContext = LayerHostingContext::createForExternalHostingProcess();
    completionHandler(m_inlineLayerHostingContext->contextID());
}

void RemoteMediaPlayerProxy::mediaPlayerFirstVideoFrameAvailable()
{
    // Initially the size of the platformLayer may be 0x0 because we do not provide mediaPlayerContentBoxRect() in this class.
    setVideoInlineSizeIfPossible(*m_inlineLayerHostingContext, m_videoInlineSize);
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::FirstVideoFrameAvailable(), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerRenderingModeChanged()
{
    m_inlineLayerHostingContext->setRootLayer(m_player->platformLayer());
    m_webProcessConnection->send(Messages::MediaPlayerPrivateRemote::RenderingModeChanged(), m_id);
}
void RemoteMediaPlayerProxy::setVideoInlineSizeFenced(const WebCore::FloatSize& size, const WTF::MachSendRight& machSendRight)
{
    m_inlineLayerHostingContext->setFencePort(machSendRight.sendRight());

    m_videoInlineSize = size;
    setVideoInlineSizeIfPossible(*m_inlineLayerHostingContext, size);
}

void RemoteMediaPlayerProxy::nativeImageForCurrentTime(CompletionHandler<void(std::optional<WTF::MachSendRight>&&, WebCore::DestinationColorSpace)>&& completionHandler)
{
    if (!m_player) {
        completionHandler(std::nullopt, DestinationColorSpace::SRGB());
        return;
    }

    auto nativeImage = m_player->nativeImageForCurrentTime();
    if (!nativeImage) {
        completionHandler(std::nullopt, DestinationColorSpace::SRGB());
        return;
    }

    auto platformImage = nativeImage->platformImage();
    if (!platformImage) {
        completionHandler(std::nullopt, DestinationColorSpace::SRGB());
        return;
    }

    auto surface = WebCore::IOSurface::createFromImage(platformImage.get());
    if (!surface) {
        completionHandler(std::nullopt, DestinationColorSpace::SRGB());
        return;
    }

    completionHandler(surface->createSendRight(), nativeImage->colorSpace());
}

void RemoteMediaPlayerProxy::colorSpace(CompletionHandler<void(WebCore::DestinationColorSpace)>&& completionHandler)
{
    if (!m_player) {
        completionHandler(DestinationColorSpace::SRGB());
        return;
    }

    completionHandler(m_player->colorSpace());
}

#if USE(AVFOUNDATION)
void RemoteMediaPlayerProxy::pixelBufferForCurrentTimeIfChanged(CompletionHandler<void(std::optional<RetainPtr<CVPixelBufferRef>>&&)>&& completionHandler)
{
    std::optional<RetainPtr<CVPixelBufferRef>> result;
    RetainPtr<CVPixelBufferRef> pixelBuffer;
    if (m_player)
        pixelBuffer = m_player->pixelBufferForCurrentTime();
    if (m_pixelBufferForCurrentTime != pixelBuffer) {
        result = pixelBuffer;
        m_pixelBufferForCurrentTime = WTFMove(pixelBuffer);
    }
    completionHandler(WTFMove(result));
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
