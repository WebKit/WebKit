/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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

#import "WebVideoFullscreenController.h"

#if ENABLE(VIDEO) && PLATFORM(MAC)

#import <AVFoundation/AVPlayer.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/PlaybackSessionInterfaceAVKitLegacy.h>
#import <WebCore/PlaybackSessionModelMediaElement.h>
#import <WebCore/WebAVPlayerController.h>
#import <WebCore/WebCoreFullScreenWindow.h>
#import <objc/message.h>
#import <objc/runtime.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <pal/spi/mac/NSWindowSPI.h>
#import <wtf/RetainPtr.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS(AVKit, AVPlayerView)

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

@interface AVPlayerView (SecretStuff)
@property (nonatomic, assign) BOOL showsAudioOnlyIndicatorView;
@end

@interface WebVideoFullscreenOverlayLayer : CALayer
@end

@implementation WebVideoFullscreenOverlayLayer
- (void)layoutSublayers
{
    for (CALayer* layer in self.sublayers)
        layer.frame = self.bounds;
}
@end

@class WebAVPlayerView;

@protocol WebAVPlayerViewDelegate
- (BOOL)playerViewIsFullScreen:(WebAVPlayerView*)playerView;
- (void)playerViewRequestEnterFullscreen:(WebAVPlayerView*)playerView;
- (void)playerViewRequestExitFullscreen:(WebAVPlayerView*)playerView;
@end

@interface WebAVPlayerView : AVPlayerView
@property (weak) id<WebAVPlayerViewDelegate> webDelegate;
@end

static id<WebAVPlayerViewDelegate> WebAVPlayerView_webDelegate(id aSelf, SEL)
{
    void* webDelegate = nil;
    object_getInstanceVariable(aSelf, "_webDelegate", &webDelegate);
    return static_cast<id<WebAVPlayerViewDelegate>>(webDelegate);
}

static void WebAVPlayerView_setWebDelegate(id aSelf, SEL, id<WebAVPlayerViewDelegate> webDelegate)
{
    object_setInstanceVariable(aSelf, "_webDelegate", webDelegate);
}

static BOOL WebAVPlayerView_isFullScreen(id aSelf, SEL)
{
    WebAVPlayerView *playerView = aSelf;
    return [playerView.webDelegate playerViewIsFullScreen:playerView];
}

static void WebAVPlayerView_enterFullScreen(id aSelf, SEL, id sender)
{
    WebAVPlayerView *playerView = aSelf;
    [playerView.webDelegate playerViewRequestEnterFullscreen:playerView];
}

static void WebAVPlayerView_exitFullScreen(id aSelf, SEL, id sender)
{
    WebAVPlayerView *playerView = aSelf;
    [playerView.webDelegate playerViewRequestExitFullscreen:playerView];
}

static WebAVPlayerView *allocWebAVPlayerViewInstance()
{
    static Class theClass = [] {
        ASSERT(getAVPlayerViewClassSingleton());
        Class aClass = objc_allocateClassPair(getAVPlayerViewClassSingleton(), "WebAVPlayerView", 0);
        Class theClass = aClass;
        class_addMethod(theClass, @selector(setWebDelegate:), (IMP)WebAVPlayerView_setWebDelegate, "v@:@");
        class_addMethod(theClass, @selector(webDelegate), (IMP)WebAVPlayerView_webDelegate, "@@:");
        class_addMethod(theClass, @selector(isFullScreen), (IMP)WebAVPlayerView_isFullScreen, "B@:");
        class_addMethod(theClass, @selector(enterFullScreen:), (IMP)WebAVPlayerView_enterFullScreen, "v@:@");
        class_addMethod(theClass, @selector(exitFullScreen:), (IMP)WebAVPlayerView_exitFullScreen, "v@:@");

        class_addIvar(theClass, "_webDelegate", sizeof(id), log2(sizeof(id)), "@");
        class_addIvar(theClass, "_webIsFullScreen", sizeof(BOOL), log2(sizeof(BOOL)), "B");

        objc_registerClassPair(theClass);
        return theClass;
    }();
    return (WebAVPlayerView *)[theClass alloc];
}

@interface WebVideoFullscreenController () <WebAVPlayerViewDelegate, NSWindowDelegate> {
    RefPtr<WebCore::PlaybackSessionModelMediaElement> _playbackModel;
    RefPtr<WebCore::PlaybackSessionInterfaceIOS> _playbackInterface;
    RetainPtr<NSView> _contentOverlay;
    RetainPtr<WebAVPlayerView> _playerView;
    BOOL _isFullScreen;
}
@property (readonly) WebCoreFullScreenWindow* fullscreenWindow;
@end

@implementation WebVideoFullscreenController

- (id)init
{
    // Do not defer window creation, to make sure -windowNumber is created (needed by WebWindowScaleAnimation).
    auto window = adoptNS([[WebCoreFullScreenWindow alloc] initWithContentRect:NSZeroRect styleMask:(NSWindowStyleMaskFullSizeContentView | NSWindowStyleMaskResizable) backing:NSBackingStoreBuffered defer:NO]);
    [window setCollectionBehavior:([window collectionBehavior] | NSWindowCollectionBehaviorFullScreenPrimary)];
    [window setDelegate: self];
    self = [super initWithWindow:window.get()];
    if (!self)
        return nil;
    _playbackModel = WebCore::PlaybackSessionModelMediaElement::create();
    _playbackInterface = WebCore::PlaybackSessionInterfaceAVKitLegacy::create(*_playbackModel);
    _contentOverlay = adoptNS([[NSView alloc] initWithFrame:NSZeroRect]);
    _contentOverlay.get().layerContentsRedrawPolicy = NSViewLayerContentsRedrawNever;
    _contentOverlay.get().layer = adoptNS([[WebVideoFullscreenOverlayLayer alloc] init]).get();
    [_contentOverlay setWantsLayer:YES];
    [_contentOverlay setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    [self windowDidLoad];

    return self;
}
- (void)dealloc
{
    ASSERT(!_backgroundFullscreenWindow);
    ASSERT(!_fadeAnimation);
    _playerView.get().webDelegate = nil;
    _playbackModel = nil;
    [super dealloc];
}

- (WebAVPlayerView *)playerView
{
    return _playerView.get();
}

- (WebCoreFullScreenWindow *)fullscreenWindow
{
    return (WebCoreFullScreenWindow *)[super window];
}

- (void)windowDidLoad
{
    auto window = [self fullscreenWindow];

    [window setHasShadow:YES]; // This is nicer with a shadow.
    [window setLevel:NSPopUpMenuWindowLevel-1];

    _playerView = adoptNS([allocWebAVPlayerViewInstance() initWithFrame:window.contentLayoutRect]);
    _playerView.get().controlsStyle = AVPlayerViewControlsStyleNone;
    _playerView.get().showsFullScreenToggleButton = YES;
    _playerView.get().showsAudioOnlyIndicatorView = NO;
    _playerView.get().webDelegate = self;
    window.contentView = _playerView.get();
    [_contentOverlay setFrame:_playerView.get().contentOverlayView.bounds];
    [_playerView.get().contentOverlayView addSubview:_contentOverlay.get()];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationDidResignActive:) name:NSApplicationDidResignActiveNotification object:NSApp];
}

- (NakedPtr<WebCore::HTMLVideoElement>)videoElement
{
    return _videoElement.get();
}

// FIXME: This method is not really a setter. The caller relies on its side effects, and it's
// called once each time we enter full screen. So it should have a different name.
- (void)setVideoElement:(NakedPtr<WebCore::HTMLVideoElement>)videoElement
{
    ASSERT(videoElement);
    _videoElement = videoElement;

    if (![self isWindowLoaded])
        return;

    _playbackModel->setMediaElement(videoElement);
    self.playerView.playerController = (AVPlayerController*)_playbackInterface->playerController();
}

- (void)enterFullscreen:(NSScreen *)screen
{
    if (!_videoElement)
        return;
    [NSAnimationContext beginGrouping];
    _videoElement->setVideoFullscreenLayer(_contentOverlay.get().layer, [self, protectedSelf = retainPtr(self)] {
        [self.fullscreenWindow setFrame:self.videoElementRect display:YES];
        [self.fullscreenWindow makeKeyAndOrderFront:self];
        [self.fullscreenWindow enterFullScreenMode:self];
        [NSAnimationContext endGrouping];
    });
}

- (void)exitFullscreen
{
    [self.fullscreenWindow exitFullScreenMode:self];
}

- (NSRect)videoElementRect
{
    return _videoElement->screenRect();
}

- (void)applicationDidResignActive:(NSNotification*)notification
{
    UNUSED_PARAM(notification);
    NSWindow* fullscreenWindow = [self fullscreenWindow];

    // Replicate the QuickTime Player (X) behavior when losing active application status:
    // Is the fullscreen screen the main screen? (Note: this covers the case where only a
    // single screen is available.)  Is the fullscreen screen on the current space? IFF so,
    // then exit fullscreen mode.
    if (fullscreenWindow.screen == [NSScreen screens][0] && fullscreenWindow.onActiveSpace)
        [self _requestExit];
}

- (void)_requestExit
{
    [self.fullscreenWindow exitFullScreenMode:self];
}

- (void)_requestEnter
{
    if (_videoElement)
        _videoElement->enterFullscreen();
}

- (void)cancelOperation:(id)sender
{
    [self _requestExit];
}

- (BOOL)playerViewIsFullScreen:(WebAVPlayerView*)playerView
{
    return _isFullScreen;
}

- (void)playerViewRequestEnterFullscreen:(AVPlayerView*)playerView
{
    [self _requestEnter];
}

- (void)playerViewRequestExitFullscreen:(AVPlayerView*)playerView
{
    [self _requestExit];
}

- (nullable NSArray<NSWindow *> *)customWindowsToEnterFullScreenForWindow:(NSWindow *)window
{
    return @[self.fullscreenWindow];
}

- (void)window:(NSWindow *)window startCustomAnimationToEnterFullScreenWithDuration:(NSTimeInterval)duration
{
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        context.allowsImplicitAnimation = YES;
        context.duration = duration;
        [window setFrame:window.screen.frame display:YES];
    } completionHandler:NULL];
}

- (nullable NSArray<NSWindow *> *)customWindowsToExitFullScreenForWindow:(NSWindow *)window
{
    return @[self.fullscreenWindow];
}

- (void)window:(NSWindow *)window startCustomAnimationToExitFullScreenWithDuration:(NSTimeInterval)duration
{
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        context.allowsImplicitAnimation = YES;
        context.duration = duration;
        [window setFrame:self.videoElementRect display:YES];
    } completionHandler:NULL];
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    _playerView.get().controlsStyle = AVPlayerViewControlsStyleFloating;
    [_playerView willChangeValueForKey:@"isFullScreen"];
    _isFullScreen = YES;
    [_playerView didChangeValueForKey:@"isFullScreen"];
    if (_videoElement)
        _videoElement->didBecomeFullscreenElement();
}

- (void)windowWillExitFullScreen:(NSNotification *)notification
{
    _playerView.get().controlsStyle = AVPlayerViewControlsStyleNone;
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    [_playerView willChangeValueForKey:@"isFullScreen"];
    _isFullScreen = NO;
    [_playerView didChangeValueForKey:@"isFullScreen"];

    if (!_videoElement) {
        [self.fullscreenWindow close];
        return;
    }

    [NSAnimationContext beginGrouping];
    _videoElement->setVideoFullscreenLayer(nil, [self, protectedSelf = retainPtr(self)] {
        [self.fullscreenWindow close];
        [NSAnimationContext endGrouping];
    });

    if (_videoElement->isFullscreen())
        _videoElement->exitFullscreen();
}

@end

ALLOW_DEPRECATED_DECLARATIONS_END

#endif
