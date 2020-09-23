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

#import <WebCore/FloatRect.h>
#import <WebCore/TextTrackRepresentation.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/MachSendRight.h>

namespace WebKit {

#if ENABLE(VIDEO_PRESENTATION_MODE)

PlatformLayerContainer MediaPlayerPrivateRemote::createVideoFullscreenLayer()
{
    if (!m_fullscreenLayerHostingContextId)
        return nullptr;

    if (!m_videoFullscreenLayer) {
        m_videoFullscreenLayer = adoptNS([[CALayer alloc] init]);
        auto sublayer = LayerHostingContext::createPlatformLayerForHostingContext(m_fullscreenLayerHostingContextId.value());
        [sublayer setName:@"VideoFullscreenLayerSublayer"];
        [sublayer setPosition:CGPointMake(0, 0)];
        [m_videoFullscreenLayer addSublayer:sublayer.get()];
    }

    return m_videoFullscreenLayer;
}

void MediaPlayerPrivateRemote::setVideoFullscreenFrame(WebCore::FloatRect rect)
{
    auto context = [m_videoFullscreenLayer context];
    if (!context)
        return;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    MachSendRight fenceSendRight = MachSendRight::adopt([context createFencePort]);
    setVideoFullscreenFrameFenced(rect, fenceSendRight);

    [CATransaction commit];

    syncTextTrackBounds();
}

#endif

void MediaPlayerPrivateRemote::setTextTrackRepresentation(WebCore::TextTrackRepresentation* representation)
{
#if !ENABLE(VIDEO_PRESENTATION_MODE)
    UNUSED_PARAM(representation);
#else
    PlatformLayer* representationLayer = representation ? representation->platformLayer() : nil;

    if (representationLayer == m_textTrackRepresentationLayer) {
        syncTextTrackBounds();
        return;
    }

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    if (m_textTrackRepresentationLayer)
        [m_textTrackRepresentationLayer removeFromSuperlayer];

    m_textTrackRepresentationLayer = representationLayer;

    if (m_videoFullscreenLayer && m_textTrackRepresentationLayer) {
        syncTextTrackBounds();
        [m_videoFullscreenLayer addSublayer:m_textTrackRepresentationLayer.get()];
    }

    [CATransaction commit];
#endif
}

void MediaPlayerPrivateRemote::syncTextTrackBounds()
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (!m_videoFullscreenLayer || !m_textTrackRepresentationLayer)
        return;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    [m_textTrackRepresentationLayer setFrame:m_videoFullscreenLayer.get().bounds];

    [CATransaction commit];
#endif
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
