/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <mach/mach_time.h>
#import "WebNodeHighlightView.h"
#import "WebNodeHighlight.h"

@implementation WebNodeHighlight
- (id)initWithBounds:(NSRect)bounds andRects:(NSArray *)rects forView:(NSView *)view
{
    if (![self init])
        return nil;

    _startTime = 0.0;
    _duration = 3.0;

    if ([[[NSApplication sharedApplication] currentEvent] modifierFlags] & NSShiftKeyMask)
        _duration = 6.0;

    if (!rects)
        rects = [NSArray arrayWithObject:[NSValue valueWithRect:bounds]];

    _webNodeHighlightView = [[WebNodeHighlightView alloc] initWithHighlight:self andRects:rects forView:view];
    if (!_webNodeHighlightView) {
        [self release];
        return nil;
    }

    // adjust size and position for rect padding that the view adds
    bounds.origin.y -= 3.0;
    bounds.origin.x -= 3.0;
    bounds.size = [_webNodeHighlightView frame].size;

    NSRect windowBounds = [view convertRect:bounds toView:nil];
    windowBounds.origin = [[view window] convertBaseToScreen:windowBounds.origin]; // adjust for screen coords

    _webNodeHighlightWindow = [[NSWindow alloc] initWithContentRect:windowBounds styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
    [_webNodeHighlightWindow setBackgroundColor:[NSColor clearColor]];
    [_webNodeHighlightWindow setOpaque:NO];
    [_webNodeHighlightWindow setHasShadow:NO];
    [_webNodeHighlightWindow setIgnoresMouseEvents:YES];
    [_webNodeHighlightWindow setReleasedWhenClosed:YES];
    [_webNodeHighlightWindow setLevel:[[view window] level] + 1];
    [_webNodeHighlightWindow setContentView:_webNodeHighlightView];
    [_webNodeHighlightView release];

    [_webNodeHighlightWindow orderFront:self];

    // 30 frames per second time interval will play well with the CPU and still look smooth
    _timer = [[NSTimer scheduledTimerWithTimeInterval:(1.0 / 30.0) target:self selector:@selector(redraw:) userInfo:nil repeats:YES] retain];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(expire) name:NSViewBoundsDidChangeNotification object:view];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(expire) name:NSViewBoundsDidChangeNotification object:[view superview]];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(expire) name:NSWindowWillMoveNotification object:[view window]];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(expire) name:NSWindowWillCloseNotification object:[view window]];

    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSViewBoundsDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowWillMoveNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowWillCloseNotification object:nil];

    [_timer invalidate];
    [_timer release];

    [super dealloc];
}

- (double)fractionComplete
{
    if (_startTime == 0.0)
        _startTime = CFAbsoluteTimeGetCurrent();

    return ((CFAbsoluteTimeGetCurrent() - _startTime) / _duration);
}

- (void)expire
{
    [_timer invalidate];
    [_timer release];
    _timer = nil;

    [_webNodeHighlightWindow close];
    _webNodeHighlightWindow = nil;

    [[NSNotificationCenter defaultCenter] postNotificationName:@"WebNodeHighlightExpired" object:self userInfo:nil];
}

- (void)redraw:(NSTimer *)timer
{
    [_webNodeHighlightView setNeedsDisplay:YES];    

    if (_startTime == 0.0)
        _startTime = CFAbsoluteTimeGetCurrent();

    if ((CFAbsoluteTimeGetCurrent() - _startTime) > _duration)
        [self expire];
}
@end
