/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "WKBaseScrollView.h"

#if PLATFORM(IOS_FAMILY)

#import "Logging.h"
#import "RemoteLayerTreeViews.h"
#import "UIKitSPI.h"
#import "WKContentView.h"
#import <objc/runtime.h>
#import <wtf/ApproximateTime.h>
#import <wtf/RetainPtr.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/SetForScope.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
#import "RemoteLayerTreeHost.h"
#import "UIKitUtilities.h"
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/cocoa/VectorCocoa.h>
#endif

template <size_t windowSize>
struct ScrollingDeltaWindow {
public:
    static constexpr auto maxDeltaDuration = 100_ms;

    void update(CGPoint contentOffset)
    {
        auto currentTime = ApproximateTime::now();
        auto deltaDuration = currentTime - m_lastTimestamp;
        if (deltaDuration > maxDeltaDuration)
            reset();
        else {
            m_deltas[m_lastIndex] = {
                CGSizeMake(contentOffset.x - m_lastOffset.x, contentOffset.y - m_lastOffset.y),
                deltaDuration
            };
            m_lastIndex = ++m_lastIndex % windowSize;
        }
        m_lastTimestamp = currentTime;
        m_lastOffset = contentOffset;
    }

    void reset()
    {
        for (auto& delta : m_deltas)
            delta = { CGSizeZero, 0_ms };
    }

    CGSize averageVelocity() const
    {
        if (ApproximateTime::now() - m_lastTimestamp > maxDeltaDuration)
            return CGSizeZero;

        auto cumulativeDelta = CGSizeZero;
        CGFloat numberOfDeltas = 0;
        for (auto [delta, duration] : m_deltas) {
            if (!duration)
                continue;

            cumulativeDelta.width += delta.width / duration.seconds();
            cumulativeDelta.height += delta.height / duration.seconds();
            numberOfDeltas += 1;
        }

        if (!numberOfDeltas)
            return CGSizeZero;

        cumulativeDelta.width /= numberOfDeltas;
        cumulativeDelta.height /= numberOfDeltas;
        return cumulativeDelta;
    }

private:
    std::array<std::pair<CGSize, Seconds>, windowSize> m_deltas;
    size_t m_lastIndex { 0 };
    ApproximateTime m_lastTimestamp;
    CGPoint m_lastOffset { CGPointZero };
};

@interface UIScrollView (GestureRecognizerDelegate) <UIGestureRecognizerDelegate>
@end

@implementation WKBaseScrollView {
    RetainPtr<UIPanGestureRecognizer> _axisLockingPanGestureRecognizer;
    UIAxis _axesToPreventMomentumScrolling;
    BOOL _isBeingRemovedFromSuperview;
    ScrollingDeltaWindow<3> _scrollingDeltaWindow;
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    HashSet<WebCore::IntRect> _overlayRegionRects;
    HashSet<WebCore::PlatformLayerIdentifier> _overlayRegionAssociatedLayers;
#endif
}

- (instancetype)initWithFrame:(CGRect)frame
{
    [WKBaseScrollView _overrideAddGestureRecognizerIfNeeded];

    if (!(self = [super initWithFrame:frame]))
        return nil;

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING) && !USE(BROWSERENGINEKIT)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    self._allowsAsyncScrollEvent = YES;
ALLOW_DEPRECATED_DECLARATIONS_END
#endif

    _axesToPreventMomentumScrolling = UIAxisNeither;
    [self.panGestureRecognizer addTarget:self action:@selector(_updatePanGestureToPreventScrolling)];
    return self;
}

+ (void)_overrideAddGestureRecognizerIfNeeded
{
    static bool hasOverridenAddGestureRecognizer = false;
    if (std::exchange(hasOverridenAddGestureRecognizer, true))
        return;

    if (WTF::IOSApplication::isHimalaya() && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ScrollViewSubclassImplementsAddGestureRecognizer)) {
        // This check can be removed and -_wk_addGestureRecognizer: can be renamed to -addGestureRecognizer: once the 喜马拉雅 app updates to a version of
        // the iOS 17 SDK with this WKBaseScrollView refactoring. Otherwise, the call to `-[super addGestureRecognizer:]` below will fail, due to how this
        // app uses `class_getInstanceMethod` and `method_setImplementation` to intercept and override all calls to `-[UIView addGestureRecognizer:]`.
        return;
    }

    auto method = class_getInstanceMethod(self.class, @selector(_wk_addGestureRecognizer:));
    class_addMethod(self.class, @selector(addGestureRecognizer:), method_getImplementation(method), method_getTypeEncoding(method));
}

- (void)_wk_addGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
    if (self.panGestureRecognizer == gestureRecognizer) {
        if (!_axisLockingPanGestureRecognizer) {
            _axisLockingPanGestureRecognizer = adoptNS([[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(_updatePanGestureToPreventScrolling)]);
            [_axisLockingPanGestureRecognizer setName:@"Scroll axis locking"];
            [_axisLockingPanGestureRecognizer setDelegate:self];
        }
        [self addGestureRecognizer:_axisLockingPanGestureRecognizer.get()];
    }

    [super addGestureRecognizer:gestureRecognizer];
}

- (void)removeGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
    if (self.panGestureRecognizer == gestureRecognizer) {
        if (auto gesture = std::exchange(_axisLockingPanGestureRecognizer, nil))
            [self removeGestureRecognizer:gesture.get()];
    }

    [super removeGestureRecognizer:gestureRecognizer];
}

- (void)_updatePanGestureToPreventScrolling
{
    auto panGesture = self.panGestureRecognizer;
    switch (self.panGestureRecognizer.state) {
    case UIGestureRecognizerStatePossible:
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateCancelled:
    case UIGestureRecognizerStateFailed:
        return;
    case UIGestureRecognizerStateBegan:
    case UIGestureRecognizerStateChanged:
        break;
    }

    switch ([_axisLockingPanGestureRecognizer state]) {
    case UIGestureRecognizerStateCancelled:
    case UIGestureRecognizerStateFailed:
        return;
    case UIGestureRecognizerStatePossible:
    case UIGestureRecognizerStateBegan:
    case UIGestureRecognizerStateChanged:
    case UIGestureRecognizerStateEnded:
        break;
    }

    auto axesToPrevent = self._axesToPreventScrollingFromDelegate;
    if (axesToPrevent == UIAxisNeither)
        return;

    auto adjustedTranslation = [panGesture translationInView:nil];
    bool translationChanged = false;
    if ((axesToPrevent & UIAxisHorizontal) && std::abs(adjustedTranslation.x) > CGFLOAT_EPSILON) {
        adjustedTranslation.x = 0;
        _axesToPreventMomentumScrolling |= UIAxisHorizontal;
        translationChanged = true;
    }

    if ((axesToPrevent & UIAxisVertical) && std::abs(adjustedTranslation.y) > CGFLOAT_EPSILON) {
        adjustedTranslation.y = 0;
        _axesToPreventMomentumScrolling |= UIAxisVertical;
        translationChanged = true;
    }

    if (translationChanged)
        [panGesture setTranslation:adjustedTranslation inView:nil];
}

- (void)removeFromSuperview
{
    SetForScope removeFromSuperviewScope { _isBeingRemovedFromSuperview, YES };

    [super removeFromSuperview];
}

- (UIAxis)_axesToPreventScrollingFromDelegate
{
    if (_isBeingRemovedFromSuperview || !self.window)
        return UIAxisNeither;
    auto delegate = self.baseScrollViewDelegate;
    return delegate ? [delegate axesToPreventScrollingForPanGestureInScrollView:self] : UIAxisNeither;
}

- (void)updateInteractiveScrollVelocity
{
    if (!self.tracking && !self.decelerating)
        return;

    _scrollingDeltaWindow.update(self.contentOffset);
}

- (CGSize)interactiveScrollVelocityInPointsPerSecond
{
    return _scrollingDeltaWindow.averageVelocity();
}

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/WKBaseScrollViewAdditions.mm>)
#import <WebKitAdditions/WKBaseScrollViewAdditions.mm>
#else
constexpr float overlayRegionContentFactor = 1.1;

- (BOOL)_hasEnoughContentForOverlayRegions
{
    return ((self._wk_contentHeightIncludingInsets > self.bounds.size.height * overlayRegionContentFactor)
        || (self._wk_contentWidthIncludingInsets > self.bounds.size.width * overlayRegionContentFactor));
}
- (void)_updateOverlayRegionsBehavior:(BOOL)selected
{
    UIAxis scrollableAxes = UIAxisNeither;

    if (selected) {
        if (self._wk_contentHeightIncludingInsets > self.bounds.size.height * overlayRegionContentFactor)
            scrollableAxes |= UIAxisVertical;
        if (self._wk_contentWidthIncludingInsets > self.bounds.size.width * overlayRegionContentFactor)
            scrollableAxes |= UIAxisHorizontal;
    } else if (_overlayRegionRects.size())
        [self _updateOverlayRegionRects: { } whileStable:YES];

    if (self._scrollingBehavior == scrollableAxes)
        return;

    self._scrollingBehavior = scrollableAxes;
}

- (void)_updateOverlayRegionRects:(const HashSet<WebCore::IntRect>&)overlayRegions whileStable:(BOOL)stable
{
    auto diff = _overlayRegionRects.symmetricDifferenceWith(overlayRegions);
    if (!diff.size())
        return;

    if (!stable && overlayRegions.size() == _overlayRegionRects.size())
        return;

    _overlayRegionRects = overlayRegions;
}

- (void)_associateRelatedLayersForOverlayRegions:(const HashSet<WebCore::PlatformLayerIdentifier>&)relatedLayers with:(const WebKit::RemoteLayerTreeHost&)host
{
    auto diff = _overlayRegionAssociatedLayers.symmetricDifferenceWith(relatedLayers);
    if (!diff.size())
        return;

    _overlayRegionAssociatedLayers = relatedLayers;
}
#endif

#endif // ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    if (gestureRecognizer == _axisLockingPanGestureRecognizer || otherGestureRecognizer == _axisLockingPanGestureRecognizer)
        return YES;

    static BOOL callIntoSuperclass = [UIScrollView instancesRespondToSelector:@selector(gestureRecognizer:shouldRecognizeSimultaneouslyWithGestureRecognizer:)];
    if (!callIntoSuperclass)
        return NO;

    return [super gestureRecognizer:gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:otherGestureRecognizer];
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
    if (self.panGestureRecognizer == gestureRecognizer)
        _axesToPreventMomentumScrolling = UIAxisNeither;

    static BOOL callIntoSuperclass = [UIScrollView instancesRespondToSelector:@selector(gestureRecognizerShouldBegin:)];
    if (!callIntoSuperclass)
        return YES;

    return [super gestureRecognizerShouldBegin:gestureRecognizer];
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch
{
    if (self.panGestureRecognizer == gestureRecognizer) {
        RetainPtr delegate = [self baseScrollViewDelegate];
        if (delegate && ![delegate shouldAllowPanGestureRecognizerToReceiveTouchesInScrollView:self])
            return NO;
    }

    static BOOL callIntoSuperclass = [UIScrollView instancesRespondToSelector:@selector(gestureRecognizer:shouldReceiveTouch:)];
    if (!callIntoSuperclass)
        return YES;

    return [super gestureRecognizer:gestureRecognizer shouldReceiveTouch:touch];
}

@end

#endif // PLATFORM(IOS_FAMILY)
