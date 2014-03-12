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
#include "ScrollingStateScrollingNode.h"

#if ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)

#include "ScrollingStateTree.h"
#include "TextStream.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

PassOwnPtr<ScrollingStateScrollingNode> ScrollingStateScrollingNode::create(ScrollingStateTree& stateTree, ScrollingNodeID nodeID)
{
    return adoptPtr(new ScrollingStateScrollingNode(stateTree, nodeID));
}

ScrollingStateScrollingNode::ScrollingStateScrollingNode(ScrollingStateTree& stateTree, ScrollingNodeID nodeID)
    : ScrollingStateNode(ScrollingNode, stateTree, nodeID)
#if PLATFORM(MAC)
    , m_verticalScrollbarPainter(0)
    , m_horizontalScrollbarPainter(0)
#endif
    , m_frameScaleFactor(1)
    , m_wheelEventHandlerCount(0)
    , m_synchronousScrollingReasons(0)
    , m_behaviorForFixed(StickToDocumentBounds)
    , m_headerHeight(0)
    , m_footerHeight(0)
    , m_requestedScrollPositionRepresentsProgrammaticScroll(false)
{
}

ScrollingStateScrollingNode::ScrollingStateScrollingNode(const ScrollingStateScrollingNode& stateNode, ScrollingStateTree& adoptiveTree)
    : ScrollingStateNode(stateNode, adoptiveTree)
#if PLATFORM(MAC)
    , m_verticalScrollbarPainter(stateNode.verticalScrollbarPainter())
    , m_horizontalScrollbarPainter(stateNode.horizontalScrollbarPainter())
#endif
    , m_viewportSize(stateNode.viewportSize())
    , m_totalContentsSize(stateNode.totalContentsSize())
    , m_scrollPosition(stateNode.scrollPosition())
    , m_scrollOrigin(stateNode.scrollOrigin())
    , m_scrollableAreaParameters(stateNode.scrollableAreaParameters())
    , m_nonFastScrollableRegion(stateNode.nonFastScrollableRegion())
    , m_frameScaleFactor(stateNode.frameScaleFactor())
    , m_wheelEventHandlerCount(stateNode.wheelEventHandlerCount())
    , m_synchronousScrollingReasons(stateNode.synchronousScrollingReasons())
    , m_behaviorForFixed(stateNode.scrollBehaviorForFixedElements())
    , m_headerHeight(stateNode.headerHeight())
    , m_footerHeight(stateNode.footerHeight())
    , m_requestedScrollPosition(stateNode.requestedScrollPosition())
    , m_requestedScrollPositionRepresentsProgrammaticScroll(stateNode.requestedScrollPositionRepresentsProgrammaticScroll())
{
    if (hasChangedProperty(ScrolledContentsLayer))
        setScrolledContentsLayer(stateNode.scrolledContentsLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(CounterScrollingLayer))
        setCounterScrollingLayer(stateNode.counterScrollingLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(HeaderLayer))
        setHeaderLayer(stateNode.headerLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(FooterLayer))
        setFooterLayer(stateNode.footerLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));
}

ScrollingStateScrollingNode::~ScrollingStateScrollingNode()
{
}

PassOwnPtr<ScrollingStateNode> ScrollingStateScrollingNode::clone(ScrollingStateTree& adoptiveTree)
{
    return adoptPtr(new ScrollingStateScrollingNode(*this, adoptiveTree));
}

void ScrollingStateScrollingNode::setViewportSize(const FloatSize& size)
{
    if (m_viewportSize == size)
        return;

    m_viewportSize = size;
    setPropertyChanged(ViewportSize);
}

void ScrollingStateScrollingNode::setTotalContentsSize(const IntSize& totalContentsSize)
{
    if (m_totalContentsSize == totalContentsSize)
        return;

    m_totalContentsSize = totalContentsSize;
    setPropertyChanged(TotalContentsSize);
}

void ScrollingStateScrollingNode::setScrollPosition(const FloatPoint& scrollPosition)
{
    if (m_scrollPosition == scrollPosition)
        return;

    m_scrollPosition = scrollPosition;
    setPropertyChanged(ScrollPosition);
}

void ScrollingStateScrollingNode::setScrollOrigin(const IntPoint& scrollOrigin)
{
    if (m_scrollOrigin == scrollOrigin)
        return;

    m_scrollOrigin = scrollOrigin;
    setPropertyChanged(ScrollOrigin);
}

void ScrollingStateScrollingNode::setScrollableAreaParameters(const ScrollableAreaParameters& parameters)
{
    if (m_scrollableAreaParameters == parameters)
        return;

    m_scrollableAreaParameters = parameters;
    setPropertyChanged(ScrollableAreaParams);
}

void ScrollingStateScrollingNode::setFrameScaleFactor(float scaleFactor)
{
    if (m_frameScaleFactor == scaleFactor)
        return;

    m_frameScaleFactor = scaleFactor;

    setPropertyChanged(FrameScaleFactor);
}

void ScrollingStateScrollingNode::setNonFastScrollableRegion(const Region& nonFastScrollableRegion)
{
    if (m_nonFastScrollableRegion == nonFastScrollableRegion)
        return;

    m_nonFastScrollableRegion = nonFastScrollableRegion;
    setPropertyChanged(NonFastScrollableRegion);
}

void ScrollingStateScrollingNode::setWheelEventHandlerCount(unsigned wheelEventHandlerCount)
{
    if (m_wheelEventHandlerCount == wheelEventHandlerCount)
        return;

    m_wheelEventHandlerCount = wheelEventHandlerCount;
    setPropertyChanged(WheelEventHandlerCount);
}

void ScrollingStateScrollingNode::setSynchronousScrollingReasons(SynchronousScrollingReasons reasons)
{
    if (m_synchronousScrollingReasons == reasons)
        return;

    m_synchronousScrollingReasons = reasons;
    setPropertyChanged(ReasonsForSynchronousScrolling);
}

void ScrollingStateScrollingNode::setScrollBehaviorForFixedElements(ScrollBehaviorForFixedElements behaviorForFixed)
{
    if (m_behaviorForFixed == behaviorForFixed)
        return;

    m_behaviorForFixed = behaviorForFixed;
    setPropertyChanged(BehaviorForFixedElements);
}

void ScrollingStateScrollingNode::setRequestedScrollPosition(const FloatPoint& requestedScrollPosition, bool representsProgrammaticScroll)
{
    m_requestedScrollPosition = requestedScrollPosition;
    m_requestedScrollPositionRepresentsProgrammaticScroll = representsProgrammaticScroll;
    setPropertyChanged(RequestedScrollPosition);
}

void ScrollingStateScrollingNode::setHeaderHeight(int headerHeight)
{
    if (m_headerHeight == headerHeight)
        return;

    m_headerHeight = headerHeight;
    setPropertyChanged(HeaderHeight);
}

void ScrollingStateScrollingNode::setFooterHeight(int footerHeight)
{
    if (m_footerHeight == footerHeight)
        return;

    m_footerHeight = footerHeight;
    setPropertyChanged(FooterHeight);
}

void ScrollingStateScrollingNode::setScrolledContentsLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_scrolledContentsLayer)
        return;
    
    m_scrolledContentsLayer = layerRepresentation;
    setPropertyChanged(ScrolledContentsLayer);
}

void ScrollingStateScrollingNode::setCounterScrollingLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_counterScrollingLayer)
        return;
    
    m_counterScrollingLayer = layerRepresentation;
    setPropertyChanged(CounterScrollingLayer);
}

void ScrollingStateScrollingNode::setHeaderLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_headerLayer)
        return;
    
    m_headerLayer = layerRepresentation;
    setPropertyChanged(HeaderLayer);
}


void ScrollingStateScrollingNode::setFooterLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_footerLayer)
        return;
    
    m_footerLayer = layerRepresentation;
    setPropertyChanged(FooterLayer);
}

#if !PLATFORM(MAC)
void ScrollingStateScrollingNode::setScrollbarPaintersFromScrollbars(Scrollbar*, Scrollbar*)
{
}
#endif

void ScrollingStateScrollingNode::dumpProperties(TextStream& ts, int indent) const
{
    ts << "(" << "Scrolling node" << "\n";

    if (!m_viewportSize.isEmpty()) {
        writeIndent(ts, indent + 1);
        ts << "(viewport rect "
            << TextStream::FormatNumberRespectingIntegers(m_viewportSize.width()) << " "
            << TextStream::FormatNumberRespectingIntegers(m_viewportSize.height()) << ")\n";
    }

    if (m_scrollPosition != FloatPoint()) {
        writeIndent(ts, indent + 1);
        ts << "(scroll position "
            << TextStream::FormatNumberRespectingIntegers(m_scrollPosition.x()) << " "
            << TextStream::FormatNumberRespectingIntegers(m_scrollPosition.y()) << ")\n";
    }

    if (!m_totalContentsSize.isEmpty()) {
        writeIndent(ts, indent + 1);
        ts << "(contents size " << m_totalContentsSize.width() << " " << m_totalContentsSize.height() << ")\n";
    }

    if (m_frameScaleFactor != 1) {
        writeIndent(ts, indent + 1);
        ts << "(frame scale factor " << m_frameScaleFactor << ")\n";
    }

    if (m_synchronousScrollingReasons) {
        writeIndent(ts, indent + 1);
        ts << "(Scrolling on main thread because: " << ScrollingCoordinator::synchronousScrollingReasonsAsText(m_synchronousScrollingReasons) << ")\n";
    }

    if (m_requestedScrollPosition != IntPoint()) {
        writeIndent(ts, indent + 1);
        ts << "(requested scroll position " << TextStream::FormatNumberRespectingIntegers(m_requestedScrollPosition.x()) << " " << TextStream::FormatNumberRespectingIntegers(m_requestedScrollPosition.y()) << ")\n";
    }

    if (m_scrollOrigin != IntPoint()) {
        writeIndent(ts, indent + 1);
        ts << "(scroll origin " << m_scrollOrigin.x() << " " << m_scrollOrigin.y() << ")\n";
    }
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
