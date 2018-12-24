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
#import "ScrollingTreeScrollingNodeDelegateIOS.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(ASYNC_SCROLLING)

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

#if ENABLE(CSS_SCROLL_SNAP)
- (void)scrollViewWillEndDragging:(UIScrollView *)scrollView withVelocity:(CGPoint)velocity targetContentOffset:(inout CGPoint *)targetContentOffset
{
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
}
#endif

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

@end

namespace WebKit {
using namespace WebCore;

ScrollingTreeScrollingNodeDelegateIOS::ScrollingTreeScrollingNodeDelegateIOS(ScrollingTreeScrollingNode& scrollingNode)
    : ScrollingTreeScrollingNodeDelegate(scrollingNode)
    , m_updatingFromStateNode(false)
{
}

ScrollingTreeScrollingNodeDelegateIOS::~ScrollingTreeScrollingNodeDelegateIOS()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (UIScrollView *scrollView = (UIScrollView *)[scrollLayer() delegate]) {
        ASSERT([scrollView isKindOfClass:[UIScrollView self]]);
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
        ASSERT([scrollView isKindOfClass:[UIScrollView self]]);
        scrollView.delegate = nil;
    }
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollingTreeScrollingNodeDelegateIOS::commitStateBeforeChildren(const ScrollingStateScrollingNode& scrollingStateNode)
{
    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer))
        m_scrollLayer = scrollingStateNode.layer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
        m_scrolledContentsLayer = scrollingStateNode.scrolledContentsLayer();
}

void ScrollingTreeScrollingNodeDelegateIOS::commitStateAfterChildren(const ScrollingStateScrollingNode& scrollingStateNode)
{
    SetForScope<bool> updatingChange(m_updatingFromStateNode, true);
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollLayer)
        || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::TotalContentsSize)
        || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ReachableContentsSize)
        || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollPosition)
        || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollOrigin)) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        UIScrollView *scrollView = (UIScrollView *)[scrollLayer() delegate];
        ASSERT([scrollView isKindOfClass:[UIScrollView self]]);

        if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollLayer)) {
            if (!m_scrollViewDelegate)
                m_scrollViewDelegate = adoptNS([[WKScrollingNodeScrollViewDelegate alloc] initWithScrollingTreeNodeDelegate:this]);

            scrollView.scrollsToTop = NO;
            scrollView.delegate = m_scrollViewDelegate.get();
        }

        bool recomputeInsets = scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::TotalContentsSize);
        if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ReachableContentsSize)) {
            scrollView.contentSize = scrollingStateNode.reachableContentsSize();
            recomputeInsets = true;
        }

        if ((scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollPosition)
            || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollOrigin))
            && ![m_scrollViewDelegate _isInUserInteraction]) {
            scrollView.contentOffset = scrollingStateNode.scrollPosition() + scrollOrigin();
            recomputeInsets = true;
        }

        if (recomputeInsets) {
            UIEdgeInsets insets = UIEdgeInsetsMake(0, 0, 0, 0);
            // With RTL or bottom-to-top scrolling (non-zero origin), we need extra space on the left or top.
            if (scrollOrigin().x())
                insets.left = reachableContentsSize().width() - totalContentsSize().width();

            if (scrollOrigin().y())
                insets.top = reachableContentsSize().height() - totalContentsSize().height();

            scrollView.contentInset = insets;
        }

#if ENABLE(CSS_SCROLL_SNAP)
        // FIXME: If only one axis snaps in 2D scrolling, the other axis will decelerate fast as well. Is this what we want?
        if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::HorizontalSnapOffsets) || scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::VerticalSnapOffsets))
            scrollView.decelerationRate = scrollingNode().horizontalSnapOffsets().size() || scrollingNode().verticalSnapOffsets().size() ? UIScrollViewDecelerationRateFast : UIScrollViewDecelerationRateNormal;
#endif
        END_BLOCK_OBJC_EXCEPTIONS
    }
}

void ScrollingTreeScrollingNodeDelegateIOS::updateLayersAfterAncestorChange(const ScrollingTreeNode& changedNode, const FloatRect& fixedPositionRect, const FloatSize& cumulativeDelta)
{
    if (!scrollingNode().children())
        return;

    FloatSize scrollDelta = lastCommittedScrollPosition() - scrollingNode().scrollPosition();

    for (auto& child : *scrollingNode().children())
        child->updateLayersAfterAncestorChange(changedNode, fixedPositionRect, cumulativeDelta + scrollDelta);
}

FloatPoint ScrollingTreeScrollingNodeDelegateIOS::scrollPosition() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    UIScrollView *scrollView = (UIScrollView *)[scrollLayer() delegate];
    ASSERT([scrollView isKindOfClass:[UIScrollView self]]);
    return [scrollView contentOffset];
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollingTreeScrollingNodeDelegateIOS::setScrollLayerPosition(const FloatPoint& scrollPosition)
{
    [m_scrollLayer setPosition:CGPointMake(scrollPosition.x() + scrollOrigin().x(), scrollPosition.y() + scrollOrigin().y())];
    updateChildNodesAfterScroll(scrollPosition);
}

void ScrollingTreeScrollingNodeDelegateIOS::updateChildNodesAfterScroll(const FloatPoint& scrollPosition)
{
    if (!scrollingNode().children())
        return;

    FloatRect fixedPositionRect;
    auto* frameNode = scrollingNode().enclosingFrameNodeIncludingSelf();
    if (frameNode && frameNode->nodeType() == ScrollingNodeType::Subframe)
        fixedPositionRect = frameNode->fixedPositionRect();
    else
        fixedPositionRect = scrollingTree().fixedPositionRect();
    FloatSize scrollDelta = lastCommittedScrollPosition() - scrollPosition;

    for (auto& child : *scrollingNode().children())
        child->updateLayersAfterAncestorChange(scrollingNode(), fixedPositionRect, scrollDelta);
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

void ScrollingTreeScrollingNodeDelegateIOS::scrollViewDidScroll(const FloatPoint& scrollPosition, bool inUserInteraction) const
{
    if (m_updatingFromStateNode)
        return;

    scrollingTree().scrollPositionChangedViaDelegatedScrolling(scrollingNode().scrollingNodeID(), scrollPosition, inUserInteraction);
}

void ScrollingTreeScrollingNodeDelegateIOS::currentSnapPointIndicesDidChange(unsigned horizontal, unsigned vertical) const
{
    if (m_updatingFromStateNode)
        return;

    scrollingTree().currentSnapPointIndicesDidChange(scrollingNode().scrollingNodeID(), horizontal, vertical);
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) && ENABLE(ASYNC_SCROLLING)
