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
#import "ScrollingTreeOverflowScrollingNodeMac.h"

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

#import "Logging.h"
#import "ScrollingStateOverflowScrollingNode.h"
#import "ScrollingTree.h"
#import <wtf/text/TextStream.h>

namespace WebCore {

Ref<ScrollingTreeOverflowScrollingNodeMac> ScrollingTreeOverflowScrollingNodeMac::create(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingTreeOverflowScrollingNodeMac(scrollingTree, nodeID));
}

ScrollingTreeOverflowScrollingNodeMac::ScrollingTreeOverflowScrollingNodeMac(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeOverflowScrollingNode(scrollingTree, nodeID)
    , m_delegate(*this)
{
}

ScrollingTreeOverflowScrollingNodeMac::~ScrollingTreeOverflowScrollingNodeMac()
{
}

void ScrollingTreeOverflowScrollingNodeMac::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeOverflowScrollingNode::commitStateBeforeChildren(stateNode);
    const auto& scrollingStateNode = downcast<ScrollingStateOverflowScrollingNode>(stateNode);

    if (scrollingStateNode.hasChangedProperty(ScrollingStateNode::ScrollLayer))
        m_scrollLayer = scrollingStateNode.layer();

    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::ScrolledContentsLayer))
        m_scrolledContentsLayer = scrollingStateNode.scrolledContentsLayer();

    // FIXME: Scroll snap data.
}

void ScrollingTreeOverflowScrollingNodeMac::commitStateAfterChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeOverflowScrollingNode::commitStateAfterChildren(stateNode);
    
    // FIXME: RequestedScrollPosition etc.
}

ScrollingEventResult ScrollingTreeOverflowScrollingNodeMac::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (!canHaveScrollbars())
        return ScrollingEventResult::DidNotHandleEvent;


#if ENABLE(CSS_SCROLL_SNAP) || ENABLE(RUBBER_BANDING)
    if (expectsWheelEventTestTrigger()) {
        if (scrollingTree().shouldHandleWheelEventSynchronously(wheelEvent))
            m_delegate.removeTestDeferralForReason(reinterpret_cast<WheelEventTestTrigger::ScrollableAreaIdentifier>(scrollingNodeID()), WheelEventTestTrigger::ScrollingThreadSyncNeeded);
        else
            m_delegate.deferTestsForReason(reinterpret_cast<WheelEventTestTrigger::ScrollableAreaIdentifier>(scrollingNodeID()), WheelEventTestTrigger::ScrollingThreadSyncNeeded);
    }
#endif

    m_delegate.handleWheelEvent(wheelEvent);

    // FIXME: Scroll snap

    scrollingTree().setOrClearLatchedNode(wheelEvent, scrollingNodeID());
    scrollingTree().handleWheelEventPhase(wheelEvent.phase());
    
    // FIXME: This needs to return whether the event was handled.
    return ScrollingEventResult::DidHandleEvent;
}



void ScrollingTreeOverflowScrollingNodeMac::updateLayersAfterAncestorChange(const ScrollingTreeNode& changedNode, const FloatRect& fixedPositionRect, const FloatSize& cumulativeDelta)
{
    UNUSED_PARAM(changedNode);
    UNUSED_PARAM(fixedPositionRect);
    UNUSED_PARAM(cumulativeDelta);
}

FloatPoint ScrollingTreeOverflowScrollingNodeMac::scrollPosition() const
{
    return -m_scrolledContentsLayer.get().position;
}

void ScrollingTreeOverflowScrollingNodeMac::setScrollPosition(const FloatPoint& scrollPosition)
{
    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeOverflowScrollingNodeMac::setScrollPosition " << scrollPosition << " scrollPosition(): " << this->scrollPosition() << " min: " << minimumScrollPosition() << " max: " << maximumScrollPosition());

    // Scroll deltas can be non-integral with some input devices, so scrollPosition may not be integral.
    // FIXME: when we support half-pixel scroll positions on Retina displays, this will need to round to half pixels.
    FloatPoint roundedPosition(roundf(scrollPosition.x()), roundf(scrollPosition.y()));

    ScrollingTreeOverflowScrollingNode::setScrollPosition(roundedPosition);
}

void ScrollingTreeOverflowScrollingNodeMac::setScrollPositionWithoutContentEdgeConstraints(const FloatPoint& scrollPosition)
{
    setScrollLayerPosition(scrollPosition, { });
    scrollingTree().scrollingTreeNodeDidScroll(scrollingNodeID(), scrollPosition, { });
}

void ScrollingTreeOverflowScrollingNodeMac::setScrollLayerPosition(const FloatPoint& scrollPosition, const FloatRect& fixedPositionRect)
{
    m_scrolledContentsLayer.get().position = -scrollPosition;
    if (!m_children)
        return;

    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(*this, fixedPositionRect, { });
}

void ScrollingTreeOverflowScrollingNodeMac::updateLayersAfterDelegatedScroll(const FloatPoint& scrollPosition)
{
    UNUSED_PARAM(scrollPosition);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)
