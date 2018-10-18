/*
 * Copyright (C) 2012-2015 Apple Inc. All rights reserved.
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

#include "PlatformWheelEvent.h"
#include "Region.h"
#include "ScrollingCoordinator.h"
#include "WheelEventTestTrigger.h"
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

class IntPoint;
class ScrollingStateTree;
class ScrollingStateNode;
class ScrollingTreeNode;
class ScrollingTreeScrollingNode;

class ScrollingTree : public ThreadSafeRefCounted<ScrollingTree> {
public:
    WEBCORE_EXPORT ScrollingTree();
    WEBCORE_EXPORT virtual ~ScrollingTree();

    enum EventResult {
        DidNotHandleEvent,
        DidHandleEvent,
        SendToMainThread
    };
    
    virtual bool isThreadedScrollingTree() const { return false; }
    virtual bool isRemoteScrollingTree() const { return false; }
    virtual bool isScrollingTreeIOS() const { return false; }

    bool visualViewportEnabled() const { return m_visualViewportEnabled; }

    virtual EventResult tryToHandleWheelEvent(const PlatformWheelEvent&) = 0;
    WEBCORE_EXPORT bool shouldHandleWheelEventSynchronously(const PlatformWheelEvent&);
    
    void setMainFrameIsRubberBanding(bool);
    bool isRubberBandInProgress();
    void setMainFrameIsScrollSnapping(bool);
    bool isScrollSnapInProgress();

    virtual void invalidate() { }
    WEBCORE_EXPORT virtual void commitTreeState(std::unique_ptr<ScrollingStateTree>);

    void setMainFramePinState(bool pinnedToTheLeft, bool pinnedToTheRight, bool pinnedToTheTop, bool pinnedToTheBottom);

    virtual Ref<ScrollingTreeNode> createScrollingTreeNode(ScrollingNodeType, ScrollingNodeID) = 0;

    // Called after a scrolling tree node has handled a scroll and updated its layers.
    // Updates FrameView/RenderLayer scrolling state and GraphicsLayers.
    virtual void scrollingTreeNodeDidScroll(ScrollingNodeID, const FloatPoint& scrollPosition, const std::optional<FloatPoint>& layoutViewportOrigin, ScrollingLayerPositionAction = ScrollingLayerPositionAction::Sync) = 0;

    // Called for requested scroll position updates.
    virtual void scrollingTreeNodeRequestsScroll(ScrollingNodeID, const FloatPoint& /*scrollPosition*/, bool /*representsProgrammaticScroll*/) { }

    // Delegated scrolling/zooming has caused the viewport to change, so update viewport-constrained layers
    // (but don't cause scroll events to be fired).
    WEBCORE_EXPORT virtual void viewportChangedViaDelegatedScrolling(ScrollingNodeID, const WebCore::FloatRect& fixedPositionRect, double scale);

    // Delegated scrolling has scrolled a node. Update layer positions on descendant tree nodes,
    // and call scrollingTreeNodeDidScroll().
    WEBCORE_EXPORT virtual void scrollPositionChangedViaDelegatedScrolling(ScrollingNodeID, const WebCore::FloatPoint& scrollPosition, bool inUserInteraction);

    virtual void reportSynchronousScrollingReasonsChanged(MonotonicTime, SynchronousScrollingReasons) { }
    virtual void reportExposedUnfilledArea(MonotonicTime, unsigned /* unfilledArea */) { }

    FloatPoint mainFrameScrollPosition();
    
#if PLATFORM(IOS_FAMILY)
    virtual FloatRect fixedPositionRect() = 0;
    virtual void scrollingTreeNodeWillStartPanGesture() { }
    virtual void scrollingTreeNodeWillStartScroll() { }
    virtual void scrollingTreeNodeDidEndScroll() { }
#endif

    WEBCORE_EXPORT TrackingType eventTrackingTypeForPoint(const AtomicString& eventName, IntPoint);
    
#if PLATFORM(MAC)
    virtual void handleWheelEventPhase(PlatformWheelEventPhase) = 0;
    virtual void setActiveScrollSnapIndices(ScrollingNodeID, unsigned /*horizontalIndex*/, unsigned /*verticalIndex*/) { }
    virtual void deferTestsForReason(WheelEventTestTrigger::ScrollableAreaIdentifier, WheelEventTestTrigger::DeferTestTriggerReason) { }
    virtual void removeTestDeferralForReason(WheelEventTestTrigger::ScrollableAreaIdentifier, WheelEventTestTrigger::DeferTestTriggerReason) { }
#endif

#if PLATFORM(COCOA)
    WEBCORE_EXPORT virtual void currentSnapPointIndicesDidChange(ScrollingNodeID, unsigned horizontal, unsigned vertical) = 0;
#endif

    // Can be called from any thread. Will update what edges allow rubber-banding.
    WEBCORE_EXPORT void setCanRubberBandState(bool canRubberBandAtLeft, bool canRubberBandAtRight, bool canRubberBandAtTop, bool canRubberBandAtBottom);

    bool rubberBandsAtLeft();
    bool rubberBandsAtRight();
    bool rubberBandsAtTop();
    bool rubberBandsAtBottom();
    bool isHandlingProgrammaticScroll();
    
    void setScrollPinningBehavior(ScrollPinningBehavior);
    ScrollPinningBehavior scrollPinningBehavior();

    WEBCORE_EXPORT bool willWheelEventStartSwipeGesture(const PlatformWheelEvent&);

    WEBCORE_EXPORT void setScrollingPerformanceLoggingEnabled(bool flag);
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
    
    WEBCORE_EXPORT String scrollingTreeAsText();
    
protected:
    void setMainFrameScrollPosition(FloatPoint);
    void setVisualViewportEnabled(bool b) { m_visualViewportEnabled = b; }

    WEBCORE_EXPORT virtual void handleWheelEvent(const PlatformWheelEvent&);

private:
    void removeDestroyedNodes(const ScrollingStateTree&);
    
    typedef HashMap<ScrollingNodeID, RefPtr<ScrollingTreeNode>> OrphanScrollingNodeMap;
    void updateTreeFromStateNode(const ScrollingStateNode*, OrphanScrollingNodeMap&);

    ScrollingTreeNode* nodeForID(ScrollingNodeID) const;

    RefPtr<ScrollingTreeNode> m_rootNode;

    typedef HashMap<ScrollingNodeID, ScrollingTreeNode*> ScrollingTreeNodeMap;
    ScrollingTreeNodeMap m_nodeMap;

    Lock m_mutex;
    EventTrackingRegions m_eventTrackingRegions;
    FloatPoint m_mainFrameScrollPosition;

    Lock m_swipeStateMutex;
    ScrollPinningBehavior m_scrollPinningBehavior { DoNotPin };
    ScrollingNodeID m_latchedNode { 0 };

    unsigned m_fixedOrStickyNodeCount { 0 };

    bool m_rubberBandsAtLeft { true };
    bool m_rubberBandsAtRight { true };
    bool m_rubberBandsAtTop { true };
    bool m_rubberBandsAtBottom { true };
    bool m_mainFramePinnedToTheLeft { true };
    bool m_mainFramePinnedToTheRight { true };
    bool m_mainFramePinnedToTheTop { true };
    bool m_mainFramePinnedToTheBottom { true };
    bool m_mainFrameIsRubberBanding { false };
    bool m_mainFrameIsScrollSnapping { false };
    bool m_scrollingPerformanceLoggingEnabled { false };
    bool m_isHandlingProgrammaticScroll { false };
    bool m_visualViewportEnabled { false };
};
    
} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_SCROLLING_TREE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(ToValueTypeName) \
    static bool isType(const WebCore::ScrollingTree& tree) { return tree.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
#endif // ENABLE(ASYNC_SCROLLING)
