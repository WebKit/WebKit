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
#import <WebCore/BlockExceptions.h>
#import <WebCore/ScrollingStateOverflowScrollingNode.h>
#import <WebCore/ScrollingTree.h>
#import <UIKit/UIPanGestureRecognizer.h>
#import <UIKit/UIScrollView.h>
#import <wtf/TemporaryChange.h>

using namespace WebCore;

@interface WKOverflowScrollViewDelegate : NSObject <UIScrollViewDelegate> {
    WebKit::ScrollingTreeOverflowScrollingNodeIOS* _scrollingTreeNode;
}

@property (nonatomic, getter=_isInUserInteraction) BOOL inUserInteraction;

- (instancetype)initWithScrollingTreeNode:(WebKit::ScrollingTreeOverflowScrollingNodeIOS*)node;

@end

@implementation WKOverflowScrollViewDelegate

- (instancetype)initWithScrollingTreeNode:(WebKit::ScrollingTreeOverflowScrollingNodeIOS*)node
{
    if ((self = [super init]))
        _scrollingTreeNode = node;

    return self;
}

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
    _scrollingTreeNode->scrollViewDidScroll(scrollView.contentOffset, _inUserInteraction);
}

- (void)scrollViewWillBeginDragging:(UIScrollView *)scrollView
{
    _inUserInteraction = YES;

    if (scrollView.panGestureRecognizer.state == UIGestureRecognizerStateBegan)
        _scrollingTreeNode->overflowScrollViewWillStartPanGesture();
    _scrollingTreeNode->overflowScrollWillStart();
}

- (void)scrollViewDidEndDragging:(UIScrollView *)scrollView willDecelerate:(BOOL)willDecelerate
{
    if (_inUserInteraction && !willDecelerate) {
        _inUserInteraction = NO;
        _scrollingTreeNode->scrollViewDidScroll(scrollView.contentOffset, _inUserInteraction);
        _scrollingTreeNode->overflowScrollDidEnd();
    }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView
{
    if (_inUserInteraction) {
        _inUserInteraction = NO;
        _scrollingTreeNode->scrollViewDidScroll(scrollView.contentOffset, _inUserInteraction);
        _scrollingTreeNode->overflowScrollDidEnd();
    }
}

@end

namespace WebKit {

PassRefPtr<ScrollingTreeOverflowScrollingNodeIOS> ScrollingTreeOverflowScrollingNodeIOS::create(WebCore::ScrollingTree& scrollingTree, WebCore::ScrollingNodeID nodeID)
{
    return adoptRef(new ScrollingTreeOverflowScrollingNodeIOS(scrollingTree, nodeID));
}

ScrollingTreeOverflowScrollingNodeIOS::ScrollingTreeOverflowScrollingNodeIOS(WebCore::ScrollingTree& scrollingTree, WebCore::ScrollingNodeID nodeID)
    : ScrollingTreeOverflowScrollingNode(scrollingTree, nodeID)
    , m_updatingFromStateNode(false)
{
}

ScrollingTreeOverflowScrollingNodeIOS::~ScrollingTreeOverflowScrollingNodeIOS()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (UIScrollView *scrollView = (UIScrollView *)[scrollLayer() delegate]) {
        ASSERT([scrollView isKindOfClass:[UIScrollView self]]);
        scrollView.delegate = nil;
    }
    END_BLOCK_OBJC_EXCEPTIONS
}

void ScrollingTreeOverflowScrollingNodeIOS::updateBeforeChildren(const WebCore::ScrollingStateNode& stateNode)
{
    if (stateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrollLayer)) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        if (UIScrollView *scrollView = (UIScrollView *)[scrollLayer() delegate]) {
            ASSERT([scrollView isKindOfClass:[UIScrollView self]]);
            scrollView.delegate = nil;
        }
        END_BLOCK_OBJC_EXCEPTIONS
    }

    ScrollingTreeOverflowScrollingNode::updateBeforeChildren(stateNode);

    const auto& scrollingStateNode = toScrollingStateOverflowScrollingNode(stateNode);
    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer))
        m_scrollLayer = scrollingStateNode.layer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateOverflowScrollingNode::ScrolledContentsLayer))
        m_scrolledContentsLayer = scrollingStateNode.scrolledContentsLayer();
}

void ScrollingTreeOverflowScrollingNodeIOS::updateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeOverflowScrollingNode::updateAfterChildren(stateNode);

    TemporaryChange<bool> updatingChange(m_updatingFromStateNode, true);

    const auto& scrollingStateNode = toScrollingStateOverflowScrollingNode(stateNode);

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
                m_scrollViewDelegate = adoptNS([[WKOverflowScrollViewDelegate alloc] initWithScrollingTreeNode:this]);

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

void ScrollingTreeOverflowScrollingNodeIOS::setScrollLayerPosition(const FloatPoint& scrollPosition)
{
    [m_scrollLayer setPosition:CGPointMake(-scrollPosition.x() + scrollOrigin().x(), -scrollPosition.y() + scrollOrigin().y())];

    updateChildNodesAfterScroll(scrollPosition);
}

void ScrollingTreeOverflowScrollingNodeIOS::updateLayersAfterDelegatedScroll(const FloatPoint& scrollPosition)
{
    updateChildNodesAfterScroll(scrollPosition);
}

void ScrollingTreeOverflowScrollingNodeIOS::updateChildNodesAfterScroll(const FloatPoint& scrollPosition)
{
    if (!m_children)
        return;

    FloatRect fixedPositionRect = scrollingTree().fixedPositionRect();
    FloatSize scrollDelta = lastCommittedScrollPosition() - scrollPosition;

    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(*this, fixedPositionRect, scrollDelta);
}

void ScrollingTreeOverflowScrollingNodeIOS::overflowScrollWillStart()
{
    scrollingTree().scrollingTreeNodeWillStartScroll();
}

void ScrollingTreeOverflowScrollingNodeIOS::overflowScrollDidEnd()
{
    scrollingTree().scrollingTreeNodeDidEndScroll();
}

void ScrollingTreeOverflowScrollingNodeIOS::overflowScrollViewWillStartPanGesture()
{
    scrollingTree().scrollingTreeNodeWillStartPanGesture();
}

void ScrollingTreeOverflowScrollingNodeIOS::scrollViewDidScroll(const FloatPoint& scrollPosition, bool inUserInteration)
{
    if (m_updatingFromStateNode)
        return;

    scrollingTree().scrollPositionChangedViaDelegatedScrolling(scrollingNodeID(), scrollPosition, inUserInteration);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
#endif // PLATFORM(IOS)
