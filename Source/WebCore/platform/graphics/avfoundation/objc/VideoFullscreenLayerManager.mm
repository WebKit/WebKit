/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "VideoFullscreenLayerManager.h"

#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#import "Color.h"
#import "WebCoreCALayerExtras.h"
#import <mach/mach_init.h>
#import <mach/mach_port.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockPtr.h>

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

std::unique_ptr<VideoFullscreenLayerManager> VideoFullscreenLayerManager::create()
{
    return std::unique_ptr<VideoFullscreenLayerManager>(new VideoFullscreenLayerManager());
}

VideoFullscreenLayerManager::VideoFullscreenLayerManager()
{
}

void VideoFullscreenLayerManager::setVideoLayer(PlatformLayer *videoLayer, IntSize contentSize)
{
    m_videoLayer = videoLayer;

    [m_videoLayer web_disableAllActions];
    m_videoInlineLayer = adoptNS([[WebVideoContainerLayer alloc] init]);
#ifndef NDEBUG
    [m_videoInlineLayer setName:@"WebVideoContainerLayer"];
#endif
    [m_videoInlineLayer setFrame:CGRectMake(0, 0, contentSize.width(), contentSize.height())];
    if (m_videoFullscreenLayer) {
        [m_videoLayer setFrame:CGRectMake(0, 0, m_videoFullscreenFrame.width(), m_videoFullscreenFrame.height())];
        [m_videoFullscreenLayer insertSublayer:m_videoLayer.get() atIndex:0];
    } else {
        [m_videoInlineLayer insertSublayer:m_videoLayer.get() atIndex:0];
        [m_videoLayer setFrame:m_videoInlineLayer.get().bounds];
    }
}

void VideoFullscreenLayerManager::setVideoFullscreenLayer(PlatformLayer *videoFullscreenLayer, WTF::Function<void()>&& completionHandler)
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
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
            oldContext.commitPriority = 0;
            newContext.commitPriority = 1;
#endif
            mach_port_t fencePort = [oldContext createFencePort];
            [newContext setFencePort:fencePort];
            mach_port_deallocate(mach_task_self(), fencePort);
        }
    }

    [CATransaction setCompletionBlock:BlockPtr<void ()>::fromCallable([completionHandler = WTFMove(completionHandler)] {
        completionHandler();
    }).get()];

    [CATransaction commit];
}

void VideoFullscreenLayerManager::setVideoFullscreenFrame(FloatRect videoFullscreenFrame)
{
    m_videoFullscreenFrame = videoFullscreenFrame;
    if (!m_videoFullscreenLayer)
        return;

    [m_videoLayer setFrame:CGRectMake(0, 0, videoFullscreenFrame.width(), videoFullscreenFrame.height())];
}

void VideoFullscreenLayerManager::didDestroyVideoLayer()
{
    [m_videoLayer removeFromSuperlayer];

    m_videoInlineLayer = nil;
    m_videoLayer = nil;
}

}

#endif // PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
