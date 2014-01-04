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

protected:
    AsyncScrollingCoordinator(Page*);

    void setScrollingTree(PassRefPtr<ScrollingTree> scrollingTree) { m_scrollingTree = scrollingTree; }

    ScrollingStateTree* scrollingStateTree() { return m_scrollingStateTree.get(); }

    PassRefPtr<ScrollingTree> releaseScrollingTree() { return m_scrollingTree.release(); }

private:
    virtual bool isAsyncScrollingCoordinator() const OVERRIDE { return true; }

    virtual bool supportsFixedPositionLayers() const OVERRIDE { return true; }
    virtual bool hasVisibleSlowRepaintViewportConstrainedObjects(FrameView*) const OVERRIDE { return false; }

    virtual void frameViewLayoutUpdated(FrameView*) OVERRIDE;
    virtual void frameViewRootLayerDidChange(FrameView*) OVERRIDE;

    virtual bool requestScrollPositionUpdate(FrameView*, const IntPoint&) OVERRIDE;

    virtual ScrollingNodeID attachToStateTree(ScrollingNodeType, ScrollingNodeID newNodeID, ScrollingNodeID parentID) OVERRIDE;
    virtual void detachFromStateTree(ScrollingNodeID) OVERRIDE;
    virtual void clearStateTree() OVERRIDE;

    virtual void updateViewportConstrainedNode(ScrollingNodeID, const ViewportConstraints&, GraphicsLayer*) OVERRIDE;
    virtual void updateScrollingNode(ScrollingNodeID, GraphicsLayer* scrollLayer, GraphicsLayer* counterScrollingLayer) OVERRIDE;
    virtual String scrollingStateTreeAsText() const OVERRIDE;
    virtual bool isRubberBandInProgress() const OVERRIDE;
    virtual void setScrollPinningBehavior(ScrollPinningBehavior) OVERRIDE;

    virtual void syncChildPositions(const LayoutRect& viewportRect) OVERRIDE;
    virtual void scrollableAreaScrollbarLayerDidChange(ScrollableArea*, ScrollbarOrientation) OVERRIDE;

    virtual void recomputeWheelEventHandlerCountForFrameView(FrameView*) OVERRIDE;
    virtual void setSynchronousScrollingReasons(SynchronousScrollingReasons) OVERRIDE;

    virtual void scheduleTreeStateCommit() = 0;

    void ensureRootStateNodeForFrameView(FrameView*);
    void updateMainFrameScrollLayerPosition();

    void setScrollLayerForNode(GraphicsLayer*, ScrollingStateNode*);
    void setCounterScrollingLayerForNode(GraphicsLayer*, ScrollingStateScrollingNode*);
    void setHeaderLayerForNode(GraphicsLayer*, ScrollingStateScrollingNode*);
    void setFooterLayerForNode(GraphicsLayer*, ScrollingStateScrollingNode*);
    void setNonFastScrollableRegionForNode(const Region&, ScrollingStateScrollingNode*);
    void setWheelEventHandlerCountForNode(unsigned, ScrollingStateScrollingNode*);
    void setScrollBehaviorForFixedElementsForNode(ScrollBehaviorForFixedElements, ScrollingStateScrollingNode*);
    // FIXME: move somewhere else?
    void setScrollbarPaintersFromScrollbarsForNode(Scrollbar* verticalScrollbar, Scrollbar* horizontalScrollbar, ScrollingStateScrollingNode*);

    OwnPtr<ScrollingStateTree> m_scrollingStateTree;
    RefPtr<ScrollingTree> m_scrollingTree;
};

SCROLLING_COORDINATOR_TYPE_CASTS(AsyncScrollingCoordinator, isAsyncScrollingCoordinator());

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)

#endif // AsyncScrollingCoordinator_h
