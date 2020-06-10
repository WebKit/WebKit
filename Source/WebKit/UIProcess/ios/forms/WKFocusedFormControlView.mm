/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "WKFocusedFormControlView.h"

#if PLATFORM(WATCHOS)

asm(".linker_option \"-framework\", \"PepperUICore\"");

#import <WebCore/LocalizedStrings.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/WTFString.h>

static const NSTimeInterval formControlViewAnimationDuration = 0.25;
static const CGFloat focusedRectDimmingViewAlpha = 0.8;
static const CGFloat focusedRectDimmingViewCornerRadius = 6;
static const CGFloat focusedRectExpandedHitTestMargin = 15;
static const CGFloat submitButtonHeight = 24;

static const NSTimeInterval dimmingViewAnimationDuration = 0.25;
// Determines the rate at which digital crown scrolling drives animations in the focus overlay.
static const CGFloat digitalCrownPointsPerUnit = 20;
static const CGFloat digitalCrownInactiveScrollDistancePerUnit = 4;
static const CGFloat digitalCrownActiveScrollDistancePerUnit = 8;
static const CGFloat digitalCrownMaximumScrollDistance = 12;

static UIBezierPath *pathWithRoundedRectInFrame(CGRect rect, CGFloat borderRadius, CGRect frame)
{
    UIBezierPath *path = [UIBezierPath bezierPathWithRoundedRect:rect cornerRadius:borderRadius];
    [path appendPath:[UIBezierPath bezierPathWithRect:frame]];
    return path;
}

@interface WKFocusedFormControlView () <PUICCrownInputSequencerDelegate>
@property (nonatomic, readonly) CAShapeLayer *dimmingMaskLayer;
@property (nonatomic) CGRect previousHighlightedFrame;
@property (nonatomic) CGRect nextHighlightedFrame;
@property (nonatomic) CGRect highlightedFrame;
@property (nonatomic, copy) NSString *submitActionName;
@property (nonatomic) BOOL hasNextNode;
@property (nonatomic) BOOL hasPreviousNode;
@end

@implementation WKFocusedFormControlView {
    RetainPtr<UIView> _dimmingView;
    RetainPtr<UIButton> _submitButton;
    RetainPtr<UIButton> _dismissButton;
    RetainPtr<UIView> _submitButtonBackgroundView;
    RetainPtr<UITapGestureRecognizer> _tapGestureRecognizer;
    RetainPtr<NSArray<UITextSuggestion *>> _textSuggestions;
    WeakObjCPtr<id <WKFocusedFormControlViewDelegate>> _delegate;
    RetainPtr<NSString> _submitActionName;
    RetainPtr<PUICCrownInputSequencer> _crownInputSequencer;
    Optional<CGPoint> _initialScrollViewContentOffset;
    BOOL _hasPendingFocusRequest;
}

- (instancetype)initWithFrame:(CGRect)frame delegate:(id <WKFocusedFormControlViewDelegate>)delegate
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    _delegate = delegate;
    _highlightedFrame = CGRectZero;

    _dimmingView = adoptNS([[UIView alloc] init]);
    [_dimmingView setOpaque:NO];
    [_dimmingView setBackgroundColor:[UIColor colorWithWhite:0 alpha:focusedRectDimmingViewAlpha]];
    auto maskLayer = adoptNS([[CAShapeLayer alloc] init]);
    [maskLayer setBackgroundColor:[UIColor blackColor].CGColor];
    [maskLayer setFillRule:kCAFillRuleEvenOdd];
    [_dimmingView layer].mask = maskLayer.get();
    [maskLayer web_disableAllActions];

    // FIXME: If we decide to turn on the focused form control overlay by default, we will need to move the submit and dismiss
    // buttons to the status bar. See <rdar://problem/39264218>.
    _submitButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_submitButton addTarget:self action:@selector(didSubmit) forControlEvents:UIControlEventTouchUpInside];

    _dismissButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_dismissButton addTarget:self action:@selector(didDismiss) forControlEvents:UIControlEventTouchUpInside];
    [_dismissButton setTitle:WebCore::formControlHideButtonTitle() forState:UIControlStateNormal];
    [_dismissButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];

    _tapGestureRecognizer = adoptNS([[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(handleTap)]);
    [_tapGestureRecognizer setNumberOfTapsRequired:1];

    self.opaque = NO;
    self.layer.masksToBounds = YES;
    [self addGestureRecognizer:_tapGestureRecognizer.get()];

    _submitButtonBackgroundView = adoptNS([[UIView alloc] init]);
    [_submitButtonBackgroundView setOpaque:YES];
    [_submitButtonBackgroundView setBackgroundColor:[UIColor blackColor]];

    [_submitButtonBackgroundView addSubview:_dismissButton.get()];
    [_submitButtonBackgroundView addSubview:_submitButton.get()];
    [self addSubview:_dimmingView.get()];
    [self addSubview:_submitButtonBackgroundView.get()];

    _hasPendingFocusRequest = NO;
    _initialScrollViewContentOffset = WTF::nullopt;

    return self;
}

- (BOOL)handleWheelEvent:(UIEvent *)event
{
    [self _wheelChangedWithEvent:event];
    return YES;
}

- (void)show:(BOOL)animated
{
    if (!self.hidden)
        return;

    self.hidden = NO;

    auto fadeInViewBlock = [view = retainPtr(self)] {
        [view setAlpha:1];
    };

    if (animated)
        [UIView animateWithDuration:formControlViewAnimationDuration delay:0 options:UIViewAnimationOptionBeginFromCurrentState animations:fadeInViewBlock completion:nil];
    else
        fadeInViewBlock();
}

- (void)hide:(BOOL)animated
{
    if (self.hidden)
        return;

    if (!animated) {
        self.alpha = 0;
        self.hidden = YES;
        return;
    }

    [UIView animateWithDuration:formControlViewAnimationDuration delay:0 options:UIViewAnimationOptionBeginFromCurrentState animations:[view = retainPtr(self)] {
        [view setAlpha:0];
    } completion:[view = retainPtr(self)] (BOOL finished)
    {
        if (finished)
            [view setHidden:YES];
    }];
}

- (id <WKFocusedFormControlViewDelegate>)delegate
{
    return _delegate.get().get();
}

- (void)setDelegate:(id <WKFocusedFormControlViewDelegate>)delegate
{
    _delegate = delegate;
}

- (CAShapeLayer *)dimmingMaskLayer
{
    return (CAShapeLayer *)[_dimmingView layer].mask;
}

- (void)handleTap
{
    if (CGRectContainsPoint(CGRectInset(_highlightedFrame, -focusedRectExpandedHitTestMargin, -focusedRectExpandedHitTestMargin), [_tapGestureRecognizer location])) {
        [_delegate focusedFormControlViewDidBeginEditing:self];
        return;
    }

    [self didDismiss];
}

- (void)_wheelChangedWithEvent:(UIEvent *)event
{
    [super _wheelChangedWithEvent:event];

    if ([_crownInputSequencer delegate] == self)
        [_crownInputSequencer updateWithCrownInputEvent:event];
}

- (void)didDismiss
{
    [_delegate focusedFormControlViewDidCancel:self];
}

- (void)didSubmit
{
    [_delegate focusedFormControlViewDidSubmit:self];
}

- (void)layoutSubviews
{
    [super layoutSubviews];

    CGRect viewBounds = self.bounds;
    [_dimmingView setFrame:UIRectInsetEdges(viewBounds, UIRectEdgeAll, -digitalCrownMaximumScrollDistance)];
    [_dismissButton sizeToFit];
    [_dismissButton setFrame:CGRect { CGPointMake(0, 0), [_dismissButton size] }];
    [_submitButton sizeToFit];
    [_submitButton setFrame:CGRect { CGPointMake(CGRectGetWidth(viewBounds) - CGRectGetWidth([_submitButton bounds]), 0), [_submitButton size] }];
    [_submitButtonBackgroundView setFrame:CGRectMake(0, 0, CGRectGetWidth(viewBounds), submitButtonHeight)];
}

- (void)setHighlightedFrame:(CGRect)highlightedFrame
{
    [self setHighlightedFrame:highlightedFrame animated:NO];
}

- (UIBezierPath *)computeDimmingViewCutoutPath
{
    if (CGRectIsEmpty(_highlightedFrame))
        return nil;

    CGRect inflatedHighlightFrame = UIRectInsetEdges(_highlightedFrame, UIRectEdgeAll, -focusedRectDimmingViewCornerRadius);
    return pathWithRoundedRectInFrame([self convertRect:inflatedHighlightFrame toView:_dimmingView.get()], focusedRectDimmingViewCornerRadius, [_dimmingView bounds]);
}

- (void)disengageFocusedFormControlNavigation
{
    [self cancelPendingCrownInputSequencerUpdate];
    [_crownInputSequencer stopVelocityTrackingAndDecelerationImmediately];
    [_crownInputSequencer setOffset:0];
    [_crownInputSequencer setDelegate:nil];
}

- (void)engageFocusedFormControlNavigation
{
    if (!_crownInputSequencer) {
        _crownInputSequencer = adoptNS([[PUICCrownInputSequencer alloc] init]);
        [_crownInputSequencer setRubberBandingEnabled:YES];
        [_crownInputSequencer setUseWideIdleCheck:YES];
        [_crownInputSequencer setStart:-1];
        [_crownInputSequencer setEnd:1];
        [_crownInputSequencer setScreenSpaceMultiplier:digitalCrownPointsPerUnit];
    }

    _initialScrollViewContentOffset = self.scrollViewForCrownInputSequencer.contentOffset;
    [_crownInputSequencer stopVelocityTrackingAndDecelerationImmediately];
    [_crownInputSequencer setOffset:0];
    [_crownInputSequencer setDelegate:self];
}

- (void)reloadData:(BOOL)animated
{
    [self setPreviousHighlightedFrame:[_delegate previousRectForFocusedFormControlView:self]];
    [self setNextHighlightedFrame:[_delegate nextRectForFocusedFormControlView:self]];
    [self setHighlightedFrame:[_delegate rectForFocusedFormControlView:self] animated:animated];
    self.submitActionName = [_delegate actionNameForFocusedFormControlView:self];
    self.hasNextNode = [_delegate hasNextNodeForFocusedFormControlView:self];
    self.hasPreviousNode = [_delegate hasPreviousNodeForFocusedFormControlView:self];
    [self setMaskLayerPosition:CGPointZero animated:animated];

    _hasPendingFocusRequest = NO;
}

- (void)setMaskLayerPosition:(CGPoint)position animated:(BOOL)animated
{
    if (animated) {
        CABasicAnimation *maskAnimation = [CABasicAnimation animationWithKeyPath:@"position"];
        maskAnimation.fromValue = [NSValue valueWithCGPoint:self.dimmingMaskLayer.position];
        maskAnimation.toValue = [NSValue valueWithCGPoint:position];
        maskAnimation.duration = dimmingViewAnimationDuration;
        maskAnimation.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
        [self.dimmingMaskLayer addAnimation:maskAnimation forKey:nil];
    }
    self.dimmingMaskLayer.position = position;
}

- (void)setHighlightedFrame:(CGRect)highlightedFrame animated:(BOOL)animated
{
    if (CGRectEqualToRect(_highlightedFrame, highlightedFrame))
        return;

    _highlightedFrame = highlightedFrame;

    UIBezierPath *updatedCutoutPath = [self computeDimmingViewCutoutPath];
    if (!animated || !self.dimmingMaskLayer.path || !updatedCutoutPath) {
        self.dimmingMaskLayer.path = updatedCutoutPath.CGPath;
        return;
    }

    CABasicAnimation *maskAnimation = [CABasicAnimation animationWithKeyPath:@"path"];
    maskAnimation.fromValue = (__bridge UIBezierPath *)self.dimmingMaskLayer.path;
    maskAnimation.toValue = updatedCutoutPath;
    maskAnimation.duration = dimmingViewAnimationDuration;
    maskAnimation.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
    [self.dimmingMaskLayer addAnimation:maskAnimation forKey:nil];
    self.dimmingMaskLayer.path = updatedCutoutPath.CGPath;
}

- (NSString *)submitActionName
{
    return _submitActionName.get();
}

static NSDictionary *submitActionNameFontAttributes()
{
    return @{ NSFontAttributeName: [UIFont boldSystemFontOfSize:15] };
}

- (void)setSubmitActionName:(NSString *)submitActionName
{
    if (_submitActionName == submitActionName || [_submitActionName isEqualToString:submitActionName])
        return;

    _submitActionName = adoptNS(submitActionName.copy);

    if (submitActionName.length) {
        auto boldTitle = adoptNS([[NSAttributedString alloc] initWithString:submitActionName attributes:submitActionNameFontAttributes()]);
        [_submitButton setAttributedTitle:boldTitle.get() forState:UIControlStateNormal];
    } else
        [_submitButton setAttributedTitle:nil forState:UIControlStateNormal];

    [self setNeedsLayout];
}

- (UIScrollView *)scrollViewForCrownInputSequencer
{
    return [_delegate scrollViewForFocusedFormControlView:self];
}

- (void)updateViewForCurrentCrownInputSequencerState
{
    if (!_initialScrollViewContentOffset)
        return;

    CGFloat crownOffset = [_crownInputSequencer offset];
    UIScrollView *scrollView = self.scrollViewForCrownInputSequencer;
    CGPoint targetContentOffset = [self scrollOffsetForCrownInputOffset:crownOffset];
    scrollView.contentOffset = targetContentOffset;
    self.dimmingMaskLayer.position = CGPointMake(_initialScrollViewContentOffset->x - targetContentOffset.x, _initialScrollViewContentOffset->y - targetContentOffset.y);

    if (_hasPendingFocusRequest)
        return;

    if (crownOffset > 1 && self.hasNextNode) {
        [_delegate focusedFormControlViewDidRequestNextNode:self];
        _hasPendingFocusRequest = YES;
    }

    if (crownOffset < -1 && self.hasPreviousNode) {
        [_delegate focusedFormControlViewDidRequestPreviousNode:self];
        _hasPendingFocusRequest = YES;
    }
}

- (CGPoint)scrollOffsetForCrownInputOffset:(CGFloat)crownOffset
{
    ASSERT(_initialScrollViewContentOffset);
    if (!crownOffset)
        return *_initialScrollViewContentOffset;

    CGPoint highlightedFrameMidpoint { CGRectGetMidX(_highlightedFrame), CGRectGetMidY(_highlightedFrame) };
    CGVector unitVectorToTarget = CGVectorMake(0, crownOffset < 0 ? -1 : 1);
    CGFloat targetHighlightedFrameDeltaX = 0;
    CGFloat targetHighlightedFrameDeltaY = 0;
    BOOL isMovingTowardsNextOrPreviousNode = NO;
    if (crownOffset < 0 && self.hasPreviousNode) {
        targetHighlightedFrameDeltaX = CGRectGetMidX(_previousHighlightedFrame) - highlightedFrameMidpoint.x;
        targetHighlightedFrameDeltaY = CGRectGetMidY(_previousHighlightedFrame) - highlightedFrameMidpoint.y;
        isMovingTowardsNextOrPreviousNode = YES;
    } else if (crownOffset > 0 && self.hasNextNode) {
        targetHighlightedFrameDeltaX = CGRectGetMidX(_nextHighlightedFrame) - highlightedFrameMidpoint.x;
        targetHighlightedFrameDeltaY = CGRectGetMidY(_nextHighlightedFrame) - highlightedFrameMidpoint.y;
        isMovingTowardsNextOrPreviousNode = YES;
    }

    if (targetHighlightedFrameDeltaX || targetHighlightedFrameDeltaY) {
        CGFloat distanceToTarget = std::hypot(targetHighlightedFrameDeltaX, targetHighlightedFrameDeltaY);
        unitVectorToTarget = CGVector { targetHighlightedFrameDeltaX / distanceToTarget, targetHighlightedFrameDeltaY / distanceToTarget };
    }

    CGFloat distancePerUnit = isMovingTowardsNextOrPreviousNode ? digitalCrownActiveScrollDistancePerUnit : digitalCrownInactiveScrollDistancePerUnit;
    CGFloat absoluteClampedCrownOffset = std::min<CGFloat>(digitalCrownMaximumScrollDistance / distancePerUnit, ABS(crownOffset));
    return {
        _initialScrollViewContentOffset->x + distancePerUnit * unitVectorToTarget.dx * absoluteClampedCrownOffset,
        _initialScrollViewContentOffset->y + distancePerUnit * unitVectorToTarget.dy * absoluteClampedCrownOffset
    };
}

- (void)_crownInputSequencerTimerFired
{
    CGFloat currentCrownInputSequencerOffset = [_crownInputSequencer offset];
    if (ABS(currentCrownInputSequencerOffset) < 0.01) {
        [_crownInputSequencer setOffset:0];
        [self updateViewForCurrentCrownInputSequencerState];
        return;
    }

    [_crownInputSequencer setOffset:0.75 * currentCrownInputSequencerOffset];
    [self updateViewForCurrentCrownInputSequencerState];
    [self scheduleCrownInputSequencerUpdate];
}

- (void)cancelPendingCrownInputSequencerUpdate
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_crownInputSequencerTimerFired) object:nil];
}

- (void)scheduleCrownInputSequencerUpdate
{
    [self performSelector:@selector(_crownInputSequencerTimerFired) withObject:nil afterDelay:1 / 60.];
}

#pragma mark - PUICCrownInputSequencerDelegate

- (void)crownInputSequencerOffsetDidChange:(PUICCrownInputSequencer *)crownInputSequencer
{
    [self updateViewForCurrentCrownInputSequencerState];
}

- (void)crownInputSequencerDidBecomeIdle:(PUICCrownInputSequencer *)crownInputSequencer willDecelerate:(BOOL)willDecelerate
{
    [_crownInputSequencer stopVelocityTrackingAndDecelerationImmediately];
    [self scheduleCrownInputSequencerUpdate];
}

- (void)crownInputSequencerIdleDidChange:(PUICCrownInputSequencer *)crownInputSequencer
{
    if (!crownInputSequencer.idle)
        [self cancelPendingCrownInputSequencerUpdate];
}

#pragma mark - UITextInputSuggestionDelegate

// FIXME: Move text suggestion state out of the focus form overlay and into the content view or input session.
// This class should only be concerned about the focused form overlay.
- (NSArray<UITextSuggestion *> *)suggestions
{
    return _textSuggestions.get();
}

- (void)setSuggestions:(NSArray<UITextSuggestion *> *)suggestions
{
    RetainPtr<NSMutableArray> displayableTextSuggestions;
    if (suggestions) {
        displayableTextSuggestions = adoptNS([[NSMutableArray alloc] initWithCapacity:suggestions.count]);
        for (UITextSuggestion *suggestion in suggestions) {
            if (!suggestion.displayText.length)
                continue;

            [displayableTextSuggestions addObject:suggestion];
        }
    }

    if (_textSuggestions == displayableTextSuggestions.get() || [_textSuggestions isEqualToArray:displayableTextSuggestions.get()])
        return;

    _textSuggestions = WTFMove(displayableTextSuggestions);
    [_delegate focusedFormControllerDidUpdateSuggestions:self];
}

- (void)handleWebViewCredentialsSaveForWebsiteURL:(NSURL *)websiteURL user:(NSString *)user password:(NSString *)password passwordIsAutoGenerated:(BOOL)passwordIsAutoGenerated
{
}

- (void)selectionWillChange:(id <UITextInput>)textInput
{
}

- (void)selectionDidChange:(id <UITextInput>)textInput
{
}

- (void)textWillChange:(id <UITextInput>)textInput
{
}

- (void)textDidChange:(id <UITextInput>)textInput
{
}

@end

#endif // PLATFORM(WATCHOS)
