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

#pragma once

#if ENABLE(ASYNC_SCROLLING)

#include "IntRect.h"
#include "ScrollSnapOffsetsInfo.h"
#include "ScrollTypes.h"
#include "ScrollingCoordinator.h"
#include "ScrollingTreeNode.h"

namespace WebCore {

class ScrollingTree;
class ScrollingStateScrollingNode;

class WEBCORE_EXPORT ScrollingTreeScrollingNode : public ScrollingTreeNode {
    friend class ScrollingTreeScrollingNodeDelegate;
#if PLATFORM(MAC)
    friend class ScrollingTreeScrollingNodeDelegateMac;
#endif

public:
    virtual ~ScrollingTreeScrollingNode();

    void commitStateBeforeChildren(const ScrollingStateNode&) override;
    void commitStateAfterChildren(const ScrollingStateNode&) override;

    void updateLayersAfterAncestorChange(const ScrollingTreeNode& changedNode, const FloatRect& fixedPositionRect, const FloatSize& cumulativeDelta) override;

    virtual ScrollingEventResult handleWheelEvent(const PlatformWheelEvent&);
    virtual void setScrollPosition(const FloatPoint&, ScrollPositionClamp = ScrollPositionClamp::ToContentEdges);

    void scrollBy(const FloatSize&, ScrollPositionClamp = ScrollPositionClamp::ToContentEdges);

    virtual void updateLayersAfterViewportChange(const FloatRect& fixedPositionRect, double scale) = 0;
    virtual void updateLayersAfterDelegatedScroll(const FloatPoint&) { }

    virtual FloatPoint scrollPosition() const = 0;
    const FloatSize& scrollableAreaSize() const { return m_scrollableAreaSize; }
    const FloatSize& totalContentsSize() const { return m_totalContentsSize; }

#if ENABLE(CSS_SCROLL_SNAP)
    const Vector<float>& horizontalSnapOffsets() const { return m_snapOffsetsInfo.horizontalSnapOffsets; }
    const Vector<float>& verticalSnapOffsets() const { return m_snapOffsetsInfo.verticalSnapOffsets; }
    const Vector<ScrollOffsetRange<float>>& horizontalSnapOffsetRanges() const { return m_snapOffsetsInfo.horizontalSnapOffsetRanges; }
    const Vector<ScrollOffsetRange<float>>& verticalSnapOffsetRanges() const { return m_snapOffsetsInfo.verticalSnapOffsetRanges; }
    unsigned currentHorizontalSnapPointIndex() const { return m_currentHorizontalSnapPointIndex; }
    unsigned currentVerticalSnapPointIndex() const { return m_currentVerticalSnapPointIndex; }
    void setCurrentHorizontalSnapPointIndex(unsigned index) { m_currentHorizontalSnapPointIndex = index; }
    void setCurrentVerticalSnapPointIndex(unsigned index) { m_currentVerticalSnapPointIndex = index; }
#endif

    bool useDarkAppearanceForScrollbars() const { return m_scrollableAreaParameters.useDarkAppearanceForScrollbars; }

    bool scrollLimitReached(const PlatformWheelEvent&) const;
    ScrollingTreeScrollingNode* scrollingNodeForPoint(LayoutPoint) const override;

protected:
    ScrollingTreeScrollingNode(ScrollingTree&, ScrollingNodeType, ScrollingNodeID);

    virtual FloatPoint minimumScrollPosition() const;
    virtual FloatPoint maximumScrollPosition() const;

    FloatPoint clampScrollPosition(const FloatPoint&) const;

    virtual void setScrollLayerPosition(const FloatPoint&, const FloatRect& layoutViewport) = 0;

    FloatPoint lastCommittedScrollPosition() const { return m_lastCommittedScrollPosition; }
    const FloatSize& reachableContentsSize() const { return m_reachableContentsSize; }
    const LayoutRect& parentRelativeScrollableRect() const { return m_parentRelativeScrollableRect; }
    const IntPoint& scrollOrigin() const { return m_scrollOrigin; }

    // If the totalContentsSize changes in the middle of a rubber-band, we still want to use the old totalContentsSize for the sake of
    // computing the stretchAmount(). Using the old value will keep the animation smooth. When there is no rubber-band in progress at
    // all, m_totalContentsSizeForRubberBand should be equivalent to m_totalContentsSize.
    const FloatSize& totalContentsSizeForRubberBand() const { return m_totalContentsSizeForRubberBand; }
    void setTotalContentsSizeForRubberBand(const FloatSize& totalContentsSizeForRubberBand) { m_totalContentsSizeForRubberBand = totalContentsSizeForRubberBand; }

    ScrollElasticity horizontalScrollElasticity() const { return m_scrollableAreaParameters.horizontalScrollElasticity; }
    ScrollElasticity verticalScrollElasticity() const { return m_scrollableAreaParameters.verticalScrollElasticity; }

    bool hasEnabledHorizontalScrollbar() const { return m_scrollableAreaParameters.hasEnabledHorizontalScrollbar; }
    bool hasEnabledVerticalScrollbar() const { return m_scrollableAreaParameters.hasEnabledVerticalScrollbar; }

    bool canHaveScrollbars() const { return m_scrollableAreaParameters.horizontalScrollbarMode != ScrollbarAlwaysOff || m_scrollableAreaParameters.verticalScrollbarMode != ScrollbarAlwaysOff; }

    bool expectsWheelEventTestTrigger() const { return m_expectsWheelEventTestTrigger; }

#if PLATFORM(COCOA)
    CALayer *scrollContainerLayer() const { return m_scrollContainerLayer.get(); }
    CALayer *scrolledContentsLayer() const { return m_scrolledContentsLayer.get(); }
#endif

    LayoutPoint parentToLocalPoint(LayoutPoint) const override;
    LayoutPoint localToContentsPoint(LayoutPoint) const override;

    void dumpProperties(WTF::TextStream&, ScrollingStateTreeAsTextBehavior) const override;

private:
    FloatSize m_scrollableAreaSize;
    FloatSize m_totalContentsSize;
    FloatSize m_totalContentsSizeForRubberBand;
    FloatSize m_reachableContentsSize;
    FloatPoint m_lastCommittedScrollPosition;
    LayoutRect m_parentRelativeScrollableRect;
    IntPoint m_scrollOrigin;
#if ENABLE(CSS_SCROLL_SNAP)
    ScrollSnapOffsetsInfo<float> m_snapOffsetsInfo;
    unsigned m_currentHorizontalSnapPointIndex { 0 };
    unsigned m_currentVerticalSnapPointIndex { 0 };
#endif
    ScrollableAreaParameters m_scrollableAreaParameters;
    bool m_expectsWheelEventTestTrigger { false };

#if PLATFORM(COCOA)
    RetainPtr<CALayer> m_scrollContainerLayer;
    RetainPtr<CALayer> m_scrolledContentsLayer;
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_SCROLLING_NODE(ScrollingTreeScrollingNode, isScrollingNode())

#endif // ENABLE(ASYNC_SCROLLING)
