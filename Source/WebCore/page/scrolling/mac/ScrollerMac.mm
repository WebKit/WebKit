/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ScrollerMac.h"

#if PLATFORM(MAC)

#import "FloatPoint.h"
#import "IntRect.h"
#import "NSScrollerImpDetails.h"
#import "PlatformWheelEvent.h"
#import "ScrollTypesMac.h"
#import "ScrollerPairMac.h"
#import "ScrollingTreeScrollingNode.h"
#import <QuartzCore/CALayer.h>
#import <pal/spi/mac/NSScrollerImpSPI.h>
#import <wtf/BlockObjCExceptions.h>

enum class FeatureToAnimate {
    KnobAlpha,
    TrackAlpha,
    UIStateTransition,
    ExpansionTransition
};

@interface WebScrollbarPartAnimationMac : NSAnimation {
    WebCore::ScrollerMac* _scroller;
    FeatureToAnimate _featureToAnimate;
    CGFloat _startValue;
    CGFloat _endValue;
}
- (id)initWithScroller:(WebCore::ScrollerMac*)scroller featureToAnimate:(FeatureToAnimate)featureToAnimate animateFrom:(CGFloat)startValue animateTo:(CGFloat)endValue duration:(NSTimeInterval)duration;
@end

@implementation WebScrollbarPartAnimationMac

- (id)initWithScroller:(WebCore::ScrollerMac*)scroller featureToAnimate:(FeatureToAnimate)featureToAnimate animateFrom:(CGFloat)startValue animateTo:(CGFloat)endValue duration:(NSTimeInterval)duration
{
    self = [super initWithDuration:duration animationCurve:NSAnimationEaseInOut];
    if (!self)
        return nil;

    _scroller = scroller;
    _featureToAnimate = featureToAnimate;
    _startValue = startValue;
    _endValue = endValue;

    [self setAnimationBlockingMode:NSAnimationNonblocking];

    return self;
}

- (void)startAnimation
{
    ASSERT(_scroller);

    [super startAnimation];
}

- (void)setStartValue:(CGFloat)startValue
{
    _startValue = startValue;
}

- (void)setEndValue:(CGFloat)endValue
{
    _endValue = endValue;
}

- (void)setCurrentProgress:(NSAnimationProgress)progress
{
    [super setCurrentProgress:progress];

    CGFloat currentValue;
    if (_startValue > _endValue)
        currentValue = 1 - progress;
    else
        currentValue = progress;

    switch (_featureToAnimate) {
    case FeatureToAnimate::KnobAlpha:
        [_scroller->scrollerImp() setKnobAlpha:currentValue];
        break;
    case FeatureToAnimate::TrackAlpha:
        [_scroller->scrollerImp() setTrackAlpha:currentValue];
        break;
    case FeatureToAnimate::UIStateTransition:
        [_scroller->scrollerImp() setUiStateTransitionProgress:currentValue];
        break;
    case FeatureToAnimate::ExpansionTransition:
        [_scroller->scrollerImp() setExpansionTransitionProgress:currentValue];
        break;
    }
}

- (void)invalidate
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [self stopAnimation];
    END_BLOCK_OBJC_EXCEPTIONS
    _scroller = nullptr;
}

@end

@interface WebScrollerImpDelegateMac : NSObject<NSAnimationDelegate, NSScrollerImpDelegate> {
    WebCore::ScrollerMac* _scroller;

    RetainPtr<WebScrollbarPartAnimationMac> _knobAlphaAnimation;
    RetainPtr<WebScrollbarPartAnimationMac> _trackAlphaAnimation;
    RetainPtr<WebScrollbarPartAnimationMac> _uiStateTransitionAnimation;
    RetainPtr<WebScrollbarPartAnimationMac> _expansionTransitionAnimation;
}
- (id)initWithScroller:(WebCore::ScrollerMac*)scroller;
- (void)cancelAnimations;
@end

@implementation WebScrollerImpDelegateMac

- (id)initWithScroller:(WebCore::ScrollerMac*)scroller
{
    self = [super init];
    if (!self)
        return nil;

    _scroller = scroller;
    return self;
}

- (void)cancelAnimations
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [_knobAlphaAnimation stopAnimation];
    [_trackAlphaAnimation stopAnimation];
    [_uiStateTransitionAnimation stopAnimation];
    [_expansionTransitionAnimation stopAnimation];
    END_BLOCK_OBJC_EXCEPTIONS
}

- (NSRect)convertRectToBacking:(NSRect)aRect
{
    return aRect;
}

- (NSRect)convertRectFromBacking:(NSRect)aRect
{
    return aRect;
}

- (CALayer *)layer
{
    return nil;
}

- (NSPoint)mouseLocationInScrollerForScrollerImp:(NSScrollerImp *)scrollerImp
{
    if (!_scroller)
        return NSZeroPoint;

    ASSERT_UNUSED(scrollerImp, scrollerImp == _scroller->scrollerImp());

    return _scroller->lastKnownMousePositionInScrollbar();
}

- (NSRect)convertRectToLayer:(NSRect)rect
{
    return rect;
}

- (BOOL)shouldUseLayerPerPartForScrollerImp:(NSScrollerImp *)scrollerImp
{
    UNUSED_PARAM(scrollerImp);

    return true;
}

- (NSAppearance *)effectiveAppearanceForScrollerImp:(NSScrollerImp *)scrollerImp
{
    UNUSED_PARAM(scrollerImp);

    if (!_scroller)
        return [NSAppearance currentDrawingAppearance];
    // The base system does not support dark Aqua, so we might get a null result.
    if (auto *appearance = [NSAppearance appearanceNamed:_scroller->pair().useDarkAppearance() ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua])
        return appearance;
    return [NSAppearance currentDrawingAppearance];
}

- (void)setUpAlphaAnimation:(RetainPtr<WebScrollbarPartAnimationMac>&)scrollbarPartAnimation featureToAnimate:(FeatureToAnimate)featureToAnimate animateAlphaTo:(CGFloat)newAlpha duration:(NSTimeInterval)duration
{
    // If we are currently animating,  stop
    if (scrollbarPartAnimation) {
        [scrollbarPartAnimation stopAnimation];
        scrollbarPartAnimation = nil;
    }

    scrollbarPartAnimation = adoptNS([[WebScrollbarPartAnimationMac alloc] initWithScroller:_scroller
        featureToAnimate:featureToAnimate
        animateFrom:featureToAnimate == FeatureToAnimate::KnobAlpha ? [_scroller->scrollerImp() knobAlpha] : [_scroller->scrollerImp() trackAlpha]
        animateTo:newAlpha
        duration:duration]);
    [scrollbarPartAnimation startAnimation];
}

- (void)scrollerImp:(NSScrollerImp *)scrollerImp animateKnobAlphaTo:(CGFloat)newKnobAlpha duration:(NSTimeInterval)duration
{
    if (!_scroller)
        return;

    ASSERT_UNUSED(scrollerImp, scrollerImp == _scroller->scrollerImp());
    _scroller->visibilityChanged(newKnobAlpha > 0);
    [self setUpAlphaAnimation:_knobAlphaAnimation featureToAnimate:FeatureToAnimate::KnobAlpha animateAlphaTo:newKnobAlpha duration:duration];
}

- (void)scrollerImp:(NSScrollerImp *)scrollerImp animateTrackAlphaTo:(CGFloat)newTrackAlpha duration:(NSTimeInterval)duration
{
    if (!_scroller)
        return;

    ASSERT_UNUSED(scrollerImp, scrollerImp == _scroller->scrollerImp());
    [self setUpAlphaAnimation:_trackAlphaAnimation featureToAnimate:FeatureToAnimate::TrackAlpha animateAlphaTo:newTrackAlpha duration:duration];
}

- (void)scrollerImp:(NSScrollerImp *)scrollerImp animateUIStateTransitionWithDuration:(NSTimeInterval)duration
{
    if (!_scroller)
        return;

    ASSERT(scrollerImp == _scroller->scrollerImp());

    // UIStateTransition always animates to 1. In case an animation is in progress this avoids a hard transition.
    [scrollerImp setUiStateTransitionProgress:1 - [scrollerImp uiStateTransitionProgress]];

    if (!_uiStateTransitionAnimation) {
        _uiStateTransitionAnimation = adoptNS([[WebScrollbarPartAnimationMac alloc] initWithScroller:_scroller
            featureToAnimate:FeatureToAnimate::UIStateTransition
            animateFrom:[scrollerImp uiStateTransitionProgress]
            animateTo:1.0
            duration:duration]);
    } else {
        // If we don't need to initialize the animation, just reset the values in case they have changed.
        [_uiStateTransitionAnimation setStartValue:[scrollerImp uiStateTransitionProgress]];
        [_uiStateTransitionAnimation setEndValue:1.0];
        [_uiStateTransitionAnimation setDuration:duration];
    }
    [_uiStateTransitionAnimation startAnimation];
}

- (void)scrollerImp:(NSScrollerImp *)scrollerImp animateExpansionTransitionWithDuration:(NSTimeInterval)duration
{
    if (!_scroller)
        return;

    ASSERT(scrollerImp == _scroller->scrollerImp());

    // ExpansionTransition always animates to 1. In case an animation is in progress this avoids a hard transition.
    [scrollerImp setExpansionTransitionProgress:1 - [scrollerImp expansionTransitionProgress]];

    if (!_expansionTransitionAnimation) {
        _expansionTransitionAnimation = adoptNS([[WebScrollbarPartAnimationMac alloc] initWithScroller:_scroller
            featureToAnimate:FeatureToAnimate::ExpansionTransition
            animateFrom:[scrollerImp expansionTransitionProgress]
            animateTo:1.0
            duration:duration]);
    } else {
        // If we don't need to initialize the animation, just reset the values in case they have changed.
        [_expansionTransitionAnimation setStartValue:[scrollerImp uiStateTransitionProgress]];
        [_expansionTransitionAnimation setEndValue:1.0];
        [_expansionTransitionAnimation setDuration:duration];
    }
    [_expansionTransitionAnimation startAnimation];
}

- (void)scrollerImp:(NSScrollerImp *)scrollerImp overlayScrollerStateChangedTo:(NSOverlayScrollerState)newOverlayScrollerState
{
    UNUSED_PARAM(scrollerImp);
    UNUSED_PARAM(newOverlayScrollerState);
}

- (void)invalidate
{
    _scroller = nil;
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [_knobAlphaAnimation invalidate];
    [_trackAlphaAnimation invalidate];
    [_uiStateTransitionAnimation invalidate];
    [_expansionTransitionAnimation invalidate];
    END_BLOCK_OBJC_EXCEPTIONS
}

@end

namespace WebCore {

ScrollerMac::ScrollerMac(ScrollerPairMac& pair, ScrollbarOrientation orientation)
    : m_pair(pair)
    , m_orientation(orientation)
{
}

ScrollerMac::~ScrollerMac()
{
}

void ScrollerMac::attach()
{
    m_scrollerImpDelegate = adoptNS([[WebScrollerImpDelegateMac alloc] initWithScroller:this]);
    setScrollerImp([NSScrollerImp scrollerImpWithStyle:nsScrollerStyle(m_pair.scrollbarStyle()) controlSize:nsControlSizeFromScrollbarWidth(m_pair.scrollbarWidthStyle()) horizontal:m_orientation == ScrollbarOrientation::Horizontal replacingScrollerImp:nil]);
    [m_scrollerImp setDelegate:m_scrollerImpDelegate.get()];
}

void ScrollerMac::detach()
{
    [m_scrollerImpDelegate invalidate];
    [m_scrollerImp setDelegate:nil];
}

void ScrollerMac::setHostLayer(CALayer *layer)
{
    if (m_hostLayer == layer)
        return;

    m_hostLayer = layer;

    [m_scrollerImp setLayer:layer];

    updatePairScrollerImps();
}

void ScrollerMac::setHiddenByStyle(NativeScrollbarVisibility visibility)
{
    m_isHiddenByStyle = visibility != NativeScrollbarVisibility::Visible;
    if (m_isHiddenByStyle) {
        detach();
        setScrollerImp(nullptr);
    } else {
        attach();
        [m_scrollerImp setLayer:m_hostLayer.get()];
        updateValues();
    }
    updatePairScrollerImps();
}

void ScrollerMac::updateValues()
{
    auto values = m_pair.valuesForOrientation(m_orientation);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_scrollerImp setEnabled:m_isEnabled];
    [m_scrollerImp setBoundsSize:NSSizeFromCGSize([m_hostLayer bounds].size)];
    [m_scrollerImp setDoubleValue:values.value];
    [m_scrollerImp setPresentationValue:values.value];
    [m_scrollerImp setKnobProportion:values.proportion];

    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollerMac::updateScrollbarStyle()
{
    setScrollerImp([NSScrollerImp scrollerImpWithStyle:nsScrollerStyle(m_pair.scrollbarStyle()) controlSize:nsControlSizeFromScrollbarWidth(m_pair.scrollbarWidthStyle()) horizontal:m_orientation == ScrollbarOrientation::Horizontal replacingScrollerImp:nil]);
    [m_scrollerImp setDelegate:m_scrollerImpDelegate.get()];

    [m_scrollerImp setLayer:m_hostLayer.get()];

    updatePairScrollerImps();
    updateValues();
}

void ScrollerMac::updatePairScrollerImps()
{
    NSScrollerImp *scrollerImp = m_scrollerImp.get();
    if (m_orientation == ScrollbarOrientation::Vertical)
        m_pair.setVerticalScrollerImp(scrollerImp);
    else
        m_pair.setHorizontalScrollerImp(scrollerImp);
}

void ScrollerMac::mouseEnteredScrollbar()
{
    m_pair.ensureOnMainThreadWithProtectedThis([this] {
        // At this time, only legacy scrollbars needs to send notifications here.
        if (m_pair.scrollbarStyle() != WebCore::ScrollbarStyle::AlwaysVisible)
            return;

        if ([m_pair.scrollerImpPair() overlayScrollerStateIsLocked])
            return;

        [m_scrollerImp mouseEnteredScroller];
    });
}

void ScrollerMac::mouseExitedScrollbar()
{
    m_pair.ensureOnMainThreadWithProtectedThis([this] {
        // At this time, only legacy scrollbars needs to send notifications here.
        if (m_pair.scrollbarStyle() != WebCore::ScrollbarStyle::AlwaysVisible)
            return;

        if ([m_pair.scrollerImpPair() overlayScrollerStateIsLocked])
            return;

        [m_scrollerImp mouseExitedScroller];
    });
}

IntPoint ScrollerMac::lastKnownMousePositionInScrollbar() const
{
    // When we dont have an update from the Web Process, return
    // a point outside of the scrollbars
    if (!m_pair.mouseInContentArea())
        return { -1, -1 };
    return m_lastKnownMousePositionInScrollbar;
}

void ScrollerMac::visibilityChanged(bool isVisible)
{
    if (m_isVisible == isVisible)
        return;
    m_isVisible = isVisible;

    if (RefPtr node = m_pair.protectedNode())
        node->scrollbarVisibilityDidChange(m_orientation, isVisible);
}

void ScrollerMac::updateMinimumKnobLength(int minimumKnobLength)
{
    if (m_minimumKnobLength == minimumKnobLength)
        return;
    m_minimumKnobLength = minimumKnobLength;

    if (RefPtr node = m_pair.protectedNode())
        node->scrollbarMinimumThumbLengthDidChange(m_orientation, m_minimumKnobLength);
}

void ScrollerMac::setScrollerImp(NSScrollerImp *imp)
{
    if (m_isHiddenByStyle && imp)
        return;
    m_scrollerImp = imp;
    updateMinimumKnobLength([m_scrollerImp knobMinLength]);
}

void ScrollerMac::setScrollbarLayoutDirection(UserInterfaceLayoutDirection scrollbarLayoutDirection)
{
    [m_scrollerImp setUserInterfaceLayoutDirection: scrollbarLayoutDirection == UserInterfaceLayoutDirection::RTL ? NSUserInterfaceLayoutDirectionRightToLeft : NSUserInterfaceLayoutDirectionLeftToRight];
}

void ScrollerMac::setNeedsDisplay()
{
    [m_scrollerImp setNeedsDisplay:YES];
}

String ScrollerMac::scrollbarState() const
{
    if (!m_hostLayer || !m_scrollerImp)
        return "none"_s;

    StringBuilder result;
    result.append([m_scrollerImp isEnabled] ? "enabled"_s: "disabled"_s);

    if (m_pair.scrollbarStyle() != WebCore::ScrollbarStyle::Overlay)
        return result.toString();

    if ([m_scrollerImp isExpanded])
        result.append(",expanded"_s);

    if ([m_scrollerImp trackAlpha] > 0)
        result.append(",visible_track"_s);

    if ([m_scrollerImp knobAlpha] > 0)
        result.append(",visible_thumb"_s);

    if ([m_scrollerImp userInterfaceLayoutDirection] == NSUserInterfaceLayoutDirectionRightToLeft)
        result.append(",RTL"_s);

    if ([m_scrollerImp controlSize] != NSControlSizeRegular)
        result.append(",thin"_s);

    return result.toString();
}

}

#endif
