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

#import "WebVideoFullscreenHUDWindowController.h"
#import "WebWindowAnimation.h"
#import <AVFoundation/AVPlayer.h>
#import <AVFoundation/AVPlayerLayer.h>
#import <Carbon/Carbon.h>
#import <WebCore/HTMLVideoElement.h>
#import <objc/runtime.h>
#import <pal/system/SleepDisabler.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

SOFT_LINK_FRAMEWORK(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVPlayerLayer)

@interface WebVideoFullscreenWindow : NSWindow<NSAnimationDelegate> {
    SEL _controllerActionOnAnimationEnd;
    WebWindowScaleAnimation *_fullscreenAnimation; // (retain)
}
- (void)animateFromRect:(NSRect)startRect toRect:(NSRect)endRect withSubAnimation:(NSAnimation *)subAnimation controllerAction:(SEL)controllerAction;
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
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

- (WebVideoFullscreenWindow *)fullscreenWindow
{
    return (WebVideoFullscreenWindow *)[super window];
}

- (void)windowDidLoad
{
    auto window = [self fullscreenWindow];
    auto contentView = [window contentView];

    [window setHasShadow:YES]; // This is nicer with a shadow.
    [window setLevel:NSPopUpMenuWindowLevel-1];

    [contentView setLayer:[CALayer layer]];
    [contentView setWantsLayer:YES];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationDidResignActive:) name:NSApplicationDidResignActiveNotification object:NSApp];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationDidChangeScreenParameters:) name:NSApplicationDidChangeScreenParametersNotification object:NSApp];
}

- (WebCore::HTMLVideoElement *)videoElement
{
    return _videoElement.get();
}

// FIXME: This method is not really a setter. The caller relies on its side effects, and it's
// called once each time we enter full screen. So it should have a different name.
- (void)setVideoElement:(WebCore::HTMLVideoElement *)videoElement
{
    ASSERT(videoElement);
    _videoElement = videoElement;

    if (![self isWindowLoaded])
        return;
    auto corePlayer = videoElement->player();
    if (!corePlayer)
        return;
    auto player = corePlayer->objCAVFoundationAVPlayer();
    if (!player)
        return;

    auto contentView = [[self fullscreenWindow] contentView];

    auto layer = adoptNS([allocAVPlayerLayerInstance() init]);
    [layer setPlayer:player];

    [contentView setLayer:layer.get()];

    // FIXME: The windowDidLoad method already called this, so it should
    // not be necessary to do it again here.
    [contentView setWantsLayer:YES];

    // FIXME: This can be called multiple times, and won't necessarily be
    // balanced by calls to windowDidExitFullscreen in some cases, so it
    // would be better to change things so the observer is reliably added
    // only once and guaranteed removed even in unusual edge cases.
    [player addObserver:self forKeyPath:@"rate" options:0 context:nullptr];
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
    CALayer *layer = [[[self window] contentView] layer];
    if ([layer isKindOfClass:getAVPlayerLayerClass()])
        [[(AVPlayerLayer *)layer player] removeObserver:self forKeyPath:@"rate"];

    [self clearFadeAnimation];
    [[self window] close];
    [self setWindow:nil];
    [self updateMenuAndDockForFullscreen];   
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

    [self updateMenuAndDockForFullscreen];
    [NSCursor setHiddenUntilMouseMoves:YES];
    
    // Give the HUD keyboard focus initially
    [_hudController fadeWindowIn];
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
        [self requestExitFullscreenWithAnimation:NO];
}


// MARK: -
// MARK: Exposed Interface

static NSRect frameExpandedToRatioOfFrame(NSRect frameToExpand, NSRect frame)
{
    // Keep a constrained aspect ratio for the destination window
    NSRect result = frameToExpand;
    CGFloat newRatio = frame.size.width / frame.size.height;
    CGFloat originalRatio = frameToExpand.size.width / frameToExpand.size.height;
    if (newRatio > originalRatio) {
        CGFloat newWidth = newRatio * frameToExpand.size.height;
        CGFloat diff = newWidth - frameToExpand.size.width;
        result.size.width = newWidth;
        result.origin.x -= diff / 2;
    } else {
        CGFloat newHeight = frameToExpand.size.width / newRatio;
        CGFloat diff = newHeight - frameToExpand.size.height;
        result.size.height = newHeight;
        result.origin.y -= diff / 2;
    }
    return result;
}

static NSWindow *createBackgroundFullscreenWindow(NSRect frame, int level)
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:frame styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    [window setOpaque:YES];
    [window setBackgroundColor:[NSColor blackColor]];
    [window setLevel:level];
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

- (void)enterFullscreen:(NSScreen *)screen
{
    if (!screen)
        screen = [NSScreen mainScreen];

    NSRect endFrame = [screen frame];
    NSRect frame = frameExpandedToRatioOfFrame([self videoElementRect], endFrame);

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

    NSRect endFrame = [self videoElementRect];

    [self setupFadeAnimationIfNeededAndFadeIn:NO];
    if (_forceDisableAnimation) {
        // This will disable scale animation
        endFrame = NSZeroRect;
    }
    
    // We have to retain ourselves because we want to be alive for the end of the animation.
    // If our owner releases us we could crash if this is not the case.
    // Balanced in windowDidExitFullscreen
    [self retain];    

    NSRect startFrame = [[self window] frame];
    endFrame = frameExpandedToRatioOfFrame(endFrame, startFrame);

    [[self fullscreenWindow] animateFromRect:startFrame toRect:endFrame withSubAnimation:_fadeAnimation controllerAction:@selector(windowDidExitFullscreen)];
}

- (void)applicationDidChangeScreenParameters:(NSNotification*)notification
{
    UNUSED_PARAM(notification);
    // The user may have changed the main screen by moving the menu bar, or they may have changed
    // the Dock's size or location, or they may have changed the fullscreen screen's dimensions.  
    // Update our presentation parameters, and ensure that the full screen window occupies the 
    // entire screen:
    [self updateMenuAndDockForFullscreen];
    [[self window] setFrame:[[[self window] screen] frame] display:YES];
}

- (void)updateMenuAndDockForFullscreen
{
    // NSApplicationPresentationOptions is available on > 10.6 only:
    NSApplicationPresentationOptions options = NSApplicationPresentationDefault;
    NSScreen* fullscreenScreen = [[self window] screen];

    if (!_isEndingFullscreen) {
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

// MARK: -
// MARK: Window callback

- (void)_requestExit
{
    if (_videoElement)
        _videoElement->exitFullscreen();
    _forceDisableAnimation = NO;
}

- (void)requestExitFullscreenWithAnimation:(BOOL)animation
{
    if (_isEndingFullscreen)
        return;

    _forceDisableAnimation = !animation;
    [self performSelector:@selector(_requestExit) withObject:nil afterDelay:0];

}

- (void)requestExitFullscreen
{
    [self requestExitFullscreenWithAnimation:YES];
}

- (void)fadeHUDIn
{
    [_hudController fadeWindowIn];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(change);
    UNUSED_PARAM(context);

    if ([keyPath isEqualTo:@"rate"])
        [self rateChanged:nil];
}

- (void)rateChanged:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    [_hudController updateRate];
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
    [self setIgnoresMouseEvents:NO];
    [self setAcceptsMouseMovedEvents:YES];
    return self;
}

- (void)dealloc
{
    ASSERT(!_fullscreenAnimation);
    [super dealloc];
}

- (BOOL)resignFirstResponder
{
    return NO;
}

- (BOOL)canBecomeKeyWindow
{
    return NO;
}

- (void)mouseDown:(NSEvent *)event
{
    UNUSED_PARAM(event);
}

- (void)cancelOperation:(id)sender
{
    UNUSED_PARAM(sender);
    [[self windowController] requestExitFullscreen];
}

- (void)animatedResizeDidEnd
{
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
        [subAnimation setCurrentProgress:1];
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

- (void)mouseMoved:(NSEvent *)event
{
    UNUSED_PARAM(event);
    [[self windowController] fadeHUDIn];
}

@end

ALLOW_DEPRECATED_DECLARATIONS_END

#endif
