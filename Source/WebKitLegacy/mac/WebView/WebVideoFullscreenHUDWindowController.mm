/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO) && PLATFORM(MAC)

#import "WebVideoFullscreenHUDWindowController.h"

#import <WebCore/FloatConversion.h>
#import <WebCore/HTMLVideoElement.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/QTKitSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK(QTKit)

SOFT_LINK_CLASS(QTKit, QTHUDBackgroundView)
SOFT_LINK_CLASS(QTKit, QTHUDButton)
SOFT_LINK_CLASS(QTKit, QTHUDSlider)
SOFT_LINK_CLASS(QTKit, QTHUDTimeline)

#define QTHUDBackgroundView getQTHUDBackgroundViewClass()
#define QTHUDButton getQTHUDButtonClass()
#define QTHUDSlider getQTHUDSliderClass()
#define QTHUDTimeline getQTHUDTimelineClass()

enum class MediaUIControl {
    Timeline,
    Slider,
    PlayPauseButton,
    ExitFullscreenButton,
    RewindButton,
    FastForwardButton,
    VolumeUpButton,
    VolumeDownButton,
};

using WebCore::HTMLVideoElement;
using WebCore::narrowPrecisionToFloat;

@interface WebVideoFullscreenHUDWindowController (Private) <NSWindowDelegate>

- (void)updateTime;
- (void)timelinePositionChanged:(id)sender;
- (float)currentTime;
- (void)setCurrentTime:(float)currentTime;
- (double)duration;

- (void)volumeChanged:(id)sender;
- (float)maxVolume;
- (float)volume;
- (void)setVolume:(float)volume;
- (void)decrementVolume;
- (void)incrementVolume;

- (void)updatePlayButton;
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
    self = [super initWithContentRect:contentRect styleMask:NSWindowStyleMaskBorderless backing:bufferingType defer:flag];
    if (!self)
        return nil;

    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setLevel:NSPopUpMenuWindowLevel];
    [self setAcceptsMouseMovedEvents:YES];
    [self setIgnoresMouseEvents:NO];
    [self setMovableByWindowBackground:YES];

    return self;
}

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (void)cancelOperation:(id)sender
{
    UNUSED_PARAM(sender);
    [[self windowController] exitFullscreen:self];
}

- (void)center
{
    NSRect hudFrame = [self frame];
    NSRect screenFrame = [[NSScreen mainScreen] frame];
    [self setFrameTopLeftPoint:NSMakePoint(screenFrame.origin.x + (screenFrame.size.width - hudFrame.size.width) / 2, screenFrame.origin.y + (screenFrame.size.height - hudFrame.size.height) / 6)];
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
    if ([event type] != NSEventTypeKeyDown)
        return NO;
    
    if (!([event modifierFlags] & NSEventModifierFlagCommand))
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
    NSWindow *window = [[WebVideoFullscreenHUDWindow alloc] initWithContentRect:NSMakeRect(0, 0, windowWidth, windowHeight) styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
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
    ASSERT(!_area);
    ASSERT(!_isScrubbing);
    [_timeline release];
    [_remainingTimeText release];
    [_elapsedTimeText release];
    [_volumeSlider release];
    [_playButton release];
    [super dealloc];
}

- (void)setArea:(NSTrackingArea *)area
{
    if (area == _area)
        return;
    [_area release];
    _area = [area retain];
}

- (void)keyDown:(NSEvent *)event
{
    NSString *charactersIgnoringModifiers = [event charactersIgnoringModifiers];
    if ([charactersIgnoringModifiers length] == 1) {
        switch ([charactersIgnoringModifiers characterAtIndex:0]) {
        case ' ':
            [self togglePlaying:nil];
            return;
        case NSUpArrowFunctionKey:
            if ([event modifierFlags] & NSEventModifierFlagOption)
                [self setVolume:[self maxVolume]];
            else
                [self incrementVolume];
            return;
        case NSDownArrowFunctionKey:
            if ([event modifierFlags] & NSEventModifierFlagOption)
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

- (id <WebVideoFullscreenHUDWindowControllerDelegate>)delegate
{
    return _delegate;
}

- (void)setDelegate:(id <WebVideoFullscreenHUDWindowControllerDelegate>)delegate
{
    _delegate = delegate;
}

- (void)scheduleTimeUpdate
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(unscheduleTimeUpdate) object:self];

    // First, update right away, then schedule future update
    [self updateTime];
    [self updatePlayButton];

    [_timelineUpdateTimer invalidate];
    [_timelineUpdateTimer release];

    // Note that this creates a retain cycle between the window and us.
    _timelineUpdateTimer = [[NSTimer timerWithTimeInterval:0.25 target:self selector:@selector(updateTime) userInfo:nil repeats:YES] retain];
    [[NSRunLoop currentRunLoop] addTimer:_timelineUpdateTimer forMode:NSRunLoopCommonModes];
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
    [[window animator] setAlphaValue:1];
    [self scheduleTimeUpdate];

    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(fadeWindowOut) object:nil];
    if (!_mouseIsInHUD && [self playing]) // Don't fade out when paused.
        [self performSelector:@selector(fadeWindowOut) withObject:nil afterDelay:HUDWindowFadeOutDelay];
}

- (void)fadeWindowOut
{
    [NSCursor setHiddenUntilMouseMoves:YES];
    [[[self window] animator] setAlphaValue:0];
    [self performSelector:@selector(unscheduleTimeUpdate) withObject:nil afterDelay:1];
}

- (void)closeWindow
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(fadeWindowOut) object:nil];
    [self unscheduleTimeUpdate];
    NSWindow *window = [self window];
    [[window contentView] removeTrackingArea:_area];
    [self setArea:nil];
    [window close];
    [window setDelegate:nil];
    [self setWindow:nil];
}

static NSControl *createMediaUIControl(MediaUIControl controlType)
{
    switch (controlType) {
    case MediaUIControl::Timeline: {
        NSSlider *slider = [[QTHUDTimeline alloc] init];
        [[slider cell] setContinuous:YES];
        return slider;
    }
    case MediaUIControl::Slider: {
        NSButton *slider = [[QTHUDSlider alloc] init];
        [[slider cell] setContinuous:YES];
        return slider;
    }
    case MediaUIControl::PlayPauseButton: {
        NSButton *button = [[QTHUDButton alloc] init];
        [button setImage:[NSImage imageNamed:@"NSPlayTemplate"]];
        [button setAlternateImage:[NSImage imageNamed:@"NSPauseQTPrivateTemplate"]];

        [[button cell] setShowsStateBy:NSContentsCellMask];
        [button setBordered:NO];
        return button;
    }
    case MediaUIControl::ExitFullscreenButton: {
        NSButton *button = [[QTHUDButton alloc] init];
        [button setImage:[NSImage imageNamed:@"NSExitFullScreenTemplate"]];
        [button setBordered:NO];
        return button;
    }
    case MediaUIControl::RewindButton: {
        NSButton *button = [[QTHUDButton alloc] init];
        [button setImage:[NSImage imageNamed:@"NSRewindTemplate"]];
        [button setBordered:NO];
        return button;
    }
    case MediaUIControl::FastForwardButton: {
        NSButton *button = [[QTHUDButton alloc] init];
        [button setImage:[NSImage imageNamed:@"NSFastForwardTemplate"]];
        [button setBordered:NO];
        return button;
    }
    case MediaUIControl::VolumeUpButton: {
        NSButton *button = [[QTHUDButton alloc] init];
        [button setImage:[NSImage imageNamed:@"NSAudioOutputVolumeHighTemplate"]];
        [button setBordered:NO];
        return button;
    }
    case MediaUIControl::VolumeDownButton: {
        NSButton *button = [[QTHUDButton alloc] init];
        [button setImage:[NSImage imageNamed:@"NSAudioOutputVolumeLowTemplate"]];
        [button setBordered:NO];
        return button;
    }
    }

    ASSERT_NOT_REACHED();
    return nil;
}

static NSControl *createControlWithMediaUIControlType(MediaUIControl controlType, NSRect frame)
{
    NSControl *control = createMediaUIControl(controlType);
    control.frame = frame;
    return control;
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

static NSView *createMediaUIBackgroundView()
{
    id view = [[QTHUDBackgroundView alloc] init];

    const CGFloat quickTimePlayerHUDHeight = 59;
    const CGFloat quickTimePlayerHUDContentBorderPosition = 38;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [view setContentBorderPosition:quickTimePlayerHUDContentBorderPosition / quickTimePlayerHUDHeight];
    ALLOW_DEPRECATED_DECLARATIONS_END

    return view;
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
    static const CGFloat exitFullscreenButtonWidth = 25;
    static const CGFloat exitFullscreenButtonHeight = 21;
    static const CGFloat exitFullscreenButtonTopMargin = 11;
    static const CGFloat timelineWidth = 315;
    static const CGFloat timelineHeight = 14;
    static const CGFloat timelineBottomMargin = 7;
    static const CGFloat timeTextFieldWidth = 54;
    static const CGFloat timeTextFieldHeight = 13;
    static const CGFloat timeTextFieldHorizontalMargin = 7;

    NSWindow *window = [self window];
    ASSERT(window);

    NSView *background = createMediaUIBackgroundView();

    [window setContentView:background];
    _area = [[NSTrackingArea alloc] initWithRect:[background bounds] options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways owner:self userInfo:nil];
    [background addTrackingArea:_area];
    [background release];    

    NSView *contentView = [window contentView];

    CGFloat center = CGFloor((windowWidth - playButtonWidth) / 2);
    _playButton = (NSButton *)createControlWithMediaUIControlType(MediaUIControl::PlayPauseButton, NSMakeRect(center, windowHeight - playButtonTopMargin - playButtonHeight, playButtonWidth, playButtonHeight));
    ASSERT([_playButton isKindOfClass:[NSButton class]]);
    [_playButton setTarget:self];
    [_playButton setAction:@selector(togglePlaying:)];
    [contentView addSubview:_playButton];

    CGFloat closeToRight = windowWidth - horizontalMargin - exitFullscreenButtonWidth;
    NSControl *exitFullscreenButton = createControlWithMediaUIControlType(MediaUIControl::ExitFullscreenButton, NSMakeRect(closeToRight, windowHeight - exitFullscreenButtonTopMargin - exitFullscreenButtonHeight, exitFullscreenButtonWidth, exitFullscreenButtonHeight));
    [exitFullscreenButton setAction:@selector(exitFullscreen:)];
    [exitFullscreenButton setTarget:self];
    [contentView addSubview:exitFullscreenButton];
    [exitFullscreenButton release];
    
    CGFloat volumeControlsBottom = windowHeight - volumeControlsTopMargin - volumeButtonHeight;
    CGFloat left = horizontalMargin;
    NSControl *volumeDownButton = createControlWithMediaUIControlType(MediaUIControl::VolumeDownButton, NSMakeRect(left, volumeControlsBottom, volumeButtonWidth, volumeButtonHeight));
    [contentView addSubview:volumeDownButton];
    [volumeDownButton setTarget:self];
    [volumeDownButton setAction:@selector(setVolumeToZero:)];
    [volumeDownButton release];

    left += volumeButtonWidth;
    _volumeSlider = createControlWithMediaUIControlType(MediaUIControl::Slider, NSMakeRect(left, volumeControlsBottom + CGFloor((volumeButtonHeight - volumeSliderHeight) / 2), volumeSliderWidth, volumeSliderHeight));
    [_volumeSlider setValue:[NSNumber numberWithDouble:[self maxVolume]] forKey:@"maxValue"];
    [_volumeSlider setTarget:self];
    [_volumeSlider setAction:@selector(volumeChanged:)];
    [contentView addSubview:_volumeSlider];

    left += volumeSliderWidth + volumeUpButtonLeftMargin;
    NSControl *volumeUpButton = createControlWithMediaUIControlType(MediaUIControl::VolumeUpButton, NSMakeRect(left, volumeControlsBottom, volumeButtonWidth, volumeButtonHeight));
    [volumeUpButton setTarget:self];
    [volumeUpButton setAction:@selector(setVolumeToMaximum:)];
    [contentView addSubview:volumeUpButton];
    [volumeUpButton release];

    _timeline = createMediaUIControl(MediaUIControl::Timeline);

    [_timeline setTarget:self];
    [_timeline setAction:@selector(timelinePositionChanged:)];
    [_timeline setFrame:NSMakeRect(CGFloor((windowWidth - timelineWidth) / 2), timelineBottomMargin, timelineWidth, timelineHeight)];
    [contentView addSubview:_timeline];

    _elapsedTimeText = createTimeTextField(NSMakeRect(timeTextFieldHorizontalMargin, timelineBottomMargin, timeTextFieldWidth, timeTextFieldHeight));
    [_elapsedTimeText setAlignment:NSTextAlignmentLeft];
    [contentView addSubview:_elapsedTimeText];

    _remainingTimeText = createTimeTextField(NSMakeRect(windowWidth - timeTextFieldHorizontalMargin - timeTextFieldWidth, timelineBottomMargin, timeTextFieldWidth, timeTextFieldHeight));
    [_remainingTimeText setAlignment:NSTextAlignmentRight];
    [contentView addSubview:_remainingTimeText];

    [window recalculateKeyViewLoop];
    [window setInitialFirstResponder:_playButton];
    [window center];
}

- (void)updateVolume
{
    [_volumeSlider setFloatValue:[self volume]];
}

- (void)updateTime
{
    [self updateVolume];

    [_timeline setFloatValue:[self currentTime]];
    [_timeline setValue:[NSNumber numberWithDouble:[self duration]] forKey:@"maxValue"];

    [_remainingTimeText setStringValue:[self remainingTimeText]];
    [_elapsedTimeText setStringValue:[self elapsedTimeText]];
}

- (void)endScrubbing
{
    ASSERT(_isScrubbing);
    _isScrubbing = NO;
    if (HTMLVideoElement* videoElement = [_delegate videoElement])
        videoElement->endScrubbing();
}

- (void)timelinePositionChanged:(id)sender
{
    UNUSED_PARAM(sender);
    [self setCurrentTime:[_timeline floatValue]];
    if (!_isScrubbing) {
        _isScrubbing = YES;
        if (HTMLVideoElement* videoElement = [_delegate videoElement])
            videoElement->beginScrubbing();
        static NSArray *endScrubbingModes = [[NSArray alloc] initWithObjects:NSDefaultRunLoopMode, NSModalPanelRunLoopMode, nil];
        // Schedule -endScrubbing for when leaving mouse tracking mode.
        [[NSRunLoop currentRunLoop] performSelector:@selector(endScrubbing) target:self argument:nil order:0 modes:endScrubbingModes];
    }
}

- (float)currentTime
{
    return [_delegate videoElement] ? [_delegate videoElement]->currentTime() : 0;
}

- (void)setCurrentTime:(float)currentTime
{
    if (![_delegate videoElement])
        return;
    [_delegate videoElement]->setCurrentTime(currentTime);
    [self updateTime];
}

- (double)duration
{
    return [_delegate videoElement] ? [_delegate videoElement]->duration() : 0;
}

- (float)maxVolume
{
    // Set the volume slider resolution
    return 100;
}

- (void)volumeChanged:(id)sender
{
    UNUSED_PARAM(sender);
    [self setVolume:[_volumeSlider floatValue]];
}

- (void)setVolumeToZero:(id)sender
{
    UNUSED_PARAM(sender);
    [self setVolume:0];
}

- (void)setVolumeToMaximum:(id)sender
{
    UNUSED_PARAM(sender);
    [self setVolume:[self maxVolume]];
}

- (void)decrementVolume
{
    if (![_delegate videoElement])
        return;

    float volume = [self volume] - 10;
    [self setVolume:std::max(volume, 0.0f)];
}

- (void)incrementVolume
{
    if (![_delegate videoElement])
        return;

    float volume = [self volume] + 10;
    [self setVolume:std::min(volume, [self maxVolume])];
}

- (float)volume
{
    return [_delegate videoElement] ? [_delegate videoElement]->volume() * [self maxVolume] : 0;
}

- (void)setVolume:(float)volume
{
    if (![_delegate videoElement])
        return;
    if ([_delegate videoElement]->muted())
        [_delegate videoElement]->setMuted(false);
    [_delegate videoElement]->setVolume(volume / [self maxVolume]);
    [self updateVolume];
}

- (void)updatePlayButton
{
    [_playButton setIntValue:[self playing]];
}

- (void)updateRate
{
    BOOL playing = [self playing];

    // Keep the HUD visible when paused.
    if (!playing)
        [self fadeWindowIn];
    else if (!_mouseIsInHUD) {
        [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(fadeWindowOut) object:nil];
        [self performSelector:@selector(fadeWindowOut) withObject:nil afterDelay:HUDWindowFadeOutDelay];
    }
    [self updatePlayButton];
}

- (void)togglePlaying:(id)sender
{
    UNUSED_PARAM(sender);
    [self setPlaying:![self playing]];
}

- (BOOL)playing
{
    HTMLVideoElement* videoElement = [_delegate videoElement];
    if (!videoElement)
        return NO;

    return !videoElement->canPlay();
}

- (void)setPlaying:(BOOL)playing
{
    HTMLVideoElement* videoElement = [_delegate videoElement];

    if (!videoElement)
        return;

    if (playing)
        videoElement->play();
    else
        videoElement->pause();
}

static NSString *timeToString(double time)
{
    ASSERT_ARG(time, time >= 0);

    if (!std::isfinite(time))
        time = 0;

    int seconds = narrowPrecisionToFloat(std::abs(time));
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;

    if (hours)
        return [NSString stringWithFormat:@"%d:%02d:%02d", hours, minutes, seconds];

    return [NSString stringWithFormat:@"%02d:%02d", minutes, seconds];    
}

- (NSString *)remainingTimeText
{
    HTMLVideoElement* videoElement = [_delegate videoElement];
    if (!videoElement)
        return @"";

    double remainingTime = 0;

    if (std::isfinite(videoElement->duration()) && std::isfinite(videoElement->currentTime()))
        remainingTime = videoElement->duration() - videoElement->currentTime();

    return [@"-" stringByAppendingString:timeToString(remainingTime)];
}

- (NSString *)elapsedTimeText
{
    if (![_delegate videoElement])
        return @"";

    return timeToString([_delegate videoElement]->currentTime());
}

// MARK: NSResponder

- (void)mouseEntered:(NSEvent *)theEvent
{
    UNUSED_PARAM(theEvent);
    // Make sure the HUD won't be hidden from now
    _mouseIsInHUD = YES;
    [self fadeWindowIn];
}

- (void)mouseExited:(NSEvent *)theEvent
{
    UNUSED_PARAM(theEvent);
    _mouseIsInHUD = NO;
    [self fadeWindowIn];
}

- (void)rewind:(id)sender
{
    UNUSED_PARAM(sender);
    if (![_delegate videoElement])
        return;
    [_delegate videoElement]->rewind(30);
}

- (void)fastForward:(id)sender
{
    UNUSED_PARAM(sender);
    if (![_delegate videoElement])
        return;
}

- (void)exitFullscreen:(id)sender
{
    UNUSED_PARAM(sender);
    if (_isEndingFullscreen)
        return;
    _isEndingFullscreen = YES;
    [_delegate requestExitFullscreen]; 
}

// MARK: NSWindowDelegate

- (void)windowDidExpose:(NSNotification *)notification
{
    UNUSED_PARAM(notification);
    [self scheduleTimeUpdate];
}

- (void)windowDidClose:(NSNotification *)notification
{
    UNUSED_PARAM(notification);
    [self unscheduleTimeUpdate];
}

@end

#endif
