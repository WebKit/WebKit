/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "WebVideoFullscreenInterfaceAVKit.h"

#import "Logging.h"
#import <CoreMedia/CMTime.h>
#import <UIKit/UIKit.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/WebCoreThreadRun.h>
#import <wtf/RetainPtr.h>
#import "WebVideoFullscreenModel.h"

using namespace WebCore;

SOFT_LINK_FRAMEWORK(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVPlayerLayer)

SOFT_LINK_FRAMEWORK(AVKit)
SOFT_LINK_CLASS(AVKit, AVPlayerController)
SOFT_LINK_CLASS(AVKit, AVPlayerViewController)
SOFT_LINK_CLASS(AVKit, AVValueTiming)

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIScreen)
SOFT_LINK_CLASS(UIKit, UIWindow)
SOFT_LINK_CLASS(UIKit, UIView)
SOFT_LINK_CLASS(UIKit, UIViewController)
SOFT_LINK_CLASS(UIKit, UIColor)

SOFT_LINK_FRAMEWORK(CoreMedia)
SOFT_LINK(CoreMedia, CMTimeMakeWithSeconds, CMTime, (Float64 seconds, int32_t preferredTimeScale), (seconds, preferredTimeScale))
SOFT_LINK(CoreMedia, CMTimeGetSeconds, Float64, (CMTime time), (time))

@interface AVValueTiming : NSObject <NSCoding, NSCopying, NSMutableCopying>
+ (id)valueTimingWithAnchorValue:(double)anchorValue anchorTimeStamp:(NSTimeInterval) timeStamp rate:(double)rate;
+ (NSTimeInterval)currentTimeStamp;
@end

@protocol AVPlayerViewControllerDelegate
@end

@protocol AVPlayerLayer
@end

@interface AVPlayerController : UIResponder
@end

@interface AVPlayerViewController : UIViewController
@property (nonatomic, weak) id <AVPlayerViewControllerDelegate> delegate;
@property (nonatomic, strong) AVPlayerController *playerController;
@end

typedef NS_ENUM(NSInteger, AVPlayerViewControllerDismissalReason) {
    AVPlayerViewControllerDismissalReasonDoneButtonTapped,
    AVPlayerViewControllerDismissalReasonRemoteControlStopEventReceived,
    AVPlayerViewControllerDismissalReasonError
} NS_ENUM_AVAILABLE_IOS(8_0);

typedef NS_ENUM(NSInteger, AVPlayerControllerStatus) {
    AVPlayerControllerStatusUnknown,
    AVPlayerControllerStatusLoading,
    AVPlayerControllerStatusReadyToPlay,
    AVPlayerControllerStatusFailed
} NS_ENUM_AVAILABLE(10_10, 8_0);

@interface WebAVPlayerController : NSObject <AVPlayerViewControllerDelegate>

@property(retain) AVPlayerController* playerControllerProxy;
@property(retain) CALayer<AVPlayerLayer> *playerLayer;
@property(assign) WebVideoFullscreenModel* delegate;

@property BOOL canPlay;
@property(getter=isPlaying) BOOL playing;
@property BOOL canPause;
@property BOOL canTogglePlayback;
@property double rate;
@property BOOL canSeek;
@property NSTimeInterval contentDuration;
@property NSSize contentDimensions;
@property BOOL hasEnabledAudio;
@property BOOL hasEnabledVideo;
@property NSTimeInterval minTime;
@property NSTimeInterval maxTime;
@property NSTimeInterval contentDurationWithinEndTimes;
@property(retain) NSArray *loadedTimeRanges;
@property AVPlayerControllerStatus status;
@property(retain) AVValueTiming *timing;

- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldDismissWithReason:(AVPlayerViewControllerDismissalReason)reason;
@end

@implementation WebAVPlayerController

- (instancetype)init
{
    if (!(self = [super init]))
        return self;
    
    initAVPlayerController();
    self.playerControllerProxy = [[classAVPlayerController alloc] init];
    return self;
}

- (void)dealloc
{
    self.delegate = nil;
    self.playerControllerProxy = nil;
    self.playerLayer = nil;
    self.loadedTimeRanges = nil;
    self.timing = nil;
    [super dealloc];
}

- (id)forwardingTargetForSelector:(SEL)selector
{
    UNUSED_PARAM(selector);
    return self.playerControllerProxy;
}

- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldDismissWithReason:(AVPlayerViewControllerDismissalReason)reason
{
    UNUSED_PARAM(playerViewController);
    UNUSED_PARAM(reason);
    ASSERT(self.delegate);
    self.delegate->requestExitFullScreen();
    return NO;
}

- (void)play:(id)sender
{
    UNUSED_PARAM(sender);
    ASSERT(self.delegate);
    self.delegate->play();
}

- (void)pause:(id)sender
{
    UNUSED_PARAM(sender);
    ASSERT(self.delegate);
    self.delegate->pause();
}

- (void)togglePlayback:(id)sender
{
    UNUSED_PARAM(sender);
    ASSERT(self.delegate);
    self.delegate->togglePlayState();
}

- (BOOL)isPlaying
{
    return [self rate] != 0;
}

- (void)setPlaying:(BOOL)playing
{
    ASSERT(self.delegate);
    if (playing)
        self.delegate->play();
    else
        self.delegate->pause();
    }

+ (NSSet *)keyPathsForValuesAffectingPlaying
{
    return [NSSet setWithObject:@"rate"];
}

- (void)seekToTime:(NSTimeInterval)time
{
    ASSERT(self.delegate);
    self.delegate->seekToTime(time);
}

@end

WebVideoFullscreenInterfaceAVKit::WebVideoFullscreenInterfaceAVKit()
    : m_videoFullscreenModel(nullptr)
{
    initUIScreen();
    initUIView();
    initUIColor();
    initUIWindow();
    initAVPlayerController();
    initAVPlayerViewController();
    initAVValueTiming();
    initUIViewController();
    initAVPlayerLayer();
    
    m_playerController = adoptNS([[WebAVPlayerController alloc] init]);
}

void WebVideoFullscreenInterfaceAVKit::setWebVideoFullscreenModel(WebVideoFullscreenModel* model)
{
    m_videoFullscreenModel = model;
    m_playerController.get().delegate = m_videoFullscreenModel;
}

void WebVideoFullscreenInterfaceAVKit::setDuration(double duration)
{
    WebAVPlayerController* playerController = m_playerController.get();
    
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=127017 use correct values instead of duration for all these
    playerController.contentDuration = duration;
    playerController.maxTime = duration;
    playerController.contentDurationWithinEndTimes = duration;
    playerController.loadedTimeRanges = @[@0, @(duration)];
    
    // FIXME: we take this as an indication that playback is ready.
    playerController.canPlay = YES;
    playerController.canPause = YES;
    playerController.canTogglePlayback = YES;
    playerController.hasEnabledAudio = YES;
    playerController.canSeek = YES;
    playerController.minTime = 0;
    playerController.status = AVPlayerControllerStatusReadyToPlay;
}

void WebVideoFullscreenInterfaceAVKit::setCurrentTime(double currentTime, double anchorTime)
{
    NSTimeInterval anchorTimeStamp = ![m_playerController rate] ? NAN : anchorTime;
    AVValueTiming *timing = [classAVValueTiming valueTimingWithAnchorValue:currentTime
        anchorTimeStamp:anchorTimeStamp rate:0];
    m_playerController.get().timing = timing;
}

void WebVideoFullscreenInterfaceAVKit::setRate(bool isPlaying, float playbackRate)
{
    m_playerController.get().rate = isPlaying ? playbackRate : 0.;
}

void WebVideoFullscreenInterfaceAVKit::setVideoDimensions(bool hasVideo, float width, float height)
{
    m_playerController.get().hasEnabledVideo = hasVideo;
    m_playerController.get().contentDimensions = CGSizeMake(width, height);
}

void WebVideoFullscreenInterfaceAVKit::setVideoLayer(PlatformLayer* videoLayer)
{
    [m_playerController.get().playerLayer removeFromSuperlayer];
    [videoLayer removeFromSuperlayer];
    ASSERT([videoLayer isKindOfClass:[classAVPlayerLayer class]]
        || ([videoLayer isKindOfClass:[CALayer class]] && [videoLayer conformsToProtocol:@protocol(AVPlayerLayer)]));
    m_playerController.get().playerLayer = (CALayer<AVPlayerLayer>*)videoLayer;
}

void WebVideoFullscreenInterfaceAVKit::enterFullscreen(std::function<void()> completion)
{
    __block RefPtr<WebVideoFullscreenInterfaceAVKit> protect(this);
    
    dispatch_async(dispatch_get_main_queue(), ^{
        m_playerViewController = [[[classAVPlayerViewController alloc] init] autorelease];
        m_playerViewController.get().playerController = (AVPlayerController *)m_playerController.get();
        
        if ([m_playerViewController respondsToSelector:@selector(setDelegate:)])
            m_playerViewController.get().delegate = m_playerController.get();
        
        m_viewController = adoptNS([[classUIViewController alloc] init]);
        
        m_window = [[[classUIWindow alloc] initWithFrame:[[classUIScreen mainScreen] bounds]] autorelease];
        m_window.get().backgroundColor = [classUIColor clearColor];
        m_window.get().rootViewController = m_viewController.get();
        [m_window makeKeyAndVisible];
        
        [m_viewController presentViewController:m_playerViewController.get() animated:YES completion:nil];
        completion();
        protect.clear();
    });
}

void WebVideoFullscreenInterfaceAVKit::enterFullscreen()
{
    enterFullscreen(^{ });
}

void WebVideoFullscreenInterfaceAVKit::exitFullscreen(std::function<void()> completion)
{
    m_playerController.clear();
    __block RefPtr<WebVideoFullscreenInterfaceAVKit> protect(this);
    
    dispatch_async(dispatch_get_main_queue(), ^{
        [m_viewController dismissViewControllerAnimated:YES completion:^{
            m_window.get().hidden = YES;
            m_window.get().rootViewController = nil;
            m_playerViewController = nil;
            m_viewController = nil;
            m_window = nil;
            if (m_videoFullscreenModel)
                m_videoFullscreenModel->didExitFullscreen();
            completion();
            protect.clear();
        }];
    });
}

void WebVideoFullscreenInterfaceAVKit::exitFullscreen()
{
    exitFullscreen(^{ });
}

#endif
