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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if ENABLE(VIDEO)

#import "WebVideoFullscreenHUDWindowController.h"

#import "WebKitSystemInterface.h"
#import "WebTypesInternal.h"
#import <JavaScriptCore/RetainPtr.h>
#import <JavaScriptCore/UnusedParam.h>
#import <WebCore/HTMLMediaElement.h>

using namespace WebCore;
using namespace std;

static inline CGFloat webkit_CGFloor(CGFloat value)
{
    if (sizeof(value) == sizeof(float))
        return floorf(value);
    return floor(value);
}

#define HAVE_MEDIA_CONTROL (!defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD))

@interface WebVideoFullscreenHUDWindowController (Private) <NSWindowDelegate>

- (void)updateTime;
- (void)timelinePositionChanged:(id)sender;
- (float)currentTime;
- (void)setCurrentTime:(float)currentTime;
- (double)duration;

- (void)volumeChanged:(id)sender;
- (double)maxVolume;
- (double)volume;
- (void)setVolume:(double)volume;
- (void)decrementVolume;
- (void)incrementVolume;

- (void)togglePlaying:(id)sender;
- (BOOL)playing;
- (void)setPlaying:(BOOL)playing;

- (void)rewind:(id)sender;
- (void)fastForward:(id)sender;

- (NSString *)remainingTimeText;
- (NSString *)elapsedTimeText;

- (void)exitFullscreen:(id)sender;
@end

@interface WebVideoFullscreenHUDWindow : NSWindow
@end

@implementation WebVideoFullscreenHUDWindow

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)aStyle backing:(NSBackingStoreType)bufferingType defer:(BOOL)flag
{
    UNUSED_PARAM(aStyle);
    self = [super initWithContentRect:contentRect styleMask:NSBorderlessWindowMask backing:bufferingType defer:flag];
    if (!self)
        return nil;

    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setLevel:NSPopUpMenuWindowLevel];
    [self setAcceptsMouseMovedEvents:YES];
    [self setIgnoresMouseEvents:NO];
    [self setMovableByWindowBackground:YES];
    [self setHidesOnDeactivate:YES];

    return self;
}

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (void)cancelOperation:(id)sender
{
    [[self windowController] exitFullscreen:self];
}

- (void)center
{
    NSRect hudFrame = [self frame];
    NSRect screenFrame = [[NSScreen mainScreen] frame];
    [self setFrameTopLeftPoint:NSMakePoint(screenFrame.origin.x + (screenFrame.size.width - hudFrame.size.width) / 2,
                                           screenFrame.origin.y + (screenFrame.size.height - hudFrame.size.height) / 6)];
}

- (void)keyDown:(NSEvent *)event
{
    [super keyDown:event];
    [[self windowController] fadeWindowIn];
}

- (BOOL)resignFirstResponder
{
    return NO;
}

- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    // Block all command key events while the fullscreen window is up.
    if ([event type] != NSKeyDown)
        return NO;
    
    if (!([event modifierFlags] & NSCommandKeyMask))
        return NO;
    
    return YES;
}

@end

static const CGFloat windowHeight = 59;
static const CGFloat windowWidth = 438;

static const NSTimeInterval HUDWindowFadeOutDelay = 3;

@implementation WebVideoFullscreenHUDWindowController

- (id)init
{
    NSWindow *window = [[WebVideoFullscreenHUDWindow alloc] initWithContentRect:NSMakeRect(0, 0, windowWidth, windowHeight)
                            styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    self = [super initWithWindow:window];
    [window setDelegate:self];
    [window release];
    if (!self)
        return nil;
    [self windowDidLoad];
    return self;
}

- (void)dealloc
{
    ASSERT(!_timelineUpdateTimer);
#if !defined(BUILDING_ON_TIGER)
    ASSERT(!_area);
#endif
    [_timeline release];
    [_remainingTimeText release];
    [_elapsedTimeText release];
    [_volumeSlider release];
    [_playButton release];
    [super dealloc];
}

#if !defined(BUILDING_ON_TIGER)
- (void)setArea:(NSTrackingArea *)area
{
    if (area == _area)
        return;
    [_area release];
    _area = [area retain];
}
#endif

- (void)keyDown:(NSEvent *)event
{
    NSString *charactersIgnoringModifiers = [event charactersIgnoringModifiers];
    if ([charactersIgnoringModifiers length] == 1) {
        switch ([charactersIgnoringModifiers characterAtIndex:0]) {
            case ' ':
                [self togglePlaying:nil];
                return;
            case NSUpArrowFunctionKey:
                if ([event modifierFlags] & NSAlternateKeyMask)
                    [self setVolume:[self maxVolume]];
                else
                    [self incrementVolume];
                return;
            case NSDownArrowFunctionKey:
                if ([event modifierFlags] & NSAlternateKeyMask)
                    [self setVolume:0];
                else
                    [self decrementVolume];
                return;
            default:
                break;
        }
    }

    [super keyDown:event];
}

- (id<WebVideoFullscreenHUDWindowControllerDelegate>)delegate
{
    return _delegate;
}

- (void)setDelegate:(id<WebVideoFullscreenHUDWindowControllerDelegate>)delegate
{
    _delegate = delegate;
}

- (void)scheduleTimeUpdate
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(unscheduleTimeUpdate) object:self];

    // First, update right away, then schedule future update
    [self updateTime];
    [self updateRate];

    [_timelineUpdateTimer invalidate];
    [_timelineUpdateTimer release];

    // Note that this creates a retain cycle between the window and us.
    _timelineUpdateTimer = [[NSTimer timerWithTimeInterval:0.25 target:self selector:@selector(updateTime) userInfo:nil repeats:YES] retain];
#if defined(BUILDING_ON_TIGER)
    [[NSRunLoop currentRunLoop] addTimer:_timelineUpdateTimer forMode:(NSString *)kCFRunLoopCommonModes];
#else
    [[NSRunLoop currentRunLoop] addTimer:_timelineUpdateTimer forMode:NSRunLoopCommonModes];
#endif
}

- (void)unscheduleTimeUpdate
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(unscheduleTimeUpdate) object:nil];

    [_timelineUpdateTimer invalidate];
    [_timelineUpdateTimer release];
    _timelineUpdateTimer = nil;
}

- (void)fadeWindowIn
{
    NSWindow *window = [self window];
    if (![window isVisible])
        [window setAlphaValue:0];

    [window makeKeyAndOrderFront:self];
#if defined(BUILDING_ON_TIGER)
    [window setAlphaValue:1];
#else
    [[window animator] setAlphaValue:1];
#endif
    [self scheduleTimeUpdate];

    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(fadeWindowOut) object:nil];
    if (!_mouseIsInHUD && [self playing])   // Don't fade out when paused.
        [self performSelector:@selector(fadeWindowOut) withObject:nil afterDelay:HUDWindowFadeOutDelay];
}

- (void)fadeWindowOut
{
    [NSCursor setHiddenUntilMouseMoves:YES];
#if defined(BUILDING_ON_TIGER)
    [[self window] setAlphaValue:0];
#else
    [[[self window] animator] setAlphaValue:0];
#endif
    [self performSelector:@selector(unscheduleTimeUpdate) withObject:nil afterDelay:1];
}

- (void)closeWindow
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(fadeWindowOut) object:nil];
    [self unscheduleTimeUpdate];
    NSWindow *window = [self window];
#if !defined(BUILDING_ON_TIGER)
    [[window contentView] removeTrackingArea:_area];
    [self setArea:nil];
#endif
    [window close];
    [window setDelegate:nil];
    [self setWindow:nil];
}

#ifndef HAVE_MEDIA_CONTROL
enum {
    WKMediaUIControlPlayPauseButton,
    WKMediaUIControlRewindButton,
    WKMediaUIControlFastForwardButton,
    WKMediaUIControlExitFullscreenButton,
    WKMediaUIControlVolumeDownButton,
    WKMediaUIControlSlider,
    WKMediaUIControlVolumeUpButton,
    WKMediaUIControlTimeline
};
#endif

static NSControl *createControlWithMediaUIControlType(int controlType, NSRect frame)
{
#ifdef HAVE_MEDIA_CONTROL
    NSControl *control = WKCreateMediaUIControl(controlType);
    [control setFrame:frame];
    return control;
#else
    if (controlType == WKMediaUIControlSlider)
        return [[NSSlider alloc] initWithFrame:frame];
    return [[NSControl alloc] initWithFrame:frame];
#endif
}

static NSTextField *createTimeTextField(NSRect frame)
{
    NSTextField *textField = [[NSTextField alloc] initWithFrame:frame];
    [textField setTextColor:[NSColor whiteColor]];
    [textField setBordered:NO];
    [textField setFont:[NSFont boldSystemFontOfSize:10]];
    [textField setDrawsBackground:NO];
    [textField setBezeled:NO];
    [textField setEditable:NO];
    [textField setSelectable:NO];
    return textField;
}

- (void)windowDidLoad
{
    static const CGFloat horizontalMargin = 10;
    static const CGFloat playButtonWidth = 41;
    static const CGFloat playButtonHeight = 35;
    static const CGFloat playButtonTopMargin = 4;
    static const CGFloat volumeSliderWidth = 50;
    static const CGFloat volumeSliderHeight = 13;
    static const CGFloat volumeButtonWidth = 18;
    static const CGFloat volumeButtonHeight = 16;
    static const CGFloat volumeUpButtonLeftMargin = 4;
    static const CGFloat volumeControlsTopMargin = 13;
    static const CGFloat exitFullScreenButtonWidth = 25;
    static const CGFloat exitFullScreenButtonHeight = 21;
    static const CGFloat exitFullScreenButtonTopMargin = 11;
    static const CGFloat timelineWidth = 315;
    static const CGFloat timelineHeight = 14;
    static const CGFloat timelineBottomMargin = 7;
    static const CGFloat timeTextFieldWidth = 54;
    static const CGFloat timeTextFieldHeight = 13;
    static const CGFloat timeTextFieldHorizontalMargin = 7;

    NSWindow *window = [self window];
    ASSERT(window);

#ifdef HAVE_MEDIA_CONTROL
    NSView *background = WKCreateMediaUIBackgroundView();
#else
    NSView *background = [[NSView alloc] init];
#endif
    [window setContentView:background];
#if !defined(BUILDING_ON_TIGER)
    _area = [[NSTrackingArea alloc] initWithRect:[background bounds] options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways owner:self userInfo:nil];
    [background addTrackingArea:_area];
#endif
    [background release];    

    NSView *contentView = [window contentView];

    CGFloat center = webkit_CGFloor((windowWidth - playButtonWidth) / 2);
    _playButton = (NSButton *)createControlWithMediaUIControlType(WKMediaUIControlPlayPauseButton, NSMakeRect(center, windowHeight - playButtonTopMargin - playButtonHeight, playButtonWidth, playButtonHeight));
    ASSERT([_playButton isKindOfClass:[NSButton class]]);
    [_playButton setTarget:self];
    [_playButton setAction:@selector(togglePlaying:)];
    [contentView addSubview:_playButton];

    CGFloat closeToRight = windowWidth - horizontalMargin - exitFullScreenButtonWidth;
    NSControl *exitFullscreenButton = createControlWithMediaUIControlType(WKMediaUIControlExitFullscreenButton, NSMakeRect(closeToRight, windowHeight - exitFullScreenButtonTopMargin - exitFullScreenButtonHeight, exitFullScreenButtonWidth, exitFullScreenButtonHeight));
    [exitFullscreenButton setAction:@selector(exitFullscreen:)];
    [exitFullscreenButton setTarget:self];
    [contentView addSubview:exitFullscreenButton];
    [exitFullscreenButton release];
    
    CGFloat volumeControlsBottom = windowHeight - volumeControlsTopMargin - volumeButtonHeight;
    CGFloat left = horizontalMargin;
    NSControl *volumeDownButton = createControlWithMediaUIControlType(WKMediaUIControlVolumeDownButton, NSMakeRect(left, volumeControlsBottom, volumeButtonWidth, volumeButtonHeight));
    [contentView addSubview:volumeDownButton];
    [volumeDownButton setTarget:self];
    [volumeDownButton setAction:@selector(setVolumeToZero:)];
    [volumeDownButton release];

    left += volumeButtonWidth;
    _volumeSlider = createControlWithMediaUIControlType(WKMediaUIControlSlider, NSMakeRect(left, volumeControlsBottom + webkit_CGFloor((volumeButtonHeight - volumeSliderHeight) / 2), volumeSliderWidth, volumeSliderHeight));
    [_volumeSlider setValue:[NSNumber numberWithDouble:[self maxVolume]] forKey:@"maxValue"];
    [_volumeSlider setTarget:self];
    [_volumeSlider setAction:@selector(volumeChanged:)];
    [contentView addSubview:_volumeSlider];

    left += volumeSliderWidth + volumeUpButtonLeftMargin;
    NSControl *volumeUpButton = createControlWithMediaUIControlType(WKMediaUIControlVolumeUpButton, NSMakeRect(left, volumeControlsBottom, volumeButtonWidth, volumeButtonHeight));
    [volumeUpButton setTarget:self];
    [volumeUpButton setAction:@selector(setVolumeToMaximum:)];
    [contentView addSubview:volumeUpButton];
    [volumeUpButton release];

#ifdef HAVE_MEDIA_CONTROL
    _timeline = WKCreateMediaUIControl(WKMediaUIControlTimeline);
#else
    _timeline = [[NSSlider alloc] init];
#endif
    [_timeline setTarget:self];
    [_timeline setAction:@selector(timelinePositionChanged:)];
    [_timeline setFrame:NSMakeRect(webkit_CGFloor((windowWidth - timelineWidth) / 2), timelineBottomMargin, timelineWidth, timelineHeight)];
    [contentView addSubview:_timeline];

    _elapsedTimeText = createTimeTextField(NSMakeRect(timeTextFieldHorizontalMargin, timelineBottomMargin, timeTextFieldWidth, timeTextFieldHeight));
    [_elapsedTimeText setAlignment:NSLeftTextAlignment];
    [contentView addSubview:_elapsedTimeText];

    _remainingTimeText = createTimeTextField(NSMakeRect(windowWidth - timeTextFieldHorizontalMargin - timeTextFieldWidth, timelineBottomMargin, timeTextFieldWidth, timeTextFieldHeight));
    [_remainingTimeText setAlignment:NSRightTextAlignment];
    [contentView addSubview:_remainingTimeText];

    [window recalculateKeyViewLoop];
    [window setInitialFirstResponder:_playButton];
    [window center];
}

- (void)updateVolume
{
    [_volumeSlider setDoubleValue:[self volume]];
}

- (void)updateTime
{
    [self updateVolume];

    [_timeline setFloatValue:[self currentTime]];
    [_timeline setValue:[NSNumber numberWithDouble:[self duration]] forKey:@"maxValue"];

    [_remainingTimeText setStringValue:[self remainingTimeText]];
    [_elapsedTimeText setStringValue:[self elapsedTimeText]];
}

- (void)timelinePositionChanged:(id)sender
{
    [self setCurrentTime:[_timeline floatValue]];
}

- (float)currentTime
{
    return [_delegate mediaElement] ? [_delegate mediaElement]->currentTime() : 0;
}

- (void)setCurrentTime:(float)currentTime
{
    if (![_delegate mediaElement])
        return;
    WebCore::ExceptionCode e;
    [_delegate mediaElement]->setCurrentTime(currentTime, e);
    [self updateTime];
}

- (double)duration
{
    return [_delegate mediaElement] ? [_delegate mediaElement]->duration() : 0;
}

- (double)maxVolume
{
    // Set the volume slider resolution
    return 100;
}

- (void)volumeChanged:(id)sender
{
    [self setVolume:[_volumeSlider doubleValue]];
}

- (void)setVolumeToZero:(id)sender
{
    [self setVolume:0];
}

- (void)setVolumeToMaximum:(id)sender
{
    [self setVolume:[self maxVolume]];
}

- (void)decrementVolume
{
    if (![_delegate mediaElement])
        return;

    double volume = [self volume] - 10;
    [self setVolume:max(volume, 0.)];
}

- (void)incrementVolume
{
    if (![_delegate mediaElement])
        return;

    double volume = [self volume] + 10;
    [self setVolume:min(volume, [self maxVolume])];
}

- (double)volume
{
    return [_delegate mediaElement] ? [_delegate mediaElement]->volume() * [self maxVolume] : 0;
}

- (void)setVolume:(double)volume
{
    if (![_delegate mediaElement])
        return;
    WebCore::ExceptionCode e;
    if ([_delegate mediaElement]->muted())
        [_delegate mediaElement]->setMuted(false);
    [_delegate mediaElement]->setVolume(volume / [self maxVolume], e);
    [self updateVolume];
}

- (void)updateRate
{
    [_playButton setIntValue:[self playing]];
}

- (void)togglePlaying:(id)sender
{
    BOOL nowPlaying = [self playing];
    [self setPlaying:!nowPlaying];

    // Keep HUD visible when paused
    if (!nowPlaying)
        [self fadeWindowIn];
    else if (!_mouseIsInHUD) {
        [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(fadeWindowOut) object:nil];
        [self performSelector:@selector(fadeWindowOut) withObject:nil afterDelay:HUDWindowFadeOutDelay];
    }
}

- (BOOL)playing
{
    if (![_delegate mediaElement])
        return false;
    return ![_delegate mediaElement]->canPlay();
}

- (void)setPlaying:(BOOL)playing
{
    if (![_delegate mediaElement])
        return;

    if (playing)
        [_delegate mediaElement]->play();
    else
        [_delegate mediaElement]->pause();
}

static NSString *timeToString(double time)
{
    ASSERT_ARG(time, time >= 0);

    if (!isfinite(time))
        time = 0;

    int seconds = fabs(time); 
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;

    if (hours)
        return [NSString stringWithFormat:@"%d:%02d:%02d", hours, minutes, seconds];

    return [NSString stringWithFormat:@"%02d:%02d", minutes, seconds];    
}

- (NSString *)remainingTimeText
{
    HTMLMediaElement* mediaElement = [_delegate mediaElement];
    if (!mediaElement)
        return @"";

    return [@"-" stringByAppendingString:timeToString(mediaElement->duration() - mediaElement->currentTime())];
}

- (NSString *)elapsedTimeText
{
    if (![_delegate mediaElement])
        return @"";

    return timeToString([_delegate mediaElement]->currentTime());
}

#pragma mark NSResponder

- (void)mouseEntered:(NSEvent *)theEvent
{
    // Make sure the HUD won't be hidden from now
    _mouseIsInHUD = YES;
    [self fadeWindowIn];
}

- (void)mouseExited:(NSEvent *)theEvent
{
    _mouseIsInHUD = NO;
    [self fadeWindowIn];
}

- (void)rewind:(id)sender
{
    if (![_delegate mediaElement])
        return;
    [_delegate mediaElement]->rewind(30);
}

- (void)fastForward:(id)sender
{
    if (![_delegate mediaElement])
        return;
}

- (void)exitFullscreen:(id)sender
{
    if (_isEndingFullscreen)
        return;
    _isEndingFullscreen = YES;
    [_delegate requestExitFullscreen]; 
}

#pragma mark NSWindowDelegate

- (void)windowDidExpose:(NSNotification *)notification
{
    [self scheduleTimeUpdate];
}

- (void)windowDidClose:(NSNotification *)notification
{
    [self unscheduleTimeUpdate];
}

@end

#endif
