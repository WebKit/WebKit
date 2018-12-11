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
#import "VideoFullscreenLayerManagerObjC.h"

#import "Color.h"
#import "TextTrackRepresentation.h"
#import "WebCoreCALayerExtras.h"
#import <mach/mach_init.h>
#import <mach/mach_port.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/MachSendRight.h>

@interface WebVideoContainerLayer : CALayer
@end

@implementation WebVideoContainerLayer

- (void)setBounds:(CGRect)bounds
{
    [super setBounds:bounds];
    for (CALayer* layer in self.sublayers)
        layer.frame = bounds;
}

- (void)setPosition:(CGPoint)position
{
    if (!CATransform3DIsIdentity(self.transform)) {
        // Pre-apply the transform added in the WebProcess to fix <rdar://problem/18316542> to the position.
        position = CGPointApplyAffineTransform(position, CATransform3DGetAffineTransform(self.transform));
    }
    [super setPosition:position];
}
@end

namespace WebCore {

VideoFullscreenLayerManagerObjC::VideoFullscreenLayerManagerObjC()
    : VideoFullscreenLayerManager()
{
}

void VideoFullscreenLayerManagerObjC::setVideoLayer(PlatformLayer *videoLayer, IntSize contentSize)
{
    m_videoLayer = videoLayer;
    m_videoInlineFrame = CGRectMake(0, 0, contentSize.width(), contentSize.height());

    [m_videoLayer web_disableAllActions];
    m_videoInlineLayer = adoptNS([[WebVideoContainerLayer alloc] init]);
#ifndef NDEBUG
    [m_videoInlineLayer setName:@"WebVideoContainerLayer"];
#endif
    [m_videoInlineLayer setFrame:m_videoInlineFrame];
    [m_videoInlineLayer setContentsGravity:kCAGravityResizeAspect];

    if (m_videoFullscreenLayer) {
        [m_videoLayer setFrame:CGRectMake(0, 0, m_videoFullscreenFrame.width(), m_videoFullscreenFrame.height())];
        [m_videoFullscreenLayer insertSublayer:m_videoLayer.get() atIndex:0];
    } else {
        [m_videoInlineLayer insertSublayer:m_videoLayer.get() atIndex:0];
        [m_videoLayer setFrame:m_videoInlineLayer.get().bounds];
    }
}

void VideoFullscreenLayerManagerObjC::updateVideoFullscreenInlineImage(NativeImagePtr image)
{
    if (m_videoInlineLayer)
        [m_videoInlineLayer setContents:(__bridge id)image.get()];
}

void VideoFullscreenLayerManagerObjC::setVideoFullscreenLayer(PlatformLayer *videoFullscreenLayer, WTF::Function<void()>&& completionHandler, NativeImagePtr currentImage)
{
    if (m_videoFullscreenLayer == videoFullscreenLayer) {
        completionHandler();
        return;
    }

    m_videoFullscreenLayer = videoFullscreenLayer;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    if (m_videoLayer) {
        CAContext *oldContext = [m_videoLayer context];

        if (m_videoInlineLayer)
            [m_videoInlineLayer setContents:(__bridge id)currentImage.get()];

        if (m_videoFullscreenLayer) {
            [m_videoFullscreenLayer insertSublayer:m_videoLayer.get() atIndex:0];
            [m_videoLayer setFrame:CGRectMake(0, 0, m_videoFullscreenFrame.width(), m_videoFullscreenFrame.height())];
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

void VideoFullscreenLayerManagerObjC::setVideoFullscreenFrame(FloatRect videoFullscreenFrame)
{
    m_videoFullscreenFrame = videoFullscreenFrame;
    if (!m_videoFullscreenLayer)
        return;

    [m_videoLayer setFrame:m_videoFullscreenFrame];
    syncTextTrackBounds();
}

void VideoFullscreenLayerManagerObjC::didDestroyVideoLayer()
{
    [m_videoLayer removeFromSuperlayer];

    m_videoInlineLayer = nil;
    m_videoLayer = nil;
}

bool VideoFullscreenLayerManagerObjC::requiresTextTrackRepresentation() const
{
    return m_videoFullscreenLayer;
}

void VideoFullscreenLayerManagerObjC::syncTextTrackBounds()
{
    if (!m_videoFullscreenLayer || !m_textTrackRepresentationLayer)
        return;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    [m_textTrackRepresentationLayer setFrame:m_videoFullscreenFrame];

    [CATransaction commit];
}

void VideoFullscreenLayerManagerObjC::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
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

}

}

