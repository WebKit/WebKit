/*
 * Copyright (C) 2017 Igalia S.L. All rights reserved.
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
#import "PageClient.h"
#import "ScrollingTreeScrollingNodeDelegateIOS.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(ASYNC_SCROLLING)

#import "RemoteLayerTreeViews.h"
#import "RemoteScrollingCoordinatorProxyIOS.h"
#import "RemoteScrollingTree.h"
#import "UIKitSPI.h"
#import "UIKitUtilities.h"
#import "WKBaseScrollView.h"
#import "WKScrollView.h"
#import "WebPageProxy.h"
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIPanGestureRecognizer.h>
#import <UIKit/UIScrollView.h>
#import <WebCore/ScrollSnapOffsetsInfo.h>
#import <WebCore/ScrollingStateOverflowScrollingNode.h>
#import <WebCore/ScrollingTree.h>
#import <WebCore/ScrollingTreeFrameScrollingNode.h>
#import <WebCore/ScrollingTreeScrollingNode.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/SetForScope.h>

@interface WKScrollingNodeScrollViewDelegate () <WKBaseScrollViewDelegate>
@end

@implementation WKScrollingNodeScrollViewDelegate

- (instancetype)initWithScrollingTreeNodeDelegate:(WebKit::ScrollingTreeScrollingNodeDelegateIOS&)delegate
{
    if ((self = [super init]))
        _scrollingTreeNodeDelegate = delegate;

    return self;
}

#if !USE(BROWSERENGINEKIT)

- (UIScrollView *)_actingParentScrollViewForScrollView:(WKBaseScrollView *)scrollView
{
    return [self parentScrollViewForScrollView:scrollView];
}

#endif

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
    if (UNLIKELY(!_scrollingTreeNodeDelegate))
        return;

    _scrollingTreeNodeDelegate->scrollViewDidScroll(scrollView.contentOffset, _inUserInteraction);
}

- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
    if (UNLIKELY(!_scrollingTreeNodeDelegate))
        return;

    _inUserInteraction = YES;

    if (scrollView.panGestureRecognizer.state == UIGestureRecognizerStateBegan)
        _scrollingTreeNodeDelegate->scrollViewWillStartPanGesture();
    _scrollingTreeNodeDelegate->scrollWillStart();
}

- (void)scrollViewWillEndDragging:(UIScrollView *)scrollView withVelocity:(CGPoint)velocity targetContentOffset:(inout CGPoint *)targetContentOffset
{
    if (UNLIKELY(!_scrollingTreeNodeDelegate))
        return;

    if (![scrollView isZooming]) {
        auto touchActions = _scrollingTreeNodeDelegate->activeTouchActions();
        _scrollingTreeNodeDelegate->clearActiveTouchActions();
        
        if (touchActions && !touchActions.containsAny({ WebCore::TouchAction::Auto, WebCore::TouchAction::Manipulation })) {
            auto axesToPreventMomentumScrolling = dynamic_objc_cast<WKBaseScrollView>(scrollView).axesToPreventMomentumScrolling;
            if (!touchActions.contains(WebCore::TouchAction::PanX) || (axesToPreventMomentumScrolling & UIAxisHorizontal))
                targetContentOffset->x = scrollView.contentOffset.x;
            if (!touchActions.contains(WebCore::TouchAction::PanY) || (axesToPreventMomentumScrolling & UIAxisVertical))
                targetContentOffset->y = scrollView.contentOffset.y;
        }
    }

    std::optional<unsigned> originalHorizontalSnapPosition = _scrollingTreeNodeDelegate->scrollingNode().currentHorizontalSnapPointIndex();
    std::optional<unsigned> originalVerticalSnapPosition = _scrollingTreeNodeDelegate->scrollingNode().currentVerticalSnapPointIndex();

    WebCore::FloatSize viewportSize(static_cast<float>(CGRectGetWidth([scrollView bounds])), static_cast<float>(CGRectGetHeight([scrollView bounds])));
    const auto& snapOffsetsInfo = _scrollingTreeNodeDelegate->scrollingNode().snapOffsetsInfo();
    if (!snapOffsetsInfo.horizontalSnapOffsets.isEmpty()) {
        auto [potentialSnapPosition, index] = snapOffsetsInfo.closestSnapOffset(WebCore::ScrollEventAxis::Horizontal, viewportSize, WebCore::FloatPoint(*targetContentOffset), velocity.x, scrollView.contentOffset.x);
        _scrollingTreeNodeDelegate->scrollingNode().setCurrentHorizontalSnapPointIndex(index);
        if (targetContentOffset->x >= 0 && targetContentOffset->x <= scrollView.contentSize.width)
            targetContentOffset->x = potentialSnapPosition;
    }

    if (!snapOffsetsInfo.verticalSnapOffsets.isEmpty()) {
        auto [potentialSnapPosition, index] = snapOffsetsInfo.closestSnapOffset(WebCore::ScrollEventAxis::Vertical, viewportSize, WebCore::FloatPoint(*targetContentOffset), velocity.y, scrollView.contentOffset.y);
        _scrollingTreeNodeDelegate->scrollingNode().setCurrentVerticalSnapPointIndex(index);
        if (targetContentOffset->y >= 0 && targetContentOffset->y <= scrollView.contentSize.height)
            targetContentOffset->y = potentialSnapPosition;
    }

    if (originalHorizontalSnapPosition != _scrollingTreeNodeDelegate->scrollingNode().currentHorizontalSnapPointIndex()
        || originalVerticalSnapPosition != _scrollingTreeNodeDelegate->scrollingNode().currentVerticalSnapPointIndex()) {
        _scrollingTreeNodeDelegate->currentSnapPointIndicesDidChange(_scrollingTreeNodeDelegate->scrollingNode().currentHorizontalSnapPointIndex(), _scrollingTreeNodeDelegate->scrollingNode().currentVerticalSnapPointIndex());
    }
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)willDecelerate
{
    if (UNLIKELY(!_scrollingTreeNodeDelegate))
        return;

    if (_inUserInteraction && !willDecelerate) {
        _inUserInteraction = NO;
        _scrollingTreeNodeDelegate->scrollViewDidScroll(scrollView.contentOffset, _inUserInteraction);
        _scrollingTreeNodeDelegate->scrollDidEnd();
    }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
    if (UNLIKELY(!_scrollingTreeNodeDelegate))
        return;

    if (_inUserInteraction) {
        _inUserInteraction = NO;
        _scrollingTreeNodeDelegate->scrollViewDidScroll(scrollView.contentOffset, _inUserInteraction);
        _scrollingTreeNodeDelegate->scrollDidEnd();
    }
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView *)scrollView
{
    if (UNLIKELY(!_scrollingTreeNodeDelegate))
        return;

    _scrollingTreeNodeDelegate->scrollDidEnd();
}

- (void)scrollViewWillBeginZooming:(UIScrollView *)scrollView withView:(UIView *)view
{
    [self cancelPointersForGestureRecognizer:scrollView.pinchGestureRecognizer];
}

- (void)cancelPointersForGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
    if (UNLIKELY(!_scrollingTreeNodeDelegate))
        return;

    _scrollingTreeNodeDelegate->cancelPointersForGestureRecognizer(gestureRecognizer);
}

#pragma mark - WKBaseScrollViewDelegate

- (UIAxis)axesToPreventScrollingForPanGestureInScrollView:(WKBaseScrollView *)scrollView
{
    if (UNLIKELY(!_scrollingTreeNodeDelegate))
        return UIAxisNeither;

    auto panGestureRecognizer = scrollView.panGestureRecognizer;
    _scrollingTreeNodeDelegate->computeActiveTouchActionsForGestureRecognizer(panGestureRecognizer);

    auto touchActions = _scrollingTreeNodeDelegate->activeTouchActions();
    if (!touchActions) {
        [self cancelPointersForGestureRecognizer:panGestureRecognizer];
        return UIAxisNeither;
    }

    if (touchActions.containsAny({ WebCore::TouchAction::Auto, WebCore::TouchAction::Manipulation }))
        return UIAxisNeither;

    UIAxis axesToPrevent = UIAxisNeither;
    if (!touchActions.contains(WebCore::TouchAction::PanX))
        axesToPrevent |= UIAxisHorizontal;
    if (!touchActions.contains(WebCore::TouchAction::PanY))
        axesToPrevent |= UIAxisVertical;

    auto translation = [panGestureRecognizer translationInView:scrollView];
    if ((touchActions.contains(WebCore::TouchAction::PanX) && std::abs(translation.x) > CGFLOAT_EPSILON)
        || (touchActions.contains(WebCore::TouchAction::PanY) && std::abs(translation.y) > CGFLOAT_EPSILON))
        [self cancelPointersForGestureRecognizer:panGestureRecognizer];

    return axesToPrevent;
}

#pragma mark - WKBEScrollViewDelegate

- (WKBEScrollView *)parentScrollViewForScrollView:(WKBEScrollView *)scrollView
{
    if (UNLIKELY(!_scrollingTreeNodeDelegate))
        return nil;

    // An "acting parent" is a non-ancestor scrolling parent. We tell this to UIKit so it can propagate scrolls correctly.
    return dynamic_objc_cast<WKBEScrollView>(_scrollingTreeNodeDelegate->findActingScrollParent(scrollView));
}

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)

#if !USE(BROWSERENGINEKIT)

- (void)_scrollView:(WKChildScrollView *)scrollView asynchronouslyHandleScrollEvent:(WKBEScrollViewScrollUpdate *)scrollEvent completion:(void (^)(BOOL handled))completion
{
    [self scrollView:scrollView handleScrollUpdate:scrollEvent completion:completion];
}

#endif // !USE(BROWSERENGINEKIT)

- (void)scrollView:(WKBaseScrollView *)scrollView handleScrollUpdate:(WKBEScrollViewScrollUpdate *)update completion:(void (^)(BOOL handled))completion
{
    if (UNLIKELY(!_scrollingTreeNodeDelegate))
        return;

    _scrollingTreeNodeDelegate->handleAsynchronousCancelableScrollEvent(scrollView, update, completion);
}

#endif // HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)

@end

namespace WebKit {
using namespace WebCore;

ScrollingTreeScrollingNodeDelegateIOS::ScrollingTreeScrollingNodeDelegateIOS(ScrollingTreeScrollingNode& scrollingNode)
    : ScrollingTreeScrollingNodeDelegate(scrollingNode)
{
}

ScrollingTreeScrollingNodeDelegateIOS::~ScrollingTreeScrollingNodeDelegateIOS()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (UIScrollView *scrollView = (UIScrollView *)[scrollLayer() delegate]) {
        ASSERT([scrollView isKindOfClass:[UIScrollView class]]);
        // The scrollView may have been adopted by another node, so only clear the delegate if it's ours.
        if (scrollView.delegate == m_scrollViewDelegate.get())
            scrollView.delegate = nil;
    }
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollingTreeScrollingNodeDelegateIOS::resetScrollViewDelegate()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (UIScrollView *scrollView = (UIScrollView *)[scrollLayer() delegate]) {
        ASSERT([scrollView isKindOfClass:[UIScrollView class]]);
        scrollView.delegate = nil;
    }
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollingTreeScrollingNodeDelegateIOS::commitStateBeforeChildren(const ScrollingStateScrollingNode& scrollingStateNode)
{
    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer))
        m_scrollLayer = static_cast<CALayer*>(scrollingStateNode.scrollContainerLayer());
}

void ScrollingTreeScrollingNodeDelegateIOS::updateScrollViewForOverscrollBehavior(UIScrollView *scrollView, const WebCore::OverscrollBehavior horizontalOverscrollBehavior, WebCore::OverscrollBehavior verticalOverscrollBehavior, AllowOverscrollToPreventScrollPropagation allowPropogation)
{
    if ([scrollView isKindOfClass:[WKScrollView class]])
        [(WKScrollView*)scrollView _setBouncesInternal:horizontalOverscrollBehavior != WebCore::OverscrollBehavior::None vertical: verticalOverscrollBehavior != WebCore::OverscrollBehavior::None];
    else {
        scrollView.bouncesHorizontally = horizontalOverscrollBehavior != OverscrollBehavior::None;
        scrollView.bouncesVertically = verticalOverscrollBehavior != OverscrollBehavior::None;
    }
    if (allowPropogation == AllowOverscrollToPreventScrollPropagation::Yes) {
        [scrollView _wk_setTransfersHorizontalScrollingToParent:horizontalOverscrollBehavior == OverscrollBehavior::Auto];
        [scrollView _wk_setTransfersVerticalScrollingToParent:verticalOverscrollBehavior == OverscrollBehavior::Auto];
    }
}

void ScrollingTreeScrollingNodeDelegateIOS::commitStateAfterChildren(const ScrollingStateScrollingNode& scrollingStateNode)
{
    SetForScope updatingChange(m_updatingFromStateNode, true);

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer)
        || scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::TotalContentsSize)
        || scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ReachableContentsSize)
        || scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollPosition)
        || scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollOrigin)
        || scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollableAreaParams)) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        auto scrollView = this->scrollView();
        if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollContainerLayer)) {
            if (!m_scrollViewDelegate)
                m_scrollViewDelegate = adoptNS([[WKScrollingNodeScrollViewDelegate alloc] initWithScrollingTreeNodeDelegate:*this]);

            scrollView.scrollsToTop = NO;
            scrollView.delegate = m_scrollViewDelegate.get();
            scrollView.baseScrollViewDelegate = m_scrollViewDelegate.get();

            if ([scrollView respondsToSelector:@selector(_setAvoidsJumpOnInterruptedBounce:)]) {
                scrollView.tracksImmediatelyWhileDecelerating = NO;
                scrollView._avoidsJumpOnInterruptedBounce = YES;
            }
        }

        bool recomputeInsets = scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::TotalContentsSize);
        if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ReachableContentsSize)) {
            scrollView.contentSize = scrollingStateNode.reachableContentsSize();
            recomputeInsets = true;
        }
        if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollableAreaParams)) {
            auto params = scrollingStateNode.scrollableAreaParameters();
            updateScrollViewForOverscrollBehavior(scrollView, params.horizontalOverscrollBehavior, params.verticalOverscrollBehavior, AllowOverscrollToPreventScrollPropagation::Yes);
        }
        if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollOrigin))
            recomputeInsets = true;

        if (recomputeInsets) {
            UIEdgeInsets insets = UIEdgeInsetsMake(0, 0, 0, 0);
            // With RTL or bottom-to-top scrolling (non-zero origin), we need extra space on the left or top.
            if (scrollOrigin().x())
                insets.left = reachableContentsSize().width() - totalContentsSize().width();

            if (scrollOrigin().y())
                insets.top = reachableContentsSize().height() - totalContentsSize().height();

            scrollView.contentInset = insets;
        }
        END_BLOCK_OBJC_EXCEPTIONS
    }

    // FIXME: If only one axis snaps in 2D scrolling, the other axis will decelerate fast as well. Is this what we want?
    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::SnapOffsetsInfo)) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        scrollView().decelerationRate = scrollingNode().snapOffsetsInfo().horizontalSnapOffsets.size() || scrollingNode().snapOffsetsInfo().verticalSnapOffsets.size() ? UIScrollViewDecelerationRateFast : UIScrollViewDecelerationRateNormal;
        END_BLOCK_OBJC_EXCEPTIONS
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::ScrollableAreaParams)) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        UIScrollView *scrollView = this->scrollView();

        [scrollView setShowsHorizontalScrollIndicator:!(scrollingNode().horizontalNativeScrollbarVisibility() == NativeScrollbarVisibility::HiddenByStyle)];
        [scrollView setShowsVerticalScrollIndicator:!(scrollingNode().verticalNativeScrollbarVisibility() == NativeScrollbarVisibility::HiddenByStyle)];
        [scrollView setScrollEnabled:scrollingNode().canHaveScrollbars()];

        END_BLOCK_OBJC_EXCEPTIONS
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::Property::RequestedScrollPosition)) {
        scrollingNode().handleScrollPositionRequest(scrollingStateNode.requestedScrollData());
        scrollingTree().setNeedsApplyLayerPositionsAfterCommit();
    }
}

bool ScrollingTreeScrollingNodeDelegateIOS::startAnimatedScrollToPosition(FloatPoint scrollPosition)
{
    auto scrollOffset = ScrollableArea::scrollOffsetFromPosition(scrollPosition, toFloatSize(scrollOrigin()));

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [scrollView() setContentOffset:scrollOffset animated:YES];
    END_BLOCK_OBJC_EXCEPTIONS
    return true;
}

void ScrollingTreeScrollingNodeDelegateIOS::stopAnimatedScroll()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [scrollView() _wk_stopScrollingAndZooming];
    END_BLOCK_OBJC_EXCEPTIONS
}

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)
void ScrollingTreeScrollingNodeDelegateIOS::handleAsynchronousCancelableScrollEvent(WKBaseScrollView *scrollView, WKBEScrollViewScrollUpdate *update, void (^completion)(BOOL handled))
{
    auto* scrollingCoordinatorProxy = downcast<WebKit::RemoteScrollingTree>(scrollingTree()).scrollingCoordinatorProxy();
    if (scrollingCoordinatorProxy)
        scrollingCoordinatorProxy->webPageProxy().pageClient().handleAsynchronousCancelableScrollEvent(scrollView, update, completion);
}
#endif

void ScrollingTreeScrollingNodeDelegateIOS::repositionScrollingLayers()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (scrollView()._wk_isScrollAnimating)
ALLOW_DEPRECATED_DECLARATIONS_END
        return;

    [scrollView() setContentOffset:scrollingNode().currentScrollOffset()];
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollingTreeScrollingNodeDelegateIOS::scrollWillStart() const
{
    scrollingTree().scrollingTreeNodeWillStartScroll(scrollingNode().scrollingNodeID());
}

void ScrollingTreeScrollingNodeDelegateIOS::scrollDidEnd() const
{
    scrollingTree().scrollingTreeNodeDidEndScroll(scrollingNode().scrollingNodeID());
}

void ScrollingTreeScrollingNodeDelegateIOS::scrollViewWillStartPanGesture() const
{
    scrollingTree().scrollingTreeNodeWillStartPanGesture(scrollingNode().scrollingNodeID());
}

void ScrollingTreeScrollingNodeDelegateIOS::scrollViewDidScroll(const FloatPoint& scrollOffset, bool inUserInteraction)
{
    if (m_updatingFromStateNode)
        return;

    auto scrollPosition = ScrollableArea::scrollPositionFromOffset(scrollOffset, toFloatSize(scrollOrigin()));
    scrollingNode().wasScrolledByDelegatedScrolling(scrollPosition, { }, inUserInteraction ? ScrollingLayerPositionAction::Sync : ScrollingLayerPositionAction::Set);
}

void ScrollingTreeScrollingNodeDelegateIOS::currentSnapPointIndicesDidChange(std::optional<unsigned> horizontal, std::optional<unsigned> vertical) const
{
    if (m_updatingFromStateNode)
        return;

    scrollingTree().currentSnapPointIndicesDidChange(scrollingNode().scrollingNodeID(), horizontal, vertical);
}

WKBaseScrollView *ScrollingTreeScrollingNodeDelegateIOS::scrollView() const
{
    auto scrollView = dynamic_objc_cast<WKBaseScrollView>(scrollLayer().delegate);
    ASSERT(scrollView);
    return scrollView;
}

UIScrollView *ScrollingTreeScrollingNodeDelegateIOS::findActingScrollParent(UIScrollView *scrollView)
{
    ASSERT(scrollView == this->scrollView());

    auto* scrollingCoordinatorProxy = downcast<RemoteScrollingTree>(scrollingTree()).scrollingCoordinatorProxy();
    if (!scrollingCoordinatorProxy)
        return nil;

    auto host = scrollingCoordinatorProxy->layerTreeHost();
    if (!host)
        return nil;
    
    return WebKit::findActingScrollParent(scrollView, *host);
}

void ScrollingTreeScrollingNodeDelegateIOS::computeActiveTouchActionsForGestureRecognizer(UIGestureRecognizer* gestureRecognizer)
{
    auto* scrollingCoordinatorProxy = dynamicDowncast<RemoteScrollingCoordinatorProxyIOS>(downcast<RemoteScrollingTree>(scrollingTree()).scrollingCoordinatorProxy());
    if (!scrollingCoordinatorProxy)
        return;

    if (auto touchIdentifier = scrollingCoordinatorProxy->webPageProxy().pageClient().activeTouchIdentifierForGestureRecognizer(gestureRecognizer))
        m_activeTouchActions = scrollingCoordinatorProxy->activeTouchActionsForTouchIdentifier(*touchIdentifier);
}

void ScrollingTreeScrollingNodeDelegateIOS::cancelPointersForGestureRecognizer(UIGestureRecognizer* gestureRecognizer)
{
    auto* scrollingCoordinatorProxy = downcast<RemoteScrollingTree>(scrollingTree()).scrollingCoordinatorProxy();
    if (!scrollingCoordinatorProxy)
        return;

    scrollingCoordinatorProxy->webPageProxy().pageClient().cancelPointersForGestureRecognizer(gestureRecognizer);
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) && ENABLE(ASYNC_SCROLLING)
