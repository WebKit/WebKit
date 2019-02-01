/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

#include "ScrollingCoordinator.h"
#include "ScrollingTree.h"
#include "Timer.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class Page;
class Scrollbar;
class ScrollingStateNode;
class ScrollingStateScrollingNode;
class ScrollingStateTree;

// ScrollingCoordinator subclass that maintains a ScrollingStateTree and a ScrollingTree,
// allowing asynchronous scrolling (in another thread or process).
class AsyncScrollingCoordinator : public ScrollingCoordinator {
public:
    static Ref<AsyncScrollingCoordinator> create(Page*);
    WEBCORE_EXPORT virtual ~AsyncScrollingCoordinator();

    ScrollingTree* scrollingTree() const { return m_scrollingTree.get(); }

    void scrollingStateTreePropertiesChanged();

    WEBCORE_EXPORT void scheduleUpdateScrollPositionAfterAsyncScroll(ScrollingNodeID, const FloatPoint&, const Optional<FloatPoint>& layoutViewportOrigin, bool programmaticScroll, ScrollingLayerPositionAction);

#if PLATFORM(COCOA)
    WEBCORE_EXPORT void setActiveScrollSnapIndices(ScrollingNodeID, unsigned horizontalIndex, unsigned verticalIndex);
    void deferTestsForReason(WheelEventTestTrigger::ScrollableAreaIdentifier, WheelEventTestTrigger::DeferTestTriggerReason) const;
    void removeTestDeferralForReason(WheelEventTestTrigger::ScrollableAreaIdentifier, WheelEventTestTrigger::DeferTestTriggerReason) const;
#endif

#if ENABLE(CSS_SCROLL_SNAP)
    WEBCORE_EXPORT void updateScrollSnapPropertiesWithFrameView(const FrameView&) override;
#endif

    WEBCORE_EXPORT void updateExpectsWheelEventTestTriggerWithFrameView(const FrameView&) override;

    void reportExposedUnfilledArea(MonotonicTime, unsigned unfilledArea);
    void reportSynchronousScrollingReasonsChanged(MonotonicTime, SynchronousScrollingReasons);

protected:
    WEBCORE_EXPORT AsyncScrollingCoordinator(Page*);

    void setScrollingTree(Ref<ScrollingTree>&& scrollingTree) { m_scrollingTree = WTFMove(scrollingTree); }

    ScrollingStateTree* scrollingStateTree() { return m_scrollingStateTree.get(); }

    RefPtr<ScrollingTree> releaseScrollingTree() { return WTFMove(m_scrollingTree); }

    void updateScrollPositionAfterAsyncScroll(ScrollingNodeID, const FloatPoint&, Optional<FloatPoint> layoutViewportOrigin, bool programmaticScroll, ScrollingLayerPositionAction);

    WEBCORE_EXPORT String scrollingStateTreeAsText(ScrollingStateTreeAsTextBehavior = ScrollingStateTreeAsTextBehaviorNormal) const override;
    WEBCORE_EXPORT void willCommitTree() override;

    bool eventTrackingRegionsDirty() const { return m_eventTrackingRegionsDirty; }

private:
    bool isAsyncScrollingCoordinator() const override { return true; }

    bool hasVisibleSlowRepaintViewportConstrainedObjects(const FrameView&) const override { return false; }
    
    bool visualViewportEnabled() const;
    bool asyncFrameOrOverflowScrollingEnabled() const;

    WEBCORE_EXPORT void frameViewLayoutUpdated(FrameView&) override;
    WEBCORE_EXPORT void frameViewRootLayerDidChange(FrameView&) override;
    WEBCORE_EXPORT void frameViewEventTrackingRegionsChanged(FrameView&) override;

    WEBCORE_EXPORT bool requestScrollPositionUpdate(FrameView&, const IntPoint&) override;

    WEBCORE_EXPORT ScrollingNodeID createNode(ScrollingNodeType, ScrollingNodeID newNodeID) override;
    WEBCORE_EXPORT ScrollingNodeID insertNode(ScrollingNodeType, ScrollingNodeID newNodeID, ScrollingNodeID parentID, size_t childIndex) override;
    WEBCORE_EXPORT void unparentNode(ScrollingNodeID) override;
    WEBCORE_EXPORT void unparentChildrenAndDestroyNode(ScrollingNodeID) override;
    WEBCORE_EXPORT void detachAndDestroySubtree(ScrollingNodeID) override;
    WEBCORE_EXPORT void clearAllNodes() override;

    WEBCORE_EXPORT ScrollingNodeID parentOfNode(ScrollingNodeID) const override;
    WEBCORE_EXPORT Vector<ScrollingNodeID> childrenOfNode(ScrollingNodeID) const override;

    WEBCORE_EXPORT void setNodeLayers(ScrollingNodeID, const NodeLayers&) override;
    WEBCORE_EXPORT void setScrollingNodeGeometry(ScrollingNodeID, const ScrollingGeometry&) override;
    WEBCORE_EXPORT void setViewportConstraintedNodeGeometry(ScrollingNodeID, const ViewportConstraints&) override;

    WEBCORE_EXPORT void reconcileScrollingState(FrameView&, const FloatPoint&, const LayoutViewportOriginOrOverrideRect&, bool programmaticScroll, ViewportRectStability, ScrollingLayerPositionAction) override;

    bool isRubberBandInProgress() const override;
    void setScrollPinningBehavior(ScrollPinningBehavior) override;

#if ENABLE(CSS_SCROLL_SNAP)
    bool isScrollSnapInProgress() const override;
#endif

    WEBCORE_EXPORT void reconcileViewportConstrainedLayerPositions(ScrollingNodeID, const LayoutRect& viewportRect, ScrollingLayerPositionAction) override;
    WEBCORE_EXPORT void scrollableAreaScrollbarLayerDidChange(ScrollableArea&, ScrollbarOrientation) override;

    WEBCORE_EXPORT void setSynchronousScrollingReasons(FrameView&, SynchronousScrollingReasons) final;

    virtual void scheduleTreeStateCommit() = 0;

    void ensureRootStateNodeForFrameView(FrameView&);
    void updateScrollLayerPosition(FrameView&);

    void updateScrollPositionAfterAsyncScrollTimerFired();
    void setEventTrackingRegionsDirty();
    void updateEventTrackingRegions();
    
    FrameView* frameViewForScrollingNode(ScrollingNodeID) const;

    Timer m_updateNodeScrollPositionTimer;

    struct ScheduledScrollUpdate {
        ScheduledScrollUpdate() = default;
        ScheduledScrollUpdate(ScrollingNodeID scrollingNodeID, FloatPoint point, Optional<FloatPoint> viewportOrigin, bool isProgrammatic, ScrollingLayerPositionAction udpateAction)
            : nodeID(scrollingNodeID)
            , scrollPosition(point)
            , layoutViewportOrigin(viewportOrigin)
            , isProgrammaticScroll(isProgrammatic)
            , updateLayerPositionAction(udpateAction)
        { }

        ScrollingNodeID nodeID { 0 };
        FloatPoint scrollPosition;
        Optional<FloatPoint> layoutViewportOrigin;
        bool isProgrammaticScroll { false };
        ScrollingLayerPositionAction updateLayerPositionAction { ScrollingLayerPositionAction::Sync };
        
        bool matchesUpdateType(const ScheduledScrollUpdate& other) const
        {
            return nodeID == other.nodeID
                && isProgrammaticScroll == other.isProgrammaticScroll
                && updateLayerPositionAction == other.updateLayerPositionAction;
        }
    };

    ScheduledScrollUpdate m_scheduledScrollUpdate;

    std::unique_ptr<ScrollingStateTree> m_scrollingStateTree;
    RefPtr<ScrollingTree> m_scrollingTree;

    bool m_eventTrackingRegionsDirty { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_SCROLLING_COORDINATOR(WebCore::AsyncScrollingCoordinator, isAsyncScrollingCoordinator());

#endif // ENABLE(ASYNC_SCROLLING)
