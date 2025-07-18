/*
 * Copyright (C) 2009-2023 Apple Inc. All rights reserved.
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
#import "WKFullScreenWindowController.h"

#if ENABLE(FULLSCREEN_API) && !PLATFORM(IOS_FAMILY)

#import "AppKitSPI.h"
#import "GPUProcessProxy.h"
#import "LayerTreeContext.h"
#import "NativeWebMouseEvent.h"
#import "VideoPresentationManagerProxy.h"
#import "WKAPICast.h"
#import "WKViewInternal.h"
#import "WKViewPrivate.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/CGWindowUtilities.h>
#import <WebCore/FloatRect.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/IntRect.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/VideoPresentationInterfaceMac.h>
#import <WebCore/VideoPresentationModel.h>
#import <WebCore/WebCoreFullScreenPlaceholderView.h>
#import <WebCore/WebCoreFullScreenWindow.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/NSWindowSPI.h>
#import <pal/system/SleepDisabler.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/LoggerHelper.h>

static const NSTimeInterval DefaultWatchdogTimerInterval = 1;

@interface WKFullScreenWindowController (VideoPresentationManagerProxyClient)
- (void)didEnterPictureInPicture;
- (void)didExitPictureInPicture;
@end

enum FullScreenState : NSInteger {
    NotInFullScreen,
    WaitingToEnterFullScreen,
    EnteringFullScreen,
    InFullScreen,
    WaitingToExitFullScreen,
    ExitingFullScreen,
};

@interface WKFullScreenWindowController (Private) <NSAnimationDelegate>
- (void)_replaceView:(NSView *)view with:(NSView *)otherView;
- (WebKit::WebFullScreenManagerProxy *)_manager;
- (RefPtr<WebKit::WebFullScreenManagerProxy>)_protectedManager;
- (void)_startEnterFullScreenAnimationWithDuration:(NSTimeInterval)duration;
- (void)_startExitFullScreenAnimationWithDuration:(NSTimeInterval)duration;
@end

#if !RELEASE_LOG_DISABLED
@interface WKFullScreenWindowController (Logging)
@property (readonly, nonatomic) uint64_t logIdentifier;
@property (readonly, nonatomic) const Logger* loggerPtr;
@property (readonly, nonatomic) WTFLogChannel* logChannel;
@end
#endif

static NSRect convertRectToScreen(NSWindow *window, NSRect rect)
{
    return [window convertRectToScreen:rect];
}

static void makeResponderFirstResponderIfDescendantOfView(NSWindow *window, NSResponder *responder, NSView *view)
{
    if (auto *responderView = dynamic_objc_cast<NSView>(responder); responderView && [responderView isDescendantOf:view])
        [window makeFirstResponder:responder];
}

@implementation WKFullScreenWindowController {
    std::unique_ptr<WebKit::VideoPresentationManagerProxy::VideoInPictureInPictureDidChangeObserver> _pipObserver;

#if !RELEASE_LOG_DISABLED
    RefPtr<Logger> _logger;
    uint64_t _logIdentifier;
#endif
}

#pragma mark -
#pragma mark Initialization
- (id)initWithWindow:(NSWindow *)window webView:(NSView *)webView page:(std::reference_wrapper<WebKit::WebPageProxy>)pageWrapper
{
    self = [super initWithWindow:window];
    if (!self)
        return nil;
    Ref page = pageWrapper.get();
    [window setDelegate:self];
    [window setCollectionBehavior:([window collectionBehavior] | NSWindowCollectionBehaviorFullScreenPrimary | NSWindowCollectionBehaviorStationary)];

    // Hide the titlebar during the animation to full screen so that only the WKWebView content is visible.
    window.titlebarAlphaValue = 0;
    window.animationBehavior = NSWindowAnimationBehaviorNone;

    RetainPtr contentView = [window contentView];
    contentView.get().hidden = YES;
    contentView.get().autoresizesSubviews = YES;

    _backgroundView = adoptNS([[NSView alloc] initWithFrame:contentView.get().bounds]);
    _backgroundView.get().layer = [CALayer layer];
    _backgroundView.get().wantsLayer = YES;
    _backgroundView.get().autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [contentView addSubview:_backgroundView.get()];

    _clipView = adoptNS([[NSView alloc] initWithFrame:contentView.get().bounds]);
    [_clipView setWantsLayer:YES];
    [_clipView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    [_backgroundView addSubview:_clipView.get()];

    [self windowDidLoad];
    [window displayIfNeeded];
    _webView = webView;
    _page = page.get();

#if !RELEASE_LOG_DISABLED
    _logger = page.get().logger();
    _logIdentifier = page.get().logIdentifier();
#endif

    [self videoControlsManagerDidChange];

    return self;
}

- (void)dealloc
{
    [[self window] setDelegate:nil];
    
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    if (_enterFullScreenCompletionHandler)
        _enterFullScreenCompletionHandler(false);
    if (_beganExitFullScreenCompletionHandler)
        _beganExitFullScreenCompletionHandler();
    if (_exitFullScreenCompletionHandler)
        _exitFullScreenCompletionHandler();

    [super dealloc];
}

#pragma mark -
#pragma mark Accessors

@synthesize initialFrame = _initialFrame;
@synthesize finalFrame = _finalFrame;

- (BOOL)isFullScreen
{
    return _fullScreenState == WaitingToEnterFullScreen
        || _fullScreenState == EnteringFullScreen
        || _fullScreenState == InFullScreen;
}

- (WebCoreFullScreenPlaceholderView*)webViewPlaceholder
{
    return _webViewPlaceholder.get();
}

- (void)setSavedConstraints:(NSArray *)savedConstraints
{
    _savedConstraints = savedConstraints;
}

- (NSArray *)savedConstraints
{
    return _savedConstraints.get();
}

#pragma mark -
#pragma mark NSWindowController overrides

- (void)cancelOperation:(id)sender
{
    // If the page doesn't respond in DefaultWatchdogTimerInterval seconds, it could be because
    // the WebProcess has hung, so exit anyway.
    if (!_watchdogTimer) {
        [self _protectedManager]->requestExitFullScreen();
        _watchdogTimer = adoptNS([[NSTimer alloc] initWithFireDate:[NSDate dateWithTimeIntervalSinceNow:DefaultWatchdogTimerInterval] interval:0 target:self selector:@selector(_watchdogTimerFired:) userInfo:nil repeats:NO]);
        [[NSRunLoop mainRunLoop] addTimer:_watchdogTimer.get() forMode:NSDefaultRunLoopMode];
    }
}

#pragma mark -
#pragma mark NSResponder overrides

- (void)noResponderFor:(SEL)eventMethod
{
    // The default behavior of the last link in the responder chain is to call NSBeep() if the
    // event in question is a -keyDown:. Adding a no-op override of -noResponderFor: in a subclass
    // of NSWindowController, which is typically the last link in a responder chain, avoids the
    // NSBeep() when -keyDown: goes unhandled.
    UNUSED_PARAM(eventMethod);
}

#pragma mark -
#pragma mark Exposed Interface

static RetainPtr<CGDataProviderRef> createImageProviderWithCopiedData(CGDataProviderRef sourceProvider)
{
    RetainPtr<CFDataRef> data = adoptCF(CGDataProviderCopyData(sourceProvider));
    return adoptCF(CGDataProviderCreateWithCFData(data.get()));
}

static RetainPtr<CGImageRef> createImageWithCopiedData(CGImageRef sourceImage)
{
    size_t width = CGImageGetWidth(sourceImage);
    size_t height = CGImageGetHeight(sourceImage);
    size_t bitsPerComponent = CGImageGetBitsPerComponent(sourceImage);
    size_t bitsPerPixel = CGImageGetBitsPerPixel(sourceImage);
    size_t bytesPerRow = CGImageGetBytesPerRow(sourceImage);
    RetainPtr<CGColorSpaceRef> colorSpace = CGImageGetColorSpace(sourceImage);
    CGBitmapInfo bitmapInfo = CGImageGetBitmapInfo(sourceImage);
    RetainPtr<CGDataProviderRef> provider = createImageProviderWithCopiedData(CGImageGetDataProvider(sourceImage));
    bool shouldInterpolate = CGImageGetShouldInterpolate(sourceImage);
    CGColorRenderingIntent intent = CGImageGetRenderingIntent(sourceImage);

    return adoptCF(CGImageCreate(width, height, bitsPerComponent, bitsPerPixel, bytesPerRow, colorSpace.get(), bitmapInfo, provider.get(), 0, shouldInterpolate, intent));
}

- (void)_continueEnteringFullscreenAfterPostingNotification:(CompletionHandler<void(bool)>&&)completionHandler
{
    if ([self isFullScreen])
        return completionHandler(false);
    _fullScreenState = WaitingToEnterFullScreen;

    RetainPtr screen = [NSScreen mainScreen];

    NSRect screenFrame = WebCore::safeScreenFrame(screen.get());
    NSRect webViewFrame = convertRectToScreen([_webView window], [_webView convertRect:[_webView frame] toView:nil]);

    // Flip coordinate system:
    webViewFrame.origin.y = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - NSMaxY(webViewFrame);

    CGWindowID windowID = [[_webView window] windowNumber];
    RetainPtr webViewContents = WebCore::cgWindowListCreateImage(NSRectToCGRect(webViewFrame), kCGWindowListOptionIncludingWindow, windowID, kCGWindowImageShouldBeOpaque);

    // Using the returned CGImage directly would result in calls to the WindowServer every time
    // the image was painted. Instead, copy the image data into our own process to eliminate that
    // future overhead.
    webViewContents = createImageWithCopiedData(webViewContents.get());

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[self window] setAutodisplay:NO];
ALLOW_DEPRECATED_DECLARATIONS_END

    RefPtr page = _page.get();
    page->startDeferringResizeEvents();
    page->startDeferringScrollEvents();
    _savedObscuredContentInsets = page->obscuredContentInsets();
    page->setObscuredContentInsets({ });
    [[self window] setFrame:screenFrame display:NO];

    // Painting is normally suspended when the WKView is removed from the window, but this is
    // unnecessary in the full-screen animation case, and can cause bugs; see
    // https://bugs.webkit.org/show_bug.cgi?id=88940 and https://bugs.webkit.org/show_bug.cgi?id=88374
    // We will resume the normal behavior in -finishedEnterFullScreenAnimation:
    page->setSuppressVisibilityUpdates(true);

    // Swap the webView placeholder into place.
    if (!_webViewPlaceholder)
        _webViewPlaceholder = adoptNS([[WebCoreFullScreenPlaceholderView alloc] initWithFrame:[_webView frame]]);
    [_webViewPlaceholder setTarget:nil];
    [_webViewPlaceholder setContents:(__bridge id)webViewContents.get()];
    [self _saveConstraintsOf:[_webView superview]];
    [self _replaceView:_webView.get().get() with:_webViewPlaceholder.get()];
    
    // Then insert the WebView into the full screen window
    RetainPtr contentView = [[self window] contentView];
    [_clipView addSubview:_webView.get().get() positioned:NSWindowBelow relativeTo:nil];
    auto obscuredContentInsets = page->obscuredContentInsets();
    [_webView setFrame:NSInsetRect(contentView.get().bounds, -obscuredContentInsets.left(), -obscuredContentInsets.top())];

    _savedScale = page->pageScaleFactor();
    page->scalePageRelativeToScrollPosition(1, { });
    [self _protectedManager]->setAnimatingFullScreen(true);
    completionHandler(true);
}

- (void)enterFullScreen:(CompletionHandler<void(bool)>&&)completionHandler
{
#if ENABLE(GPU_PROCESS)
    RefPtr gpuProcess = WebKit::GPUProcessProxy::singletonIfCreated();
    if (!gpuProcess)
        return completionHandler(false);

    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);

    gpuProcess->postWillTakeSnapshotNotification([self, protectedSelf = RetainPtr { self }, completionHandler = WTFMove(completionHandler), logIdentifier = OBJC_LOGIDENTIFIER] () mutable {
        OBJC_ALWAYS_LOG(logIdentifier, " - finished posting snapshot notification");

        [protectedSelf _continueEnteringFullscreenAfterPostingNotification:WTFMove(completionHandler)];
    });
#else
    [self _continueEnteringFullscreenAfterPostingNotification:WTFMove(completionHandler)];
#endif
}

- (void)beganEnterFullScreenWithInitialFrame:(NSRect)initialFrame finalFrame:(NSRect)finalFrame completionHandler:(CompletionHandler<void(bool)>&&)completionHandler
{
    if (_fullScreenState != WaitingToEnterFullScreen) {
        OBJC_ERROR_LOG(OBJC_LOGIDENTIFIER, "fullScreenState is not WaitingToEnterFullScreen! Bailing");
        return completionHandler(false);
    }
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);
    _enterFullScreenCompletionHandler = WTFMove(completionHandler);
    _fullScreenState = EnteringFullScreen;

    _initialFrame = initialFrame;
    _finalFrame = finalFrame;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    RetainPtr<CALayer> clipLayer = _clipView.get().layer;
    RetainPtr contentView = [[self window] contentView];

    // Give the initial animations a speed of "0". We want the animations in place when we order in
    // the window, but to not start animating until we get the callback from AppKit with the required
    // animation duration. These animations will be replaced with the final animations in
    // -_startEnterFullScreenAnimationWithDuration:
    [clipLayer addAnimation:zoomAnimation(_initialFrame, _finalFrame, self.window.screen.frame, 1, 0, AnimateIn).get() forKey:@"fullscreen"];
    clipLayer.get().mask = createMask(contentView.get().bounds).get();
    [clipLayer.get().mask addAnimation:maskAnimation(_initialFrame, _finalFrame, self.window.screen.frame, 1, 0, AnimateIn).get() forKey:@"fullscreen"];
    contentView.get().hidden = NO;

    RetainPtr<NSWindow> window = self.window;
    NSWindowCollectionBehavior behavior = [window collectionBehavior];
    [window setCollectionBehavior:(behavior | NSWindowCollectionBehaviorCanJoinAllSpaces)];
    [window makeFirstResponder:_webView.get().get()];
    [window makeKeyAndOrderFront:self];
    [window setCollectionBehavior:behavior];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[self window] setAutodisplay:YES];
ALLOW_DEPRECATED_DECLARATIONS_END
    [[self window] displayIfNeeded];

    [CATransaction commit];

    [self.window enterFullScreenMode:self];
}

static const float minVideoWidth = 468; // Keep in sync with `--controls-bar-width`.

- (void)finishedEnterFullScreenAnimation:(bool)completed
{
    if (_fullScreenState != EnteringFullScreen)
        return;
    
    RefPtr manager = [self _manager];
    RefPtr page = _page.get();
    if (completed) {
        _fullScreenState = InFullScreen;

        if (_enterFullScreenCompletionHandler)
            _enterFullScreenCompletionHandler(true);
        manager->setAnimatingFullScreen(false);
        page->setSuppressVisibilityUpdates(false);

        [_backgroundView.get().layer removeAllAnimations];
        [[_clipView layer] removeAllAnimations];
        [[_clipView layer] setMask:nil];

        [_webViewPlaceholder setExitWarningVisible:YES];
        [_webViewPlaceholder setTarget:self];

        NSSize minContentSize = self.window.contentMinSize;
        minContentSize.width = minVideoWidth;
        self.window.contentMinSize = minContentSize;

        // Always show the titlebar in full screen mode.
        self.window.titlebarAlphaValue = 1;
    } else {
        // Transition to fullscreen failed. Clean up.
        _fullScreenState = NotInFullScreen;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [[self window] setAutodisplay:YES];
ALLOW_DEPRECATED_DECLARATIONS_END
        page->setSuppressVisibilityUpdates(false);

        RetainPtr firstResponder = [[self window] firstResponder];
        [self _replaceView:_webViewPlaceholder.get() with:_webView.get().get()];
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [NSLayoutConstraint activateConstraints:self.savedConstraints];
        END_BLOCK_OBJC_EXCEPTIONS
        self.savedConstraints = nil;
        makeResponderFirstResponderIfDescendantOfView([_webView window], firstResponder.get(), _webView.get().get());
        [[_webView window] makeKeyAndOrderFront:self];

        page->scalePageRelativeToScrollPosition(_savedScale, { });
        page->setObscuredContentInsets(_savedObscuredContentInsets);
        manager->setAnimatingFullScreen(false);
        manager->requestExitFullScreen();

        // FIXME(53342): remove once pointer events fire when elements move out from under the pointer.
        RetainPtr fakeEvent = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
            location:[NSEvent mouseLocation]
            modifierFlags:[[NSApp currentEvent] modifierFlags]
            timestamp:[NSDate timeIntervalSinceReferenceDate]
            windowNumber:[[_webView window] windowNumber]
            context:nullptr
            eventNumber:0
            clickCount:0
            pressure:0];
        WebKit::NativeWebMouseEvent webEvent(fakeEvent.get(), nil, _webView.get().get());
        page->handleMouseEvent(webEvent);
    }
    page->flushDeferredResizeEvents();
    page->flushDeferredScrollEvents();

    if (_exitFullScreenCompletionHandler)
        [self exitFullScreen:WTFMove(_exitFullScreenCompletionHandler)];
}

- (void)exitFullScreen:(CompletionHandler<void()>&&)completionHandler
{
    if (_fullScreenState == EnteringFullScreen
        || _fullScreenState == WaitingToEnterFullScreen) {
        // Do not try to exit fullscreen during the enter animation; remember
        // that exit was requested and perform the exit upon enter fullscreen
        // animation complete.
        _exitFullScreenCompletionHandler = WTFMove(completionHandler);
        return;
    }

    if (_watchdogTimer) {
        [_watchdogTimer invalidate];
        _watchdogTimer.clear();
    }

    if (![self isFullScreen])
        return completionHandler();
    _fullScreenState = WaitingToExitFullScreen;

    [_webViewPlaceholder setExitWarningVisible:NO];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[self window] setAutodisplay:NO];
ALLOW_DEPRECATED_DECLARATIONS_END

    // See the related comment in enterFullScreen:
    // We will resume the normal behavior in _startExitFullScreenAnimationWithDuration:
    RefPtr page = _page.get();
    page->setSuppressVisibilityUpdates(true);
    page->startDeferringResizeEvents();
    page->startDeferringScrollEvents();
    [_webViewPlaceholder setTarget:nil];

    [self _protectedManager]->setAnimatingFullScreen(true);
    completionHandler();
}

- (void)exitFullScreenImmediately
{
    if (_fullScreenState == NotInFullScreen)
        return;

    [self _protectedManager]->requestExitFullScreen();
    [_webViewPlaceholder setExitWarningVisible:NO];
    _fullScreenState = ExitingFullScreen;
    [self finishedExitFullScreenAnimationAndExitImmediately:YES];
}

- (void)requestExitFullScreen
{
    [self _protectedManager]->requestExitFullScreen();
}

- (void)beganExitFullScreenWithInitialFrame:(NSRect)initialFrame finalFrame:(NSRect)finalFrame completionHandler:(CompletionHandler<void()>&&)completionHandler
{
    if (_fullScreenState != WaitingToExitFullScreen)
        return completionHandler();
    _fullScreenState = ExitingFullScreen;
    _beganExitFullScreenCompletionHandler = WTFMove(completionHandler);

    if (![[self window] isOnActiveSpace]) {
        // If the full screen window is not in the active space, the NSWindow full screen animation delegate methods
        // will never be called. So call finishedExitFullScreenAnimationAndExitImmediately explicitly.
        [self finishedExitFullScreenAnimationAndExitImmediately:NO];
    }

    [[self window] exitFullScreenMode:self];
}

WTF_DECLARE_CF_TYPE_TRAIT(CGImage);

static RetainPtr<CGImageRef> takeWindowSnapshot(CGSWindowID windowID, bool captureAtNominalResolution)
{
    CGSWindowCaptureOptions options = kCGSCaptureIgnoreGlobalClipShape;
    if (captureAtNominalResolution)
        options |= kCGSWindowCaptureNominalResolution;
    RetainPtr<CFArrayRef> windowSnapshotImages = adoptCF(CGSHWCaptureWindowList(CGSMainConnectionID(), &windowID, 1, options));

    if (windowSnapshotImages && CFArrayGetCount(windowSnapshotImages.get()))
        return checked_cf_cast<CGImageRef>(CFArrayGetValueAtIndex(windowSnapshotImages.get(), 0));

    // Fall back to the non-hardware capture path if we didn't get a snapshot
    // (which usually happens if the window is fully off-screen).
    CGWindowImageOption imageOptions = kCGWindowImageBoundsIgnoreFraming | kCGWindowImageShouldBeOpaque;
    if (captureAtNominalResolution)
        imageOptions |= kCGWindowImageNominalResolution;
    return WebCore::cgWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, windowID, imageOptions);
}

- (void)_continueExitingFullscreenAfterPostingNotificationAndExitImmediately:(bool)immediately
{
    RefPtr manager = [self _manager];
    if (!manager)
        return;

    if (_fullScreenState == InFullScreen) {
        // If we are currently in the InFullScreen state, this notification is unexpected, meaning
        // fullscreen was exited without being initiated by WebKit. Do not return early, but continue to
        // clean up our state by calling those methods which would have been called by -exitFullscreen,
        // and proceed to close the fullscreen window.
        manager->requestExitFullScreen();
        [_webViewPlaceholder setTarget:nil];
        manager->setAnimatingFullScreen(false);
    } else if (_fullScreenState != ExitingFullScreen)
        return;
    _fullScreenState = NotInFullScreen;

    // Hide the titlebar at the end of the animation so that it can slide away without turning blank.
    self.window.titlebarAlphaValue = 0;

    RetainPtr firstResponder = [[self window] firstResponder];

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    NSRect exitPlaceholderScreenRect = _initialFrame;
    exitPlaceholderScreenRect.origin.y = NSMaxY(WebCore::safeScreenFrame([[NSScreen screens] objectAtIndex:0])) - NSMaxY(exitPlaceholderScreenRect);

    RetainPtr<CGImageRef> webViewContents = takeWindowSnapshot([[_webView window] windowNumber], true);
    webViewContents = adoptCF(CGImageCreateWithImageInRect(webViewContents.get(), NSRectToCGRect(exitPlaceholderScreenRect)));
    
    _exitPlaceholder = adoptNS([[NSView alloc] initWithFrame:[_webView frame]]);
    [_exitPlaceholder setWantsLayer: YES];
    [_exitPlaceholder setAutoresizesSubviews: YES];
    [_exitPlaceholder setLayerContentsPlacement: NSViewLayerContentsPlacementScaleProportionallyToFit];
    [_exitPlaceholder setLayerContentsRedrawPolicy: NSViewLayerContentsRedrawNever];
    [_exitPlaceholder setFrame:[_webView frame]];
    [[_exitPlaceholder layer] setContents:(__bridge id)webViewContents.get()];
    [[_webView superview] addSubview:_exitPlaceholder.get() positioned:NSWindowAbove relativeTo:_webView.get().get()];

    [CATransaction commit];
    [CATransaction flush];

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    
    [_backgroundView.get().layer removeAllAnimations];
    RefPtr page = _page.get();
    page->setSuppressVisibilityUpdates(true);
    [_webView removeFromSuperview];
    [_webView setFrame:[_webViewPlaceholder frame]];
    [_webView setAutoresizingMask:[_webViewPlaceholder autoresizingMask]];
    [[_webViewPlaceholder superview] addSubview:_webView.get().get() positioned:NSWindowBelow relativeTo:_webViewPlaceholder.get()];

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [NSLayoutConstraint activateConstraints:self.savedConstraints];
    END_BLOCK_OBJC_EXCEPTIONS
    self.savedConstraints = nil;
    makeResponderFirstResponderIfDescendantOfView([_webView window], firstResponder.get(), _webView.get().get());

    // These messages must be sent after the swap or flashing will occur during forceRepaint:
    manager->setAnimatingFullScreen(false);
    if (_beganExitFullScreenCompletionHandler)
        _beganExitFullScreenCompletionHandler();
    page->scalePageRelativeToScrollPosition(_savedScale, { });
    page->setObscuredContentInsets(_savedObscuredContentInsets);
    page->flushDeferredResizeEvents();
    page->flushDeferredScrollEvents();

    [CATransaction commit];
    [CATransaction flush];

    if (immediately) {
        [self completeFinishExitFullScreenAnimation];
        return;
    }

    page->updateRenderingWithForcedRepaint([weakSelf = WeakObjCPtr<WKFullScreenWindowController>(self)] {
        [weakSelf completeFinishExitFullScreenAnimation];
    });
}

- (void)finishedExitFullScreenAnimationAndExitImmediately:(bool)immediately
{
#if ENABLE(GPU_PROCESS)
    RefPtr gpuProcess = WebKit::GPUProcessProxy::singletonIfCreated();
    if (!gpuProcess)
        return;

    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);

    gpuProcess->postWillTakeSnapshotNotification([self, protectedSelf = RetainPtr { self }, immediately, logIdentifier = OBJC_LOGIDENTIFIER] () mutable {
        OBJC_ALWAYS_LOG(logIdentifier, " - finished posting snapshot notification");

        [protectedSelf _continueExitingFullscreenAfterPostingNotificationAndExitImmediately:immediately];
    });
#else
    [self _continueExitingFullscreenAfterPostingNotificationAndExitImmediately:immediately];
#endif
}

- (void)completeFinishExitFullScreenAnimation
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    [_webViewPlaceholder removeFromSuperview];
    [[self window] orderOut:self];
    RetainPtr contentView = [[self window] contentView];
    contentView.get().hidden = YES;
    [_exitPlaceholder removeFromSuperview];
    [[_exitPlaceholder layer] setContents:nil];
    _exitPlaceholder = nil;
    
    [[_webView window] makeKeyAndOrderFront:self];
    _webViewPlaceholder = nil;
    
    RefPtr page = _page.get();
    page->setSuppressVisibilityUpdates(false);
    page->setNeedsDOMWindowResizeEvent();

    [CATransaction commit];
    [CATransaction flush];
}

- (void)performClose:(id)sender
{
    if ([self isFullScreen])
        [self cancelOperation:sender];
}

- (void)close
{
    // We are being asked to close rapidly, most likely because the page 
    // has closed or the web process has crashed.  Just walk through our
    // normal exit full screen sequence, but don't wait to be called back
    // in response.
    [self exitFullScreenImmediately];

    [super close];

    _webView = nil;
}

- (void)videoControlsManagerDidChange
{
}

- (void)clearVideoPresentationManagerObserver
{
    _pipObserver = nullptr;
}

- (void)setVideoPresentationManagerObserver
{
    RefPtr<WebKit::VideoPresentationManagerProxy> videoPresentationManager = self._videoPresentationManager;
    if (!videoPresentationManager)
        return;

    ASSERT(!_pipObserver);
    if (_pipObserver)
        return;

    _pipObserver = WTF::makeUnique<WebKit::VideoPresentationManagerProxy::VideoInPictureInPictureDidChangeObserver>([strongSelf = retainPtr(self)] (bool inPiP) {
        if (inPiP)
            [strongSelf didEnterPictureInPicture];
        else
            [strongSelf didExitPictureInPicture];
    });

    videoPresentationManager->addVideoInPictureInPictureDidChangeObserver(*_pipObserver);
}

- (void)didEnterPictureInPicture
{
    if ([self isFullScreen])
        [self requestExitFullScreen];
}

- (void)didExitPictureInPicture
{
    [self clearVideoPresentationManagerObserver];
}

#pragma mark -
#pragma mark Custom NSWindow Full Screen Animation

- (NSArray *)customWindowsToEnterFullScreenForWindow:(NSWindow *)window
{
    return @[self.window];
}

- (NSArray *)customWindowsToExitFullScreenForWindow:(NSWindow *)window
{
    return @[self.window];
}

- (void)window:(NSWindow *)window startCustomAnimationToEnterFullScreenWithDuration:(NSTimeInterval)duration
{
    [self _startEnterFullScreenAnimationWithDuration:duration];
}

- (void)window:(NSWindow *)window startCustomAnimationToExitFullScreenWithDuration:(NSTimeInterval)duration
{
    [self _startExitFullScreenAnimationWithDuration:duration];
}

- (void)windowDidFailToEnterFullScreen:(NSWindow *)window
{
    [self finishedEnterFullScreenAnimation:NO];
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    RetainPtr<WKFullScreenWindowController> retain = self;
    [self finishedEnterFullScreenAnimation:YES];
    [self setVideoPresentationManagerObserver];
}

- (void)windowDidFailToExitFullScreen:(NSWindow *)window
{
    RetainPtr<WKFullScreenWindowController> retain = self;
    [self finishedExitFullScreenAnimationAndExitImmediately:YES];
    [self clearVideoPresentationManagerObserver];
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    RetainPtr<WKFullScreenWindowController> retain = self;
    [self finishedExitFullScreenAnimationAndExitImmediately:NO];
    [self clearVideoPresentationManagerObserver];
}

- (NSWindow *)destinationWindowToExitFullScreenForWindow:(NSWindow *)window
{
    return self.webViewPlaceholder.window;
}

#pragma mark -
#pragma mark Internal Interface

- (WebKit::WebFullScreenManagerProxy*)_manager
{
    RefPtr page = _page.get();
    return page ? page->fullScreenManager() : nullptr;
}

- (RefPtr<WebKit::WebFullScreenManagerProxy>)_protectedManager
{
    RefPtr page = _page.get();
    return page ? page->fullScreenManager() : nullptr;
}

- (WebKit::VideoPresentationManagerProxy*)_videoPresentationManager
{
    RefPtr page = _page.get();
    return page ? page->videoPresentationManager() : nullptr;
}

- (void)_replaceView:(NSView *)view with:(NSView *)otherView
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [otherView setFrame:[view frame]];        
    [otherView setAutoresizingMask:[view autoresizingMask]];
    [otherView removeFromSuperview];
    [[view superview] addSubview:otherView positioned:NSWindowAbove relativeTo:view];
    [view removeFromSuperview];
    [CATransaction commit];
}

- (void)_saveConstraintsOf:(NSView *)view
{
    RetainPtr<NSArray<NSLayoutConstraint *>> constraints = view.constraints;
    RetainPtr<NSIndexSet> validConstraints = [constraints indexesOfObjectsPassingTest:^BOOL(NSLayoutConstraint *constraint, NSUInteger, BOOL *) {
        return ![constraint isKindOfClass:objc_getClass("NSAutoresizingMaskLayoutConstraint")];
    }];
    self.savedConstraints = [constraints objectsAtIndexes:validConstraints.get()];
}

static CAMediaTimingFunction *timingFunctionForDuration(CFTimeInterval duration)
{
    if (duration >= 0.8)
        return [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
    return [CAMediaTimingFunction functionWithControlPoints:.25 :0 :0 :1];
}

enum AnimationDirection { AnimateIn, AnimateOut };
static RetainPtr<CAAnimation> zoomAnimation(const WebCore::FloatRect& initialFrame, const WebCore::FloatRect& finalFrame, const WebCore::FloatRect& screenFrame, CFTimeInterval duration, float speed, AnimationDirection direction)
{
    RetainPtr scaleAnimation = [CABasicAnimation animationWithKeyPath:@"transform"];
    WebCore::FloatRect scaleRect = smallestRectWithAspectRatioAroundRect(finalFrame.size().aspectRatio(), initialFrame);
    CGAffineTransform resetOriginTransform = CGAffineTransformMakeTranslation(screenFrame.x() - finalFrame.x(), screenFrame.y() - finalFrame.y());
    CGAffineTransform scaleTransform = CGAffineTransformMakeScale(scaleRect.width() / finalFrame.width(), scaleRect.height() / finalFrame.height());
    CGAffineTransform translateTransform = CGAffineTransformMakeTranslation(scaleRect.x() - screenFrame.x(), scaleRect.y() - screenFrame.y());

    CGAffineTransform finalTransform = CGAffineTransformConcat(CGAffineTransformConcat(resetOriginTransform, scaleTransform), translateTransform);
    RetainPtr scaleValue = [NSValue valueWithCATransform3D:CATransform3DMakeAffineTransform(finalTransform)];
    if (direction == AnimateIn)
        scaleAnimation.get().fromValue = scaleValue.get();
    else
        scaleAnimation.get().toValue = scaleValue.get();

    scaleAnimation.get().duration = duration;
    scaleAnimation.get().speed = speed;
    scaleAnimation.get().removedOnCompletion = NO;
    scaleAnimation.get().fillMode = kCAFillModeBoth;
    scaleAnimation.get().timingFunction = timingFunctionForDuration(duration);
    return scaleAnimation;
}

static RetainPtr<CALayer> createMask(const WebCore::FloatRect& bounds)
{
    RetainPtr maskLayer = [CALayer layer];
    maskLayer.get().anchorPoint = CGPointZero;
    maskLayer.get().frame = bounds;
    maskLayer.get().backgroundColor = CGColorGetConstantColor(kCGColorBlack);
    maskLayer.get().autoresizingMask = (NSViewWidthSizable | NSViewHeightSizable);
    return maskLayer;
}

static RetainPtr<CAAnimation> maskAnimation(const WebCore::FloatRect& initialFrame, const WebCore::FloatRect& finalFrame, const WebCore::FloatRect& screenFrame, CFTimeInterval duration, float speed, AnimationDirection direction)
{
    RetainPtr boundsAnimation = [CABasicAnimation animationWithKeyPath:@"bounds"];
    WebCore::FloatRect boundsRect = largestRectWithAspectRatioInsideRect(initialFrame.size().aspectRatio(), finalFrame);
    RetainPtr boundsValue = [NSValue valueWithRect:WebCore::FloatRect(WebCore::FloatPoint(), boundsRect.size())];
    if (direction == AnimateIn)
        boundsAnimation.get().fromValue = boundsValue.get();
    else
        boundsAnimation.get().toValue = boundsValue.get();

    RetainPtr positionAnimation = [CABasicAnimation animationWithKeyPath:@"position"];
    RetainPtr positionValue = [NSValue valueWithPoint:WebCore::FloatPoint(boundsRect.location() - screenFrame.location())];
    if (direction == AnimateIn)
        positionAnimation.get().fromValue = positionValue.get();
    else
        positionAnimation.get().toValue = positionValue.get();

    RetainPtr animation = [CAAnimationGroup animation];
    animation.get().animations = @[boundsAnimation.get(), positionAnimation.get()];
    animation.get().duration = duration;
    animation.get().speed = speed;
    animation.get().removedOnCompletion = NO;
    animation.get().fillMode = kCAFillModeBoth;
    animation.get().timingFunction = timingFunctionForDuration(duration);
    return animation;
}

static RetainPtr<CAAnimation> fadeAnimation(CFTimeInterval duration, AnimationDirection direction)
{
    RetainPtr fadeAnimation = [CABasicAnimation animationWithKeyPath:@"backgroundColor"];
    if (direction == AnimateIn)
        fadeAnimation.get().toValue = (id)CGColorGetConstantColor(kCGColorBlack);
    else
        fadeAnimation.get().fromValue = (id)CGColorGetConstantColor(kCGColorBlack);
    fadeAnimation.get().duration = duration;
    fadeAnimation.get().removedOnCompletion = NO;
    fadeAnimation.get().fillMode = kCAFillModeBoth;
    fadeAnimation.get().timingFunction = timingFunctionForDuration(duration);
    return fadeAnimation;
}

- (void)_startEnterFullScreenAnimationWithDuration:(NSTimeInterval)duration
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    RetainPtr<CALayer> clipLayer = _clipView.get().layer;
    [clipLayer addAnimation:zoomAnimation(_initialFrame, _finalFrame, self.window.screen.frame, duration, 1, AnimateIn).get() forKey:@"fullscreen"];
    [clipLayer.get().mask addAnimation:maskAnimation(_initialFrame, _finalFrame, self.window.screen.frame, duration, 1, AnimateIn).get() forKey:@"fullscreen"];
    [_backgroundView.get().layer addAnimation:fadeAnimation(duration, AnimateIn).get() forKey:@"fullscreen"];

    [CATransaction commit];
}

- (void)_startExitFullScreenAnimationWithDuration:(NSTimeInterval)duration
{
    if ([self isFullScreen]) {
        // We still believe we're in full screen mode, so we must have been asked to exit full
        // screen by the system full screen button.
        [self _protectedManager]->requestExitFullScreen();
        [self exitFullScreen:[] { }];
        _fullScreenState = ExitingFullScreen;
    }

    [[_clipView layer] addAnimation:zoomAnimation(_initialFrame, _finalFrame, self.window.screen.frame, duration, 1, AnimateOut).get() forKey:@"fullscreen"];
    RetainPtr contentView = [[self window] contentView];
    RetainPtr maskLayer = createMask(contentView.get().bounds);
    [maskLayer addAnimation:maskAnimation(_initialFrame, _finalFrame, self.window.screen.frame, duration, 1, AnimateOut).get() forKey:@"fullscreen"];
    [_clipView layer].mask = maskLayer.get();

    contentView.get().hidden = NO;
    [_backgroundView.get().layer addAnimation:fadeAnimation(duration, AnimateOut).get() forKey:@"fullscreen"];

    Ref { *_page }->setSuppressVisibilityUpdates(false);
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[self window] setAutodisplay:YES];
ALLOW_DEPRECATED_DECLARATIONS_END
    [[self window] displayIfNeeded];
}

- (void)_watchdogTimerFired:(NSTimer *)timer
{
    [self exitFullScreen:[] { }];
}

@end

#if !RELEASE_LOG_DISABLED
@implementation WKFullScreenWindowController (Logging)
- (uint64_t)logIdentifier
{
    return _logIdentifier;
}

- (const Logger*)loggerPtr
{
    return _logger.get();
}

- (WTFLogChannel*)logChannel
{
    return &WebKit2LogFullscreen;
}
@end
#endif

#endif // ENABLE(FULLSCREEN_API) && !PLATFORM(IOS_FAMILY)
