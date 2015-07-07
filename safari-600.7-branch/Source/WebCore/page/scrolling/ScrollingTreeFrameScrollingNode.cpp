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

#include "config.h"
#include "ScrollingTreeFrameScrollingNode.h"

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingStateTree.h"
#include "ScrollingTree.h"

namespace WebCore {

ScrollingTreeFrameScrollingNode::ScrollingTreeFrameScrollingNode(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeScrollingNode(scrollingTree, FrameScrollingNode, nodeID)
    , m_frameScaleFactor(1)
    , m_headerHeight(0)
    , m_footerHeight(0)
    , m_topContentInset(0)
    , m_synchronousScrollingReasons(0)
    , m_behaviorForFixed(StickToDocumentBounds)
{
}

ScrollingTreeFrameScrollingNode::~ScrollingTreeFrameScrollingNode()
{
}

void ScrollingTreeFrameScrollingNode::updateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeScrollingNode::updateBeforeChildren(stateNode);
    
    const ScrollingStateFrameScrollingNode& state = toScrollingStateFrameScrollingNode(stateNode);

    if (state.hasChangedProperty(ScrollingStateFrameScrollingNode::FrameScaleFactor))
        m_frameScaleFactor = state.frameScaleFactor();

    if (state.hasChangedProperty(ScrollingStateFrameScrollingNode::ReasonsForSynchronousScrolling))
        m_synchronousScrollingReasons = state.synchronousScrollingReasons();

    if (state.hasChangedProperty(ScrollingStateFrameScrollingNode::HeaderHeight))
        m_headerHeight = state.headerHeight();

    if (state.hasChangedProperty(ScrollingStateFrameScrollingNode::FooterHeight))
        m_footerHeight = state.footerHeight();

    if (state.hasChangedProperty(ScrollingStateFrameScrollingNode::BehaviorForFixedElements))
        m_behaviorForFixed = state.scrollBehaviorForFixedElements();

    if (state.hasChangedProperty(ScrollingStateFrameScrollingNode::TopContentInset))
        m_topContentInset = state.topContentInset();
}

void ScrollingTreeFrameScrollingNode::scrollBy(const FloatSize& offset)
{
    setScrollPosition(scrollPosition() + offset);
}

void ScrollingTreeFrameScrollingNode::scrollByWithoutContentEdgeConstraints(const FloatSize& offset)
{
    setScrollPositionWithoutContentEdgeConstraints(scrollPosition() + offset);
}

void ScrollingTreeFrameScrollingNode::setScrollPosition(const FloatPoint& scrollPosition)
{
    FloatPoint newScrollPosition = scrollPosition;
    newScrollPosition = newScrollPosition.shrunkTo(maximumScrollPosition());
    newScrollPosition = newScrollPosition.expandedTo(minimumScrollPosition());

    setScrollPositionWithoutContentEdgeConstraints(newScrollPosition);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
