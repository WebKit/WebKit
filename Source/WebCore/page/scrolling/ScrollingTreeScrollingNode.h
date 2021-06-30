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
#include "ScrollableArea.h"
#include "ScrollingTree.h"
#include "ScrollingTreeNode.h"

namespace WebCore {

class ScrollingTree;
class ScrollingStateScrollingNode;
struct WheelEventHandlingResult;

class WEBCORE_EXPORT ScrollingTreeScrollingNode : public ScrollingTreeNode {
    friend class ScrollingTreeScrollingNodeDelegate;
#if PLATFORM(MAC)
    friend class ScrollingTreeScrollingNodeDelegateMac;
#endif
    friend class ScrollingTree;

public:
    virtual ~ScrollingTreeScrollingNode();

    void commitStateBeforeChildren(const ScrollingStateNode&) override;
    void commitStateAfterChildren(const ScrollingStateNode&) override;
    void didCompleteCommitForNode() final;

    virtual bool canHandleWheelEvent(const PlatformWheelEvent&, EventTargeting) const;
    virtual WheelEventHandlingResult handleWheelEvent(const PlatformWheelEvent&, EventTargeting = EventTargeting::Propagate);

    FloatPoint currentScrollPosition() const { return m_currentScrollPosition; }
    FloatPoint currentScrollOffset() const { return ScrollableArea::scrollOffsetFromPosition(m_currentScrollPosition, toFloatSize(m_scrollOrigin)); }
    FloatPoint lastCommittedScrollPosition() const { return m_lastCommittedScrollPosition; }
    FloatSize scrollDeltaSinceLastCommit() const { return m_currentScrollPosition - m_lastCommittedScrollPosition; }

    const IntPoint& scrollOrigin() const { return m_scrollOrigin; }

    RectEdges<bool> edgePinnedState() const;

    bool isUserScrollProgress() const;
    void setUserScrollInProgress(bool);

    bool isScrollSnapInProgress() const;
    void setScrollSnapInProgress(bool);

    // These are imperative; they adjust the scrolling layers.
    void scrollTo(const FloatPoint&, ScrollType = ScrollType::User, ScrollClamping = ScrollClamping::Clamped);
    void scrollBy(const FloatSize&, ScrollClamping = ScrollClamping::Clamped);

    virtual void stopScrollAnimations() { };

    void wasScrolledByDelegatedScrolling(const FloatPoint& position, std::optional<FloatRect> overrideLayoutViewport = { }, ScrollingLayerPositionAction = ScrollingLayerPositionAction::Sync);
    
#if ENABLE(SCROLLING_THREAD)
    OptionSet<SynchronousScrollingReason> synchronousScrollingReasons() const { return m_synchronousScrollingReasons; }
    void addSynchronousScrollingReason(SynchronousScrollingReason reason) { m_synchronousScrollingReasons.add(reason); }
    bool hasSynchronousScrollingReasons() const { return !m_synchronousScrollingReasons.isEmpty(); }
#endif

    const FloatSize& scrollableAreaSize() const { return m_scrollableAreaSize; }
    const FloatSize& totalContentsSize() const { return m_totalContentsSize; }

    bool horizontalScrollbarHiddenByStyle() const { return m_scrollableAreaParameters.horizontalScrollbarHiddenByStyle; }
    bool verticalScrollbarHiddenByStyle() const { return m_scrollableAreaParameters.verticalScrollbarHiddenByStyle; }
    bool canHaveHorizontalScrollbar() const { return m_scrollableAreaParameters.horizontalScrollbarMode != ScrollbarAlwaysOff; }
    bool canHaveVerticalScrollbar() const { return m_scrollableAreaParameters.verticalScrollbarMode != ScrollbarAlwaysOff; }
    bool canHaveScrollbars() const { return m_scrollableAreaParameters.horizontalScrollbarMode != ScrollbarAlwaysOff || m_scrollableAreaParameters.verticalScrollbarMode != ScrollbarAlwaysOff; }

    const FloatScrollSnapOffsetsInfo& snapOffsetsInfo() const;
    std::optional<unsigned> currentHorizontalSnapPointIndex() const;
    std::optional<unsigned> currentVerticalSnapPointIndex() const;
    void setCurrentHorizontalSnapPointIndex(std::optional<unsigned>);
    void setCurrentVerticalSnapPointIndex(std::optional<unsigned>);

    bool useDarkAppearanceForScrollbars() const { return m_scrollableAreaParameters.useDarkAppearanceForScrollbars; }

    bool eventCanScrollContents(const PlatformWheelEvent&) const;
    
    bool scrolledSinceLastCommit() const { return m_scrolledSinceLastCommit; }

    const LayerRepresentation& scrollContainerLayer() const { return m_scrollContainerLayer; }
    const LayerRepresentation& scrolledContentsLayer() const { return m_scrolledContentsLayer; }

protected:
    ScrollingTreeScrollingNode(ScrollingTree&, ScrollingNodeType, ScrollingNodeID);

    virtual FloatPoint minimumScrollPosition() const;
    virtual FloatPoint maximumScrollPosition() const;

    FloatPoint clampScrollPosition(const FloatPoint&) const;
    
    virtual void willDoProgrammaticScroll(const FloatPoint&) { }
    
    virtual FloatPoint adjustedScrollPosition(const FloatPoint&, ScrollClamping = ScrollClamping::Clamped) const;

    virtual void currentScrollPositionChanged(ScrollType, ScrollingLayerPositionAction = ScrollingLayerPositionAction::Sync);
    virtual void updateViewportForCurrentScrollPosition(std::optional<FloatRect> = { }) { }
    virtual bool scrollPositionAndLayoutViewportMatch(const FloatPoint& position, std::optional<FloatRect> overrideLayoutViewport);

    virtual void repositionScrollingLayers() { }
    virtual void repositionRelatedLayers() { }

    void applyLayerPositions() override;

    const FloatSize& reachableContentsSize() const { return m_reachableContentsSize; }
    
    bool isLatchedNode() const;

    // If the totalContentsSize changes in the middle of a rubber-band, we still want to use the old totalContentsSize for the sake of
    // computing the stretchAmount(). Using the old value will keep the animation smooth. When there is no rubber-band in progress at
    // all, m_totalContentsSizeForRubberBand should be equivalent to m_totalContentsSize.
    const FloatSize& totalContentsSizeForRubberBand() const { return m_totalContentsSizeForRubberBand; }
    void setTotalContentsSizeForRubberBand(const FloatSize& totalContentsSizeForRubberBand) { m_totalContentsSizeForRubberBand = totalContentsSizeForRubberBand; }

    ScrollElasticity horizontalScrollElasticity() const { return m_scrollableAreaParameters.horizontalScrollElasticity; }
    ScrollElasticity verticalScrollElasticity() const { return m_scrollableAreaParameters.verticalScrollElasticity; }

    bool allowsHorizontalScrolling() const { return m_scrollableAreaParameters.allowsHorizontalScrolling; }
    bool allowsVerticalScrolling() const { return m_scrollableAreaParameters.allowsVerticalScrolling; }

    void dumpProperties(WTF::TextStream&, ScrollingStateTreeAsTextBehavior) const override;

private:
    FloatSize m_scrollableAreaSize;
    FloatSize m_totalContentsSize;
    FloatSize m_totalContentsSizeForRubberBand;
    FloatSize m_reachableContentsSize;
    FloatPoint m_lastCommittedScrollPosition;
    FloatPoint m_currentScrollPosition;
    IntPoint m_scrollOrigin;
    FloatScrollSnapOffsetsInfo m_snapOffsetsInfo;
    std::optional<unsigned> m_currentHorizontalSnapPointIndex;
    std::optional<unsigned> m_currentVerticalSnapPointIndex;
    ScrollableAreaParameters m_scrollableAreaParameters;
#if ENABLE(SCROLLING_THREAD)
    OptionSet<SynchronousScrollingReason> m_synchronousScrollingReasons;
#endif
    bool m_isFirstCommit { true };
    bool m_scrolledSinceLastCommit { false };

    LayerRepresentation m_scrollContainerLayer;
    LayerRepresentation m_scrolledContentsLayer;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_SCROLLING_NODE(ScrollingTreeScrollingNode, isScrollingNode())

#endif // ENABLE(ASYNC_SCROLLING)
