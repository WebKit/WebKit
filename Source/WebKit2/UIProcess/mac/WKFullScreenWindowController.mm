/*
 * Copyright (C) 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#if ENABLE(FULLSCREEN_API)

#import "WKFullScreenWindowController.h"

#import "LayerTreeContext.h"
#import "WKAPICast.h"
#import "WKViewInternal.h"
#import "WKViewPrivate.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageProxy.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/DisplaySleepDisabler.h>
#import <WebCore/FloatRect.h>
#import <WebCore/IntRect.h>
#import <WebCore/WebCoreFullScreenWindow.h>
#import <WebCore/WebWindowAnimation.h>
#import <WebKit/WebNSWindowExtras.h>
#import <WebKitSystemInterface.h>
#import <wtf/UnusedParam.h>

using namespace WebKit;
using namespace WebCore;

static RetainPtr<NSWindow> createBackgroundFullscreenWindow(NSRect frame);

static const CFTimeInterval defaultAnimationDuration = 0.5;
static const NSTimeInterval DefaultWatchdogTimerInterval = 1;

@interface WKFullScreenWindowController(Private)<NSAnimationDelegate>
- (void)_updateMenuAndDockForFullScreen;
- (void)_swapView:(NSView*)view with:(NSView*)otherView;
- (WebPageProxy*)_page;
- (WebFullScreenManagerProxy*)_manager;
- (void)_startEnterFullScreenAnimationWithDuration:(NSTimeInterval)duration;
- (void)_startExitFullScreenAnimationWithDuration:(NSTimeInterval)duration;
@end

#if defined(BUILDING_ON_LEOPARD) || defined(BUILDING_ON_SNOW_LEOPARD)
@interface NSWindow(convertRectToScreenForLeopardAndSnowLeopard)
- (NSRect)convertRectToScreen:(NSRect)aRect;
@end

@implementation NSWindow(convertRectToScreenForLeopardAndSnowLeopard)
- (NSRect)convertRectToScreen:(NSRect)rect
{
    NSRect frame = [self frame];
    rect.origin.x += frame.origin.x;
    rect.origin.y += frame.origin.y;
    return rect;
}
@end
#endif

@interface NSWindow(IsOnActiveSpaceAdditionForTigerAndLeopard)
- (BOOL)isOnActiveSpace;
@end

@implementation WKFullScreenWindowController

#pragma mark -
#pragma mark Initialization
- (id)init
{
    NSWindow *window = [[WebCoreFullScreenWindow alloc] initWithContentRect:NSZeroRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    self = [super initWithWindow:window];
    [window release];
    if (!self)
        return nil;
    [self windowDidLoad];
    
    return self;
}

- (void)dealloc
{
    [self setWebView:nil];
    
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

- (void)windowDidLoad
{
    [super windowDidLoad];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationDidResignActive:) name:NSApplicationDidResignActiveNotification object:NSApp];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationDidChangeScreenParameters:) name:NSApplicationDidChangeScreenParametersNotification object:NSApp];
}

#pragma mark -
#pragma mark Accessors

- (WKView*)webView
{
    return _webView;
}

- (void)setWebView:(WKView *)webView
{
    [webView retain];
    [_webView release];
    _webView = webView;
}

- (BOOL)isFullScreen
{
    return _isFullScreen;
}

#pragma mark -
#pragma mark NSWindowController overrides

- (void)cancelOperation:(id)sender
{
    [self _manager]->requestExitFullScreen();

    // If the page doesn't respond in DefaultWatchdogTimerInterval seconds, it could be because
    // the WebProcess has hung, so exit anyway.
    if (!_watchdogTimer)
        _watchdogTimer = adoptNS([NSTimer scheduledTimerWithTimeInterval:DefaultWatchdogTimerInterval target:self selector:@selector(exitFullScreen) userInfo:nil repeats:NO]);
}

#pragma mark -
#pragma mark Notifications

- (void)applicationDidResignActive:(NSNotification*)notification
{
    // Check to see if the fullScreenWindow is on the active space; this function is available
    // on 10.6 and later, so default to YES if the function is not available:
    NSWindow* fullScreenWindow = [self window];
    BOOL isOnActiveSpace = ([fullScreenWindow respondsToSelector:@selector(isOnActiveSpace)] ? [fullScreenWindow isOnActiveSpace] : YES);
    
    // Replicate the QuickTime Player (X) behavior when losing active application status:
    // Is the fullScreen screen the main screen? (Note: this covers the case where only a 
    // single screen is available.)  Is the fullScreen screen on the current space? IFF so, 
    // then exit fullScreen mode. 
    if ([fullScreenWindow screen] == [[NSScreen screens] objectAtIndex:0] && isOnActiveSpace)
        [self cancelOperation:self];
}

- (void)applicationDidChangeScreenParameters:(NSNotification*)notification
{
    // The user may have changed the main screen by moving the menu bar, or they may have changed
    // the Dock's size or location, or they may have changed the fullScreen screen's dimensions. 
    // Update our presentation parameters, and ensure that the full screen window occupies the 
    // entire screen:
    [self _updateMenuAndDockForFullScreen];
    NSWindow* window = [self window];
    NSRect screenFrame = [[window screen] frame];
    [window setFrame:screenFrame display:YES];
    [_backgroundWindow.get() setFrame:screenFrame display:YES];
}

#pragma mark -
#pragma mark Exposed Interface

- (void)enterFullScreen:(NSScreen *)screen
{
    if (_isFullScreen)
        return;
    _isFullScreen = YES;

    [self _updateMenuAndDockForFullScreen];   

    if (!screen)
        screen = [NSScreen mainScreen];
    NSRect screenFrame = [screen frame];

    NSRect webViewFrame = [[_webView window] convertRectToScreen:
                           [_webView convertRect:[_webView frame] toView:nil]];

    // Flip coordinate system:
    webViewFrame.origin.y = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - NSMaxY(webViewFrame);

    CGWindowID windowID = [[_webView window] windowNumber];
    RetainPtr<CGImageRef> webViewContents(AdoptCF, CGWindowListCreateImage(NSRectToCGRect(webViewFrame), kCGWindowListOptionIncludingWindow, windowID, kCGWindowImageShouldBeOpaque));

    // Screen updates to be re-enabled in beganEnterFullScreenWithInitialFrame:finalFrame:
    NSDisableScreenUpdates();
    [[self window] setAutodisplay:NO];

    NSResponder *webWindowFirstResponder = [[_webView window] firstResponder];
    [[self window] setFrame:screenFrame display:NO];

    // Swap the webView placeholder into place.
    if (!_webViewPlaceholder) {
        _webViewPlaceholder.adoptNS([[NSImageView alloc] init]);
        [_webViewPlaceholder.get() setLayer:[CALayer layer]];
        [_webViewPlaceholder.get() setWantsLayer:YES];
    }
    [[_webViewPlaceholder.get() layer] setContents:(id)webViewContents.get()];
    [self _swapView:_webView with:_webViewPlaceholder.get()];
    
    // Then insert the WebView into the full screen window
    NSView* contentView = [[self window] contentView];
    [contentView addSubview:_webView positioned:NSWindowBelow relativeTo:nil];
    [_webView setFrame:[contentView bounds]];

    [[self window] makeResponder:webWindowFirstResponder firstResponderIfDescendantOfView:_webView];

    [self _manager]->setAnimatingFullScreen(true);
    [self _manager]->willEnterFullScreen();
}

- (void)beganEnterFullScreenWithInitialFrame:(const WebCore::IntRect&)initialFrame finalFrame:(const WebCore::IntRect&)finalFrame
{
    if (_isEnteringFullScreen)
        return;
    _isEnteringFullScreen = YES;

    _initialFrame = initialFrame;
    _finalFrame = finalFrame;

    [self _updateMenuAndDockForFullScreen];   

    [self _startEnterFullScreenAnimationWithDuration:defaultAnimationDuration];
}

- (void)finishedEnterFullScreenAnimation:(bool)completed
{
    if (!_isEnteringFullScreen)
        return;
    _isEnteringFullScreen = NO;

    if (completed) {
        // Screen updates to be re-enabled ta the end of the current block.
        NSDisableScreenUpdates();
        [self _manager]->didEnterFullScreen();
        [self _manager]->setAnimatingFullScreen(false);

        NSRect windowBounds = [[self window] frame];
        windowBounds.origin = NSZeroPoint;
        WKWindowSetClipRect([self window], windowBounds);

        NSWindow *webWindow = [_webViewPlaceholder.get() window];
#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
        // In Lion, NSWindow will animate into and out of orderOut operations. Suppress that
        // behavior here, making sure to reset the animation behavior afterward.
        NSWindowAnimationBehavior animationBehavior = [webWindow animationBehavior];
        [webWindow setAnimationBehavior:NSWindowAnimationBehaviorNone];
#endif
        [webWindow orderOut:self];
#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
        [webWindow setAnimationBehavior:animationBehavior];
#endif

        [_fadeAnimation.get() stopAnimation];
        [_fadeAnimation.get() setWindow:nil];
        _fadeAnimation = nullptr;
        
        [_backgroundWindow.get() orderOut:self];
        [_backgroundWindow.get() setFrame:NSZeroRect display:YES];
        NSEnableScreenUpdates();
    } else
        [_scaleAnimation.get() stopAnimation];
}

- (void)exitFullScreen
{
    if (_watchdogTimer) {
        [_watchdogTimer.get() invalidate];
        _watchdogTimer.clear();
    }

    if (!_isFullScreen)
        return;
    _isFullScreen = NO;

    // Screen updates to be re-enabled in beganExitFullScreenWithInitialFrame:finalFrame:
    NSDisableScreenUpdates();
    [[self window] setAutodisplay:NO];

    [self _manager]->setAnimatingFullScreen(true);
    [self _manager]->willExitFullScreen();
}

- (void)beganExitFullScreenWithInitialFrame:(const WebCore::IntRect&)initialFrame finalFrame:(const WebCore::IntRect&)finalFrame
{
    if (_isExitingFullScreen)
        return;
    _isExitingFullScreen = YES;

    if (_isEnteringFullScreen)
        [self finishedEnterFullScreenAnimation:NO];

    [self _updateMenuAndDockForFullScreen];
    
    NSWindow* webWindow = [_webViewPlaceholder.get() window];
#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
    // In Lion, NSWindow will animate into and out of orderOut operations. Suppress that
    // behavior here, making sure to reset the animation behavior afterward.
    NSWindowAnimationBehavior animationBehavior = [webWindow animationBehavior];
    [webWindow setAnimationBehavior:NSWindowAnimationBehaviorNone];
#endif
    // If the user has moved the fullScreen window into a new space, temporarily change
    // the collectionBehavior of the webView's window so that it is pulled into the active space:
    if (!([webWindow respondsToSelector:@selector(isOnActiveSpace)] ? [webWindow isOnActiveSpace] : YES)) {
        NSWindowCollectionBehavior behavior = [webWindow collectionBehavior];
        [webWindow setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces];
        [webWindow orderWindow:NSWindowBelow relativeTo:[[self window] windowNumber]];
        [webWindow setCollectionBehavior:behavior];
    } else
        [webWindow orderWindow:NSWindowBelow relativeTo:[[self window] windowNumber]];
    
#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
    [webWindow setAnimationBehavior:animationBehavior];
#endif

    [self _startExitFullScreenAnimationWithDuration:defaultAnimationDuration];
}

static void completeFinishExitFullScreenAnimationAfterRepaint(WKErrorRef, void*);

- (void)finishedExitFullScreenAnimation:(bool)completed
{
    if (!_isExitingFullScreen)
        return;
    _isExitingFullScreen = NO;

    [self _updateMenuAndDockForFullScreen];

    // Screen updates to be re-enabled in completeFinishExitFullScreenAnimationAfterRepaint.
    NSDisableScreenUpdates();
    [[_webViewPlaceholder.get() window] setAutodisplay:NO];

    NSResponder *firstResponder = [[self window] firstResponder];
    [self _swapView:_webViewPlaceholder.get() with:_webView];
    [[_webView window] makeResponder:firstResponder firstResponderIfDescendantOfView:_webView];

    NSRect windowBounds = [[self window] frame];
    windowBounds.origin = NSZeroPoint;
    WKWindowSetClipRect([self window], windowBounds);

    [[self window] orderOut:self];
    [[self window] setFrame:NSZeroRect display:YES];

    [_fadeAnimation.get() stopAnimation];
    [_fadeAnimation.get() setWindow:nil];
    _fadeAnimation = nullptr;

    [_backgroundWindow.get() orderOut:self];
    [_backgroundWindow.get() setFrame:NSZeroRect display:YES];

    [[_webView window] makeKeyAndOrderFront:self];

    // These messages must be sent after the swap or flashing will occur during forceRepaint:
    [self _manager]->didExitFullScreen();
    [self _manager]->setAnimatingFullScreen(false);

    [self _page]->forceRepaint(VoidCallback::create(self, completeFinishExitFullScreenAnimationAfterRepaint));
}

- (void)completeFinishExitFullScreenAnimationAfterRepaint
{
    [[_webView window] setAutodisplay:YES];
    NSEnableScreenUpdates();
}

static void completeFinishExitFullScreenAnimationAfterRepaint(WKErrorRef, void* _self)
{
    [(WKFullScreenWindowController*)_self completeFinishExitFullScreenAnimationAfterRepaint];
}

- (void)close
{
    // We are being asked to close rapidly, most likely because the page 
    // has closed or the web process has crashed.  Just walk through our
    // normal exit full screen sequence, but don't wait to be called back
    // in response.
    if (_isFullScreen)
        [self exitFullScreen];
    
    if (_isExitingFullScreen)
        [self finishedExitFullScreenAnimation:YES];

    [super close];
}

#pragma mark -
#pragma mark NSAnimation delegate

- (void)animationDidEnd:(NSAnimation*)animation
{
    if (_isFullScreen)
        [self finishedEnterFullScreenAnimation:YES];
    else
        [self finishedExitFullScreenAnimation:YES];
}

#pragma mark -
#pragma mark Internal Interface

- (void)_updateMenuAndDockForFullScreen
{
    // NSApplicationPresentationOptions is available on > 10.6 only:
#ifndef BUILDING_ON_LEOPARD
    NSApplicationPresentationOptions options = NSApplicationPresentationDefault;
    NSScreen* fullScreenScreen = [[self window] screen];
    
    if (_isFullScreen) {
        // Auto-hide the menu bar if the fullScreenScreen contains the menu bar:
        // NOTE: if the fullScreenScreen contains the menu bar but not the dock, we must still 
        // auto-hide the dock, or an exception will be thrown.
        if ([[NSScreen screens] objectAtIndex:0] == fullScreenScreen)
            options |= (NSApplicationPresentationAutoHideMenuBar | NSApplicationPresentationAutoHideDock);
        // Check if the current screen contains the dock by comparing the screen's frame to its
        // visibleFrame; if a dock is present, the visibleFrame will differ. If the current screen
        // contains the dock, hide it.
        else if (!NSEqualRects([fullScreenScreen frame], [fullScreenScreen visibleFrame]))
            options |= NSApplicationPresentationAutoHideDock;
    }
    
    if ([NSApp respondsToSelector:@selector(setPresentationOptions:)])
        [NSApp setPresentationOptions:options];
    else
#endif
        SetSystemUIMode(_isFullScreen ? kUIModeAllHidden : kUIModeNormal, 0);
}

- (WebPageProxy*)_page
{
    return toImpl([_webView pageRef]);
}

- (WebFullScreenManagerProxy*)_manager
{
    WebPageProxy* webPage = [self _page];
    if (!webPage)
        return 0;
    return webPage->fullScreenManager();
}

- (void)_swapView:(NSView*)view with:(NSView*)otherView
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

static RetainPtr<NSWindow> createBackgroundFullscreenWindow(NSRect frame)
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:frame styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    [window setOpaque:YES];
    [window setBackgroundColor:[NSColor blackColor]];
    [window setReleasedWhenClosed:NO];
    return adoptNS(window);
}

static NSRect windowFrameFromApparentFrames(NSRect screenFrame, NSRect initialFrame, NSRect finalFrame)
{
    NSRect initialWindowFrame;
    if (!NSWidth(initialFrame) || !NSWidth(finalFrame) || !NSHeight(initialFrame) || !NSHeight(finalFrame))
        return screenFrame;

    CGFloat xScale = NSWidth(screenFrame) / NSWidth(finalFrame);
    CGFloat yScale = NSHeight(screenFrame) / NSHeight(finalFrame);
    CGFloat xTrans = NSMinX(screenFrame) - NSMinX(finalFrame);
    CGFloat yTrans = NSMinY(screenFrame) - NSMinY(finalFrame);
    initialWindowFrame.size = NSMakeSize(NSWidth(initialFrame) * xScale, NSHeight(initialFrame) * yScale);
    initialWindowFrame.origin = NSMakePoint
        ( NSMinX(initialFrame) + xTrans / (NSWidth(finalFrame) / NSWidth(initialFrame))
        , NSMinY(initialFrame) + yTrans / (NSHeight(finalFrame) / NSHeight(initialFrame)));
    return initialWindowFrame;
}

- (void)_startEnterFullScreenAnimationWithDuration:(NSTimeInterval)duration
{
    NSRect screenFrame = [[[self window] screen] frame];
    NSRect initialWindowFrame = windowFrameFromApparentFrames(screenFrame, _initialFrame, _finalFrame);
    
    _scaleAnimation.adoptNS([[WebWindowScaleAnimation alloc] initWithHintedDuration:duration window:[self window] initalFrame:initialWindowFrame finalFrame:screenFrame]);
    
    [_scaleAnimation.get() setAnimationBlockingMode:NSAnimationNonblocking];
    [_scaleAnimation.get() setDelegate:self];
    [_scaleAnimation.get() setCurrentProgress:0];
    [_scaleAnimation.get() startAnimation];

    // WKWindowSetClipRect takes window coordinates, so convert from screen coordinates here:
    NSRect finalBounds = _finalFrame;
    finalBounds.origin = [[self window] convertScreenToBase:finalBounds.origin];
    WKWindowSetClipRect([self window], finalBounds);

    [[self window] makeKeyAndOrderFront:self];

    if (!_backgroundWindow)
        _backgroundWindow = createBackgroundFullscreenWindow(screenFrame);
    else
        [_backgroundWindow.get() setFrame:screenFrame display:NO];

    CGFloat currentAlpha = 0;
    if (_fadeAnimation) {
        currentAlpha = [_fadeAnimation.get() currentAlpha];
        [_fadeAnimation.get() stopAnimation];
        [_fadeAnimation.get() setWindow:nil];
    }

    _fadeAnimation.adoptNS([[WebWindowFadeAnimation alloc] initWithDuration:duration 
                                                                     window:_backgroundWindow.get() 
                                                               initialAlpha:currentAlpha 
                                                                 finalAlpha:1]);
    [_fadeAnimation.get() setAnimationBlockingMode:NSAnimationNonblocking];
    [_fadeAnimation.get() setCurrentProgress:0];
    [_fadeAnimation.get() startAnimation];

    [_backgroundWindow.get() orderWindow:NSWindowBelow relativeTo:[[self window] windowNumber]];

    [[self window] setAutodisplay:YES];
    [[self window] displayIfNeeded];
    NSEnableScreenUpdates();
}

- (void)_startExitFullScreenAnimationWithDuration:(NSTimeInterval)duration
{
    NSRect screenFrame = [[[self window] screen] frame];
    NSRect initialWindowFrame = windowFrameFromApparentFrames(screenFrame, _initialFrame, _finalFrame);

    NSRect currentFrame = _scaleAnimation ? [_scaleAnimation.get() currentFrame] : [[self window] frame];
    _scaleAnimation.adoptNS([[WebWindowScaleAnimation alloc] initWithHintedDuration:duration window:[self window] initalFrame:currentFrame finalFrame:initialWindowFrame]);

    [_scaleAnimation.get() setAnimationBlockingMode:NSAnimationNonblocking];
    [_scaleAnimation.get() setDelegate:self];
    [_scaleAnimation.get() setCurrentProgress:0];
    [_scaleAnimation.get() startAnimation];

    if (!_backgroundWindow)
        _backgroundWindow = createBackgroundFullscreenWindow(screenFrame);
    else
        [_backgroundWindow.get() setFrame:screenFrame display:NO];

    CGFloat currentAlpha = 1;
    if (_fadeAnimation) {
        currentAlpha = [_fadeAnimation.get() currentAlpha];
        [_fadeAnimation.get() stopAnimation];
        [_fadeAnimation.get() setWindow:nil];
    }
    _fadeAnimation.adoptNS([[WebWindowFadeAnimation alloc] initWithDuration:duration 
                                                                     window:_backgroundWindow.get() 
                                                               initialAlpha:currentAlpha 
                                                                 finalAlpha:0]);
    [_fadeAnimation.get() setAnimationBlockingMode:NSAnimationNonblocking];
    [_fadeAnimation.get() setCurrentProgress:0];
    [_fadeAnimation.get() startAnimation];

    [_backgroundWindow.get() orderWindow:NSWindowBelow relativeTo:[[self window] windowNumber]];

    // WKWindowSetClipRect takes window coordinates, so convert from screen coordinates here:
    NSRect finalBounds = _finalFrame;
    finalBounds.origin = [[self window] convertScreenToBase:finalBounds.origin];
    WKWindowSetClipRect([self window], finalBounds);

    [[self window] setAutodisplay:YES];
    [[self window] displayIfNeeded];
    NSEnableScreenUpdates();
}
@end

#endif
