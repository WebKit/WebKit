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

ScrollingTreeScrollingNode::ScrollingTreeScrollingNode(ScrollingTree& scrollingTree, ScrollingNodeID nodeID)
    : ScrollingTreeNode(scrollingTree, ScrollingNode, nodeID)
    , m_frameScaleFactor(1)
    , m_headerHeight(0)
    , m_footerHeight(0)
    , m_synchronousScrollingReasons(0)
    , m_behaviorForFixed(StickToDocumentBounds)
{
}

ScrollingTreeScrollingNode::~ScrollingTreeScrollingNode()
{
}

void ScrollingTreeScrollingNode::updateBeforeChildren(const ScrollingStateNode& stateNode)
{
    const ScrollingStateScrollingNode& state = toScrollingStateScrollingNode(stateNode);

    if (state.hasChangedProperty(ScrollingStateScrollingNode::ViewportConstrainedObjectRect))
        m_viewportConstrainedObjectRect = state.viewportConstrainedObjectRect();

    if (state.hasChangedProperty(ScrollingStateScrollingNode::TotalContentsSize)) {
        if (scrollingTree().isRubberBandInProgress())
            m_totalContentsSizeForRubberBand = m_totalContentsSize;
        else
            m_totalContentsSizeForRubberBand = state.totalContentsSize();
        m_totalContentsSize = state.totalContentsSize();
    }

    if (state.hasChangedProperty(ScrollingStateScrollingNode::ScrollPosition))
        m_scrollPosition = state.scrollPosition();

    if (state.hasChangedProperty(ScrollingStateScrollingNode::ScrollOrigin))
        m_scrollOrigin = state.scrollOrigin();

    if (state.hasChangedProperty(ScrollingStateScrollingNode::ScrollableAreaParams))
        m_scrollableAreaParameters = state.scrollableAreaParameters();

    if (state.hasChangedProperty(ScrollingStateScrollingNode::FrameScaleFactor))
        m_frameScaleFactor = state.frameScaleFactor();

    if (state.hasChangedProperty(ScrollingStateScrollingNode::ReasonsForSynchronousScrolling))
        m_synchronousScrollingReasons = state.synchronousScrollingReasons();

    if (state.hasChangedProperty(ScrollingStateScrollingNode::HeaderHeight))
        m_headerHeight = state.headerHeight();

    if (state.hasChangedProperty(ScrollingStateScrollingNode::FooterHeight))
        m_footerHeight = state.footerHeight();

    if (state.hasChangedProperty(ScrollingStateScrollingNode::BehaviorForFixedElements))
        m_behaviorForFixed = state.scrollBehaviorForFixedElements();
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
