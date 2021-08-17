/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#import "VideoLayerManagerObjC.h"

#import "Color.h"
#import "Logging.h"
#import "TextTrackRepresentation.h"
#import "WebCoreCALayerExtras.h"
#import "WebVideoContainerLayer.h"
#import <mach/mach_init.h>
#import <mach/mach_port.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/Logger.h>
#import <wtf/MachSendRight.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

OBJC_CLASS AVPlayerLayer;

namespace WebCore {

#if !RELEASE_LOG_DISABLED
VideoLayerManagerObjC::VideoLayerManagerObjC(const Logger& logger, const void* logIdentifier)
    : m_logger(logger)
    , m_logIdentifier(logIdentifier)
{
}
#endif

VideoLayerManagerObjC::~VideoLayerManagerObjC()
{
}

PlatformLayer* VideoLayerManagerObjC::videoInlineLayer() const
{
    return m_videoInlineLayer.get();
}

void VideoLayerManagerObjC::setVideoLayer(PlatformLayer *videoLayer, IntSize contentSize)
{
    ALWAYS_LOG(LOGIDENTIFIER, contentSize.width(), ", ", contentSize.height());

    m_videoLayer = videoLayer;
    [m_videoLayer web_disableAllActions];

    m_videoInlineLayer = adoptNS([[WebVideoContainerLayer alloc] init]);
    [m_videoInlineLayer setName:@"WebVideoContainerLayer"];
    [m_videoInlineLayer setFrame:CGRectMake(0, 0, contentSize.width(), contentSize.height())];
    [m_videoInlineLayer setContentsGravity:kCAGravityResizeAspect];
    if (PAL::isAVFoundationFrameworkAvailable() && [videoLayer isKindOfClass:PAL::getAVPlayerLayerClass()])
        [m_videoInlineLayer setPlayerLayer:(AVPlayerLayer *)videoLayer];

#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (m_videoFullscreenLayer) {
        [m_videoLayer setFrame:CGRectMake(0, 0, m_videoFullscreenFrame.width(), m_videoFullscreenFrame.height())];
        [m_videoFullscreenLayer insertSublayer:m_videoLayer.get() atIndex:0];
    } else
#endif
    {
        [m_videoLayer setFrame:m_videoInlineLayer.get().bounds];
        [m_videoInlineLayer insertSublayer:m_videoLayer.get() atIndex:0];
    }
}

void VideoLayerManagerObjC::didDestroyVideoLayer()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    [m_videoLayer removeFromSuperlayer];

    m_videoInlineLayer = nil;
    m_videoLayer = nil;
}

#if ENABLE(VIDEO_PRESENTATION_MODE)

PlatformLayer* VideoLayerManagerObjC::videoFullscreenLayer() const
{
    return m_videoFullscreenLayer.get();
}

void VideoLayerManagerObjC::setVideoFullscreenLayer(PlatformLayer *videoFullscreenLayer, WTF::Function<void()>&& completionHandler, PlatformImagePtr currentImage)
{
    if (m_videoFullscreenLayer == videoFullscreenLayer) {
        completionHandler();
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER);

    m_videoFullscreenLayer = videoFullscreenLayer;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    if (m_videoLayer) {
        CAContext *oldContext = [m_videoLayer context];

        if (m_videoInlineLayer && currentImage)
            [m_videoInlineLayer setContents:(__bridge id)currentImage.get()];

        if (m_videoFullscreenLayer) {
            [m_videoLayer setFrame:CGRectMake(0, 0, m_videoFullscreenFrame.width(), m_videoFullscreenFrame.height())];
            [m_videoFullscreenLayer insertSublayer:m_videoLayer.get() atIndex:0];
        } else if (m_videoInlineLayer) {
            [m_videoLayer setFrame:[m_videoInlineLayer bounds]];
            [m_videoInlineLayer insertSublayer:m_videoLayer.get() atIndex:0];
        } else
            [m_videoLayer removeFromSuperlayer];

        CAContext *newContext = [m_videoLayer context];
        if (oldContext && newContext && oldContext != newContext) {
#if PLATFORM(MAC)
            oldContext.commitPriority = 0;
            newContext.commitPriority = 1;
#endif
            auto fencePort = MachSendRight::adopt([oldContext createFencePort]);
            [newContext setFencePort:fencePort.sendRight()];
        }
    }

    [CATransaction setCompletionBlock:makeBlockPtr([completionHandler = WTFMove(completionHandler)] {
        completionHandler();
    }).get()];

    [CATransaction commit];
}

FloatRect VideoLayerManagerObjC::videoFullscreenFrame() const
{
    return m_videoFullscreenFrame;
}

void VideoLayerManagerObjC::setVideoFullscreenFrame(FloatRect videoFullscreenFrame)
{
    ALWAYS_LOG(LOGIDENTIFIER, videoFullscreenFrame.x(), ", ", videoFullscreenFrame.y(), ", ", videoFullscreenFrame.width(), ", ", videoFullscreenFrame.height());

    m_videoFullscreenFrame = videoFullscreenFrame;
    if (!m_videoFullscreenLayer)
        return;

    [m_videoLayer setFrame:m_videoFullscreenFrame];
    syncTextTrackBounds();
}

void VideoLayerManagerObjC::updateVideoFullscreenInlineImage(PlatformImagePtr image)
{
    if (m_videoInlineLayer)
        [m_videoInlineLayer setContents:(__bridge id)image.get()];
}

#endif

bool VideoLayerManagerObjC::requiresTextTrackRepresentation() const
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    return m_videoFullscreenLayer;
#else
    return false;
#endif
}

void VideoLayerManagerObjC::syncTextTrackBounds()
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (!m_videoFullscreenLayer || !m_textTrackRepresentationLayer)
        return;

    if (m_textTrackRepresentationLayer.get().bounds == m_videoFullscreenFrame)
        return;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    [m_textTrackRepresentationLayer setFrame:m_videoFullscreenFrame];

    [CATransaction commit];
#endif
}

void VideoLayerManagerObjC::setTextTrackRepresentationLayer(PlatformLayer* representationLayer)
{
#if !ENABLE(VIDEO_PRESENTATION_MODE)
    UNUSED_PARAM(representationLayer);
#else
    ALWAYS_LOG(LOGIDENTIFIER);

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

WTFLogChannel& VideoLayerManagerObjC::logChannel() const
{
    return LogMedia;
}

}
