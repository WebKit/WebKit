/*
 * Copyright (C) 2012, 2014-2015 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)

#include "ScrollSnapOffsetsInfo.h"
#include "ScrollTypes.h"
#include "ScrollingCoordinator.h"
#include "ScrollingStateNode.h"

namespace WebCore {

class ScrollingStateScrollingNode : public ScrollingStateNode {
public:
    virtual ~ScrollingStateScrollingNode();

    enum ChangedProperty {
        ScrollableAreaSize = NumStateNodeBits,
        TotalContentsSize,
        ReachableContentsSize,
        ParentRelativeScrollableRect,
        ScrollPosition,
        ScrollOrigin,
        ScrollableAreaParams,
        RequestedScrollPosition,
#if ENABLE(CSS_SCROLL_SNAP)
        HorizontalSnapOffsets,
        VerticalSnapOffsets,
        HorizontalSnapOffsetRanges,
        VerticalSnapOffsetRanges,
        CurrentHorizontalSnapOffsetIndex,
        CurrentVerticalSnapOffsetIndex,
#endif
        ExpectsWheelEventTestTrigger,
        ScrollContainerLayer,
        ScrolledContentsLayer,
        NumScrollingStateNodeBits // This must remain at the last position.
    };

    const FloatSize& scrollableAreaSize() const { return m_scrollableAreaSize; }
    WEBCORE_EXPORT void setScrollableAreaSize(const FloatSize&);

    const FloatSize& totalContentsSize() const { return m_totalContentsSize; }
    WEBCORE_EXPORT void setTotalContentsSize(const FloatSize&);

    const FloatSize& reachableContentsSize() const { return m_reachableContentsSize; }
    WEBCORE_EXPORT void setReachableContentsSize(const FloatSize&);

    const LayoutRect& parentRelativeScrollableRect() const { return m_parentRelativeScrollableRect; }
    WEBCORE_EXPORT void setParentRelativeScrollableRect(const LayoutRect&);

    const FloatPoint& scrollPosition() const { return m_scrollPosition; }
    WEBCORE_EXPORT void setScrollPosition(const FloatPoint&);

    const IntPoint& scrollOrigin() const { return m_scrollOrigin; }
    WEBCORE_EXPORT void setScrollOrigin(const IntPoint&);

#if ENABLE(CSS_SCROLL_SNAP)
    const Vector<float>& horizontalSnapOffsets() const { return m_snapOffsetsInfo.horizontalSnapOffsets; }
    WEBCORE_EXPORT void setHorizontalSnapOffsets(const Vector<float>&);

    const Vector<float>& verticalSnapOffsets() const { return m_snapOffsetsInfo.verticalSnapOffsets; }
    WEBCORE_EXPORT void setVerticalSnapOffsets(const Vector<float>&);

    const Vector<ScrollOffsetRange<float>>& horizontalSnapOffsetRanges() const { return m_snapOffsetsInfo.horizontalSnapOffsetRanges; }
    WEBCORE_EXPORT void setHorizontalSnapOffsetRanges(const Vector<ScrollOffsetRange<float>>&);

    const Vector<ScrollOffsetRange<float>>& verticalSnapOffsetRanges() const { return m_snapOffsetsInfo.verticalSnapOffsetRanges; }
    WEBCORE_EXPORT void setVerticalSnapOffsetRanges(const Vector<ScrollOffsetRange<float>>&);

    unsigned currentHorizontalSnapPointIndex() const { return m_currentHorizontalSnapPointIndex; }
    WEBCORE_EXPORT void setCurrentHorizontalSnapPointIndex(unsigned);

    unsigned currentVerticalSnapPointIndex() const { return m_currentVerticalSnapPointIndex; }
    WEBCORE_EXPORT void setCurrentVerticalSnapPointIndex(unsigned);
#endif

    const ScrollableAreaParameters& scrollableAreaParameters() const { return m_scrollableAreaParameters; }
    WEBCORE_EXPORT void setScrollableAreaParameters(const ScrollableAreaParameters& params);

    const FloatPoint& requestedScrollPosition() const { return m_requestedScrollPosition; }
    bool requestedScrollPositionRepresentsProgrammaticScroll() const { return m_requestedScrollPositionRepresentsProgrammaticScroll; }
    WEBCORE_EXPORT void setRequestedScrollPosition(const FloatPoint&, bool representsProgrammaticScroll);

    bool expectsWheelEventTestTrigger() const { return m_expectsWheelEventTestTrigger; }
    WEBCORE_EXPORT void setExpectsWheelEventTestTrigger(bool);

    const LayerRepresentation& scrollContainerLayer() const { return m_scrollContainerLayer; }
    WEBCORE_EXPORT void setScrollContainerLayer(const LayerRepresentation&);

    // This is a layer with the contents that move.
    const LayerRepresentation& scrolledContentsLayer() const { return m_scrolledContentsLayer; }
    WEBCORE_EXPORT void setScrolledContentsLayer(const LayerRepresentation&);

protected:
    ScrollingStateScrollingNode(ScrollingStateTree&, ScrollingNodeType, ScrollingNodeID);
    ScrollingStateScrollingNode(const ScrollingStateScrollingNode&, ScrollingStateTree&);

    void setAllPropertiesChanged() override;

    void dumpProperties(WTF::TextStream&, ScrollingStateTreeAsTextBehavior) const override;

private:
    FloatSize m_scrollableAreaSize;
    FloatSize m_totalContentsSize;
    FloatSize m_reachableContentsSize;
    LayoutRect m_parentRelativeScrollableRect;
    FloatPoint m_scrollPosition;
    FloatPoint m_requestedScrollPosition;
    IntPoint m_scrollOrigin;
#if ENABLE(CSS_SCROLL_SNAP)
    ScrollSnapOffsetsInfo<float> m_snapOffsetsInfo;
    unsigned m_currentHorizontalSnapPointIndex { 0 };
    unsigned m_currentVerticalSnapPointIndex { 0 };
#endif
    ScrollableAreaParameters m_scrollableAreaParameters;
    LayerRepresentation m_scrollContainerLayer;
    LayerRepresentation m_scrolledContentsLayer;
    bool m_requestedScrollPositionRepresentsProgrammaticScroll { false };
    bool m_expectsWheelEventTestTrigger { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_SCROLLING_STATE_NODE(ScrollingStateScrollingNode, isScrollingNode())

#endif // ENABLE(ASYNC_SCROLLING) || USE(COORDINATED_GRAPHICS)
