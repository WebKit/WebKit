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

    if (state.hasChangedProperty(ScrollingStateNode::Property::FrameScaleFactor))
        m_frameScaleFactor = state.frameScaleFactor();

    if (state.hasChangedProperty(ScrollingStateNode::Property::HeaderHeight))
        m_headerHeight = state.headerHeight();

    if (state.hasChangedProperty(ScrollingStateNode::Property::FooterHeight))
        m_footerHeight = state.footerHeight();

    if (state.hasChangedProperty(ScrollingStateNode::Property::BehaviorForFixedElements))
        m_behaviorForFixed = state.scrollBehaviorForFixedElements();

    if (state.hasChangedProperty(ScrollingStateNode::Property::TopContentInset))
        m_topContentInset = state.topContentInset();

    if (state.hasChangedProperty(ScrollingStateNode::Property::VisualViewportIsSmallerThanLayoutViewport))
        m_visualViewportIsSmallerThanLayoutViewport = state.visualViewportIsSmallerThanLayoutViewport();

    if (state.hasChangedProperty(ScrollingStateNode::Property::FixedElementsLayoutRelativeToFrame))
        m_fixedElementsLayoutRelativeToFrame = state.fixedElementsLayoutRelativeToFrame();

    if (state.hasChangedProperty(ScrollingStateNode::Property::LayoutViewport)) {
        m_layoutViewport = state.layoutViewport();
        updateViewportForCurrentScrollPosition({ });
    }

    if (state.hasChangedProperty(ScrollingStateNode::Property::MinLayoutViewportOrigin))
        m_minLayoutViewportOrigin = state.minLayoutViewportOrigin();

    if (state.hasChangedProperty(ScrollingStateNode::Property::MaxLayoutViewportOrigin))
        m_maxLayoutViewportOrigin = state.maxLayoutViewportOrigin();

    if (state.hasChangedProperty(ScrollingStateNode::Property::OverrideVisualViewportSize))
        m_overrideVisualViewportSize = state.overrideVisualViewportSize();
}

bool ScrollingTreeFrameScrollingNode::scrollPositionAndLayoutViewportMatch(const FloatPoint& position, std::optional<FloatRect> overrideLayoutViewport)
{
    return position == currentScrollPosition() && (!overrideLayoutViewport || overrideLayoutViewport.value() == m_layoutViewport);
}

FloatRect ScrollingTreeFrameScrollingNode::layoutViewportForScrollPosition(const FloatPoint& visibleContentOrigin, float scale, ScrollBehaviorForFixedElements fixedBehavior) const
{
    FloatSize visualViewportSize = m_overrideVisualViewportSize.value_or(scrollableAreaSize());
    FloatRect visibleContentRect(visibleContentOrigin, visualViewportSize);
    LayoutRect visualViewport(FrameView::visibleDocumentRect(visibleContentRect, headerHeight(), footerHeight(), totalContentsSize(), scale));
    LayoutRect layoutViewport(m_layoutViewport);

    LOG_WITH_STREAM(Scrolling, stream << "ScrollingTreeFrameScrollingNode " << scrollingNodeID() << " layoutViewportForScrollPosition: " << "(visibleContentOrigin " << visibleContentOrigin << ", visualViewportSize " << visualViewportSize << ") fixed behavior " << m_behaviorForFixed);
    LOG_WITH_STREAM(Scrolling, stream << "  layoutViewport: " << layoutViewport);
    LOG_WITH_STREAM(Scrolling, stream << "  visualViewport: " << visualViewport);
    LOG_WITH_STREAM(Scrolling, stream << "  scroll positions: min: " << minLayoutViewportOrigin() << " max: "<< maxLayoutViewportOrigin());

    LayoutPoint newLocation = FrameView::computeLayoutViewportOrigin(LayoutRect(visualViewport), LayoutPoint(minLayoutViewportOrigin()), LayoutPoint(maxLayoutViewportOrigin()), layoutViewport, fixedBehavior);

    if (layoutViewport.location() != newLocation) {
        layoutViewport.setLocation(newLocation);
        LOG_WITH_STREAM(Scrolling, stream << " new layoutViewport " << layoutViewport);
    }

    return layoutViewport;
}

void ScrollingTreeFrameScrollingNode::updateViewportForCurrentScrollPosition(std::optional<FloatRect> overrideLayoutViewport)
{
    if (overrideLayoutViewport)
        setLayoutViewport(overrideLayoutViewport.value());
    else
        setLayoutViewport(layoutViewportForScrollPosition(currentScrollPosition(), frameScaleFactor()));
}

FloatRect ScrollingTreeFrameScrollingNode::layoutViewportRespectingRubberBanding() const
{
    return layoutViewportForScrollPosition(currentScrollPosition(), frameScaleFactor(), StickToViewportBounds);
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

    if (m_overrideVisualViewportSize)
        ts.dumpProperty("override visual viewport size", m_overrideVisualViewportSize.value());

    if (m_frameScaleFactor != 1)
        ts.dumpProperty("frame scale factor", m_frameScaleFactor);
    if (m_topContentInset)
        ts.dumpProperty("top content inset", m_topContentInset);

    if (m_headerHeight)
        ts.dumpProperty("header height", m_headerHeight);
    if (m_footerHeight)
        ts.dumpProperty("footer height", m_footerHeight);

    ts.dumpProperty("behavior for fixed", m_behaviorForFixed);
    if (m_fixedElementsLayoutRelativeToFrame)
        ts.dumpProperty("fixed elements lay out relative to frame", m_fixedElementsLayoutRelativeToFrame);
    if (m_visualViewportIsSmallerThanLayoutViewport)
        ts.dumpProperty("visual viewport is smaller than layout viewport", m_visualViewportIsSmallerThanLayoutViewport);
}


} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
