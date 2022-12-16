/*
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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
#import "WebAVPlayerLayer.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(VIDEO_PRESENTATION_MODE)

#import "GeometryUtilities.h"
#import "VideoFullscreenInterfaceAVKit.h"
#import "WebAVPlayerController.h"

#import <pal/spi/cocoa/AVKitSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

using namespace WebCore;

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, __AVPlayerLayerView)

@implementation WebAVPlayerLayer {
    RefPtr<PlatformVideoFullscreenInterface> _fullscreenInterface;
    RetainPtr<WebAVPlayerController> _playerController;
    RetainPtr<CALayer> _videoSublayer;
    FloatRect _videoSublayerFrame;
    RetainPtr<NSString> _videoGravity;
    RetainPtr<NSString> _previousVideoGravity;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.masksToBounds = YES;
        self.allowsHitTesting = NO;
        _videoGravity = AVLayerVideoGravityResizeAspect;
        _previousVideoGravity = AVLayerVideoGravityResizeAspect;
    }
    return self;
}

- (void)dealloc
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(resolveBounds) object:nil];
    [_pixelBufferAttributes release];
    [super dealloc];
}

- (PlatformVideoFullscreenInterface*)fullscreenInterface
{
    return _fullscreenInterface.get();
}

- (void)setFullscreenInterface:(PlatformVideoFullscreenInterface*)fullscreenInterface
{
    _fullscreenInterface = fullscreenInterface;
}

- (AVPlayerController *)playerController
{
    return (AVPlayerController *)_playerController.get();
}

- (void)setPlayerController:(AVPlayerController *)playerController
{
    ASSERT(!playerController || [playerController isKindOfClass:webAVPlayerControllerClass()]);
    _playerController = (WebAVPlayerController *)playerController;
}

- (void)setVideoSublayer:(CALayer *)videoSublayer
{
    _videoSublayer = videoSublayer;
}

- (CALayer*)videoSublayer
{
    return _videoSublayer.get();
}

- (FloatRect)calculateTargetVideoFrame
{
    FloatRect targetVideoFrame;
    float videoAspectRatio = self.videoDimensions.width / self.videoDimensions.height;

    if ([AVLayerVideoGravityResize isEqualToString:self.videoGravity])
        targetVideoFrame = self.bounds;
    else if ([AVLayerVideoGravityResizeAspect isEqualToString:self.videoGravity])
        targetVideoFrame = largestRectWithAspectRatioInsideRect(videoAspectRatio, self.bounds);
    else if ([AVLayerVideoGravityResizeAspectFill isEqualToString:self.videoGravity])
        targetVideoFrame = smallestRectWithAspectRatioAroundRect(videoAspectRatio, self.bounds);
    else
        ASSERT_NOT_REACHED();

    return targetVideoFrame;
}

- (void)layoutSublayers
{
    if ([_videoSublayer superlayer] != self)
        return;

    [_videoSublayer setPosition:CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds))];

    if (self.videoDimensions.height <= 0 || self.videoDimensions.width <= 0)
        return;

    FloatRect sourceVideoFrame;
    FloatRect targetVideoFrame;
    float videoAspectRatio = self.videoDimensions.width / self.videoDimensions.height;

    if ([AVLayerVideoGravityResize isEqualToString:_previousVideoGravity.get()])
        sourceVideoFrame = self.modelVideoLayerFrame;
    else if ([AVLayerVideoGravityResizeAspect isEqualToString:_previousVideoGravity.get()])
        sourceVideoFrame = largestRectWithAspectRatioInsideRect(videoAspectRatio, self.modelVideoLayerFrame);
    else if ([AVLayerVideoGravityResizeAspectFill isEqualToString:_previousVideoGravity.get()])
        sourceVideoFrame = smallestRectWithAspectRatioAroundRect(videoAspectRatio, self.modelVideoLayerFrame);
    else
        ASSERT_NOT_REACHED();

    targetVideoFrame = [self calculateTargetVideoFrame];

    UIView *view = (UIView *)[_videoSublayer delegate];
    CGAffineTransform transform = CGAffineTransformMakeScale(targetVideoFrame.width() / sourceVideoFrame.width(), targetVideoFrame.height() / sourceVideoFrame.height());
    [view setTransform:transform];

    NSTimeInterval animationDuration = [CATransaction animationDuration];
    RunLoop::main().dispatch([self, strongSelf = retainPtr(self), targetVideoFrame, animationDuration] {
        [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(resolveBounds) object:nil];

        _videoSublayerFrame = targetVideoFrame;
        [self performSelector:@selector(resolveBounds) withObject:nil afterDelay:animationDuration + 0.1];
    });
}

- (void)resolveBounds
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(resolveBounds) object:nil];

    if ([_videoSublayer superlayer] != self)
        return;

    if (CGRectEqualToRect(self.modelVideoLayerFrame, [self bounds]) && CGAffineTransformIsIdentity([(UIView *)[_videoSublayer delegate] transform]))
        return;

    [CATransaction begin];
    [CATransaction setAnimationDuration:0];
    [CATransaction setDisableActions:YES];

    self.modelVideoLayerFrame = [self bounds];
    if (auto* model = _fullscreenInterface->videoFullscreenModel())
        model->setVideoLayerFrame(_videoSublayerFrame);

    _previousVideoGravity = _videoGravity;

    [(UIView *)[_videoSublayer delegate] setTransform:CGAffineTransformIdentity];

    [CATransaction commit];
}

- (void)setVideoGravity:(NSString *)videoGravity
{
#if PLATFORM(MACCATALYST)
    // FIXME<rdar://46011230>: remove this #if once this radar lands.
    if (!videoGravity)
        videoGravity = AVLayerVideoGravityResizeAspect;
#endif

    if ([_videoGravity isEqualToString:videoGravity])
        return;

    _previousVideoGravity = _videoGravity;
    _videoGravity = videoGravity;

    if (![_playerController delegate])
        return;

    MediaPlayerEnums::VideoGravity gravity = MediaPlayerEnums::VideoGravity::ResizeAspect;
    if (videoGravity == AVLayerVideoGravityResize)
        gravity = MediaPlayerEnums::VideoGravity::Resize;
    if (videoGravity == AVLayerVideoGravityResizeAspect)
        gravity = MediaPlayerEnums::VideoGravity::ResizeAspect;
    else if (videoGravity == AVLayerVideoGravityResizeAspectFill)
        gravity = MediaPlayerEnums::VideoGravity::ResizeAspectFill;
    else
        ASSERT_NOT_REACHED();

    if (auto* model = _fullscreenInterface->videoFullscreenModel())
        model->setVideoLayerGravity(gravity);

    [self setNeedsLayout];
}

- (NSString *)videoGravity
{
    return _videoGravity.get();
}

- (CGRect)videoRect
{
    if (self.videoDimensions.width <= 0 || self.videoDimensions.height <= 0)
        return self.bounds;

    float videoAspectRatio = self.videoDimensions.width / self.videoDimensions.height;

    if ([AVLayerVideoGravityResizeAspect isEqualToString:self.videoGravity])
        return largestRectWithAspectRatioInsideRect(videoAspectRatio, self.bounds);
    if ([AVLayerVideoGravityResizeAspectFill isEqualToString:self.videoGravity])
        return smallestRectWithAspectRatioAroundRect(videoAspectRatio, self.bounds);

    return self.bounds;
}

+ (NSSet *)keyPathsForValuesAffectingVideoRect
{
    return [NSSet setWithObjects:@"videoDimensions", @"videoGravity", nil];
}

@end

#endif // PLATFORM(IOS_FAMILY) && ENABLE(VIDEO_PRESENTATION_MODE)

