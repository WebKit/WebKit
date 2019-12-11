/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#pragma once

#if USE(APPKIT)

#if USE(APPLE_INTERNAL_SDK)

#import <AppKit/NSScrollerImpPair_Private.h>
#import <AppKit/NSScrollerImp_Private.h>

@interface NSScrollerImp ()
@property (getter=isTracking) BOOL tracking;
@end

@interface NSScrollerImpPair ()
+ (NSUserInterfaceLayoutDirection)scrollerLayoutDirection;
+ (void)_updateAllScrollerImpPairsForNewRecommendedScrollerStyle:(NSScrollerStyle)newRecommendedScrollerStyle;
@end

#else

enum {
    NSOverlayScrollerStateHidden = 0,
    NSOverlayScrollerStateThumbShown = 1,
    NSOverlayScrollerStateAllShown = 2,
    NSOverlayScrollerStatePulseThumb = 3,
};
typedef NSUInteger NSOverlayScrollerState;

@protocol NSScrollerImpDelegate;

@interface NSScrollerImp : NSObject
+ (NSScrollerImp *)scrollerImpWithStyle:(NSScrollerStyle)newScrollerStyle controlSize:(NSControlSize)newControlSize horizontal:(BOOL)horizontal replacingScrollerImp:(id)previous;
@property (retain) CALayer *layer;
- (void)setNeedsDisplay:(BOOL)flag;
@property NSScrollerKnobStyle knobStyle;
@property (getter=isHorizontal) BOOL horizontal;
@property NSSize boundsSize;
@property (getter=isEnabled) BOOL enabled;
@property double doubleValue;
@property double presentationValue;
@property (getter=shouldUsePresentationValue) BOOL usePresentationValue;
@property CGFloat knobProportion;
@property CGFloat uiStateTransitionProgress;
@property CGFloat expansionTransitionProgress;
@property CGFloat trackAlpha;
@property CGFloat knobAlpha;
@property (getter=isExpanded) BOOL expanded;
@property (assign) id<NSScrollerImpDelegate> delegate;
@property (readonly) CGFloat trackBoxWidth;
@property (readonly) CGFloat trackWidth;
@property (readonly) CGFloat trackSideInset;
@property (readonly) CGFloat trackEndInset;
@property (readonly) CGFloat knobEndInset;
@property (readonly) CGFloat knobMinLength;
@property (readonly) CGFloat knobOverlapEndInset;
@property (readonly) CGFloat trackOverlapEndInset;
@property NSUserInterfaceLayoutDirection userInterfaceLayoutDirection;
- (NSRect)rectForPart:(NSScrollerPart)partCode;
- (void)drawKnobSlotInRect:(NSRect)slotRect highlight:(BOOL)flag alpha:(CGFloat)alpha;
- (void)drawKnobSlotInRect:(NSRect)slotRect highlight:(BOOL)flag;
- (void)drawKnob;
- (void)mouseEnteredScroller;
- (void)mouseExitedScroller;
@end

@interface NSScrollerImp ()
@property (getter=isTracking) BOOL tracking;
@end

@protocol NSScrollerImpDelegate
@required
- (NSRect)convertRectToBacking:(NSRect)aRect;
- (NSRect)convertRectFromBacking:(NSRect)aRect;
- (CALayer *)layer;
- (void)scrollerImp:(NSScrollerImp *)scrollerImp animateKnobAlphaTo:(CGFloat)newKnobAlpha duration:(NSTimeInterval)duration;
- (void)scrollerImp:(NSScrollerImp *)scrollerImp animateTrackAlphaTo:(CGFloat)newTrackAlpha duration:(NSTimeInterval)duration;
- (void)scrollerImp:(NSScrollerImp *)scrollerImp overlayScrollerStateChangedTo:(NSOverlayScrollerState)newOverlayScrollerState;
@optional
- (void)scrollerImp:(NSScrollerImp *)scrollerImp animateUIStateTransitionWithDuration:(NSTimeInterval)duration;
- (void)scrollerImp:(NSScrollerImp *)scrollerImp animateExpansionTransitionWithDuration:(NSTimeInterval)duration;
- (NSPoint)mouseLocationInScrollerForScrollerImp:(NSScrollerImp *)scrollerImp;
- (NSRect)convertRectToLayer:(NSRect)aRect;
- (BOOL)shouldUseLayerPerPartForScrollerImp:(NSScrollerImp *)scrollerImp;
- (NSAppearance *)effectiveAppearanceForScrollerImp:(NSScrollerImp *)scrollerImp;
@end

@protocol NSScrollerImpPairDelegate;

@interface NSScrollerImpPair : NSObject
@property (assign) id<NSScrollerImpPairDelegate> delegate;
@property (retain) NSScrollerImp *verticalScrollerImp;
@property (retain) NSScrollerImp *horizontalScrollerImp;
@property NSScrollerStyle scrollerStyle;
+ (NSUserInterfaceLayoutDirection)scrollerLayoutDirection;
- (void)flashScrollers;
- (void)hideOverlayScrollers;
- (void)lockOverlayScrollerState:(NSOverlayScrollerState)state;
- (void)unlockOverlayScrollerState;
- (BOOL)overlayScrollerStateIsLocked;
- (void)contentAreaScrolled;
- (void)contentAreaScrolledInDirection:(NSPoint)direction;
- (void)contentAreaWillDraw;
- (void)windowOrderedOut;
- (void)windowOrderedIn;
- (void)mouseEnteredContentArea;
- (void)mouseExitedContentArea;
- (void)mouseMovedInContentArea;
- (void)startLiveResize;
- (void)contentAreaDidResize;
- (void)endLiveResize;
- (void)beginScrollGesture;
- (void)endScrollGesture;
+ (void)_updateAllScrollerImpPairsForNewRecommendedScrollerStyle:(NSScrollerStyle)newRecommendedScrollerStyle;
@end

@protocol NSScrollerImpPairDelegate
@required
- (NSRect)contentAreaRectForScrollerImpPair:(NSScrollerImpPair *)scrollerImpPair;
- (BOOL)inLiveResizeForScrollerImpPair:(NSScrollerImpPair *)scrollerImpPair;
- (NSPoint)mouseLocationInContentAreaForScrollerImpPair:(NSScrollerImpPair *)scrollerImpPair;
- (NSPoint)scrollerImpPair:(NSScrollerImpPair *)scrollerImpPair convertContentPoint:(NSPoint)pointInContentArea toScrollerImp:(NSScrollerImp *)scrollerImp;
- (void)scrollerImpPair:(NSScrollerImpPair *)scrollerImpPair setContentAreaNeedsDisplayInRect:(NSRect)rect;
- (void)scrollerImpPair:(NSScrollerImpPair *)scrollerImpPair updateScrollerStyleForNewRecommendedScrollerStyle:(NSScrollerStyle)newRecommendedScrollerStyle;
@optional
- (BOOL)scrollerImpPair:(NSScrollerImpPair *)scrollerImpPair isContentPointVisible:(NSPoint)pointInContentArea;
@end

#endif

WTF_EXTERN_C_BEGIN

NSScrollerStyle _NSRecommendedScrollerStyle();

WTF_EXTERN_C_END

#endif // USE(APPKIT)
