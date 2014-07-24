/*
 * Copyright (C) 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef ScrollingTree_h
#define ScrollingTree_h

#if ENABLE(ASYNC_SCROLLING)

#include "PlatformWheelEvent.h"
#include "Region.h"
#include "ScrollingCoordinator.h"
#include <wtf/Functional.h>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class IntPoint;
class ScrollingStateTree;
class ScrollingStateNode;
class ScrollingTreeNode;
class ScrollingTreeScrollingNode;

class ScrollingTree : public ThreadSafeRefCounted<ScrollingTree> {
public:
    ScrollingTree();
    virtual ~ScrollingTree();

    enum EventResult {
        DidNotHandleEvent,
        DidHandleEvent,
        SendToMainThread
    };
    
    virtual bool isThreadedScrollingTree() const { return false; }
    virtual bool isRemoteScrollingTree() const { return false; }
    virtual bool isScrollingTreeIOS() const { return false; }

    virtual EventResult tryToHandleWheelEvent(const PlatformWheelEvent&) = 0;
    bool shouldHandleWheelEventSynchronously(const PlatformWheelEvent&);
    
    void setMainFrameIsRubberBanding(bool);
    bool isRubberBandInProgress();

    virtual void invalidate() { }
    virtual void commitNewTreeState(PassOwnPtr<ScrollingStateTree>);

    void setMainFramePinState(bool pinnedToTheLeft, bool pinnedToTheRight, bool pinnedToTheTop, bool pinnedToTheBottom);

    virtual PassRefPtr<ScrollingTreeNode> createScrollingTreeNode(ScrollingNodeType, ScrollingNodeID) = 0;

    // Called after a scrolling tree node has handled a scroll and updated its layers.
    // Updates FrameView/RenderLayer scrolling state and GraphicsLayers.
    virtual void scrollingTreeNodeDidScroll(ScrollingNodeID, const FloatPoint& scrollPosition, SetOrSyncScrollingLayerPosition = SyncScrollingLayerPosition) = 0;

    // Called for requested scroll position updates.
    virtual void scrollingTreeNodeRequestsScroll(ScrollingNodeID, const FloatPoint& /*scrollPosition*/, bool /*representsProgrammaticScroll*/) { }

    // Delegated scrolling/zooming has caused the viewport to change, so update viewport-constrained layers
    // (but don't cause scroll events to be fired).
    virtual void viewportChangedViaDelegatedScrolling(ScrollingNodeID, const WebCore::FloatRect& fixedPositionRect, double scale);

    // Delegated scrolling has scrolled a node. Update layer positions on descendant tree nodes,
    // and call scrollingTreeNodeDidScroll().
    virtual void scrollPositionChangedViaDelegatedScrolling(ScrollingNodeID, const WebCore::FloatPoint& scrollPosition, bool inUserInteration);

    FloatPoint mainFrameScrollPosition();
    
#if PLATFORM(IOS)
    virtual FloatRect fixedPositionRect() = 0;
    virtual void scrollingTreeNodeWillStartPanGesture() { }
    virtual void scrollingTreeNodeWillStartScroll() { }
    virtual void scrollingTreeNodeDidEndScroll() { }
#endif

    bool isPointInNonFastScrollableRegion(IntPoint);
    
#if PLATFORM(MAC)
    virtual void handleWheelEventPhase(PlatformWheelEventPhase) = 0;
#endif

    // Can be called from any thread. Will update what edges allow rubber-banding.
    void setCanRubberBandState(bool canRubberBandAtLeft, bool canRubberBandAtRight, bool canRubberBandAtTop, bool canRubberBandAtBottom);

    bool rubberBandsAtLeft();
    bool rubberBandsAtRight();
    bool rubberBandsAtTop();
    bool rubberBandsAtBottom();
    bool isHandlingProgrammaticScroll();
    
    void setScrollPinningBehavior(ScrollPinningBehavior);
    ScrollPinningBehavior scrollPinningBehavior();

    bool willWheelEventStartSwipeGesture(const PlatformWheelEvent&);

    void setScrollingPerformanceLoggingEnabled(bool flag);
    bool scrollingPerformanceLoggingEnabled();

    ScrollingTreeNode* rootNode() const { return m_rootNode.get(); }

    ScrollingNodeID latchedNode();
    void setLatchedNode(ScrollingNodeID);
    void clearLatchedNode();

    bool hasLatchedNode() const { return m_latchedNode; }
    void setOrClearLatchedNode(const PlatformWheelEvent&, ScrollingNodeID);

    bool hasFixedOrSticky() const { return !!m_fixedOrStickyNodeCount; }
    void fixedOrStickyNodeAdded() { ++m_fixedOrStickyNodeCount; }
    void fixedOrStickyNodeRemoved()
    {
        ASSERT(m_fixedOrStickyNodeCount);
        --m_fixedOrStickyNodeCount;
    }
    
protected:
    void setMainFrameScrollPosition(FloatPoint);
    virtual void handleWheelEvent(const PlatformWheelEvent&);

private:
    void removeDestroyedNodes(const ScrollingStateTree&);
    
    typedef HashMap<ScrollingNodeID, RefPtr<ScrollingTreeNode>> OrphanScrollingNodeMap;
    void updateTreeFromStateNode(const ScrollingStateNode*, OrphanScrollingNodeMap&);

    ScrollingTreeNode* nodeForID(ScrollingNodeID) const;

    RefPtr<ScrollingTreeNode> m_rootNode;

    typedef HashMap<ScrollingNodeID, ScrollingTreeNode*> ScrollingTreeNodeMap;
    ScrollingTreeNodeMap m_nodeMap;

    Mutex m_mutex;
    Region m_nonFastScrollableRegion;
    FloatPoint m_mainFrameScrollPosition;
    bool m_hasWheelEventHandlers;

    Mutex m_swipeStateMutex;
    bool m_rubberBandsAtLeft;
    bool m_rubberBandsAtRight;
    bool m_rubberBandsAtTop;
    bool m_rubberBandsAtBottom;
    bool m_mainFramePinnedToTheLeft;
    bool m_mainFramePinnedToTheRight;
    bool m_mainFramePinnedToTheTop;
    bool m_mainFramePinnedToTheBottom;
    bool m_mainFrameIsRubberBanding;
    ScrollPinningBehavior m_scrollPinningBehavior;
    ScrollingNodeID m_latchedNode;

    bool m_scrollingPerformanceLoggingEnabled;
    
    bool m_isHandlingProgrammaticScroll;
    unsigned m_fixedOrStickyNodeCount;
};

#define SCROLLING_TREE_TYPE_CASTS(ToValueTypeName, predicate) \
    TYPE_CASTS_BASE(ToValueTypeName, WebCore::ScrollingTree, value, value->predicate, value.predicate)

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)

#endif // ScrollingTree_h
