/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#if ENABLE(VIDEO)

#import <QTKit/QTKit.h>
#import <objc/objc-runtime.h>
#import <HIToolbox/HIToolbox.h>

#import <wtf/UnusedParam.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/IntRect.h>

#import "WebVideoFullscreenController.h"
#import "WebVideoFullscreenHUDWindowController.h"
#import "WebKitSystemInterface.h"
#import "WebTypesInternal.h"
#import "WebWindowAnimation.h"

SOFT_LINK_FRAMEWORK(QTKit)
SOFT_LINK_CLASS(QTKit, QTMovieView)

@interface WebVideoFullscreenWindow : NSWindow
#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_TIGER)
<NSAnimationDelegate>
#endif
{
    SEL _controllerActionOnAnimationEnd;
    WebWindowScaleAnimation *_fullscreenAnimation; // (retain)
    QTMovieView *_movieView; // (retain)
}
- (void)animateFromRect:(NSRect)startRect toRect:(NSRect)endRect withSubAnimation:(NSAnimation *)subAnimation controllerAction:(SEL)controllerAction;
- (QTMovieView *)movieView;
- (void)setMovieView:(QTMovieView *)movieView;
@end

@interface WebVideoFullscreenController () <WebVideoFullscreenHUDWindowControllerDelegate>
@end

@implementation WebVideoFullscreenController
- (id)init
{
    // Do not defer window creation, to make sure -windowNumber is created (needed by WebWindowScaleAnimation).
    NSWindow *window = [[WebVideoFullscreenWindow alloc] initWithContentRect:NSZeroRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    self = [super initWithWindow:window];
    [window release];
    if (!self)
        return nil;
    [self windowDidLoad];
    return self;
    
}
- (void)dealloc
{
    ASSERT(!_backgroundFullscreenWindow);
    ASSERT(!_fadeAnimation);
    [super dealloc];
}

- (WebVideoFullscreenWindow *)fullscreenWindow
{
    return (WebVideoFullscreenWindow *)[super window];
}

- (void)windowDidLoad
{
    WebVideoFullscreenWindow *window = [self fullscreenWindow];
    QTMovieView *view = [[getQTMovieViewClass() alloc] init];
    [view setFillColor:[NSColor clearColor]];
    [window setMovieView:view];
    [view setControllerVisible:NO];
    [view setPreservesAspectRatio:YES];
    if (_mediaElement)
        [view setMovie:_mediaElement->platformMedia().qtMovie];
    [window setHasShadow:YES]; // This is nicer with a shadow.
    [window setLevel:NSPopUpMenuWindowLevel-1];
    [view release];
}

- (WebCore::HTMLMediaElement*)mediaElement;
{
    return _mediaElement.get();
}

- (void)setMediaElement:(WebCore::HTMLMediaElement*)mediaElement;
{
    _mediaElement = mediaElement;
    if ([self isWindowLoaded]) {
        QTMovieView *movieView = [[self fullscreenWindow] movieView];
        [movieView setMovie:_mediaElement->platformMedia().qtMovie];
    }
}

- (id<WebVideoFullscreenControllerDelegate>)delegate
{
    return _delegate;
}

- (void)setDelegate:(id<WebVideoFullscreenControllerDelegate>)delegate;
{
    _delegate = delegate;
}

- (CGFloat)clearFadeAnimation
{
    [_fadeAnimation stopAnimation];
    CGFloat previousAlpha = [_fadeAnimation currentAlpha];
    [_fadeAnimation setWindow:nil];
    [_fadeAnimation release];
    _fadeAnimation = nil;
    return previousAlpha;
}

- (void)windowDidExitFullscreen
{
    [self clearFadeAnimation];
    [[self window] close];
    [self setWindow:nil];
    SetSystemUIMode(kUIModeNormal, 0);
    [_hudController setDelegate:nil];
    [_hudController release];
    _hudController = nil;
    [_backgroundFullscreenWindow close];
    [_backgroundFullscreenWindow release];
    _backgroundFullscreenWindow = nil;
    
    [self autorelease]; // Associated -retain is in -exitFullscreen.
    _isEndingFullscreen = NO;
}

- (void)windowDidEnterFullscreen
{
    [self clearFadeAnimation];

    ASSERT(!_hudController);
    _hudController = [[WebVideoFullscreenHUDWindowController alloc] init];
    [_hudController setDelegate:self];

    SetSystemUIMode(kUIModeAllSuppressed , 0);
    [NSCursor setHiddenUntilMouseMoves:YES];
    
    // Give the HUD keyboard focus initially
    [_hudController fadeWindowIn];
}

- (NSRect)mediaElementRect
{
    return _mediaElement->screenRect();
}

#pragma mark -
#pragma mark Exposed Interface

static void constrainFrameToRatioOfFrame(NSRect *frameToConstrain, const NSRect *frame)
{
    // Keep a constrained aspect ratio for the destination window
    double originalRatio = frame->size.width / frame->size.height;
    double newRatio = frameToConstrain->size.width / frameToConstrain->size.height;
    if (newRatio > originalRatio) {
        double newWidth = originalRatio * frameToConstrain->size.height;
        double diff = frameToConstrain->size.width - newWidth;
        frameToConstrain->size.width = newWidth;
        frameToConstrain->origin.x += diff / 2;
    } else {
        double newHeight = frameToConstrain->size.width / originalRatio;
        double diff = frameToConstrain->size.height - newHeight;
        frameToConstrain->size.height = newHeight;
        frameToConstrain->origin.y += diff / 2;
    }    
}

static NSWindow *createBackgroundFullscreenWindow(NSRect frame, int level)
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:frame styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    [window setOpaque:YES];
    [window setBackgroundColor:[NSColor blackColor]];
    [window setLevel:level];
    [window setHidesOnDeactivate:YES];
    [window setReleasedWhenClosed:NO];
    return window;
}

- (void)setupFadeAnimationIfNeededAndFadeIn:(BOOL)fadeIn
{
    CGFloat initialAlpha = fadeIn ? 0 : 1;
    if (_fadeAnimation) {
        // Make sure we support queuing animation if the previous one isn't over yet
        initialAlpha = [self clearFadeAnimation];
    }
    if (!_forceDisableAnimation)
        _fadeAnimation = [[WebWindowFadeAnimation alloc] initWithDuration:0.2 window:_backgroundFullscreenWindow initialAlpha:initialAlpha finalAlpha:fadeIn ? 1 : 0];
}

- (void)enterFullscreen:(NSScreen *)screen;
{
    if (!screen)
        screen = [NSScreen mainScreen];

    NSRect frame = [self mediaElementRect];
    NSRect endFrame = [screen frame];
    constrainFrameToRatioOfFrame(&endFrame, &frame);

    // Create a black window if needed
    if (!_backgroundFullscreenWindow)
        _backgroundFullscreenWindow = createBackgroundFullscreenWindow([screen frame], [[self window] level]-1);
    else
        [_backgroundFullscreenWindow setFrame:[screen frame] display:NO];

    [self setupFadeAnimationIfNeededAndFadeIn:YES];
    if (_forceDisableAnimation) {
        // This will disable scale animation
        frame = NSZeroRect;
    }
    [[self fullscreenWindow] animateFromRect:frame toRect:endFrame withSubAnimation:_fadeAnimation controllerAction:@selector(windowDidEnterFullscreen)];

    [_backgroundFullscreenWindow orderWindow:NSWindowBelow relativeTo:[[self fullscreenWindow] windowNumber]];
}

- (void)exitFullscreen
{
    if (_isEndingFullscreen)
        return;
    _isEndingFullscreen = YES;
    [_hudController closeWindow];

    NSRect endFrame = [self mediaElementRect];

    [self setupFadeAnimationIfNeededAndFadeIn:NO];
    if (_forceDisableAnimation) {
        // This will disable scale animation
        endFrame = NSZeroRect;
    }
    
    // We have to retain ourselves because we want to be alive for the end of the animation.
    // If our owner releases us we could crash if this is not the case.
    // Balanced in windowDidExitFullscreen
    [self retain];    
    
    [[self fullscreenWindow] animateFromRect:[[self window] frame] toRect:endFrame withSubAnimation:_fadeAnimation controllerAction:@selector(windowDidExitFullscreen)];
}

#pragma mark -
#pragma mark Window callback

- (void)requestExitFullscreenWithAnimation:(BOOL)animation
{
    if (_isEndingFullscreen)
        return;

    _forceDisableAnimation = !animation;
    _mediaElement->exitFullscreen();
    _forceDisableAnimation = NO;
}

- (void)requestExitFullscreen
{
    [self requestExitFullscreenWithAnimation:YES];
}

- (void)fadeHUDIn
{
    [_hudController fadeWindowIn];
}
@end

@implementation WebVideoFullscreenWindow

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)aStyle backing:(NSBackingStoreType)bufferingType defer:(BOOL)flag
{
    UNUSED_PARAM(aStyle);
    self = [super initWithContentRect:contentRect styleMask:NSBorderlessWindowMask backing:bufferingType defer:flag];
    if (!self)
        return nil;
    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setHidesOnDeactivate:YES];
    [self setIgnoresMouseEvents:NO];
    [self setAcceptsMouseMovedEvents:YES];
    return self;
}

- (void)dealloc
{
    ASSERT(!_fullscreenAnimation);
    [super dealloc];
}

- (QTMovieView *)movieView
{
    return _movieView;
}

- (void)setMovieView:(QTMovieView *)movieView
{
    if (_movieView == movieView)
        return;
    [_movieView release];
    _movieView = [movieView retain];
    [self setContentView:_movieView];
}

- (BOOL)resignFirstResponder
{
    return NO;
}

- (BOOL)canBecomeKeyWindow
{
    return NO;
}

- (void)mouseDown:(NSEvent *)theEvent
{
    UNUSED_PARAM(theEvent);
}

- (void)cancelOperation:(id)sender
{
    UNUSED_PARAM(sender);
    [[self windowController] requestExitFullscreen];
}

- (void)animatedResizeDidEnd
{
    // Call our windowController.
    if (_controllerActionOnAnimationEnd)
        [[self windowController] performSelector:_controllerActionOnAnimationEnd];
    _controllerActionOnAnimationEnd = NULL;
}

//
// This function will animate a change of frame rectangle
// We support queuing animation, that means that we'll correctly
// interrupt the running animation, and queue the next one.
//
- (void)animateFromRect:(NSRect)startRect toRect:(NSRect)endRect withSubAnimation:(NSAnimation *)subAnimation controllerAction:(SEL)controllerAction
{
    _controllerActionOnAnimationEnd = controllerAction;

    BOOL wasAnimating = NO;
    if (_fullscreenAnimation) {
        wasAnimating = YES;

        // Interrupt any running animation.
        [_fullscreenAnimation stopAnimation];

        // Save the current rect to ensure a smooth transition.
        startRect = [_fullscreenAnimation currentFrame];
        [_fullscreenAnimation release];
        _fullscreenAnimation = nil;
    }
    
    if (NSIsEmptyRect(startRect) || NSIsEmptyRect(endRect)) {
        // Fakely end the subanimation.
        [subAnimation setCurrentProgress:1.0];
        // And remove the weak link to the window.
        [subAnimation stopAnimation];

        [self setFrame:endRect display:NO];
        [self makeKeyAndOrderFront:self];
        [self animatedResizeDidEnd];
        return;
    }

    if (!wasAnimating) {
        // We'll downscale the window during the animation based on the higher resolution rect
        BOOL higherResolutionIsEndRect = startRect.size.width < endRect.size.width && startRect.size.height < endRect.size.height;
        [self setFrame:higherResolutionIsEndRect ? endRect : startRect display:NO];        
    }
    
    ASSERT(!_fullscreenAnimation);
    _fullscreenAnimation = [[WebWindowScaleAnimation alloc] initWithHintedDuration:0.2 window:self initalFrame:startRect finalFrame:endRect];
    [_fullscreenAnimation setSubAnimation:subAnimation];
    [_fullscreenAnimation setDelegate:self];
    
    // Make sure the animation has scaled the window before showing it.
    [_fullscreenAnimation setCurrentProgress:0];
    [self makeKeyAndOrderFront:self];

    [_fullscreenAnimation startAnimation];
}

- (void)animationDidEnd:(NSAnimation *)animation
{
    if (![NSThread isMainThread]) {
        [self performSelectorOnMainThread:@selector(animationDidEnd:) withObject:animation waitUntilDone:NO];
        return;
    }
    if (animation != _fullscreenAnimation)
        return;

    // The animation is not really over and was interrupted
    // Don't send completion events.
    if ([animation currentProgress] < 1.0)
        return;

    // Ensure that animation (and subanimation) don't keep
    // the weak reference to the window ivar that may be destroyed from
    // now on.
    [_fullscreenAnimation setWindow:nil];

    [_fullscreenAnimation autorelease];
    _fullscreenAnimation = nil;

    [self animatedResizeDidEnd];
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    [[self windowController] fadeHUDIn];
}

- (void)resignKeyWindow
{
    [super resignKeyWindow];
    [[self windowController] requestExitFullscreenWithAnimation:NO];
}
@end

#endif /* ENABLE(VIDEO) */
