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

#ifndef ScrollbarOverlayUtilitiesChromiumMac_h
#define ScrollbarOverlayUtilitiesChromiumMac_h

#if USE(WK_SCROLLBAR_PAINTER)

// Public APIs not available on versions of Mac on which we build
#if (defined(BUILDING_ON_LEOPARD) || defined(BUILDING_ON_SNOW_LEOPARD))
enum {
    NSScrollerStyleLegacy       = 0,
    NSScrollerStyleOverlay      = 1
};

#ifdef __OBJC__
typedef NSInteger NSScrollerStyle;
#endif
#endif

typedef uint32 wkScrollerStyle;

#ifdef __OBJC__
@class NSScrollerImp;
@class NSScrollerImpPair;
#else
class NSScrollerImp;
class NSScrollerImpPair;
#endif

typedef NSScrollerImp* WKScrollbarPainterRef;
typedef NSScrollerImpPair* WKScrollbarPainterControllerRef;

bool isScrollbarOverlayAPIAvailable();

// Scrollbar Painter
bool wkScrollbarPainterUsesOverlayScrollers(void);
bool wkScrollbarPainterIsHorizontal(WKScrollbarPainterRef);
CGFloat wkScrollbarPainterKnobAlpha(WKScrollbarPainterRef);
void wkScrollbarPainterSetOverlayState(WKScrollbarPainterRef, int overlayScrollerState);
void wkScrollbarPainterPaint(WKScrollbarPainterRef, bool enabled, double value, CGFloat proportion, NSRect frameRect);
void wkScrollbarPainterPaintTrack(WKScrollbarPainterRef, bool enabled, double value, CGFloat proportion, NSRect frameRect);
void wkScrollbarPainterPaintKnob(WKScrollbarPainterRef);
int wkScrollbarMinimumThumbLength(WKScrollbarPainterRef);
void wkScrollbarPainterSetDelegate(WKScrollbarPainterRef, id scrollbarPainterDelegate);
void wkScrollbarPainterSetEnabled(WKScrollbarPainterRef, bool enabled);
CGFloat wkScrollbarPainterTrackAlpha(WKScrollbarPainterRef);
WKScrollbarPainterRef wkMakeScrollbarPainter(int controlSize, bool isHorizontal);
int wkScrollbarThickness(int controlSize);
int wkScrollbarMinimumTotalLengthNeededForThumb(WKScrollbarPainterRef);
CGRect wkScrollbarPainterKnobRect(WKScrollbarPainterRef);
WKScrollbarPainterRef wkMakeScrollbarReplacementPainter(WKScrollbarPainterRef oldPainter, int newStyle, int controlSize, bool isHorizontal);
void wkSetScrollbarPainterKnobAlpha(WKScrollbarPainterRef, CGFloat);
void wkSetScrollbarPainterTrackAlpha(WKScrollbarPainterRef, CGFloat);

enum {
    wkScrollerKnobStyleDefault = 0,
    wkScrollerKnobStyleDark = 1,
    wkScrollerKnobStyleLight = 2
};
typedef uint32 wkScrollerKnobStyle;
extern void wkSetScrollbarPainterKnobStyle(WKScrollbarPainterRef, wkScrollerKnobStyle);

// Scrollbar Painter Controller
WKScrollbarPainterControllerRef wkMakeScrollbarPainterController(id painterControllerDelegate);
void wkContentAreaScrolled(WKScrollbarPainterControllerRef);
void wkContentAreaWillPaint(WKScrollbarPainterControllerRef);
void wkMouseEnteredContentArea(WKScrollbarPainterControllerRef);
void wkMouseExitedContentArea(WKScrollbarPainterControllerRef);
void wkMouseMovedInContentArea(WKScrollbarPainterControllerRef);
void wkWillStartLiveResize(WKScrollbarPainterControllerRef);
void wkContentAreaResized(WKScrollbarPainterControllerRef);
void wkWillEndLiveResize(WKScrollbarPainterControllerRef);
void wkContentAreaDidShow(WKScrollbarPainterControllerRef);
void wkContentAreaDidHide(WKScrollbarPainterControllerRef);
void wkDidBeginScrollGesture(WKScrollbarPainterControllerRef);
void wkDidEndScrollGesture(WKScrollbarPainterControllerRef);
void wkScrollbarPainterForceFlashScrollers(WKScrollbarPainterControllerRef);
void wkSetScrollbarPainterControllerStyle(WKScrollbarPainterControllerRef, wkScrollerStyle newStyle);
void wkSetPainterForPainterController(WKScrollbarPainterControllerRef, WKScrollbarPainterRef, bool isHorizontal);
WKScrollbarPainterRef wkVerticalScrollbarPainterForController(WKScrollbarPainterControllerRef);
WKScrollbarPainterRef wkHorizontalScrollbarPainterForController(WKScrollbarPainterControllerRef);
wkScrollerStyle wkScrollbarPainterControllerStyle(WKScrollbarPainterControllerRef);

#endif // USE(WK_SCROLLBAR_PAINTER)

#endif // ScrollbarOverlayUtilitiesChromiumMac_h
