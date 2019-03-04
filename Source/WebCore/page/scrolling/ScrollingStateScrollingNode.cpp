/*
 * Copyright (C) 2012, 2015 Apple Inc. All rights reserved.
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
#include <wtf/text/TextStream.h>

namespace WebCore {

ScrollingStateScrollingNode::ScrollingStateScrollingNode(ScrollingStateTree& stateTree, ScrollingNodeType nodeType, ScrollingNodeID nodeID)
    : ScrollingStateNode(nodeType, stateTree, nodeID)
{
}

ScrollingStateScrollingNode::ScrollingStateScrollingNode(const ScrollingStateScrollingNode& stateNode, ScrollingStateTree& adoptiveTree)
    : ScrollingStateNode(stateNode, adoptiveTree)
    , m_scrollableAreaSize(stateNode.scrollableAreaSize())
    , m_totalContentsSize(stateNode.totalContentsSize())
    , m_reachableContentsSize(stateNode.reachableContentsSize())
    , m_parentRelativeScrollableRect(stateNode.parentRelativeScrollableRect())
    , m_scrollPosition(stateNode.scrollPosition())
    , m_requestedScrollPosition(stateNode.requestedScrollPosition())
    , m_scrollOrigin(stateNode.scrollOrigin())
#if ENABLE(CSS_SCROLL_SNAP)
    , m_snapOffsetsInfo(stateNode.m_snapOffsetsInfo)
#endif
#if PLATFORM(MAC)
    , m_verticalScrollerImp(stateNode.verticalScrollerImp())
    , m_horizontalScrollerImp(stateNode.horizontalScrollerImp())
#endif
    , m_scrollableAreaParameters(stateNode.scrollableAreaParameters())
    , m_requestedScrollPositionRepresentsProgrammaticScroll(stateNode.requestedScrollPositionRepresentsProgrammaticScroll())
    , m_expectsWheelEventTestTrigger(stateNode.expectsWheelEventTestTrigger())
{
    if (hasChangedProperty(ScrollContainerLayer))
        setScrollContainerLayer(stateNode.scrollContainerLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(ScrolledContentsLayer))
        setScrolledContentsLayer(stateNode.scrolledContentsLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(VerticalScrollbarLayer))
        setVerticalScrollbarLayer(stateNode.verticalScrollbarLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    if (hasChangedProperty(HorizontalScrollbarLayer))
        setHorizontalScrollbarLayer(stateNode.horizontalScrollbarLayer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));
}

ScrollingStateScrollingNode::~ScrollingStateScrollingNode() = default;

void ScrollingStateScrollingNode::setAllPropertiesChanged()
{
    setPropertyChangedBit(ScrollableAreaSize);
    setPropertyChangedBit(TotalContentsSize);
    setPropertyChangedBit(ReachableContentsSize);
    setPropertyChangedBit(ParentRelativeScrollableRect);
    setPropertyChangedBit(ScrollPosition);
    setPropertyChangedBit(ScrollOrigin);
    setPropertyChangedBit(ScrollableAreaParams);
    setPropertyChangedBit(RequestedScrollPosition);
#if ENABLE(CSS_SCROLL_SNAP)
    setPropertyChangedBit(HorizontalSnapOffsets);
    setPropertyChangedBit(VerticalSnapOffsets);
    setPropertyChangedBit(HorizontalSnapOffsetRanges);
    setPropertyChangedBit(VerticalSnapOffsetRanges);
    setPropertyChangedBit(CurrentHorizontalSnapOffsetIndex);
    setPropertyChangedBit(CurrentVerticalSnapOffsetIndex);
#endif
    setPropertyChangedBit(ExpectsWheelEventTestTrigger);
    setPropertyChangedBit(ScrollContainerLayer);
    setPropertyChangedBit(ScrolledContentsLayer);
    setPropertyChangedBit(HorizontalScrollbarLayer);
    setPropertyChangedBit(VerticalScrollbarLayer);
    setPropertyChangedBit(PainterForScrollbar);

    ScrollingStateNode::setAllPropertiesChanged();
}

void ScrollingStateScrollingNode::setScrollableAreaSize(const FloatSize& size)
{
    if (m_scrollableAreaSize == size)
        return;

    m_scrollableAreaSize = size;
    setPropertyChanged(ScrollableAreaSize);
}

void ScrollingStateScrollingNode::setTotalContentsSize(const FloatSize& totalContentsSize)
{
    if (m_totalContentsSize == totalContentsSize)
        return;

    m_totalContentsSize = totalContentsSize;
    setPropertyChanged(TotalContentsSize);
}

void ScrollingStateScrollingNode::setReachableContentsSize(const FloatSize& reachableContentsSize)
{
    if (m_reachableContentsSize == reachableContentsSize)
        return;

    m_reachableContentsSize = reachableContentsSize;
    setPropertyChanged(ReachableContentsSize);
}

void ScrollingStateScrollingNode::setParentRelativeScrollableRect(const LayoutRect& parentRelativeScrollableRect)
{
    if (m_parentRelativeScrollableRect == parentRelativeScrollableRect)
        return;

    m_parentRelativeScrollableRect = parentRelativeScrollableRect;
    setPropertyChanged(ParentRelativeScrollableRect);
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

#if ENABLE(CSS_SCROLL_SNAP)
void ScrollingStateScrollingNode::setHorizontalSnapOffsets(const Vector<float>& snapOffsets)
{
    if (m_snapOffsetsInfo.horizontalSnapOffsets == snapOffsets)
        return;

    m_snapOffsetsInfo.horizontalSnapOffsets = snapOffsets;
    setPropertyChanged(HorizontalSnapOffsets);
}

void ScrollingStateScrollingNode::setVerticalSnapOffsets(const Vector<float>& snapOffsets)
{
    if (m_snapOffsetsInfo.verticalSnapOffsets == snapOffsets)
        return;

    m_snapOffsetsInfo.verticalSnapOffsets = snapOffsets;
    setPropertyChanged(VerticalSnapOffsets);
}

void ScrollingStateScrollingNode::setHorizontalSnapOffsetRanges(const Vector<ScrollOffsetRange<float>>& scrollOffsetRanges)
{
    if (m_snapOffsetsInfo.horizontalSnapOffsetRanges == scrollOffsetRanges)
        return;

    m_snapOffsetsInfo.horizontalSnapOffsetRanges = scrollOffsetRanges;
    setPropertyChanged(HorizontalSnapOffsetRanges);
}

void ScrollingStateScrollingNode::setVerticalSnapOffsetRanges(const Vector<ScrollOffsetRange<float>>& scrollOffsetRanges)
{
    if (m_snapOffsetsInfo.verticalSnapOffsetRanges == scrollOffsetRanges)
        return;

    m_snapOffsetsInfo.verticalSnapOffsetRanges = scrollOffsetRanges;
    setPropertyChanged(VerticalSnapOffsetRanges);
}

void ScrollingStateScrollingNode::setCurrentHorizontalSnapPointIndex(unsigned index)
{
    if (m_currentHorizontalSnapPointIndex == index)
        return;
    
    m_currentHorizontalSnapPointIndex = index;
    setPropertyChanged(CurrentHorizontalSnapOffsetIndex);
}

void ScrollingStateScrollingNode::setCurrentVerticalSnapPointIndex(unsigned index)
{
    if (m_currentVerticalSnapPointIndex == index)
        return;
    
    m_currentVerticalSnapPointIndex = index;
    setPropertyChanged(CurrentVerticalSnapOffsetIndex);
}
#endif

void ScrollingStateScrollingNode::setScrollableAreaParameters(const ScrollableAreaParameters& parameters)
{
    if (m_scrollableAreaParameters == parameters)
        return;

    m_scrollableAreaParameters = parameters;
    setPropertyChanged(ScrollableAreaParams);
}

void ScrollingStateScrollingNode::setRequestedScrollPosition(const FloatPoint& requestedScrollPosition, bool representsProgrammaticScroll)
{
    m_requestedScrollPosition = requestedScrollPosition;
    m_requestedScrollPositionRepresentsProgrammaticScroll = representsProgrammaticScroll;
    setPropertyChanged(RequestedScrollPosition);
}

void ScrollingStateScrollingNode::setExpectsWheelEventTestTrigger(bool expectsTestTrigger)
{
    if (expectsTestTrigger == m_expectsWheelEventTestTrigger)
        return;

    m_expectsWheelEventTestTrigger = expectsTestTrigger;
    setPropertyChanged(ExpectsWheelEventTestTrigger);
}

void ScrollingStateScrollingNode::setScrollContainerLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_scrollContainerLayer)
        return;

    m_scrollContainerLayer = layerRepresentation;
    setPropertyChanged(ScrollContainerLayer);
}

void ScrollingStateScrollingNode::setScrolledContentsLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_scrolledContentsLayer)
        return;

    m_scrolledContentsLayer = layerRepresentation;
    setPropertyChanged(ScrolledContentsLayer);
}

void ScrollingStateScrollingNode::setHorizontalScrollbarLayer(const LayerRepresentation& layer)
{
    if (layer == m_horizontalScrollbarLayer)
        return;

    m_horizontalScrollbarLayer = layer;
    setPropertyChanged(HorizontalScrollbarLayer);
}

void ScrollingStateScrollingNode::setVerticalScrollbarLayer(const LayerRepresentation& layer)
{
    if (layer == m_verticalScrollbarLayer)
        return;

    m_verticalScrollbarLayer = layer;
    setPropertyChanged(VerticalScrollbarLayer);
}

#if !PLATFORM(MAC)
void ScrollingStateScrollingNode::setScrollerImpsFromScrollbars(Scrollbar*, Scrollbar*)
{
}
#endif

void ScrollingStateScrollingNode::dumpProperties(TextStream& ts, ScrollingStateTreeAsTextBehavior behavior) const
{
    ScrollingStateNode::dumpProperties(ts, behavior);
    
    if (m_scrollPosition != FloatPoint()) {
        TextStream::GroupScope scope(ts);
        ts << "scroll position "
            << TextStream::FormatNumberRespectingIntegers(m_scrollPosition.x()) << " "
            << TextStream::FormatNumberRespectingIntegers(m_scrollPosition.y());
    }

    if (!m_scrollableAreaSize.isEmpty()) {
        TextStream::GroupScope scope(ts);
        ts << "scrollable area size "
            << TextStream::FormatNumberRespectingIntegers(m_scrollableAreaSize.width()) << " "
            << TextStream::FormatNumberRespectingIntegers(m_scrollableAreaSize.height());
    }

    if (!m_totalContentsSize.isEmpty()) {
        TextStream::GroupScope scope(ts);
        ts << "contents size "
            << TextStream::FormatNumberRespectingIntegers(m_totalContentsSize.width()) << " "
            << TextStream::FormatNumberRespectingIntegers(m_totalContentsSize.height());
    }

    if (m_reachableContentsSize != m_totalContentsSize)
        ts.dumpProperty("reachable contents size", m_reachableContentsSize);

    if (m_requestedScrollPosition != IntPoint()) {
        TextStream::GroupScope scope(ts);
        ts << "requested scroll position "
            << TextStream::FormatNumberRespectingIntegers(m_requestedScrollPosition.x()) << " "
            << TextStream::FormatNumberRespectingIntegers(m_requestedScrollPosition.y());
    }
    if (m_requestedScrollPositionRepresentsProgrammaticScroll)
        ts.dumpProperty("requested scroll position represents programmatic scroll", m_requestedScrollPositionRepresentsProgrammaticScroll);

    if (!m_parentRelativeScrollableRect.isEmpty())
        ts.dumpProperty("parent relative scrollable rect", m_parentRelativeScrollableRect);

    if (m_scrollOrigin != IntPoint())
        ts.dumpProperty("scroll origin", m_scrollOrigin);

#if ENABLE(CSS_SCROLL_SNAP)
    if (m_snapOffsetsInfo.horizontalSnapOffsets.size())
        ts.dumpProperty("horizontal snap offsets", m_snapOffsetsInfo.horizontalSnapOffsets);

    if (m_snapOffsetsInfo.verticalSnapOffsets.size())
        ts.dumpProperty("vertical snap offsets", m_snapOffsetsInfo.verticalSnapOffsets);

    if (m_currentHorizontalSnapPointIndex)
        ts.dumpProperty("current horizontal snap point index", m_currentHorizontalSnapPointIndex);

    if (m_currentVerticalSnapPointIndex)
        ts.dumpProperty("current vertical snap point index", m_currentVerticalSnapPointIndex);
#endif

    ts.dumpProperty("scrollable area parameters", m_scrollableAreaParameters);

    if (m_expectsWheelEventTestTrigger)
        ts.dumpProperty("expects wheel event test trigger", m_expectsWheelEventTestTrigger);

    if (behavior & ScrollingStateTreeAsTextBehaviorIncludeLayerIDs) {
        if (m_scrollContainerLayer.layerID())
            ts.dumpProperty("scroll container layer", m_scrollContainerLayer.layerID());
        if (m_scrolledContentsLayer.layerID())
            ts.dumpProperty("scrolled contents layer", m_scrolledContentsLayer.layerID());
    }
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
