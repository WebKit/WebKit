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

#include "FrameView.h"
#include "Logging.h"
#include "ScrollingStateFrameScrollingNode.h"
#include "ScrollingStateTree.h"
#include "ScrollingTree.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

ScrollingTreeFrameScrollingNode::ScrollingTreeFrameScrollingNode(ScrollingTree& scrollingTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
    : ScrollingTreeScrollingNode(scrollingTree, nodeType, nodeID)
{
    ASSERT(isFrameScrollingNode());
}

ScrollingTreeFrameScrollingNode::~ScrollingTreeFrameScrollingNode() = default;

void ScrollingTreeFrameScrollingNode::commitStateBeforeChildren(const ScrollingStateNode& stateNode)
{
    ScrollingTreeScrollingNode::commitStateBeforeChildren(stateNode);
    
    const ScrollingStateFrameScrollingNode& state = downcast<ScrollingStateFrameScrollingNode>(stateNode);

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

    if (state.hasChangedProperty(ScrollingStateFrameScrollingNode::FixedElementsLayoutRelativeToFrame))
        m_fixedElementsLayoutRelativeToFrame = state.fixedElementsLayoutRelativeToFrame();

    if (state.hasChangedProperty(ScrollingStateFrameScrollingNode::LayoutViewport))
        m_layoutViewport = state.layoutViewport();

    if (state.hasChangedProperty(ScrollingStateFrameScrollingNode::MinLayoutViewportOrigin))
        m_minLayoutViewportOrigin = state.minLayoutViewportOrigin();

    if (state.hasChangedProperty(ScrollingStateFrameScrollingNode::MaxLayoutViewportOrigin))
        m_maxLayoutViewportOrigin = state.maxLayoutViewportOrigin();
}

void ScrollingTreeFrameScrollingNode::scrollBy(const FloatSize& delta)
{
    setScrollPosition(scrollPosition() + delta);
}

void ScrollingTreeFrameScrollingNode::scrollByWithoutContentEdgeConstraints(const FloatSize& offset)
{
    setScrollPositionWithoutContentEdgeConstraints(scrollPosition() + offset);
}

void ScrollingTreeFrameScrollingNode::setScrollPosition(const FloatPoint& scrollPosition)
{
    FloatPoint newScrollPosition = scrollPosition.constrainedBetween(minimumScrollPosition(), maximumScrollPosition());
    setScrollPositionWithoutContentEdgeConstraints(newScrollPosition);
}

FloatRect ScrollingTreeFrameScrollingNode::layoutViewportForScrollPosition(const FloatPoint& visibleContentOrigin, float scale) const
{
    ASSERT(scrollingTree().visualViewportEnabled());

    FloatRect visibleContentRect(visibleContentOrigin, scrollableAreaSize());
    LayoutRect visualViewport(FrameView::visibleDocumentRect(visibleContentRect, headerHeight(), footerHeight(), totalContentsSize(), scale));
    LayoutRect layoutViewport(m_layoutViewport);

    LOG_WITH_STREAM(Scrolling, stream << "\nScrolling thread: " << "(visibleContentOrigin " << visibleContentOrigin << ") fixed behavior " << m_behaviorForFixed);
    LOG_WITH_STREAM(Scrolling, stream << "  layoutViewport: " << layoutViewport);
    LOG_WITH_STREAM(Scrolling, stream << "  visualViewport: " << visualViewport);
    LOG_WITH_STREAM(Scrolling, stream << "  scroll positions: min: " << minLayoutViewportOrigin() << " max: "<< maxLayoutViewportOrigin());

    LayoutPoint newLocation = FrameView::computeLayoutViewportOrigin(LayoutRect(visualViewport), LayoutPoint(minLayoutViewportOrigin()), LayoutPoint(maxLayoutViewportOrigin()), layoutViewport, m_behaviorForFixed);

    if (layoutViewport.location() != newLocation) {
        layoutViewport.setLocation(newLocation);
        LOG_WITH_STREAM(Scrolling, stream << " new layoutViewport " << layoutViewport);
    }

    return layoutViewport;
}

FloatSize ScrollingTreeFrameScrollingNode::viewToContentsOffset(const FloatPoint& scrollPosition) const
{
    return toFloatSize(scrollPosition) - FloatSize(0, headerHeight() + topContentInset());
}

void ScrollingTreeFrameScrollingNode::dumpProperties(TextStream& ts, ScrollingStateTreeAsTextBehavior behavior) const
{
    ts << "frame scrolling node";
    ScrollingTreeScrollingNode::dumpProperties(ts, behavior);

    ts.dumpProperty("layout viewport", m_layoutViewport);
    ts.dumpProperty("min layoutViewport origin", m_minLayoutViewportOrigin);
    ts.dumpProperty("max layoutViewport origin", m_maxLayoutViewportOrigin);

    if (m_frameScaleFactor != 1)
        ts.dumpProperty("frame scale factor", m_frameScaleFactor);
    if (m_topContentInset)
        ts.dumpProperty("top content inset", m_topContentInset);

    if (m_headerHeight)
        ts.dumpProperty("header height", m_headerHeight);
    if (m_footerHeight)
        ts.dumpProperty("footer height", m_footerHeight);
    if (m_synchronousScrollingReasons)
        ts.dumpProperty("synchronous scrolling reasons", ScrollingCoordinator::synchronousScrollingReasonsAsText(m_synchronousScrollingReasons));

    ts.dumpProperty("behavior for fixed", m_behaviorForFixed);
    if (m_fixedElementsLayoutRelativeToFrame)
        ts.dumpProperty("fixed elements lay out relative to frame", m_fixedElementsLayoutRelativeToFrame);
}


} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
