/*
 * Copyright (C) 2008, 2011 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(WK_SCROLLBAR_PAINTER)

#include "ScrollbarOverlayUtilitiesMac.h"
#include "ScrollTypes.h"
#include <Cocoa/Cocoa.h>

// -----------------------------------------------------------------------------
// This file contains utilities to draw overlay scrollbars. There are no public
// APIs yet on the Mac to draw overlay scrollbars so we use private APIs that
// we look up at runtime. If the private APIs don't exist then the wkMake*
// functions will return nil.
//
// Note, this file contains functions copied from WebCoreSystemInterface.h.
// Using the same names makes the code easier to maintain.
// -----------------------------------------------------------------------------

// Public APIs not available on versions of Mac on which we build
#if (defined(BUILDING_ON_LEOPARD) || defined(BUILDING_ON_SNOW_LEOPARD))
@interface NSScroller (NSObject)
+ (NSScrollerStyle)preferredScrollerStyle;
@end
#endif

// These are private APIs to draw overlay scrollbars.
@interface NSScrollerImp : NSObject

+ (id)scrollerImpWithStyle:(NSScrollerStyle)style
               controlSize:(NSControlSize)size
                horizontal:(BOOL)horizontal
      replacingScrollerImp:(id)scroller;

@property CGFloat knobAlpha;
@property CGFloat trackAlpha;
@property CGFloat knobProportion;
@property(getter=isEnabled) BOOL enabled;
@property(getter=isHorizontal) BOOL horizontal;
@property double doubleValue;
@property(assign) id delegate;

- (CGFloat)knobMinLength;
- (CGFloat)trackBoxWidth;
- (CGFloat)trackWidth;
- (void)setBoundsSize:(NSSize)size;
- (void)drawKnobSlotInRect:(NSRect)rect
                 highlight:(BOOL)flag;
- (void)drawKnob;
- (void)setOverlayScrollerState:(NSScrollerStyle)state
               forceImmediately:(BOOL)flag;
- (void)setDelegate:(id)delegate;

@end

// These are private APIs to manage overlay scrollbars.
@interface NSScrollerImpPair : NSObject

@property NSScrollerStyle scrollerStyle;
@property(retain) NSScrollerImp *horizontalScrollerImp;
@property(retain) NSScrollerImp *verticalScrollerImp;
@property(assign) id delegate;

- (void)flashScrollers;
- (void)contentAreaScrolled;
- (void)contentAreaWillDraw;
- (void)contentAreaDidHide;
- (void)windowOrderedOut;
- (void)windowOrderedIn;
- (void)mouseEnteredContentArea;
- (void)mouseMovedInContentArea;
- (void)mouseExitedContentArea;
- (void)startLiveResize;
- (void)contentAreaDidResize;
- (void)endLiveResize;
- (void)beginScrollGesture;
- (void)endScrollGesture;

@end

static Class lookUpNSScrollerImpClass()
{
    static Class result = NSClassFromString(@"NSScrollerImp");
    return result;
}

static Class lookUpNSScrollerImpPairClass()
{
    static Class result = NSClassFromString(@"NSScrollerImpPair");
    return result;
}

static NSControlSize scrollbarControlSizeToNSControlSize(int controlSize)
{
    return controlSize == WebCore::RegularScrollbar ? NSRegularControlSize : NSSmallControlSize;
}

static NSScrollerStyle preferredScrollerStyle()
{
    if ([NSScroller respondsToSelector:@selector(preferredScrollerStyle)])
        return [NSScroller preferredScrollerStyle];
    return NSScrollerStyleLegacy;
}

bool wkScrollbarPainterUsesOverlayScrollers(void)
{
    return preferredScrollerStyle() == NSScrollerStyleOverlay;
}

bool wkScrollbarPainterIsHorizontal(WKScrollbarPainterRef painter)
{
    return [painter isHorizontal];
}

CGFloat wkScrollbarPainterKnobAlpha(WKScrollbarPainterRef painter)
{
    return [painter knobAlpha];
}

void wkScrollbarPainterSetOverlayState(WKScrollbarPainterRef painter, int overlayScrollerState)
{
    [painter setOverlayScrollerState:overlayScrollerState
                    forceImmediately:YES];
}

void wkScrollbarPainterPaint(WKScrollbarPainterRef painter, bool enabled, double value, CGFloat proportion, NSRect frameRect)
{
    [painter setEnabled:enabled];
    [painter setBoundsSize:frameRect.size];
    [painter setDoubleValue:value];
    [painter setKnobProportion:proportion];

    if ([painter isHorizontal])
        frameRect.size.height = [painter trackWidth];
    else
        frameRect.size.width = [painter trackWidth];

    [painter drawKnobSlotInRect:frameRect highlight:NO];
    [painter drawKnob];
}

int wkScrollbarMinimumThumbLength(WKScrollbarPainterRef painter)
{
    return [painter knobMinLength];
}

void wkScrollbarPainterSetDelegate(WKScrollbarPainterRef painter, id scrollbarPainterDelegate)
{
    [painter setDelegate:scrollbarPainterDelegate];
}

CGFloat wkScrollbarPainterTrackAlpha(WKScrollbarPainterRef painter)
{
    return [painter trackAlpha];
}

WKScrollbarPainterRef wkMakeScrollbarPainter(int controlSize, bool isHorizontal)
{
    return wkMakeScrollbarReplacementPainter(nil, preferredScrollerStyle(), controlSize, isHorizontal);
}

int wkScrollbarThickness(int controlSize)
{
    return [wkMakeScrollbarPainter(controlSize, false) trackBoxWidth];
}

int wkScrollbarMinimumTotalLengthNeededForThumb(WKScrollbarPainterRef painter)
{
    // TODO(sail): This doesn't match the implementation in WebKitSystemInterface.
    return wkScrollbarMinimumThumbLength(painter);
}

WKScrollbarPainterRef wkVerticalScrollbarPainterForController(WKScrollbarPainterControllerRef controller)
{
    return [controller verticalScrollerImp];
}

WKScrollbarPainterRef wkHorizontalScrollbarPainterForController(WKScrollbarPainterControllerRef controller)
{
    return [controller horizontalScrollerImp];
}

WKScrollbarPainterRef wkMakeScrollbarReplacementPainter(WKScrollbarPainterRef oldPainter, int newStyle, int controlSize, bool isHorizontal)
{
    if (!isScrollbarOverlayAPIAvailable())
        return nil;
    return [lookUpNSScrollerImpClass() scrollerImpWithStyle:newStyle
                                                controlSize:scrollbarControlSizeToNSControlSize(controlSize)
                                                 horizontal:isHorizontal
                                       replacingScrollerImp:oldPainter];
}

void wkSetPainterForPainterController(WKScrollbarPainterControllerRef controller, WKScrollbarPainterRef painter, bool isHorizontal)
{
    if (isHorizontal)
        [controller setHorizontalScrollerImp:painter];
    else
        [controller setVerticalScrollerImp:painter];
}

void wkSetScrollbarPainterControllerStyle(WKScrollbarPainterControllerRef painter, int newStyle)
{
    [painter setScrollerStyle:newStyle];
}

CGRect wkScrollbarPainterKnobRect(WKScrollbarPainterRef painter)
{
    return NSRectToCGRect(NSZeroRect);
}

void wkSetScrollbarPainterKnobAlpha(WKScrollbarPainterRef painter, CGFloat alpha)
{
    [painter setKnobAlpha:alpha];
}

void wkSetScrollbarPainterTrackAlpha(WKScrollbarPainterRef painter, CGFloat alpha)
{
    [painter setTrackAlpha:alpha];
}

void wkSetScrollbarPainterKnobStyle(WKScrollbarPainterRef painter, wkScrollerKnobStyle style)
{
    // TODO(sail): A knob style API doesn't exist in the seeds currently available. 
}

WKScrollbarPainterControllerRef wkMakeScrollbarPainterController(id painterControllerDelegate)
{
    if (!isScrollbarOverlayAPIAvailable())
        return nil;
    NSScrollerImpPair* controller = [[[lookUpNSScrollerImpPairClass() alloc] init] autorelease];
    [controller setDelegate:painterControllerDelegate];
    [controller setScrollerStyle:preferredScrollerStyle()];
    return controller;
}

void wkContentAreaScrolled(WKScrollbarPainterControllerRef controller)
{
    [controller contentAreaScrolled];
}

void wkContentAreaWillPaint(WKScrollbarPainterControllerRef controller)
{
    [controller contentAreaWillDraw];
}

void wkMouseEnteredContentArea(WKScrollbarPainterControllerRef controller)
{
    [controller mouseEnteredContentArea];
}

void wkMouseExitedContentArea(WKScrollbarPainterControllerRef controller)
{
    [controller mouseExitedContentArea];
}

void wkMouseMovedInContentArea(WKScrollbarPainterControllerRef controller)
{
    [controller mouseMovedInContentArea];
}

void wkWillStartLiveResize(WKScrollbarPainterControllerRef controller)
{
    [controller startLiveResize];
}

void wkContentAreaResized(WKScrollbarPainterControllerRef controller)
{
    [controller contentAreaDidResize];
}

void wkWillEndLiveResize(WKScrollbarPainterControllerRef controller)
{
    [controller endLiveResize];
}

void wkContentAreaDidShow(WKScrollbarPainterControllerRef controller)
{
    [controller windowOrderedIn];
}

void wkContentAreaDidHide(WKScrollbarPainterControllerRef controller)
{
    [controller windowOrderedOut];
}

void wkDidBeginScrollGesture(WKScrollbarPainterControllerRef controller)
{
    [controller beginScrollGesture];
}

void wkDidEndScrollGesture(WKScrollbarPainterControllerRef controller)
{
    [controller endScrollGesture];
}

void wkScrollbarPainterForceFlashScrollers(WKScrollbarPainterControllerRef controller)
{
    [controller flashScrollers];
}

bool isScrollbarOverlayAPIAvailable()
{
    // TODO(sail): Disable overlay scrollbars for now until the following issues are fixed:
    // #1: Invalidation issues causes the scrollbar to leave trailing artifacts.
    // #2: Various messages such as live resize started/ended need to be piped from the UI.
    // #3: Find tick marks need to be drawn on the scrollbar track.
    // #4: Need to have the theme engine draw the thumb.
    return false;

    static bool apiAvailable = [lookUpNSScrollerImpClass() respondsToSelector:@selector(scrollerImpWithStyle:controlSize:horizontal:replacingScrollerImp:)] &&
                               [lookUpNSScrollerImpPairClass() instancesRespondToSelector:@selector(scrollerStyle)];
    return apiAvailable;
}

#endif // USE(WK_SCROLLBAR_PAINTER)
