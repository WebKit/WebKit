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
#import "WebFullScreenManagerProxy.h"
#import "WebPageProxy.h"
#import <Carbon/Carbon.h> // For SetSystemUIMode()
#import <IOKit/pwr_mgt/IOPMLib.h> // For IOPMAssertionCreate()
#import <QuartzCore/QuartzCore.h>
#import <WebCore/FloatRect.h>
#import <WebCore/IntRect.h>
#import <WebKitSystemInterface.h>

static const NSTimeInterval tickleTimerInterval = 1.0;

using namespace WebKit;
using namespace WebCore;

#if defined(BUILDING_ON_LEOPARD)
@interface CATransaction(SnowLeopardConvenienceFunctions)
+ (void)setDisableActions:(BOOL)flag;
+ (void)setAnimationDuration:(CFTimeInterval)dur;
@end

@implementation CATransaction(SnowLeopardConvenienceFunctions)
+ (void)setDisableActions:(BOOL)flag
{
    [self setValue:[NSNumber numberWithBool:flag] forKey:kCATransactionDisableActions];
}

+ (void)setAnimationDuration:(CFTimeInterval)dur
{
    [self setValue:[NSNumber numberWithDouble:dur] forKey:kCATransactionAnimationDuration];
}
@end

#endif

@interface WKFullScreenWindow : NSWindow
{
    NSView* _animationView;
    CALayer* _backgroundLayer;
}
- (CALayer*)backgroundLayer;
- (NSView*)animationView;
@end

@interface WKFullScreenWindowController(Private)
- (void)_requestExitFullScreenWithAnimation:(BOOL)animation;
- (void)_updateMenuAndDockForFullScreen;
- (void)_updatePowerAssertions;
- (WKFullScreenWindow *)_fullScreenWindow;
- (CFTimeInterval)_animationDuration;
- (void)_swapView:(NSView*)view with:(NSView*)otherView;
- (WebFullScreenManagerProxy*)_manager;
@end

@interface NSWindow(IsOnActiveSpaceAdditionForTigerAndLeopard)
- (BOOL)isOnActiveSpace;
@end

@implementation WKFullScreenWindowController

#pragma mark -
#pragma mark Initialization
- (id)init
{
    NSWindow *window = [[WKFullScreenWindow alloc] initWithContentRect:NSZeroRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
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

#pragma mark -
#pragma mark Notifications

- (void)applicationDidResignActive:(NSNotification*)notification
{   
    // Check to see if the fullScreenWindow is on the active space; this function is available
    // on 10.6 and later, so default to YES if the function is not available:
    NSWindow* fullScreenWindow = [self _fullScreenWindow];
    BOOL isOnActiveSpace = ([fullScreenWindow respondsToSelector:@selector(isOnActiveSpace)] ? [fullScreenWindow isOnActiveSpace] : YES);
    
    // Replicate the QuickTime Player (X) behavior when losing active application status:
    // Is the fullScreen screen the main screen? (Note: this covers the case where only a 
    // single screen is available.)  Is the fullScreen screen on the current space? IFF so, 
    // then exit fullScreen mode. 
    if ([fullScreenWindow screen] == [[NSScreen screens] objectAtIndex:0] && isOnActiveSpace)
        [self _requestExitFullScreenWithAnimation:NO];
}

- (void)applicationDidChangeScreenParameters:(NSNotification*)notification
{
    // The user may have changed the main screen by moving the menu bar, or they may have changed
    // the Dock's size or location, or they may have changed the fullScreen screen's dimensions. 
    // Update our presentation parameters, and ensure that the full screen window occupies the 
    // entire screen:
    [self _updateMenuAndDockForFullScreen];
    NSWindow* window = [self window];
    [window setFrame:[[window screen] frame] display:YES];
}

#pragma mark -
#pragma mark Exposed Interface

- (void)enterFullScreen:(NSScreen *)screen
{
    if (_isFullScreen)
        return;
    
    _isFullScreen = YES;
    _isAnimating = YES;
    
    NSDisableScreenUpdates();
    
    if (!screen)
        screen = [NSScreen mainScreen];
    NSRect screenFrame = [screen frame];
    
    NSRect webViewFrame = [_webView convertRectToBase:[_webView frame]];
    webViewFrame.origin = [[_webView window] convertBaseToScreen:webViewFrame.origin];
        
    // In the case of a multi-monitor setup where the webView straddles two
    // monitors, we must create a window large enough to contain the destination
    // frame and the initial frame.
    NSRect windowFrame = NSUnionRect(screenFrame, webViewFrame);
    [[self window] setFrame:windowFrame display:YES];
    
    CALayer* backgroundLayer = [[self _fullScreenWindow] backgroundLayer];
    NSRect backgroundFrame = {[[self window] convertScreenToBase:screenFrame.origin], screenFrame.size};
    backgroundFrame = [[[self window] contentView] convertRectFromBase:backgroundFrame];
    
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [backgroundLayer setFrame:NSRectToCGRect(backgroundFrame)];
    [CATransaction commit];

    CFTimeInterval duration = [self _animationDuration];
    [self _manager]->willEnterFullScreen();
    [self _manager]->beginEnterFullScreenAnimation(duration);
}

- (void)beganEnterFullScreenAnimation
{    
    [self _updateMenuAndDockForFullScreen];   
    [self _updatePowerAssertions];
    
    // In a previous incarnation, the NSWindow attached to this controller may have
    // been on a different screen. Temporarily change the collectionBehavior of the window:
    NSWindow* fullScreenWindow = [self window];
    NSWindowCollectionBehavior behavior = [fullScreenWindow collectionBehavior];
    [fullScreenWindow setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces];
    [fullScreenWindow makeKeyAndOrderFront:self];
    [fullScreenWindow setCollectionBehavior:behavior];

    // Start the opacity animation. We can use implicit animations here because we don't care when
    // the animation finishes.
    [CATransaction begin];
    [CATransaction setAnimationDuration:[self _animationDuration]];
    [[[self _fullScreenWindow] backgroundLayer] setOpacity:1];
    [CATransaction commit];

    NSEnableScreenUpdates();
    _isAnimating = YES;
}

- (void)finishedEnterFullScreenAnimation:(bool)completed
{
    NSDisableScreenUpdates();
    
    if (completed) {                
        // Swap the webView placeholder into place.
        if (!_webViewPlaceholder)
            _webViewPlaceholder.adoptNS([[NSView alloc] init]);
        [self _swapView:_webView with:_webViewPlaceholder.get()];
        
        // Then insert the WebView into the full screen window
        NSView* animationView = [[self _fullScreenWindow] animationView];
        [animationView addSubview:_webView positioned:NSWindowBelow relativeTo:_layerHostingView.get()];
        [_webView setFrame:[animationView bounds]];
        
        // FIXME: In Barolo, orderIn will animate, which is not what we want.  Find a way
        // to work around this behavior.
        //[[_webViewPlaceholder.get() window] orderOut:self];
        [[self window] makeKeyAndOrderFront:self];
    }
    
    [self _manager]->didEnterFullScreen();
    NSEnableScreenUpdates();
    
    _isAnimating = NO;
}

- (void)exitFullScreen
{
    if (!_isFullScreen)
        return;
    
    _isFullScreen = NO;
    _isAnimating = YES;
    
    NSDisableScreenUpdates();
    
    [self _manager]->willExitFullScreen();
    [self _manager]->beginExitFullScreenAnimation([self _animationDuration]);
}

- (void)beganExitFullScreenAnimation
{   
    [self _updateMenuAndDockForFullScreen];   
    [self _updatePowerAssertions];
    
    // The user may have moved the fullScreen window in Spaces, so temporarily change 
    // the collectionBehavior of the webView's window:
    NSWindow* webWindow = [[self webView] window];
    NSWindowCollectionBehavior behavior = [webWindow collectionBehavior];
    [webWindow setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces];
    [webWindow orderWindow:NSWindowBelow relativeTo:[[self window] windowNumber]];
    [webWindow setCollectionBehavior:behavior];
    
    // Swap the webView back into its original position:
    if ([_webView window] == [self window])
        [self _swapView:_webViewPlaceholder.get() with:_webView];
    
    [CATransaction begin];
    [CATransaction setAnimationDuration:[self _animationDuration]];
    [[[self _fullScreenWindow] backgroundLayer] setOpacity:0];
    [CATransaction commit];
    
    NSEnableScreenUpdates();
    _isAnimating = YES;
}

- (void)finishedExitFullScreenAnimation:(bool)completed
{
    NSDisableScreenUpdates();
    
    if (completed) {
        [self _updateMenuAndDockForFullScreen];
        [self _updatePowerAssertions];
        [NSCursor setHiddenUntilMouseMoves:YES];
                
        [[self window] orderOut:self];
        [[_webView window] makeKeyAndOrderFront:self];
    }
    
    [self _manager]->didExitFullScreen();
    NSEnableScreenUpdates();
    
    _isAnimating = NO;
}

- (void)enterAcceleratedCompositingMode:(const WebKit::LayerTreeContext&)layerTreeContext
{
    if (_layerHostingView)
        return;

    ASSERT(!layerTreeContext.isEmpty());
    
    // Create an NSView that will host our layer tree.
    _layerHostingView.adoptNS([[NSView alloc] initWithFrame:[[self window] frame]]);
    [_layerHostingView.get() setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    WKFullScreenWindow* window = [self _fullScreenWindow];
    [[window animationView] addSubview:_layerHostingView.get()];
    
    // Create a root layer that will back the NSView.
    RetainPtr<CALayer> rootLayer(AdoptNS, [[CALayer alloc] init]);
#ifndef NDEBUG
    [rootLayer.get() setName:@"Hosting root layer"];
#endif
    
    CALayer *renderLayer = WKMakeRenderLayer(layerTreeContext.contextID);
    [rootLayer.get() addSublayer:renderLayer];
    
    [_layerHostingView.get() setLayer:rootLayer.get()];
    [_layerHostingView.get() setWantsLayer:YES];
    [[window backgroundLayer] setHidden:NO];
    [CATransaction commit];
}

- (void)exitAcceleratedCompositingMode
{
    if (!_layerHostingView)
        return;
    
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [_layerHostingView.get() removeFromSuperview];
    [_layerHostingView.get() setLayer:nil];
    [_layerHostingView.get() setWantsLayer:NO];
    [[[self _fullScreenWindow] backgroundLayer] setHidden:YES];
    [CATransaction commit];
    
    _layerHostingView = 0;
}

- (WebCore::IntRect)getFullScreenRect
{
    return enclosingIntRect([[self window] frame]);
}

#pragma mark -
#pragma mark Internal Interface

- (void)_updateMenuAndDockForFullScreen
{
    // NSApplicationPresentationOptions is available on > 10.6 only:
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
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
        SetSystemUIMode(_isFullScreen ? kUIModeNormal : kUIModeAllHidden, 0);
}

#if !defined(BUILDING_ON_TIGER) // IOPMAssertionCreateWithName not defined on < 10.5
- (void)_disableIdleDisplaySleep
{
    if (_idleDisplaySleepAssertion == kIOPMNullAssertionID) 
#if defined(BUILDING_ON_LEOPARD) // IOPMAssertionCreateWithName is not defined in the 10.5 SDK
        IOPMAssertionCreate(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, &_idleDisplaySleepAssertion);
#else // IOPMAssertionCreate is depreciated in > 10.5
    IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, CFSTR("WebKit playing a video fullScreen."), &_idleDisplaySleepAssertion);
#endif
}

- (void)_enableIdleDisplaySleep
{
    if (_idleDisplaySleepAssertion != kIOPMNullAssertionID) {
        IOPMAssertionRelease(_idleDisplaySleepAssertion);
        _idleDisplaySleepAssertion = kIOPMNullAssertionID;
    }
}

- (void)_disableIdleSystemSleep
{
    if (_idleSystemSleepAssertion == kIOPMNullAssertionID) 
#if defined(BUILDING_ON_LEOPARD) // IOPMAssertionCreateWithName is not defined in the 10.5 SDK
        IOPMAssertionCreate(kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, &_idleSystemSleepAssertion);
#else // IOPMAssertionCreate is depreciated in > 10.5
    IOPMAssertionCreateWithName(kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, CFSTR("WebKit playing a video fullScreen."), &_idleSystemSleepAssertion);
#endif
}

- (void)_enableIdleSystemSleep
{
    if (_idleSystemSleepAssertion != kIOPMNullAssertionID) {
        IOPMAssertionRelease(_idleSystemSleepAssertion);
        _idleSystemSleepAssertion = kIOPMNullAssertionID;
    }
}

- (void)_enableTickleTimer
{
    [_tickleTimer invalidate];
    [_tickleTimer release];
    _tickleTimer = [[NSTimer scheduledTimerWithTimeInterval:tickleTimerInterval target:self selector:@selector(_tickleTimerFired) userInfo:nil repeats:YES] retain];
}

- (void)_disableTickleTimer
{
    [_tickleTimer invalidate];
    [_tickleTimer release];
    _tickleTimer = nil;
}

- (void)_tickleTimerFired
{
    UpdateSystemActivity(OverallAct);
}
#endif

- (void)_updatePowerAssertions
{
#if !defined(BUILDING_ON_TIGER) 
    if (_isPlaying && _isFullScreen) {
        [self _disableIdleSystemSleep];
        [self _disableIdleDisplaySleep];
        [self _enableTickleTimer];
    } else {
        [self _enableIdleSystemSleep];
        [self _enableIdleDisplaySleep];
        [self _disableTickleTimer];
    }
#endif
}

- (WebFullScreenManagerProxy*)_manager
{
    WebPageProxy* webPage = toImpl([_webView pageRef]);
    if (!webPage)
        return 0;
    return webPage->fullScreenManager();
}

- (void)_requestExit
{
    [self exitFullScreen];
    _forceDisableAnimation = NO;
}

- (void)_requestExitFullScreenWithAnimation:(BOOL)animation
{
    _forceDisableAnimation = !animation;
    [self performSelector:@selector(_requestExit) withObject:nil afterDelay:0];
    
}

- (void)_swapView:(NSView*)view with:(NSView*)otherView
{
    [otherView setFrame:[view frame]];        
    [otherView setAutoresizingMask:[view autoresizingMask]];
    [otherView removeFromSuperview];
    [[view superview] replaceSubview:view with:otherView];
}

#pragma mark -
#pragma mark Utility Functions

- (WKFullScreenWindow *)_fullScreenWindow
{
    ASSERT([[self window] isKindOfClass:[WKFullScreenWindow class]]);
    return (WKFullScreenWindow *)[self window];
}

- (CFTimeInterval)_animationDuration
{
    static const CFTimeInterval defaultDuration = 0.5;
    CFTimeInterval duration = defaultDuration;
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
    NSUInteger modifierFlags = [NSEvent modifierFlags];
#else
    NSUInteger modifierFlags = [[NSApp currentEvent] modifierFlags];
#endif
    if ((modifierFlags & NSControlKeyMask) == NSControlKeyMask)
        duration *= 2;
    if ((modifierFlags & NSShiftKeyMask) == NSShiftKeyMask)
        duration *= 10;
    if (_forceDisableAnimation) {
        // This will disable scale animation
        duration = 0;
    }
    return duration;
}

@end

#pragma mark -
@implementation WKFullScreenWindow

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)aStyle backing:(NSBackingStoreType)bufferingType defer:(BOOL)flag
{
    UNUSED_PARAM(aStyle);
    self = [super initWithContentRect:contentRect styleMask:NSBorderlessWindowMask backing:bufferingType defer:flag];
    if (!self)
        return nil;
    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setIgnoresMouseEvents:NO];
    [self setAcceptsMouseMovedEvents:YES];
    [self setReleasedWhenClosed:NO];
    [self setHasShadow:YES];
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
    [self setMovable:NO];
#else
    [self setMovableByWindowBackground:NO];
#endif
    
    NSView* contentView = [self contentView];
    _animationView = [[NSView alloc] initWithFrame:[contentView bounds]];
    
    CALayer* contentLayer = [[CALayer alloc] init];
    [_animationView setLayer:contentLayer];
    [_animationView setWantsLayer:YES];
    [_animationView setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    [contentView addSubview:_animationView];
    
    _backgroundLayer = [[CALayer alloc] init];
    [contentLayer addSublayer:_backgroundLayer];
    
    [_backgroundLayer setBackgroundColor:CGColorGetConstantColor(kCGColorBlack)];
    [_backgroundLayer setOpacity:0];
    return self;
}

- (void)dealloc
{
    [_animationView release];
    [_backgroundLayer release];
    [super dealloc];
}

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (void)keyDown:(NSEvent *)theEvent
{
    if ([[theEvent charactersIgnoringModifiers] isEqual:@"\e"]) // Esacpe key-code
        [self cancelOperation:self];
    else [super keyDown:theEvent];
}

- (void)cancelOperation:(id)sender
{
    UNUSED_PARAM(sender);
    [[self windowController] _requestExitFullScreenWithAnimation:YES];
}

- (CALayer*)backgroundLayer
{
    return _backgroundLayer;
}

- (NSView*)animationView
{
    return _animationView;
}
@end

#endif
