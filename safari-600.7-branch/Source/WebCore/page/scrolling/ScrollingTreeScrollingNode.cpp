/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ScrollingTreeScrollingNode.h"

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingStateTree.h"
#include "ScrollingTree.h"

namespace WebCore {

ScrollingTreeScrollingNode::ScrollingTreeScrollingNode(ScrollingTree& scrollingTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
    : ScrollingTreeNode(scrollingTree, nodeType, nodeID)
{
}

ScrollingTreeScrollingNode::~ScrollingTreeScrollingNode()
{
}

void ScrollingTreeScrollingNode::updateBeforeChildren(const ScrollingStateNode& stateNode)
{
    const ScrollingStateScrollingNode& state = toScrollingStateScrollingNode(stateNode);

    if (state.hasChangedProperty(ScrollingStateScrollingNode::ScrollableAreaSize))
        m_scrollableAreaSize = state.scrollableAreaSize();

    if (state.hasChangedProperty(ScrollingStateScrollingNode::TotalContentsSize)) {
        if (scrollingTree().isRubberBandInProgress())
            m_totalContentsSizeForRubberBand = m_totalContentsSize;
        else
            m_totalContentsSizeForRubberBand = state.totalContentsSize();

        m_totalContentsSize = state.totalContentsSize();
    }

    if (state.hasChangedProperty(ScrollingStateScrollingNode::ReachableContentsSize))
        m_reachableContentsSize = state.reachableContentsSize();

    if (state.hasChangedProperty(ScrollingStateScrollingNode::ScrollPosition))
        m_lastCommittedScrollPosition = state.scrollPosition();

    if (state.hasChangedProperty(ScrollingStateScrollingNode::ScrollOrigin))
        m_scrollOrigin = state.scrollOrigin();

    if (state.hasChangedProperty(ScrollingStateScrollingNode::ScrollableAreaParams))
        m_scrollableAreaParameters = state.scrollableAreaParameters();
}

void ScrollingTreeScrollingNode::updateAfterChildren(const ScrollingStateNode& stateNode)
{
    const ScrollingStateScrollingNode& scrollingStateNode = toScrollingStateScrollingNode(stateNode);
    if (scrollingStateNode.hasChangedProperty(ScrollingStateScrollingNode::RequestedScrollPosition))
        scrollingTree().scrollingTreeNodeRequestsScroll(scrollingNodeID(), scrollingStateNode.requestedScrollPosition(), scrollingStateNode.requestedScrollPositionRepresentsProgrammaticScroll());
}

void ScrollingTreeScrollingNode::updateLayersAfterAncestorChange(const ScrollingTreeNode& changedNode, const FloatRect& fixedPositionRect, const FloatSize& cumulativeDelta)
{
    if (!m_children)
        return;

    for (auto& child : *m_children)
        child->updateLayersAfterAncestorChange(changedNode, fixedPositionRect, cumulativeDelta);
}

void ScrollingTreeScrollingNode::setScrollPosition(const FloatPoint& scrollPosition)
{
    FloatPoint newScrollPosition = scrollPosition;
    newScrollPosition = newScrollPosition.shrunkTo(maximumScrollPosition());
    newScrollPosition = newScrollPosition.expandedTo(minimumScrollPosition());

    setScrollPositionWithoutContentEdgeConstraints(newScrollPosition);
}

void ScrollingTreeScrollingNode::setScrollPositionWithoutContentEdgeConstraints(const FloatPoint& scrollPosition)
{
    setScrollLayerPosition(scrollPosition);
    scrollingTree().scrollingTreeNodeDidScroll(scrollingNodeID(), scrollPosition);
}

FloatPoint ScrollingTreeScrollingNode::minimumScrollPosition() const
{
    return FloatPoint();
}

FloatPoint ScrollingTreeScrollingNode::maximumScrollPosition() const
{
    FloatPoint contentSizePoint(totalContentsSize());
    return FloatPoint(contentSizePoint - scrollableAreaSize()).expandedTo(FloatPoint());
}


} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
