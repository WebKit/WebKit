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

#ifndef AsyncScrollingCoordinator_h
#define AsyncScrollingCoordinator_h

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingCoordinator.h"

#include "ScrollingTree.h"
#include "Timer.h"
#include <wtf/OwnPtr.h>
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
    static PassRefPtr<AsyncScrollingCoordinator> create(Page*);
    virtual ~AsyncScrollingCoordinator();

    ScrollingTree* scrollingTree() const { return m_scrollingTree.get(); }

    virtual PassOwnPtr<ScrollingTreeNode> createScrollingTreeNode(ScrollingNodeType, ScrollingNodeID) = 0;

    void scrollingStateTreePropertiesChanged();

    void scheduleUpdateScrollPositionAfterAsyncScroll(ScrollingNodeID, const FloatPoint&, bool programmaticScroll, SetOrSyncScrollingLayerPosition);

protected:
    AsyncScrollingCoordinator(Page*);

    void setScrollingTree(PassRefPtr<ScrollingTree> scrollingTree) { m_scrollingTree = scrollingTree; }

    ScrollingStateTree* scrollingStateTree() { return m_scrollingStateTree.get(); }

    PassRefPtr<ScrollingTree> releaseScrollingTree() { return m_scrollingTree.release(); }

    void updateScrollPositionAfterAsyncScroll(ScrollingNodeID, const FloatPoint&, bool programmaticScroll, SetOrSyncScrollingLayerPosition);

private:
    virtual bool isAsyncScrollingCoordinator() const override { return true; }

    virtual bool supportsFixedPositionLayers() const override { return true; }
    virtual bool hasVisibleSlowRepaintViewportConstrainedObjects(FrameView*) const override { return false; }

    virtual void frameViewLayoutUpdated(FrameView*) override;
    virtual void frameViewRootLayerDidChange(FrameView*) override;
    virtual void frameViewNonFastScrollableRegionChanged(FrameView*) override;

    virtual bool requestScrollPositionUpdate(FrameView*, const IntPoint&) override;

    virtual ScrollingNodeID attachToStateTree(ScrollingNodeType, ScrollingNodeID newNodeID, ScrollingNodeID parentID) override;
    virtual void detachFromStateTree(ScrollingNodeID) override;
    virtual void clearStateTree() override;

    virtual void updateViewportConstrainedNode(ScrollingNodeID, const ViewportConstraints&, GraphicsLayer*) override;
    virtual void updateScrollingNode(ScrollingNodeID, GraphicsLayer*, GraphicsLayer* scrolledContentsLayer, GraphicsLayer* counterScrollingLayer) override;
    virtual String scrollingStateTreeAsText() const override;
    virtual bool isRubberBandInProgress() const override;
    virtual void setScrollPinningBehavior(ScrollPinningBehavior) override;

    virtual void syncChildPositions(const LayoutRect& viewportRect) override;
    virtual void scrollableAreaScrollbarLayerDidChange(ScrollableArea*, ScrollbarOrientation) override;

    virtual void recomputeWheelEventHandlerCountForFrameView(FrameView*) override;
    virtual void setSynchronousScrollingReasons(SynchronousScrollingReasons) override;

    virtual void scheduleTreeStateCommit() = 0;

    void ensureRootStateNodeForFrameView(FrameView*);
    void updateMainFrameScrollLayerPosition();

    // FIXME: move somewhere else?
    void setScrollbarPaintersFromScrollbarsForNode(Scrollbar* verticalScrollbar, Scrollbar* horizontalScrollbar, ScrollingStateScrollingNode*);

    void updateScrollPositionAfterAsyncScrollTimerFired(Timer<AsyncScrollingCoordinator>*);

    Timer<AsyncScrollingCoordinator> m_updateNodeScrollPositionTimer;

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

    OwnPtr<ScrollingStateTree> m_scrollingStateTree;
    RefPtr<ScrollingTree> m_scrollingTree;
};

SCROLLING_COORDINATOR_TYPE_CASTS(AsyncScrollingCoordinator, isAsyncScrollingCoordinator());

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)

#endif // AsyncScrollingCoordinator_h
