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

#import <Cocoa/Cocoa.h>
#import <WebCore/HTMLMediaElement.h>

@protocol WebVideoFullscreenHUDWindowControllerDelegate;

@interface WebVideoFullscreenHUDWindowController : NSWindowController {
@private
    id<WebVideoFullscreenHUDWindowControllerDelegate> _delegate;
    NSTimer *_timelineUpdateTimer;
#if !defined(BUILDING_ON_TIGER)
    NSTrackingArea *_area;
#endif
    BOOL _mouseIsInHUD;
    BOOL _isEndingFullscreen;

    NSControl *_timeline;
    NSTextField *_remainingTimeText;
    NSTextField *_elapsedTimeText;
    NSControl *_volumeSlider;
    NSControl *_playButton;
}
- (id<WebVideoFullscreenHUDWindowControllerDelegate>)delegate;
- (void)setDelegate:(id<WebVideoFullscreenHUDWindowControllerDelegate>)delegate;
- (void)fadeWindowIn;
- (void)fadeWindowOut;
- (void)closeWindow;
- (void)updateRate;

@end

@protocol WebVideoFullscreenHUDWindowControllerDelegate <NSObject>
- (void)requestExitFullscreen;
- (WebCore::HTMLMediaElement*)mediaElement;
@end

#endif
