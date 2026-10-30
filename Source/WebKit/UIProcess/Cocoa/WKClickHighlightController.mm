/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "WKClickHighlightController.h"

#if ENABLE(TWO_PHASE_CLICKS)

#import "WKClickHighlightView.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/ColorCocoa.h>
#import <WebCore/FloatRoundedRect.h>
#import <wtf/Markable.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

static constexpr auto minimumClickHighlightRadius = 2.0;
static constexpr auto clickHighlightPaintThreshold = 0.3;
static constexpr auto clickHighlightFadeDuration = 0.1_s;
static constexpr auto debugClickHighlightFadeDuration = 0.25_s;
static NSString * const clickHighlightFadeAnimationKey = @"WKClickHighlightFade";

static WebCore::FloatQuad inflateQuad(const WebCore::FloatQuad& quad, float inflateSize)
{
    // We sort the output points like this (as expected by the highlight view):
    //  p2------p3
    //  |       |
    //  p1------p4

    // 1) Sort the points horizontally.
    std::array points { quad.p1(), quad.p4(), quad.p2(), quad.p3() };
    if (points[0].x() > points[1].x())
        std::swap(points[0], points[1]);
    if (points[2].x() > points[3].x())
        std::swap(points[2], points[3]);

    if (points[0].x() > points[2].x())
        std::swap(points[0], points[2]);
    if (points[1].x() > points[3].x())
        std::swap(points[1], points[3]);

    if (points[1].x() > points[2].x())
        std::swap(points[1], points[2]);

    // 2) Swap them vertically to have the output points [p2, p1, p3, p4].
    if (points[1].y() < points[0].y())
        std::swap(points[0], points[1]);
    if (points[3].y() < points[2].y())
        std::swap(points[2], points[3]);

    // 3) Adjust the positions.
    points[0].move(-inflateSize, -inflateSize);
    points[1].move(-inflateSize, inflateSize);
    points[2].move(inflateSize, -inflateSize);
    points[3].move(inflateSize, inflateSize);

    return WebCore::FloatQuad(points[1], points[0], points[2], points[3]);
}

struct ClickHighlightInformation {
    BOOL nodeHasBuiltInClickHandling { NO };
    WebCore::Color color;
    Vector<WebCore::FloatQuad> quads;
    WebCore::CornerRadii cornerRadii;
};

@interface WKClickHighlightController () <CAAnimationDelegate>
@end

@implementation WKClickHighlightController {
    WeakObjCPtr<id<WKClickHighlightControllerDelegate>> _delegate;
    RetainPtr<WKClickHighlightView> _highlightView;
    ClickHighlightInformation _highlightInformation;
    Markable<WebKit::TapIdentifier> _requestID;
    BOOL _isRequestValid;
    BOOL _hasHighlightForPotentialClick;
    BOOL _expectsFastCommit;
}

- (instancetype)initWithDelegate:(id<WKClickHighlightControllerDelegate>)delegate
{
    if (!(self = [super init]))
        return nil;

    _delegate = delegate;
    return self;
}

- (void)beginRequest:(WebKit::TapIdentifier)requestID expectsFastCommit:(BOOL)expectsFastCommit
{
    _requestID = requestID;
    _isRequestValid = YES;
    _expectsFastCommit = expectsFastCommit;
    // A highlight that's still pending belongs to an earlier click, which was cancelled before it was committed.
    _hasHighlightForPotentialClick = NO;
}

- (BOOL)_isPotentialClickInProgress
{
    RetainPtr delegate = _delegate.get();
    return [delegate clickHighlightControllerIsPotentialClickInProgress:self];
}

- (void)didReceiveHighlightForRequest:(WebKit::TapIdentifier)requestID color:(const WebCore::Color&)color quads:(const Vector<WebCore::FloatQuad>&)quads cornerRadii:(WebCore::CornerRadii&&)cornerRadii nodeHasBuiltInClickHandling:(BOOL)nodeHasBuiltInClickHandling
{
    if (!_isRequestValid || _requestID != requestID)
        return;

    _isRequestValid = NO;

    _highlightInformation.quads = quads;
    _highlightInformation.cornerRadii = WTF::move(cornerRadii);
    _highlightInformation.nodeHasBuiltInClickHandling = nodeHasBuiltInClickHandling;
    _highlightInformation.color = color;

    if ([self _isPotentialClickInProgress]) {
        _hasHighlightForPotentialClick = YES;
        return;
    }

    [self _showHighlight];
    if (_expectsFastCommit) {
        [self finish];
        if (![self _isPotentialClickInProgress])
            _expectsFastCommit = NO;
    }
}

- (void)showPendingHighlightForCommittedClick
{
    if (!_hasHighlightForPotentialClick)
        return;

    [self _showHighlight];
    _hasHighlightForPotentialClick = NO;
}

- (void)finishUnlessExpectingFastCommit
{
    if (!_expectsFastCommit)
        [self finish];
}

- (void)fadeUnlessAwaitingHighlight
{
    if (!_isRequestValid)
        [self _fadeHighlightIfNeeded];
}

- (void)finish
{
    _isRequestValid = NO;
    [self _fadeHighlightIfNeeded];
}

- (void)cancel
{
    _isRequestValid = NO;
    [self _removeHighlightView];
}

- (void)cleanUp
{
    _expectsFastCommit = NO;
    [self _removeHighlightView];
}

- (void)setHighlightColor:(const WebCore::Color&)color
{
    _highlightInformation.color = color;
}

- (CGRect)highlightViewFrame
{
    return [_highlightView frame];
}

- (WebCore::Color)highlightColor
{
    return _highlightInformation.color;
}

- (BOOL)isShowingHighlight
{
    return !![_highlightView superview];
}

- (BOOL)_shouldPaintHighlightInRect:(const WebCore::FloatRect&)rect
{
    // FIXME: <https://webkit.org/b/328335> Make paint decision based on input type rather than a blanket platform policy.
#if PLATFORM(MACCATALYST)
    UNUSED_PARAM(rect);
    return NO;
#else
    if (_highlightInformation.nodeHasBuiltInClickHandling)
        return YES;

    float highlightArea = 0;
    for (auto highlightQuad : _highlightInformation.quads) {
        auto boundingBox = highlightQuad.boundingBox();
        highlightArea += boundingBox.area();
        if (boundingBox.width() > (rect.width() * clickHighlightPaintThreshold) || boundingBox.height() > (rect.height() * clickHighlightPaintThreshold))
            return NO;
    }
    return highlightArea < rect.area() * clickHighlightPaintThreshold;
#endif
}

- (void)_showHighlight
{
    RetainPtr delegate = _delegate.get();
    if (!delegate)
        return;

    if (![self _shouldPaintHighlightInRect:[delegate clickHighlightControllerUnobscuredRect:self]] && !_usesDebugAppearance)
        return;

    if (!_highlightView) {
        _highlightView = adoptNS([[WKClickHighlightView alloc] initWithFrame:CGRectZero]);
        [_highlightView setMinimumCornerRadius:minimumClickHighlightRadius];
    }

    // This supersedes the fade of a previous highlight, if any.
    [[_highlightView layer] removeAnimationForKey:clickHighlightFadeAnimationKey];

    [delegate clickHighlightController:self installHighlightView:_highlightView];
    [self updateHighlight];
}

- (void)updateHighlight
{
    if (![_highlightView superview])
        return;

    RetainPtr delegate = _delegate.get();
    if (!delegate)
        return;

    [_highlightView setColor:cocoaColor(_highlightInformation.color)];

    auto& highlightedQuads = _highlightInformation.quads;
    bool allRectilinear = true;
    for (auto& quad : highlightedQuads) {
        if (!quad.isRectilinear()) {
            allRectilinear = false;
            break;
        }
    }

    auto contentScale = [delegate clickHighlightControllerContentScale:self];
    if (allRectilinear) {
        [_highlightView setFrames:highlightedQuads.map([contentScale](auto& quad) {
            auto boundingBox = quad.boundingBox();
            boundingBox.scale(contentScale);
            boundingBox.inflate(minimumClickHighlightRadius);
            return boundingBox;
        })];
    } else {
        auto inflatedHighlightQuads = highlightedQuads.map([contentScale](auto quad) {
            quad.scale(contentScale);
            return inflateQuad(quad, minimumClickHighlightRadius);
        });
        [_highlightView setQuads:WTF::move(inflatedHighlightQuads) boundaryRect:[delegate clickHighlightControllerBoundaryRect:self]];
    }

    auto cornerRadii = _highlightInformation.cornerRadii;
    cornerRadii.scale([delegate clickHighlightControllerCornerRadiusScale:self]);
    cornerRadii.expandEvenIfZero(minimumClickHighlightRadius);
    [_highlightView setCornerRadii:WTF::move(cornerRadii)];
}

- (void)_fadeHighlightIfNeeded
{
    RetainPtr layer = [_highlightView layer];
    if (![_highlightView superview] || [layer animationForKey:clickHighlightFadeAnimationKey])
        return;

    RetainPtr fadeAnimation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    [fadeAnimation setFromValue:@1];
    [fadeAnimation setToValue:@0];
    [fadeAnimation setDuration:(_usesDebugAppearance ? debugClickHighlightFadeDuration : clickHighlightFadeDuration).seconds()];
    [fadeAnimation setTimingFunction:[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut]];
    [fadeAnimation setFillMode:kCAFillModeForwards];
    [fadeAnimation setRemovedOnCompletion:NO];
    [fadeAnimation setDelegate:self];
    [layer addAnimation:fadeAnimation forKey:clickHighlightFadeAnimationKey];
}

- (void)animationDidStop:(CAAnimation *)animation finished:(BOOL)finished
{
    // A fade that was superseded by another highlight, or by removing the highlight, is no longer on the layer.
    if ([[_highlightView layer] animationForKey:clickHighlightFadeAnimationKey] == animation)
        [self _removeHighlightView];
}

- (void)_removeHighlightView
{
    [[_highlightView layer] removeAnimationForKey:clickHighlightFadeAnimationKey];
    [_highlightView removeFromSuperview];
}

@end

#endif // ENABLE(TWO_PHASE_CLICKS)
