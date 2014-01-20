/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#import "WebVideoFullscreenControllerAVKit.h"

#import "DOMEventInternal.h"
#import "Logging.h"
#import <CoreMedia/CMTime.h>
#import <UIKit/UIKit.h>
#import <WebCore/DOMEventListener.h>
#import <WebCore/Event.h>
#import <WebCore/EventListener.h>
#import <WebCore/EventNames.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/ObjCEventListener.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/WebCoreThreadRun.h>
#import <WebCore/RenderElement.h>
#import <wtf/RetainPtr.h>

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

@interface WebAVPlayerController : NSObject <DOMEventListener, AVPlayerViewControllerDelegate>

@property(retain) AVPlayerController* playerControllerProxy;
@property(retain) CALayer<AVPlayerLayer> *playerLayer;

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

- (void)handleEvent:(DOMEvent *)evt;

- (BOOL)playerViewController:(AVPlayerViewController *)playerViewController shouldDismissWithReason:(AVPlayerViewControllerDismissalReason)reason;

@end


@implementation WebAVPlayerController
{
    RefPtr<HTMLMediaElement> _mediaElement;
    RefPtr<EventListener> _listener;
}

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

    WebThreadRun(^{
        _mediaElement->pause();
        _mediaElement->exitFullscreen();
    });
    return NO;
}

- (void)play:(id)sender
{
    UNUSED_PARAM(sender);
    WebThreadRun(^{
        _mediaElement->play();
    });
}

- (void)pause:(id)sender
{
    UNUSED_PARAM(sender);
    WebThreadRun(^{
        _mediaElement->pause();
    });
}

- (void)togglePlayback:(id)sender
{
    UNUSED_PARAM(sender);
    WebThreadRun(^{
        _mediaElement->togglePlayState();
    });
}

- (BOOL)isPlaying
{
    return [self rate] != 0;
}

- (void)setPlaying:(BOOL)playing
{
    WebThreadRun(^{
        if (playing)
            _mediaElement->play();
        else
            _mediaElement->pause();
    });
}

+ (NSSet *)keyPathsForValuesAffectingPlaying
{
    return [NSSet setWithObject:@"rate"];
}

- (void)seekToTime:(NSTimeInterval)time
{
    WebThreadRun(^{
        _mediaElement->setCurrentTime(time);
    });
}

- (void)updateTimingWithCurrentTime:(NSTimeInterval)currentTime
{
    NSTimeInterval anchorTimeStamp = (![self rate]) ? NAN : [classAVValueTiming currentTimeStamp];
    AVValueTiming *timing = [classAVValueTiming valueTimingWithAnchorValue:currentTime
      anchorTimeStamp:anchorTimeStamp rate:0];
    [self setTiming:timing];
}

- (double)effectiveRate
{
    return _mediaElement->isPlaying() ? _mediaElement->playbackRate() : 0.;
}

- (void)updateDuration
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=127017 use correct values instead of duration for all these
    NSTimeInterval duration = _mediaElement->duration();
    self.contentDuration = duration;
    self.maxTime = duration;
    self.contentDurationWithinEndTimes = duration;
    self.loadedTimeRanges = @[@0, @(duration)];
}

- (void)setMediaElement:(PassRefPtr<HTMLMediaElement>)passedMediaElement
{
    RefPtr<HTMLMediaElement> mediaElement = passedMediaElement;

    if (_mediaElement == mediaElement)
        return;
    
    if (_mediaElement && _listener)
    {
        _mediaElement->removeEventListener(eventNames().durationchangeEvent, _listener.get(), false);
        _mediaElement->removeEventListener(eventNames().pauseEvent, _listener.get(), false);
        _mediaElement->removeEventListener(eventNames().playEvent, _listener.get(), false);
        _mediaElement->removeEventListener(eventNames().ratechangeEvent, _listener.get(), false);
        _mediaElement->removeEventListener(eventNames().timeupdateEvent, _listener.get(), false);
    }
    
    if (_mediaElement)
    {
        [self.playerLayer removeFromSuperlayer];
        _mediaElement->returnPlatformLayer(self.playerLayer);
        self.playerLayer = nil;
    }
    
    _mediaElement = mediaElement;
    _listener = nullptr;
    
    if (!_mediaElement)
        return;

    _listener = ObjCEventListener::wrap(self);

    _mediaElement->addEventListener(eventNames().durationchangeEvent, _listener, false);
    _mediaElement->addEventListener(eventNames().pauseEvent, _listener, false);
    _mediaElement->addEventListener(eventNames().playEvent, _listener, false);
    _mediaElement->addEventListener(eventNames().ratechangeEvent, _listener, false);
    _mediaElement->addEventListener(eventNames().timeupdateEvent, _listener, false);

    [self updateDuration];
    self.canPlay = YES;
    self.canPause = YES;
    self.canTogglePlayback = YES;
    self.hasEnabledAudio = YES;
    self.canSeek = YES;
    self.minTime = 0;
    self.rate = [self effectiveRate];
    self.status = AVPlayerControllerStatusReadyToPlay;

    self.playerLayer = (CALayer<AVPlayerLayer> *)_mediaElement->borrowPlatformLayer();
    ASSERT([self.playerLayer isKindOfClass:classAVPlayerLayer]);
    [self.playerLayer removeFromSuperlayer];

    [self updateTimingWithCurrentTime:_mediaElement->currentTime()];

    if (isHTMLVideoElement(_mediaElement.get())) {
        HTMLVideoElement *videoElement = toHTMLVideoElement(_mediaElement.get());
        self.hasEnabledVideo = YES;
        self.contentDimensions = CGSizeMake(videoElement->videoWidth(), videoElement->videoHeight());
    }
}

- (void)handleEvent:(DOMEvent *)evnt
{
    Event* event = core(evnt);
    
    if (!_mediaElement)
        return;

    LOG(Media, "handleEvent %s", event->type().characters8());

    if (event->type() == eventNames().durationchangeEvent)
        [self updateDuration];
    else if (event->type() == eventNames().pauseEvent
        || event->type() == eventNames().playEvent
        || event->type() == eventNames().ratechangeEvent)
        self.rate = [self effectiveRate];
    else if (event->type() == eventNames().timeupdateEvent)
        [self updateTimingWithCurrentTime:_mediaElement->currentTime()];
}

@end

@interface WebVideoFullscreenController () <UIViewControllerTransitioningDelegate>
@property(retain) UIWindow *window;
@property(retain) UIViewController *viewController;
@property(retain) AVPlayerViewController *playerViewController;
@property(retain) WebAVPlayerController *playerController;
@end

@implementation WebVideoFullscreenController
{
    RefPtr<HTMLMediaElement> _mediaElement;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    initUIScreen();
    initUIView();
    initUIColor();
    initUIWindow();
    initAVPlayerController();
    initAVPlayerViewController();
    initAVValueTiming();
    initUIViewController();
    initAVPlayerLayer();
    return self;
}

- (void)dealloc
{
    self.window = nil;
    self.viewController = nil;
    self.playerViewController = nil;
    self.playerController = nil;
    _mediaElement = nullptr;
    [super dealloc];
}

- (void)setMediaElement:(HTMLMediaElement*)mediaElement
{
    _mediaElement = mediaElement;
}

- (HTMLMediaElement*)mediaElement
{
    return _mediaElement.get();
}

- (void)enterFullscreen:(UIScreen *)screen
{
    if (!screen)
        screen = [classUIScreen mainScreen];

    self.playerController = [[[WebAVPlayerController alloc] init] autorelease];
    self.playerController.mediaElement = _mediaElement;

    dispatch_async(dispatch_get_main_queue(), ^{
        self.playerViewController = [[[classAVPlayerViewController alloc] init] autorelease];
        self.playerViewController.playerController = (AVPlayerController *)self.playerController;
        
        if ([self.playerViewController respondsToSelector:@selector(setDelegate:)])
            self.playerViewController.delegate = self.playerController;
       
        self.viewController = [[[classUIViewController alloc] init] autorelease];
        
        self.window = [[[classUIWindow alloc] initWithFrame:[screen bounds]] autorelease];
        self.window.backgroundColor = [classUIColor clearColor];
        self.window.rootViewController = (UIViewController *)self.viewController;
        [self.window makeKeyAndVisible];
        
        [self.viewController presentViewController:self.playerViewController animated:YES completion:nil];
    });
}

- (void)exitFullscreen
{
    self.playerController.mediaElement = nullptr;

    dispatch_async(dispatch_get_main_queue(), ^{
        [self.viewController dismissViewControllerAnimated:YES completion:^{
            self.window.hidden = YES;
            self.window.rootViewController = nil;
            
            self.playerViewController = nil;
            self.viewController = nil;
            self.window = nil;
            WebThreadRun(^{
                _mediaElement = nullptr;
            });
            
        }];
    });
}

@end

#endif
