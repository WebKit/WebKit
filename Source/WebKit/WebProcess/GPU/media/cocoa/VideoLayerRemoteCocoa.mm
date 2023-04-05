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
#import "VideoLayerRemoteCocoa.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#import "LayerHostingContext.h"
#import "MediaPlayerPrivateRemote.h"
#import "VideoLayerRemote.h"
#import <WebCore/FloatRect.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/Timer.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/MachSendRight.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WeakPtr.h>

// We want to wait for a short time after the completion of the animation (we choose 100 ms here) to fire the timer
// to avoid excessive XPC messages from the Web process to the GPU process.
static const Seconds PostAnimationDelay { 100_ms };

@implementation WKVideoLayerRemote {
    WeakPtr<WebKit::MediaPlayerPrivateRemote> _mediaPlayerPrivateRemote;
    RetainPtr<CAContext> _context;
    WebCore::MediaPlayerEnums::VideoGravity _videoGravity;

    std::unique_ptr<WebCore::Timer> _resolveBoundsTimer;
    bool _shouldRestartWhenTimerFires;
    Seconds _delay;
}

- (instancetype)init
{
    self = [super init];
    if (!self)
        return nil;

    self.masksToBounds = YES;
    _resolveBoundsTimer = makeUnique<WebCore::Timer>([weakSelf = WeakObjCPtr<WKVideoLayerRemote>(self)] {
        auto localSelf = weakSelf.get();
        if (!localSelf)
            return;

        [localSelf resolveBounds];
    });
    _shouldRestartWhenTimerFires = false;

    return self;
}

- (WebKit::MediaPlayerPrivateRemote*)mediaPlayerPrivateRemote
{
    return _mediaPlayerPrivateRemote.get();
}

- (void)setMediaPlayerPrivateRemote:(WebKit::MediaPlayerPrivateRemote*)mediaPlayerPrivateRemote
{
    _mediaPlayerPrivateRemote = *mediaPlayerPrivateRemote;
}

- (WebCore::MediaPlayerEnums::VideoGravity)videoGravity
{
    return _videoGravity;
}

- (void)setVideoGravity:(WebCore::MediaPlayerEnums::VideoGravity)videoGravity
{
    _videoGravity = videoGravity;
}

- (bool)resizePreservingGravity
{
    auto* player = self.mediaPlayerPrivateRemote;
    if (player && player->inVideoFullscreenOrPictureInPicture())
        return true;
    
    return _videoGravity != WebCore::MediaPlayer::VideoGravity::Resize;
}

- (void)layoutSublayers
{
    auto* sublayers = [self sublayers];
    
    if ([sublayers count] != 1) {
        ASSERT_NOT_REACHED();
        return;
    }

    WebCore::FloatRect sourceVideoFrame = self.videoLayerFrame;
    WebCore::FloatRect targetVideoFrame = self.bounds;

    if (sourceVideoFrame == targetVideoFrame && CGAffineTransformIsIdentity(self.affineTransform))
        return;

    if (sourceVideoFrame.isEmpty()) {
        // The initial resize will have an empty videoLayerFrame, which makes
        // the subsequent calculations incorrect. When this happens, just do
        // the synchronous resize step instead.
        [self resolveBounds];
        return;
    }

    CGAffineTransform transform = CGAffineTransformIdentity;
    if ([self resizePreservingGravity]) {
        WebCore::FloatSize naturalSize { };
        if (auto *mediaPlayer = _mediaPlayerPrivateRemote.get())
            naturalSize = mediaPlayer->naturalSize();

        if (!naturalSize.isEmpty()) {
            // The video content will be sized within the remote layer, preserving aspect
            // ratio according to its naturalSize(), so use that natural size to determine
            // the scaling factor.
            auto naturalAspectRatio = naturalSize.aspectRatio();

            sourceVideoFrame = largestRectWithAspectRatioInsideRect(naturalAspectRatio, sourceVideoFrame);
            targetVideoFrame = largestRectWithAspectRatioInsideRect(naturalAspectRatio, targetVideoFrame);
        }
        auto scale = std::fmax(targetVideoFrame.width() / sourceVideoFrame.width(), targetVideoFrame.height() / sourceVideoFrame.height());
        transform = CGAffineTransformMakeScale(scale, scale);
    } else
        transform = CGAffineTransformMakeScale(targetVideoFrame.width() / sourceVideoFrame.width(), targetVideoFrame.height() / sourceVideoFrame.height());

    auto* videoSublayer = [sublayers objectAtIndex:0];
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [videoSublayer setPosition:CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds))];
    [videoSublayer setAffineTransform:transform];
    [CATransaction commit];

    _context = [CAContext currentContext];
    NSTimeInterval animationDuration = [CATransaction animationDuration];

    _delay = Seconds(animationDuration) + PostAnimationDelay;
    if (_resolveBoundsTimer->isActive()) {
        _shouldRestartWhenTimerFires = true;
        _delay -= _resolveBoundsTimer->nextFireInterval();
        return;
    }
    _resolveBoundsTimer->startOneShot(_delay);
}

- (void)resolveBounds
{
    if (_shouldRestartWhenTimerFires) {
        _shouldRestartWhenTimerFires = false;
        _resolveBoundsTimer->startOneShot(_delay);
        return;
    }

    auto* sublayers = [self sublayers];
    if ([sublayers count] != 1) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (CGRectEqualToRect(self.videoLayerFrame, self.bounds) && CGAffineTransformIsIdentity(self.affineTransform))
        return;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    if (!CGRectEqualToRect(self.videoLayerFrame, self.bounds)) {
        self.videoLayerFrame = self.bounds;
        if (auto* mediaPlayerPrivateRemote = self.mediaPlayerPrivateRemote) {
            MachSendRight fenceSendRight = MachSendRight::adopt([_context createFencePort]);
            mediaPlayerPrivateRemote->setVideoInlineSizeFenced(WebCore::FloatSize(self.videoLayerFrame.size), fenceSendRight);
        }
    }

    auto* videoSublayer = [sublayers objectAtIndex:0];
    [videoSublayer setAffineTransform:CGAffineTransformIdentity];
    [videoSublayer setFrame:self.bounds];

    [CATransaction commit];
}

@end

namespace WebKit {

PlatformLayerContainer createVideoLayerRemote(MediaPlayerPrivateRemote* mediaPlayerPrivateRemote, LayerHostingContextID contextId, WebCore::MediaPlayerEnums::VideoGravity videoGravity, IntSize contentSize)
{
    // Initially, all the layers will be empty (both width and height are 0) and invisible.
    // The renderer will change the sizes of WKVideoLayerRemote to trigger layout of sublayers and make them visible.
    auto videoLayerRemote = adoptNS([[WKVideoLayerRemote alloc] init]);
    [videoLayerRemote setName:@"WKVideoLayerRemote"];
    [videoLayerRemote setVideoGravity:videoGravity];
    [videoLayerRemote setMediaPlayerPrivateRemote:mediaPlayerPrivateRemote];
    auto layerForHostContext = LayerHostingContext::createPlatformLayerForHostingContext(contextId).get();
    auto frame = CGRectMake(0, 0, contentSize.width(), contentSize.height());
    [videoLayerRemote setVideoLayerFrame:frame];
    [layerForHostContext setFrame:frame];
    [videoLayerRemote addSublayer:WTFMove(layerForHostContext)];

    return videoLayerRemote;
}

} // namespace WebKit

#endif
