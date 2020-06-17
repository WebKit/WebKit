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
#import "RemoteScrollingCoordinatorProxy.h"
#import "RemoteScrollingTree.h"
#import "UIKitSPI.h"
#import "WebPageProxy.h"
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIPanGestureRecognizer.h>
#import <UIKit/UIScrollView.h>
#import <WebCore/ScrollingStateOverflowScrollingNode.h>
#import <WebCore/ScrollingTree.h>
#import <WebCore/ScrollingTreeFrameScrollingNode.h>
#import <WebCore/ScrollingTreeScrollingNode.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/SetForScope.h>

#if ENABLE(CSS_SCROLL_SNAP)
#import <WebCore/AxisScrollSnapOffsets.h>
#import <WebCore/ScrollSnapOffsetsInfo.h>
#endif


@implementation WKScrollingNodeScrollViewDelegate

- (instancetype)initWithScrollingTreeNodeDelegate:(WebKit::ScrollingTreeScrollingNodeDelegateIOS*)delegate
{
    if ((self = [super init]))
        _scrollingTreeNodeDelegate = delegate;

    return self;
}

- (UIScrollView *)_actingParentScrollViewForScrollView:(UIScrollView *)scrollView
{
    // An "acting parent" is a non-ancestor scrolling parent. We tell this to UIKit so it can propagate scrolls correctly.
    return _scrollingTreeNodeDelegate->findActingScrollParent(scrollView);
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
    _scrollingTreeNodeDelegate->scrollViewDidScroll(scrollView.contentOffset, _inUserInteraction);
}

- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
    _inUserInteraction = YES;

    if (scrollView.panGestureRecognizer.state == UIGestureRecognizerStateBegan)
        _scrollingTreeNodeDelegate->scrollViewWillStartPanGesture();
    _scrollingTreeNodeDelegate->scrollWillStart();
}

- (void)scrollViewWillEndDragging:(UIScrollView *)scrollView withVelocity:(CGPoint)velocity targetContentOffset:(inout CGPoint *)targetContentOffset
{
    if (![scrollView isZooming]) {
        auto touchActions = _scrollingTreeNodeDelegate->activeTouchActions();
        _scrollingTreeNodeDelegate->clearActiveTouchActions();
        
        if (touchActions && !touchActions.containsAny({ WebCore::TouchAction::Auto, WebCore::TouchAction::Manipulation })) {
            bool canPanX = true;
            bool canPanY = true;
            if (!touchActions.contains(WebCore::TouchAction::PanX)) {
                canPanX = false;
                targetContentOffset->x = scrollView.contentOffset.x;
            }
            if (!touchActions.contains(WebCore::TouchAction::PanY)) {
                canPanY = false;
                targetContentOffset->y = scrollView.contentOffset.y;
            }
        }
    }

#if ENABLE(CSS_SCROLL_SNAP)
    CGFloat horizontalTarget = targetContentOffset->x;
    CGFloat verticalTarget = targetContentOffset->y;

    unsigned originalHorizontalSnapPosition = _scrollingTreeNodeDelegate->scrollingNode().currentHorizontalSnapPointIndex();
    unsigned originalVerticalSnapPosition = _scrollingTreeNodeDelegate->scrollingNode().currentVerticalSnapPointIndex();

    if (!_scrollingTreeNodeDelegate->scrollingNode().horizontalSnapOffsets().isEmpty()) {
        unsigned index;
        float potentialSnapPosition = WebCore::closestSnapOffset(_scrollingTreeNodeDelegate->scrollingNode().horizontalSnapOffsets(), _scrollingTreeNodeDelegate->scrollingNode().horizontalSnapOffsetRanges(), horizontalTarget, velocity.x, index);
        _scrollingTreeNodeDelegate->scrollingNode().setCurrentHorizontalSnapPointIndex(index);
        if (horizontalTarget >= 0 && horizontalTarget <= scrollView.contentSize.width)
            targetContentOffset->x = potentialSnapPosition;
    }

    if (!_scrollingTreeNodeDelegate->scrollingNode().verticalSnapOffsets().isEmpty()) {
        unsigned index;
        float potentialSnapPosition = WebCore::closestSnapOffset(_scrollingTreeNodeDelegate->scrollingNode().verticalSnapOffsets(), _scrollingTreeNodeDelegate->scrollingNode().verticalSnapOffsetRanges(), verticalTarget, velocity.y, index);
        _scrollingTreeNodeDelegate->scrollingNode().setCurrentVerticalSnapPointIndex(index);
        if (verticalTarget >= 0 && verticalTarget <= scrollView.contentSize.height)
            targetContentOffset->y = potentialSnapPosition;
    }

    if (originalHorizontalSnapPosition != _scrollingTreeNodeDelegate->scrollingNode().currentHorizontalSnapPointIndex()
        || originalVerticalSnapPosition != _scrollingTreeNodeDelegate->scrollingNode().currentVerticalSnapPointIndex()) {
        _scrollingTreeNodeDelegate->currentSnapPointIndicesDidChange(_scrollingTreeNodeDelegate->scrollingNode().currentHorizontalSnapPointIndex(), _scrollingTreeNodeDelegate->scrollingNode().currentVerticalSnapPointIndex());
    }
#endif
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)willDecelerate
{
    if (_inUserInteraction && !willDecelerate) {
        _inUserInteraction = NO;
        _scrollingTreeNodeDelegate->scrollViewDidScroll(scrollView.contentOffset, _inUserInteraction);
        _scrollingTreeNodeDelegate->scrollDidEnd();
    }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
    if (_inUserInteraction) {
        _inUserInteraction = NO;
        _scrollingTreeNodeDelegate->scrollViewDidScroll(scrollView.contentOffset, _inUserInteraction);
        _scrollingTreeNodeDelegate->scrollDidEnd();
    }
}

- (CGPoint)_scrollView:(UIScrollView *)scrollView adjustedOffsetForOffset:(CGPoint)offset translation:(CGPoint)translation startPoint:(CGPoint)start locationInView:(CGPoint)locationInView horizontalVelocity:(inout double *)hv verticalVelocity:(inout double *)vv
{
    auto* panGestureRecognizer = scrollView.panGestureRecognizer;
    _scrollingTreeNodeDelegate->computeActiveTouchActionsForGestureRecognizer(panGestureRecognizer);
    auto touchActions = _scrollingTreeNodeDelegate->activeTouchActions();

    if (!touchActions) {
        [self cancelPointersForGestureRecognizer:panGestureRecognizer];
        return offset;
    }

    if (touchActions.containsAny({ WebCore::TouchAction::Auto, WebCore::TouchAction::Manipulation }))
        return offset;

    CGPoint adjustedContentOffset = CGPointMake(offset.x, offset.y);

    if (!touchActions.contains(WebCore::TouchAction::PanX))
        adjustedContentOffset.x = start.x;
    if (!touchActions.contains(WebCore::TouchAction::PanY))
        adjustedContentOffset.y = start.y;

    if ((touchActions.contains(WebCore::TouchAction::PanX) && adjustedContentOffset.x != start.x)
        || (touchActions.contains(WebCore::TouchAction::PanY) && adjustedContentOffset.y != start.y)) {
        [self cancelPointersForGestureRecognizer:scrollView.panGestureRecognizer];
    }

    return adjustedContentOffset;
}

- (void)scrollViewWillBeginZooming:(UIScrollView *)scrollView withView:(UIView *)view
{
    [self cancelPointersForGestureRecognizer:scrollView.pinchGestureRecognizer];
}

- (void)cancelPointersForGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
    _scrollingTreeNodeDelegate->cancelPointersForGestureRecognizer(gestureRecognizer);
}

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
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollContainerLayer))
        m_scrollLayer = static_cast<CALayer*>(scrollingStateNode.scrollContainerLayer());
}

void ScrollingTreeScrollingNodeDelegateIOS::commitStateAfterChildren(const ScrollingStateScrollingNode& scrollingStateNode)
{
    SetForScope<bool> updatingChange(m_updatingFromStateNode, true);

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollContainerLayer)
        || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::TotalContentsSize)
        || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ReachableContentsSize)
        || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollPosition)
        || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollOrigin)) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        UIScrollView *scrollView = this->scrollView();
        if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollContainerLayer)) {
            if (!m_scrollViewDelegate)
                m_scrollViewDelegate = adoptNS([[WKScrollingNodeScrollViewDelegate alloc] initWithScrollingTreeNodeDelegate:this]);

            scrollView.scrollsToTop = NO;
            scrollView.delegate = m_scrollViewDelegate.get();

            if ([scrollView respondsToSelector:@selector(_setAvoidsJumpOnInterruptedBounce:)]) {
                scrollView.tracksImmediatelyWhileDecelerating = NO;
                scrollView._avoidsJumpOnInterruptedBounce = YES;
            }
        }

        bool recomputeInsets = scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::TotalContentsSize);
        if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ReachableContentsSize)) {
            scrollView.contentSize = scrollingStateNode.reachableContentsSize();
            recomputeInsets = true;
        }

        if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollOrigin))
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

#if ENABLE(CSS_SCROLL_SNAP)
    // FIXME: If only one axis snaps in 2D scrolling, the other axis will decelerate fast as well. Is this what we want?
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::HorizontalSnapOffsets) || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::VerticalSnapOffsets)) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        scrollView().decelerationRate = scrollingNode().horizontalSnapOffsets().size() || scrollingNode().verticalSnapOffsets().size() ? UIScrollViewDecelerationRateFast : UIScrollViewDecelerationRateNormal;
        END_BLOCK_OBJC_EXCEPTIONS
    }
#endif

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollableAreaParams)) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        UIScrollView *scrollView = this->scrollView();

        [scrollView setShowsHorizontalScrollIndicator:!scrollingNode().horizontalScrollbarHiddenByStyle()];
        [scrollView setShowsVerticalScrollIndicator:!scrollingNode().verticalScrollbarHiddenByStyle()];
        [scrollView setScrollEnabled:scrollingNode().canHaveScrollbars()];

        END_BLOCK_OBJC_EXCEPTIONS
    }

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition)) {
        const auto& requestedScrollData = scrollingStateNode.requestedScrollData();
        scrollingNode().scrollTo(requestedScrollData.scrollPosition, requestedScrollData.scrollType, requestedScrollData.clamping);
        scrollingTree().setNeedsApplyLayerPositionsAfterCommit();
    }
}

void ScrollingTreeScrollingNodeDelegateIOS::repositionScrollingLayers()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [scrollView() setContentOffset:scrollingNode().currentScrollOffset()];
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollingTreeScrollingNodeDelegateIOS::scrollWillStart() const
{
    scrollingTree().scrollingTreeNodeWillStartScroll();
}

void ScrollingTreeScrollingNodeDelegateIOS::scrollDidEnd() const
{
    scrollingTree().scrollingTreeNodeDidEndScroll();
}

void ScrollingTreeScrollingNodeDelegateIOS::scrollViewWillStartPanGesture() const
{
    scrollingTree().scrollingTreeNodeWillStartPanGesture();
}

void ScrollingTreeScrollingNodeDelegateIOS::scrollViewDidScroll(const FloatPoint& scrollOffset, bool inUserInteraction)
{
    if (m_updatingFromStateNode)
        return;

    auto scrollPosition = ScrollableArea::scrollPositionFromOffset(scrollOffset, toFloatSize(scrollOrigin()));
    scrollingNode().wasScrolledByDelegatedScrolling(scrollPosition, { }, inUserInteraction ? ScrollingLayerPositionAction::Sync : ScrollingLayerPositionAction::Set);
}

void ScrollingTreeScrollingNodeDelegateIOS::currentSnapPointIndicesDidChange(unsigned horizontal, unsigned vertical) const
{
    if (m_updatingFromStateNode)
        return;

    scrollingTree().currentSnapPointIndicesDidChange(scrollingNode().scrollingNodeID(), horizontal, vertical);
}

UIScrollView *ScrollingTreeScrollingNodeDelegateIOS::scrollView() const
{
    UIScrollView *scrollView = (UIScrollView *)[scrollLayer() delegate];
    ASSERT([scrollView isKindOfClass:[UIScrollView class]]);
    return scrollView;
}

UIScrollView *ScrollingTreeScrollingNodeDelegateIOS::findActingScrollParent(UIScrollView *scrollView)
{
    ASSERT(scrollView == this->scrollView());

    auto& scrollingCoordinatorProxy = downcast<RemoteScrollingTree>(scrollingTree()).scrollingCoordinatorProxy();
    return WebKit::findActingScrollParent(scrollView, *scrollingCoordinatorProxy.layerTreeHost());
}

void ScrollingTreeScrollingNodeDelegateIOS::computeActiveTouchActionsForGestureRecognizer(UIGestureRecognizer* gestureRecognizer)
{
    auto& scrollingCoordinatorProxy = downcast<RemoteScrollingTree>(scrollingTree()).scrollingCoordinatorProxy();
    if (auto touchIdentifier = scrollingCoordinatorProxy.webPageProxy().pageClient().activeTouchIdentifierForGestureRecognizer(gestureRecognizer))
        m_activeTouchActions = scrollingCoordinatorProxy.activeTouchActionsForTouchIdentifier(*touchIdentifier);
}

void ScrollingTreeScrollingNodeDelegateIOS::cancelPointersForGestureRecognizer(UIGestureRecognizer* gestureRecognizer)
{
    downcast<RemoteScrollingTree>(scrollingTree()).scrollingCoordinatorProxy().webPageProxy().pageClient().cancelPointersForGestureRecognizer(gestureRecognizer);
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) && ENABLE(ASYNC_SCROLLING)
