/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebThemeEngineDRTMac.h"

#include <public/WebCanvas.h>
#include <public/WebRect.h>
#include "skia/ext/skia_utils_mac.h"
#import <AppKit/NSAffineTransform.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSScroller.h>
#import <AppKit/NSWindow.h>
#include <Carbon/Carbon.h>

using WebKit::WebCanvas;
using WebKit::WebRect;
using WebKit::WebThemeEngine;

// We can't directly tell the NSScroller to draw itself as active or inactive,
// instead we have to make it a child of an (in)active window. This class lets
// us fake that parent window.
@interface FakeActiveWindow : NSWindow {
@private
    BOOL hasActiveControls;
}
+ (NSWindow*)alwaysActiveWindow;
+ (NSWindow*)alwaysInactiveWindow;
- (id)initWithActiveControls:(BOOL)_hasActiveControls;
- (BOOL)_hasActiveControls;
@end

@implementation FakeActiveWindow

static NSWindow* alwaysActiveWindow = nil;
static NSWindow* alwaysInactiveWindow = nil;

+ (NSWindow*)alwaysActiveWindow
{
    if (alwaysActiveWindow == nil)
        alwaysActiveWindow = [[self alloc] initWithActiveControls:YES];
    return alwaysActiveWindow;
}

+ (NSWindow*)alwaysInactiveWindow
{
    if (alwaysInactiveWindow == nil)
        alwaysInactiveWindow = [[self alloc] initWithActiveControls:NO];
    return alwaysInactiveWindow;
}

- (id)initWithActiveControls:(BOOL)_hasActiveControls
{
    self = [super init];
    hasActiveControls = _hasActiveControls;
    return self;
}

- (BOOL)_hasActiveControls
{
    return hasActiveControls;
}

@end

void WebThemeEngineDRTMac::paintScrollbarThumb(
    WebCanvas* canvas,
    WebThemeEngine::State state,
    WebThemeEngine::Size size,
    const WebRect& rect,
    const WebThemeEngine::ScrollbarInfo& scrollbarInfo)
{
    // To match the Mac port, we still use HITheme for inner scrollbars.
    if (scrollbarInfo.parent == WebThemeEngine::ScrollbarParentRenderLayer)
        paintHIThemeScrollbarThumb(canvas, state, size, rect, scrollbarInfo);
    else
        paintNSScrollerScrollbarThumb(canvas, state, size, rect, scrollbarInfo);
}

static ThemeTrackEnableState stateToHIEnableState(WebThemeEngine::State state)
{
    switch (state) {
    case WebThemeEngine::StateDisabled:
        return kThemeTrackDisabled;
    case WebThemeEngine::StateInactive:
        return kThemeTrackInactive;
    default:
        return kThemeTrackActive;
    }
}

// Duplicated from webkit/glue/webthemeengine_impl_mac.cc in the downstream
// Chromium WebThemeEngine implementation.
void WebThemeEngineDRTMac::paintHIThemeScrollbarThumb(
    WebCanvas* canvas,
    WebThemeEngine::State state,
    WebThemeEngine::Size size,
    const WebRect& rect,
    const WebThemeEngine::ScrollbarInfo& scrollbarInfo)
{
    HIThemeTrackDrawInfo trackInfo;
    trackInfo.version = 0;
    trackInfo.kind = size == WebThemeEngine::SizeRegular ? kThemeMediumScrollBar : kThemeSmallScrollBar;
    trackInfo.bounds = CGRectMake(rect.x, rect.y, rect.width, rect.height);
    trackInfo.min = 0;
    trackInfo.max = scrollbarInfo.maxValue;
    trackInfo.value = scrollbarInfo.currentValue;
    trackInfo.trackInfo.scrollbar.viewsize = scrollbarInfo.visibleSize;
    trackInfo.attributes = 0;
    if (scrollbarInfo.orientation == WebThemeEngine::ScrollbarOrientationHorizontal)
        trackInfo.attributes |= kThemeTrackHorizontal;

    trackInfo.enableState = stateToHIEnableState(state);

    trackInfo.trackInfo.scrollbar.pressState =
        state == WebThemeEngine::StatePressed ? kThemeThumbPressed : 0;
    trackInfo.attributes |= (kThemeTrackShowThumb | kThemeTrackHideTrack);
    gfx::SkiaBitLocker bitLocker(canvas);
    CGContextRef cgContext = bitLocker.cgContext();
    HIThemeDrawTrack(&trackInfo, 0, cgContext, kHIThemeOrientationNormal);
}

void WebThemeEngineDRTMac::paintNSScrollerScrollbarThumb(
    WebCanvas* canvas,
    WebThemeEngine::State state,
    WebThemeEngine::Size size,
    const WebRect& rect,
    const WebThemeEngine::ScrollbarInfo& scrollbarInfo)
{
    [NSGraphicsContext saveGraphicsState];
    NSScroller* scroller = [[NSScroller alloc] initWithFrame:NSMakeRect(rect.x, rect.y, rect.width, rect.height)];
    [scroller setEnabled:state != WebThemeEngine::StateDisabled];
    if (state == WebThemeEngine::StateInactive)
        [[[FakeActiveWindow alwaysInactiveWindow] contentView] addSubview:scroller];
    else
        [[[FakeActiveWindow alwaysActiveWindow] contentView] addSubview:scroller];

    [scroller setControlSize:size == WebThemeEngine::SizeRegular ? NSRegularControlSize : NSSmallControlSize];

    double value = double(scrollbarInfo.currentValue) / double(scrollbarInfo.maxValue);
    [scroller setDoubleValue: value];

    float knobProportion = float(scrollbarInfo.visibleSize) / float(scrollbarInfo.totalSize);
    [scroller setKnobProportion: knobProportion];

    gfx::SkiaBitLocker bitLocker(canvas);
    CGContextRef cgContext = bitLocker.cgContext();
    NSGraphicsContext* nsGraphicsContext = [NSGraphicsContext graphicsContextWithGraphicsPort:cgContext flipped:YES];
    [NSGraphicsContext setCurrentContext:nsGraphicsContext];

    // Despite passing in frameRect() to the scroller, it always draws at (0, 0).
    // Force it to draw in the right location by translating the whole graphics
    // context.
    CGContextSaveGState(cgContext);
    NSAffineTransform *transform = [NSAffineTransform transform];
    [transform translateXBy:rect.x yBy:rect.y];
    [transform concat];

    [scroller drawKnob];
    CGContextRestoreGState(cgContext);

    [scroller release];

    [NSGraphicsContext restoreGraphicsState];
}
