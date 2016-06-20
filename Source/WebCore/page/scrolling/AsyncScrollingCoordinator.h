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

#ifndef AsyncScrollingCoordinator_h
#define AsyncScrollingCoordinator_h

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingCoordinator.h"

#include "ScrollingTree.h"
#include "Timer.h"
#include <wtf/PassRefPtr.h>
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

    WEBCORE_EXPORT void scheduleUpdateScrollPositionAfterAsyncScroll(ScrollingNodeID, const FloatPoint&, bool programmaticScroll, SetOrSyncScrollingLayerPosition);

#if PLATFORM(COCOA)
    WEBCORE_EXPORT void setActiveScrollSnapIndices(ScrollingNodeID, unsigned horizontalIndex, unsigned verticalIndex);
    void deferTestsForReason(WheelEventTestTrigger::ScrollableAreaIdentifier, WheelEventTestTrigger::DeferTestTriggerReason) const;
    void removeTestDeferralForReason(WheelEventTestTrigger::ScrollableAreaIdentifier, WheelEventTestTrigger::DeferTestTriggerReason) const;
#endif

#if ENABLE(CSS_SCROLL_SNAP)
    WEBCORE_EXPORT void updateScrollSnapPropertiesWithFrameView(const FrameView&) override;
#endif

protected:
    WEBCORE_EXPORT AsyncScrollingCoordinator(Page*);

    void setScrollingTree(PassRefPtr<ScrollingTree> scrollingTree) { m_scrollingTree = scrollingTree; }

    ScrollingStateTree* scrollingStateTree() { return m_scrollingStateTree.get(); }

    PassRefPtr<ScrollingTree> releaseScrollingTree() { return WTFMove(m_scrollingTree); }

    void updateScrollPositionAfterAsyncScroll(ScrollingNodeID, const FloatPoint&, bool programmaticScroll, SetOrSyncScrollingLayerPosition);

    WEBCORE_EXPORT String scrollingStateTreeAsText() const override;
    WEBCORE_EXPORT void willCommitTree() override;

    bool eventTrackingRegionsDirty() const { return m_eventTrackingRegionsDirty; }

private:
    bool isAsyncScrollingCoordinator() const override { return true; }

    bool supportsFixedPositionLayers() const override { return true; }
    bool hasVisibleSlowRepaintViewportConstrainedObjects(const FrameView&) const override { return false; }

    WEBCORE_EXPORT void frameViewLayoutUpdated(FrameView&) override;
    WEBCORE_EXPORT void frameViewRootLayerDidChange(FrameView&) override;
    WEBCORE_EXPORT void frameViewEventTrackingRegionsChanged(FrameView&) override;

    WEBCORE_EXPORT bool requestScrollPositionUpdate(FrameView&, const IntPoint&) override;

    WEBCORE_EXPORT ScrollingNodeID attachToStateTree(ScrollingNodeType, ScrollingNodeID newNodeID, ScrollingNodeID parentID) override;
    WEBCORE_EXPORT void detachFromStateTree(ScrollingNodeID) override;
    WEBCORE_EXPORT void clearStateTree() override;
    
    WEBCORE_EXPORT void updateViewportConstrainedNode(ScrollingNodeID, const ViewportConstraints&, GraphicsLayer*) override;
    
    WEBCORE_EXPORT void updateFrameScrollingNode(ScrollingNodeID, GraphicsLayer* scrollLayer, GraphicsLayer* scrolledContentsLayer, GraphicsLayer* counterScrollingLayer, GraphicsLayer* insetClipLayer, const ScrollingGeometry* = nullptr) override;
    WEBCORE_EXPORT void updateOverflowScrollingNode(ScrollingNodeID, GraphicsLayer* scrollLayer, GraphicsLayer* scrolledContentsLayer, const ScrollingGeometry* = nullptr) override;
    
    bool isRubberBandInProgress() const override;
    void setScrollPinningBehavior(ScrollPinningBehavior) override;

#if ENABLE(CSS_SCROLL_SNAP)
    bool isScrollSnapInProgress() const override;
#endif

    WEBCORE_EXPORT void syncChildPositions(const LayoutRect& viewportRect) override;
    WEBCORE_EXPORT void scrollableAreaScrollbarLayerDidChange(ScrollableArea&, ScrollbarOrientation) override;

    WEBCORE_EXPORT void setSynchronousScrollingReasons(SynchronousScrollingReasons) override;

    virtual void scheduleTreeStateCommit() = 0;

    void ensureRootStateNodeForFrameView(FrameView&);
    void updateMainFrameScrollLayerPosition();

    void updateScrollPositionAfterAsyncScrollTimerFired();
    void setEventTrackingRegionsDirty();
    void updateEventTrackingRegions();
    
    FrameView* frameViewForScrollingNode(ScrollingNodeID) const;

    Timer m_updateNodeScrollPositionTimer;

    struct ScheduledScrollUpdate {
        ScheduledScrollUpdate()
            : nodeID(0)
            , isProgrammaticScroll(false)
            , updateLayerPositionAction(SyncScrollingLayerPosition)
        { }

        ScheduledScrollUpdate(ScrollingNodeID scrollingNodeID, FloatPoint point, bool isProgrammatic, SetOrSyncScrollingLayerPosition udpateAction)
            : nodeID(scrollingNodeID)
            , scrollPosition(point)
            , isProgrammaticScroll(isProgrammatic)
            , updateLayerPositionAction(udpateAction)
        { }

        ScrollingNodeID nodeID;
        FloatPoint scrollPosition;
        bool isProgrammaticScroll;
        SetOrSyncScrollingLayerPosition updateLayerPositionAction;
        
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

#endif // AsyncScrollingCoordinator_h
