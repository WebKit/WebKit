/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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

#if ENABLE(FULLSCREEN_API) && !PLATFORM(IOS_FAMILY)

#import "WebFullScreenController.h"

#import "WebNSWindowExtras.h"
#import "WebPreferencesPrivate.h"
#import "WebViewInternal.h"
#import "WebWindowAnimation.h"
#import <WebCore/Document.h>
#import <WebCore/Element.h>
#import <WebCore/FloatRect.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/FullscreenManager.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/IntRect.h>
#import <WebCore/RenderLayer.h>
#import <WebCore/RenderLayerBacking.h>
#import <WebCore/RenderObject.h>
#import <WebCore/RenderView.h>
#import <WebCore/WebCoreFullScreenWindow.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

using namespace WebCore;

static const CFTimeInterval defaultAnimationDuration = 0.5;

static IntRect screenRectOfContents(Element* element)
{
    ASSERT(element);
    if (element->renderer() && element->renderer()->hasLayer() && element->renderer()->enclosingLayer()->isComposited()) {
        FloatQuad contentsBox = static_cast<FloatRect>(element->renderer()->enclosingLayer()->backing()->contentsBox());
        contentsBox = element->renderer()->localToAbsoluteQuad(contentsBox);
        return element->renderer()->view().frameView().contentsToScreen(contentsBox.enclosingBoundingBox());
    }
    return element->screenRect();
}

@interface WebFullScreenController(Private)<NSAnimationDelegate>
- (void)_updateMenuAndDockForFullScreen;
- (void)_swapView:(NSView*)view with:(NSView*)otherView;
- (Document*)_document;
- (FullscreenManager*)_manager;
- (void)_startEnterFullScreenAnimationWithDuration:(NSTimeInterval)duration;
- (void)_startExitFullScreenAnimationWithDuration:(NSTimeInterval)duration;
@end

static NSRect convertRectToScreen(NSWindow *window, NSRect rect)
{
    return [window convertRectToScreen:rect];
}

@implementation WebFullScreenController

#pragma mark -
#pragma mark Initialization
- (id)init
{
    // Do not defer window creation, to make sure -windowNumber is created (needed by WebWindowScaleAnimation).
    NSWindow *window = [[WebCoreFullScreenWindow alloc] initWithContentRect:NSZeroRect styleMask:NSWindowStyleMaskClosable backing:NSBackingStoreBuffered defer:NO];
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

@synthesize initialFrame=_initialFrame;
@synthesize finalFrame=_finalFrame;

- (void)windowDidLoad
{
    [super windowDidLoad];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationDidResignActive:) name:NSApplicationDidResignActiveNotification object:NSApp];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationDidChangeScreenParameters:) name:NSApplicationDidChangeScreenParametersNotification object:NSApp];
}

#pragma mark -
#pragma mark Accessors

- (WebView*)webView
{
    return _webView;
}

- (void)setWebView:(WebView *)webView
{
    [webView retain];
    [_webView release];
    _webView = webView;
}

- (NSView*)webViewPlaceholder
{
    return _webViewPlaceholder.get();
}

- (Element*)element
{
    return _element.get();
}

- (void)setElement:(RefPtr<Element>&&)element
{
    _element = WTFMove(element);
}

- (BOOL)isFullScreen
{
    return _isFullScreen;
}

#pragma mark -
#pragma mark NSWindowController overrides

- (void)cancelOperation:(id)sender
{
    [self performSelector:@selector(requestExitFullScreen) withObject:nil afterDelay:0];
}

#pragma mark -
#pragma mark Notifications

- (void)applicationDidResignActive:(NSNotification*)notification
{   
    NSWindow* fullscreenWindow = [self window];

    // Replicate the QuickTime Player (X) behavior when losing active application status:
    // Is the fullscreen screen the main screen? (Note: this covers the case where only a 
    // single screen is available.)  Is the fullscreen screen on the current space? IFF so, 
    // then exit fullscreen mode. 
    if (fullscreenWindow.screen == [NSScreen screens][0] && fullscreenWindow.onActiveSpace)
         [self cancelOperation:self];
}

- (void)applicationDidChangeScreenParameters:(NSNotification*)notification
{
    // The user may have changed the main screen by moving the menu bar, or they may have changed
    // the Dock's size or location, or they may have changed the fullscreen screen's dimensions. 
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

    NSRect webViewFrame = convertRectToScreen([_webView window], [_webView convertRect:[_webView frame] toView:nil]);

    // Flip coordinate system:
    webViewFrame.origin.y = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - NSMaxY(webViewFrame);
    
    CGWindowID windowID = [[_webView window] windowNumber];
    RetainPtr<CGImageRef> webViewContents = adoptCF(CGWindowListCreateImage(NSRectToCGRect(webViewFrame), kCGWindowListOptionIncludingWindow, windowID, kCGWindowImageShouldBeOpaque));
    
    // Screen updates to be re-enabled in beganEnterFullScreenWithInitialFrame:finalFrame:
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSDisableScreenUpdates();
    [[self window] setAutodisplay:NO];
    ALLOW_DEPRECATED_DECLARATIONS_END

    NSResponder *webWindowFirstResponder = [[_webView window] firstResponder];
    [[self window] setFrame:screenFrame display:NO];

    _initialFrame = screenRectOfContents(_element.get());

    // Swap the webView placeholder into place.
    if (!_webViewPlaceholder) {
        _webViewPlaceholder = adoptNS([[NSView alloc] init]);
        [_webViewPlaceholder.get() setLayer:[CALayer layer]];
        [_webViewPlaceholder.get() setWantsLayer:YES];
    }
    [[_webViewPlaceholder.get() layer] setContents:(__bridge id)webViewContents.get()];
    _scrollPosition = [_webView _mainCoreFrame]->view()->scrollPosition();
    [self _swapView:_webView with:_webViewPlaceholder.get()];
    
    // Then insert the WebView into the full screen window
    NSView* contentView = [[self window] contentView];
    [contentView addSubview:_webView positioned:NSWindowBelow relativeTo:nil];
    [_webView setFrame:[contentView bounds]];
    [[_webViewPlaceholder.get() window] recalculateKeyViewLoop];
    
    [[self window] makeResponder:webWindowFirstResponder firstResponderIfDescendantOfView:_webView];

    _savedScale = [_webView _viewScaleFactor];
    [_webView _scaleWebView:1 atOrigin:NSMakePoint(0, 0)];
    [self _manager]->willEnterFullscreen(*_element);
    [self _manager]->setAnimatingFullscreen(true);
    [self _document]->updateLayout();

    _finalFrame = screenRectOfContents(_element.get());
    
    [self _updateMenuAndDockForFullScreen];   
    
    [self _startEnterFullScreenAnimationWithDuration:defaultAnimationDuration];

    _isEnteringFullScreen = true;
}

static void setClipRectForWindow(NSWindow *window, NSRect clipRect)
{
    CGSWindowID windowNumber = (CGSWindowID)window.windowNumber;
    CGSRegionObj shape;
    CGRect cgClipRect = NSRectToCGRect(clipRect);
    CGSNewRegionWithRect(&cgClipRect, &shape);
    CGSSetWindowClipShape(CGSMainConnectionID(), windowNumber, shape);
    CGSReleaseRegion(shape);
}

- (void)finishedEnterFullScreenAnimation:(bool)completed
{
    if (!_isEnteringFullScreen)
        return;
    _isEnteringFullScreen = NO;
    
    if (completed) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        // Screen updates to be re-enabled at the end of this block
        NSDisableScreenUpdates();
        ALLOW_DEPRECATED_DECLARATIONS_END
        [self _manager]->setAnimatingFullscreen(false);
        [self _manager]->didEnterFullscreen();
        
        NSRect windowBounds = [[self window] frame];
        windowBounds.origin = NSZeroPoint;
        setClipRectForWindow(self.window, windowBounds);
        
        NSWindow *webWindow = [_webViewPlaceholder.get() window];
        // In Lion, NSWindow will animate into and out of orderOut operations. Suppress that
        // behavior here, making sure to reset the animation behavior afterward.
        NSWindowAnimationBehavior animationBehavior = [webWindow animationBehavior];
        [webWindow setAnimationBehavior:NSWindowAnimationBehaviorNone];
        [webWindow orderOut:self];
        [webWindow setAnimationBehavior:animationBehavior];
        
        [_fadeAnimation.get() stopAnimation];
        [_fadeAnimation.get() setWindow:nil];
        _fadeAnimation = nullptr;
        
        [_backgroundWindow.get() orderOut:self];
        [_backgroundWindow.get() setFrame:NSZeroRect display:YES];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        NSEnableScreenUpdates();
        ALLOW_DEPRECATED_DECLARATIONS_END
    } else
        [_scaleAnimation.get() stopAnimation];
}

- (void)requestExitFullScreen
{
    if (!_element)
        return;
    _element->document().fullscreenManager().cancelFullscreen();
}

- (void)exitFullScreen
{
    if (!_isFullScreen)
        return;
    _isFullScreen = NO;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // Screen updates to be re-enabled in beganExitFullScreenWithInitialFrame:finalFrame:
    NSDisableScreenUpdates();
    [[self window] setAutodisplay:NO];
    ALLOW_DEPRECATED_DECLARATIONS_END

    _finalFrame = screenRectOfContents(_element.get());

    [self _manager]->willExitFullscreen();
    [self _manager]->setAnimatingFullscreen(true);

    if (_isEnteringFullScreen)
        [self finishedEnterFullScreenAnimation:NO];
    
    [self _updateMenuAndDockForFullScreen];
    
    NSWindow* webWindow = [_webViewPlaceholder.get() window];
    // In Lion, NSWindow will animate into and out of orderOut operations. Suppress that
    // behavior here, making sure to reset the animation behavior afterward.
    NSWindowAnimationBehavior animationBehavior = [webWindow animationBehavior];
    [webWindow setAnimationBehavior:NSWindowAnimationBehaviorNone];
    // If the user has moved the fullScreen window into a new space, temporarily change
    // the collectionBehavior of the webView's window so that it is pulled into the active space:
    if (!webWindow.onActiveSpace) {
        NSWindowCollectionBehavior behavior = [webWindow collectionBehavior];
        [webWindow setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces];
        [webWindow orderWindow:NSWindowBelow relativeTo:[[self window] windowNumber]];
        [webWindow setCollectionBehavior:behavior];
    } else
        [webWindow orderWindow:NSWindowBelow relativeTo:[[self window] windowNumber]];
    [webWindow setAnimationBehavior:animationBehavior];

    [self _startExitFullScreenAnimationWithDuration:defaultAnimationDuration];
    _isExitingFullScreen = YES;    
}

- (void)finishedExitFullScreenAnimation:(bool)completed
{
    if (!_isExitingFullScreen)
        return;
    _isExitingFullScreen = NO;
    
    [self _updateMenuAndDockForFullScreen];

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // Screen updates to be re-enabled at the end of this function
    NSDisableScreenUpdates();
    ALLOW_DEPRECATED_DECLARATIONS_END

    [self _manager]->setAnimatingFullscreen(false);
    [self _manager]->didExitFullscreen();
    [_webView _scaleWebView:_savedScale atOrigin:NSMakePoint(0, 0)];

    NSResponder *firstResponder = [[self window] firstResponder];
    [self _swapView:_webViewPlaceholder.get() with:_webView];
    [_webView _mainCoreFrame]->view()->setScrollPosition(_scrollPosition);
    [[_webView window] makeResponder:firstResponder firstResponderIfDescendantOfView:_webView];
    
    NSRect windowBounds = [[self window] frame];
    windowBounds.origin = NSZeroPoint;
    setClipRectForWindow(self.window, windowBounds);
    
    [[self window] orderOut:self];
    [[self window] setFrame:NSZeroRect display:YES];
    
    [_fadeAnimation.get() stopAnimation];
    [_fadeAnimation.get() setWindow:nil];
    _fadeAnimation = nullptr;
    
    [_backgroundWindow.get() orderOut:self];
    [_backgroundWindow.get() setFrame:NSZeroRect display:YES];

    [[_webView window] makeKeyAndOrderFront:self];

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSEnableScreenUpdates();
    ALLOW_DEPRECATED_DECLARATIONS_END
}

- (void)performClose:(id)sender
{
    if (_isFullScreen)
        [self cancelOperation:sender];
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
    NSApplicationPresentationOptions options = NSApplicationPresentationDefault;
    NSScreen* fullscreenScreen = [[self window] screen];
    
    if (_isFullScreen) {
        // Auto-hide the menu bar if the fullscreenScreen contains the menu bar:
        // NOTE: if the fullscreenScreen contains the menu bar but not the dock, we must still 
        // auto-hide the dock, or an exception will be thrown.
        if ([[NSScreen screens] objectAtIndex:0] == fullscreenScreen)
            options |= (NSApplicationPresentationAutoHideMenuBar | NSApplicationPresentationAutoHideDock);
        // Check if the current screen contains the dock by comparing the screen's frame to its
        // visibleFrame; if a dock is present, the visibleFrame will differ. If the current screen
        // contains the dock, hide it.
        else if (!NSEqualRects([fullscreenScreen frame], [fullscreenScreen visibleFrame]))
            options |= NSApplicationPresentationAutoHideDock;
    }
    
    NSApp.presentationOptions = options;
}

#pragma mark -
#pragma mark Utility Functions

- (Document*)_document 
{
    return &_element->document();
}

- (FullscreenManager*)_manager
{
    return &_element->document().fullscreenManager();
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
    NSWindow *window = [[NSWindow alloc] initWithContentRect:frame styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
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
    
    _scaleAnimation = adoptNS([[WebWindowScaleAnimation alloc] initWithHintedDuration:duration window:[self window] initalFrame:initialWindowFrame finalFrame:screenFrame]);
    
    [_scaleAnimation.get() setAnimationBlockingMode:NSAnimationNonblocking];
    [_scaleAnimation.get() setDelegate:self];
    [_scaleAnimation.get() setCurrentProgress:0];
    [_scaleAnimation.get() startAnimation];
    
    // setClipRectForWindow takes window coordinates, so convert from screen coordinates here:
    NSRect finalBounds = _finalFrame;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    finalBounds.origin = [[self window] convertScreenToBase:finalBounds.origin];
    ALLOW_DEPRECATED_DECLARATIONS_END
    setClipRectForWindow(self.window, finalBounds);
    
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
    
    _fadeAnimation = adoptNS([[WebWindowFadeAnimation alloc] initWithDuration:duration 
                                                                       window:_backgroundWindow.get() 
                                                                 initialAlpha:currentAlpha 
                                                                   finalAlpha:1]);
    [_fadeAnimation.get() setAnimationBlockingMode:NSAnimationNonblocking];
    [_fadeAnimation.get() setCurrentProgress:0];
    [_fadeAnimation.get() startAnimation];
    
    [_backgroundWindow.get() orderWindow:NSWindowBelow relativeTo:[[self window] windowNumber]];
    
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[self window] setAutodisplay:YES];
    ALLOW_DEPRECATED_DECLARATIONS_END
    [[self window] displayIfNeeded];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // Screen updates disabled in enterFullScreen:
    NSEnableScreenUpdates();
    ALLOW_DEPRECATED_DECLARATIONS_END
}

- (void)_startExitFullScreenAnimationWithDuration:(NSTimeInterval)duration
{
    NSRect screenFrame = [[[self window] screen] frame];
    NSRect initialWindowFrame = windowFrameFromApparentFrames(screenFrame, _initialFrame, _finalFrame);
    
    NSRect currentFrame = _scaleAnimation ? [_scaleAnimation.get() currentFrame] : [[self window] frame];
    _scaleAnimation = adoptNS([[WebWindowScaleAnimation alloc] initWithHintedDuration:duration window:[self window] initalFrame:currentFrame finalFrame:initialWindowFrame]);
    
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
    _fadeAnimation = adoptNS([[WebWindowFadeAnimation alloc] initWithDuration:duration 
                                                                       window:_backgroundWindow.get() 
                                                                 initialAlpha:currentAlpha 
                                                                   finalAlpha:0]);
    [_fadeAnimation.get() setAnimationBlockingMode:NSAnimationNonblocking];
    [_fadeAnimation.get() setCurrentProgress:0];
    [_fadeAnimation.get() startAnimation];
    
    [_backgroundWindow.get() orderWindow:NSWindowBelow relativeTo:[[self window] windowNumber]];
    
    // setClipRectForWindow takes window coordinates, so convert from screen coordinates here:
    NSRect finalBounds = _finalFrame;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    finalBounds.origin = [[self window] convertScreenToBase:finalBounds.origin];
    ALLOW_DEPRECATED_DECLARATIONS_END
    setClipRectForWindow(self.window, finalBounds);
    
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[self window] setAutodisplay:YES];
    ALLOW_DEPRECATED_DECLARATIONS_END
    [[self window] displayIfNeeded];

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // Screen updates disabled in exitFullScreen:
    NSEnableScreenUpdates();
    ALLOW_DEPRECATED_DECLARATIONS_END
}

@end


#endif /* ENABLE(FULLSCREEN_API) && !PLATFORM(IOS_FAMILY) */
