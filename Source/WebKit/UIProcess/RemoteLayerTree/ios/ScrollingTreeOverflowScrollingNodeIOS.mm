/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "ScrollingTreeOverflowScrollingNodeIOS.h"

#if PLATFORM(IOS)
#if ENABLE(ASYNC_SCROLLING)

#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIPanGestureRecognizer.h>
#import <UIKit/UIScrollView.h>
#import <WebCore/ScrollingStateOverflowScrollingNode.h>
#import <WebCore/ScrollingTree.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/SetForScope.h>

#if ENABLE(CSS_SCROLL_SNAP)
#import <WebCore/AxisScrollSnapOffsets.h>
#import <WebCore/ScrollSnapOffsetsInfo.h>
#endif

#import "ScrollingTreeScrollingNodeDelegateIOS.h"

using namespace WebCore;

@interface WKOverflowScrollViewDelegate : NSObject <UIScrollViewDelegate> {
    WebKit::ScrollingTreeScrollingNodeDelegateIOS* _scrollingTreeNodeDelegate;
}

@property (nonatomic, getter=_isInUserInteraction) BOOL inUserInteraction;

- (instancetype)initWithScrollingTreeNodeDelegate:(WebKit::ScrollingTreeScrollingNodeDelegateIOS*)delegate;

@end

@implementation WKOverflowScrollViewDelegate

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
        float potentialSnapPosition = closestSnapOffset(_scrollingTreeNodeDelegate->scrollingNode().horizontalSnapOffsets(), _scrollingTreeNodeDelegate->scrollingNode().horizontalSnapOffsetRanges(), horizontalTarget, velocity.x, index);
        _scrollingTreeNodeDelegate->scrollingNode().setCurrentHorizontalSnapPointIndex(index);
        if (horizontalTarget >= 0 && horizontalTarget <= scrollView.contentSize.width)
            targetContentOffset->x = potentialSnapPosition;
    }

    if (!_scrollingTreeNodeDelegate->scrollingNode().verticalSnapOffsets().isEmpty()) {
        unsigned index;
        float potentialSnapPosition = closestSnapOffset(_scrollingTreeNodeDelegate->scrollingNode().verticalSnapOffsets(), _scrollingTreeNodeDelegate->scrollingNode().verticalSnapOffsetRanges(), verticalTarget, velocity.y, index);
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

Ref<ScrollingTreeOverflowScrollingNodeIOS> ScrollingTreeOverflowScrollingNodeIOS::create(WebCore::ScrollingTree& scrollingTree, WebCore::ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeOverflowScrollingNodeIOS(scrollingTree, nodeID));
}

ScrollingTreeOverflowScrollingNodeIOS::ScrollingTreeOverflowScrollingNodeIOS(WebCore::ScrollingTree& scrollingTree, WebCore::ScrollingNodeID nodeID)
    : ScrollingTreeOverflowScrollingNode(scrollingTree, nodeID)
    , m_scrollingNodeDelegate(std::make_unique<ScrollingTreeScrollingNodeDelegateIOS>(*this))
{
}

ScrollingTreeOverflowScrollingNodeIOS::~ScrollingTreeOverflowScrollingNodeIOS()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (UIScrollView *scrollView = (UIScrollView *)[scrollLayer() delegate]) {
        ASSERT([scrollView isKindOfClass:[UIScrollView self]]);
        // The scrollView may have been adopted by another node, so only clear the delegate if it's ours.
        if (scrollView.delegate == m_scrollViewDelegate.get())
            scrollView.delegate = nil;
    }
    END_BLOCK_OBJC_EXCEPTIONS

    // WKOverflowScrollViewDelegate holds a pointer to ScrollingTreeScrollingNodeDelegateIOS, so we ensure it is destroyed first.
    m_scrollViewDelegate.clear();
    m_scrollingNodeDelegate.reset();
}

void ScrollingTreeOverflowScrollingNodeIOS::commitStateBeforeChildren(const WebCore::ScrollingStateNode& stateNode)
{
    if (stateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollLayer)) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        if (UIScrollView *scrollView = (UIScrollView *)[scrollLayer() delegate]) {
            ASSERT([scrollView isKindOfClass:[UIScrollView self]]);
            scrollView.delegate = nil;
        }
        END_BLOCK_OBJC_EXCEPTIONS
    }

    ScrollingTreeOverflowScrollingNode::commitStateBeforeChildren(stateNode);

    const auto& scrollingStateNode = downcast<ScrollingStateOverflowScrollingNode>(stateNode);
    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer))
        m_scrollLayer = scrollingStateNode.layer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
        m_scrolledContentsLayer = scrollingStateNode.scrolledContentsLayer();
}

void ScrollingTreeOverflowScrollingNodeIOS::commitStateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeOverflowScrollingNode::commitStateAfterChildren(stateNode);

    SetForScope<bool> updatingChange(m_scrollingNodeDelegate->m_updatingFromStateNode, true);

    const auto& scrollingStateNode = downcast<ScrollingStateOverflowScrollingNode>(stateNode);

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
                m_scrollViewDelegate = adoptNS([[WKOverflowScrollViewDelegate alloc] initWithScrollingTreeNodeDelegate:m_scrollingNodeDelegate.get()]);

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
            scrollView.decelerationRate = horizontalSnapOffsets().size() || verticalSnapOffsets().size() ? UIScrollViewDecelerationRateFast : UIScrollViewDecelerationRateNormal;
#endif
        END_BLOCK_OBJC_EXCEPTIONS
    }
}

void ScrollingTreeOverflowScrollingNodeIOS::updateLayersAfterAncestorChange(const ScrollingTreeNode& changedNode, const FloatRect& fixedPositionRect, const FloatSize& cumulativeDelta)
{
    if (!m_children)
        return;

    FloatSize scrollDelta = lastCommittedScrollPosition() - scrollPosition();

    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(changedNode, fixedPositionRect, cumulativeDelta + scrollDelta);
}

FloatPoint ScrollingTreeOverflowScrollingNodeIOS::scrollPosition() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    UIScrollView *scrollView = (UIScrollView *)[scrollLayer() delegate];
    ASSERT([scrollView isKindOfClass:[UIScrollView self]]);
    return [scrollView contentOffset];
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollingTreeOverflowScrollingNodeIOS::setScrollLayerPosition(const FloatPoint& scrollPosition, const FloatRect&)
{
    [m_scrollLayer setPosition:CGPointMake(-scrollPosition.x() + scrollOrigin().x(), -scrollPosition.y() + scrollOrigin().y())];

    m_scrollingNodeDelegate->updateChildNodesAfterScroll(scrollPosition);
}

void ScrollingTreeOverflowScrollingNodeIOS::updateLayersAfterDelegatedScroll(const FloatPoint& scrollPosition)
{
    m_scrollingNodeDelegate->updateChildNodesAfterScroll(scrollPosition);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
#endif // PLATFORM(IOS)
