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

#include "EventTrackingRegions.h"
#include "PageIdentifier.h"
#include "PlatformWheelEvent.h"
#include "RectEdges.h"
#include "Region.h"
#include "ScrollTypes.h"
#include "ScrollingCoordinatorTypes.h"
#include "ScrollingTreeGestureState.h"
#include "ScrollingTreeLatchingController.h"
#include "WheelEventTestMonitor.h"
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

class IntPoint;
class ScrollingStateTree;
class ScrollingStateNode;
class ScrollingTreeFrameScrollingNode;
class ScrollingTreeNode;
class ScrollingTreeOverflowScrollProxyNode;
class ScrollingTreePositionedNode;
class ScrollingTreeScrollingNode;
enum class EventListenerRegionType : uint8_t;

using FramesPerSecond = unsigned;
using PlatformDisplayID = uint32_t;

struct WheelEventHandlingResult {
    OptionSet<WheelEventProcessingSteps> steps;
    bool wasHandled { false };
    bool needsMainThreadProcessing() const { return steps.containsAny({ WheelEventProcessingSteps::MainThreadForScrolling, WheelEventProcessingSteps::MainThreadForNonBlockingDOMEventDispatch, WheelEventProcessingSteps::MainThreadForBlockingDOMEventDispatch }); }

    static WheelEventHandlingResult handled(OptionSet<WheelEventProcessingSteps> steps = { })
    {
        return { steps, true };
    }
    static WheelEventHandlingResult unhandled(OptionSet<WheelEventProcessingSteps> steps = { })
    {
        return { steps, false };
    }
    static WheelEventHandlingResult result(bool handled)
    {
        return { { }, handled };
    }
};

enum class EventTargeting : uint8_t { NodeOnly, Propagate };

class ScrollingTree : public ThreadSafeRefCounted<ScrollingTree> {
    WTF_MAKE_FAST_ALLOCATED;
    friend class ScrollingTreeLatchingController;
public:
    WEBCORE_EXPORT ScrollingTree();
    WEBCORE_EXPORT virtual ~ScrollingTree();

    virtual bool isThreadedScrollingTree() const { return false; }
    virtual bool isScrollingTreeMac() const { return false; }
    virtual bool isRemoteScrollingTree() const { return false; }
    virtual bool isScrollingTreeIOS() const { return false; }

    // This implies that we'll do hit-testing in the scrolling tree.
    bool asyncFrameOrOverflowScrollingEnabled() const { return m_asyncFrameOrOverflowScrollingEnabled; }
    void setAsyncFrameOrOverflowScrollingEnabled(bool value) { m_asyncFrameOrOverflowScrollingEnabled = value; }

    bool wheelEventGesturesBecomeNonBlocking() const { return m_wheelEventGesturesBecomeNonBlocking; }
    void setWheelEventGesturesBecomeNonBlocking(bool value) { m_wheelEventGesturesBecomeNonBlocking = value; }

    bool scrollingPerformanceTestingEnabled() const { return m_scrollingPerformanceTestingEnabled; }
    void setScrollingPerformanceTestingEnabled(bool value) { m_scrollingPerformanceTestingEnabled = value; }

    WEBCORE_EXPORT bool isUserScrollInProgressAtEventLocation(const PlatformWheelEvent&);
    WEBCORE_EXPORT OptionSet<WheelEventProcessingSteps> determineWheelEventProcessing(const PlatformWheelEvent&);
    WEBCORE_EXPORT virtual WheelEventHandlingResult handleWheelEvent(const PlatformWheelEvent&, OptionSet<WheelEventProcessingSteps> = { });

    bool isRubberBandInProgressForNode(ScrollingNodeID);
    void setRubberBandingInProgressForNode(ScrollingNodeID, bool);

    bool isUserScrollInProgressForNode(ScrollingNodeID);
    void setUserScrollInProgressForNode(ScrollingNodeID, bool);
    void clearNodesWithUserScrollInProgress();

    bool isScrollSnapInProgressForNode(ScrollingNodeID);
    void setNodeScrollSnapInProgress(ScrollingNodeID, bool);

    bool isScrollAnimationInProgressForNode(ScrollingNodeID);
    void setScrollAnimationInProgressForNode(ScrollingNodeID, bool);

    bool hasNodeWithActiveScrollAnimations();

    virtual void invalidate() { }
    WEBCORE_EXPORT void commitTreeState(std::unique_ptr<ScrollingStateTree>&&);
    
    WEBCORE_EXPORT virtual void applyLayerPositions();
    WEBCORE_EXPORT void applyLayerPositionsAfterCommit();

    virtual Ref<ScrollingTreeNode> createScrollingTreeNode(ScrollingNodeType, ScrollingNodeID) = 0;
    
    WEBCORE_EXPORT ScrollingTreeNode* nodeForID(ScrollingNodeID) const;

    using VisitorFunction = Function<void(ScrollingNodeID, ScrollingNodeType, std::optional<FloatPoint> scrollPosition, std::optional<FloatPoint> layoutViewportOrigin, bool scrolledSinceLastCommit)>;
    void traverseScrollingTree(VisitorFunction&&);

    // Called after a scrolling tree node has handled a scroll and updated its layers.
    // Updates FrameView/RenderLayer scrolling state and GraphicsLayers.
    virtual void scrollingTreeNodeDidScroll(ScrollingTreeScrollingNode&, ScrollingLayerPositionAction = ScrollingLayerPositionAction::Sync) = 0;
    virtual void scrollingTreeNodeDidStopAnimatedScroll(ScrollingTreeScrollingNode&) { }

    // Called for requested scroll position updates. Returns true if handled.
    virtual bool scrollingTreeNodeRequestsScroll(ScrollingNodeID, const RequestedScrollData&) { return false; }

    // Delegated scrolling/zooming has caused the viewport to change, so update viewport-constrained layers
    WEBCORE_EXPORT void mainFrameViewportChangedViaDelegatedScrolling(const FloatPoint& scrollPosition, const WebCore::FloatRect& layoutViewport, double scale);

    void setNeedsApplyLayerPositionsAfterCommit() { m_needsApplyLayerPositionsAfterCommit = true; }

    void notifyRelatedNodesAfterScrollPositionChange(ScrollingTreeScrollingNode& changedNode);

    virtual void reportSynchronousScrollingReasonsChanged(MonotonicTime, OptionSet<SynchronousScrollingReason>) { }
    virtual void reportExposedUnfilledArea(MonotonicTime, unsigned /* unfilledArea */) { }

#if PLATFORM(IOS_FAMILY)
    virtual void scrollingTreeNodeWillStartPanGesture(ScrollingNodeID) { }
    virtual void scrollingTreeNodeWillStartScroll(ScrollingNodeID) { }
    virtual void scrollingTreeNodeDidEndScroll(ScrollingNodeID) { }
#endif

    WEBCORE_EXPORT TrackingType eventTrackingTypeForPoint(EventTrackingRegions::EventType, IntPoint);

    WheelEventTestMonitor* wheelEventTestMonitor() { return m_wheelEventTestMonitor.get(); }
    void setWheelEventTestMonitor(RefPtr<WheelEventTestMonitor>&& monitor) { m_wheelEventTestMonitor = WTFMove(monitor); }
    void deferWheelEventTestCompletionForReason(WheelEventTestMonitor::ScrollableAreaIdentifier, WheelEventTestMonitor::DeferReason);
    void removeWheelEventTestCompletionDeferralForReason(WheelEventTestMonitor::ScrollableAreaIdentifier, WheelEventTestMonitor::DeferReason);

#if PLATFORM(MAC)
    virtual void handleWheelEventPhase(ScrollingNodeID, PlatformWheelEventPhase) = 0;
#else
    void handleWheelEventPhase(ScrollingNodeID, PlatformWheelEventPhase) { }
#endif

    virtual void setActiveScrollSnapIndices(ScrollingNodeID, std::optional<unsigned> /*horizontalIndex*/, std::optional<unsigned> /*verticalIndex*/) { }

#if PLATFORM(COCOA)
    WEBCORE_EXPORT virtual void currentSnapPointIndicesDidChange(ScrollingNodeID, std::optional<unsigned> horizontal, std::optional<unsigned> vertical) = 0;
#endif

    void setMainFramePinnedState(RectEdges<bool>);

    // Can be called from any thread. Will update what edges allow rubber-banding.
    WEBCORE_EXPORT void setMainFrameCanRubberBand(RectEdges<bool>);
    bool mainFrameCanRubberBandOnSide(BoxSide);

    bool isHandlingProgrammaticScroll() const { return m_isHandlingProgrammaticScroll; }
    void setIsHandlingProgrammaticScroll(bool isHandlingProgrammaticScroll) { m_isHandlingProgrammaticScroll = isHandlingProgrammaticScroll; }
    
    void setScrollPinningBehavior(ScrollPinningBehavior);
    WEBCORE_EXPORT ScrollPinningBehavior scrollPinningBehavior();

    WEBCORE_EXPORT bool willWheelEventStartSwipeGesture(const PlatformWheelEvent&);

    ScrollingTreeFrameScrollingNode* rootNode() const { return m_rootNode.get(); }
    std::optional<ScrollingNodeID> latchedNodeID() const;
    void clearLatchedNode();

    bool hasFixedOrSticky() const { return !!m_fixedOrStickyNodeCount; }
    void fixedOrStickyNodeAdded() { ++m_fixedOrStickyNodeCount; }
    void fixedOrStickyNodeRemoved()
    {
        ASSERT(m_fixedOrStickyNodeCount);
        --m_fixedOrStickyNodeCount;
    }

    // A map of overflow scrolling nodes to positioned nodes which need to be updated
    // when the scroller changes, but are not descendants.
    using RelatedNodesMap = HashMap<ScrollingNodeID, Vector<ScrollingNodeID>>;
    RelatedNodesMap& overflowRelatedNodes() { return m_overflowRelatedNodesMap; }

    HashSet<Ref<ScrollingTreeOverflowScrollProxyNode>>& activeOverflowScrollProxyNodes() { return m_activeOverflowScrollProxyNodes; }
    HashSet<Ref<ScrollingTreePositionedNode>>& activePositionedNodes() { return m_activePositionedNodes; }

    WEBCORE_EXPORT String scrollingTreeAsText(OptionSet<ScrollingStateTreeAsTextBehavior> = { });

    bool isMonitoringWheelEvents() const { return m_isMonitoringWheelEvents; }
    bool inCommitTreeState() const { return m_inCommitTreeState; }

    void scrollBySimulatingWheelEventForTesting(ScrollingNodeID, FloatSize);

    virtual void lockLayersForHitTesting() { }
    virtual void unlockLayersForHitTesting() { }

    virtual bool isScrollingSynchronizedWithMainThread() WTF_REQUIRES_LOCK(m_treeLock) { return true; }

    virtual void willSendEventToMainThread(const PlatformWheelEvent&) { }
    virtual void waitForEventToBeProcessedByMainThread(const PlatformWheelEvent&) { };

    Lock& treeLock() WTF_RETURNS_LOCK(m_treeLock) { return m_treeLock; }

    void windowScreenDidChange(PlatformDisplayID, std::optional<FramesPerSecond> nominalFramesPerSecond);
    PlatformDisplayID displayID();
    
    WEBCORE_EXPORT void willProcessWheelEvent();

    WEBCORE_EXPORT void addPendingScrollUpdate(ScrollUpdate&&);
    WEBCORE_EXPORT Vector<ScrollUpdate> takePendingScrollUpdates();
    WEBCORE_EXPORT bool hasPendingScrollUpdates();

    virtual void removePendingScrollAnimationForNode(ScrollingNodeID) { }

protected:
    WheelEventHandlingResult handleWheelEventWithNode(const PlatformWheelEvent&, OptionSet<WheelEventProcessingSteps>, ScrollingTreeNode*, EventTargeting = EventTargeting::Propagate);

    FloatPoint mainFrameScrollPosition() const WTF_REQUIRES_LOCK(m_treeStateLock);
    void setMainFrameScrollPosition(FloatPoint);

    void setGestureState(std::optional<WheelScrollGestureState>);
    std::optional<WheelScrollGestureState> gestureState();

    std::optional<FramesPerSecond> nominalFramesPerSecond();

    WEBCORE_EXPORT virtual void applyLayerPositionsInternal() WTF_REQUIRES_LOCK(m_treeLock);
    WEBCORE_EXPORT void removeAllNodes() WTF_REQUIRES_LOCK(m_treeLock);
    
    virtual void hasNodeWithAnimatedScrollChanged(bool /* hasNodeWithAnimatedScroll */) { }

    bool hasProcessedWheelEventsRecently();

    HashSet<ScrollingNodeID> nodesWithActiveScrollAnimations();

    Lock m_treeLock; // Protects the scrolling tree.

private:
    void updateTreeFromStateNodeRecursive(const ScrollingStateNode*, struct CommitTreeState&) WTF_REQUIRES_LOCK(m_treeLock);
    virtual void propagateSynchronousScrollingReasons(const HashSet<ScrollingNodeID>&) WTF_REQUIRES_LOCK(m_treeLock) { }

    void applyLayerPositionsRecursive(ScrollingTreeNode&) WTF_REQUIRES_LOCK(m_treeLock);
    void notifyRelatedNodesRecursive(ScrollingTreeNode&);
    void traverseScrollingTreeRecursive(ScrollingTreeNode&, const VisitorFunction&) WTF_REQUIRES_LOCK(m_treeLock);
    
    virtual void didCommitTree() { }

    WEBCORE_EXPORT virtual RefPtr<ScrollingTreeNode> scrollingNodeForPoint(FloatPoint);
#if ENABLE(WHEEL_EVENT_REGIONS)
    WEBCORE_EXPORT virtual OptionSet<EventListenerRegionType> eventListenerRegionTypesForPoint(FloatPoint) const;
#endif

    OptionSet<WheelEventProcessingSteps> computeWheelProcessingSteps(const PlatformWheelEvent&) WTF_REQUIRES_LOCK(m_treeStateLock);

    void receivedWheelEvent(const PlatformWheelEvent&);

    RefPtr<ScrollingTreeFrameScrollingNode> m_rootNode;

    using ScrollingTreeNodeMap = HashMap<ScrollingNodeID, RefPtr<ScrollingTreeNode>>;
    ScrollingTreeNodeMap m_nodeMap;

    ScrollingTreeLatchingController m_latchingController;
    ScrollingTreeGestureState m_gestureState;

    RelatedNodesMap m_overflowRelatedNodesMap;

    HashSet<Ref<ScrollingTreeOverflowScrollProxyNode>> m_activeOverflowScrollProxyNodes;
    HashSet<Ref<ScrollingTreePositionedNode>> m_activePositionedNodes;

    struct TreeState {
        EventTrackingRegions eventTrackingRegions;
        FloatPoint mainFrameScrollPosition;
        PlatformDisplayID displayID { 0 };
        std::optional<FramesPerSecond> nominalFramesPerSecond;
        std::optional<WheelScrollGestureState> gestureState;
        HashSet<ScrollingNodeID> nodesWithActiveRubberBanding;
        HashSet<ScrollingNodeID> nodesWithActiveScrollSnap;
        HashSet<ScrollingNodeID> nodesWithActiveUserScrolls;
        HashSet<ScrollingNodeID> nodesWithActiveScrollAnimations;
    };
    
    Lock m_treeStateLock;
    TreeState m_treeState WTF_GUARDED_BY_LOCK(m_treeStateLock);

    struct SwipeState {
        ScrollPinningBehavior scrollPinningBehavior { DoNotPin };
        bool rubberBandsAtLeft { true };
        bool rubberBandsAtRight { true };
        bool rubberBandsAtTop { true };
        bool rubberBandsAtBottom { true };
        
        RectEdges<bool> canRubberBand  { true, true, true, true };
        RectEdges<bool> mainFramePinnedState { true, true, true, true };
    };

    Lock m_swipeStateLock;
    SwipeState m_swipeState WTF_GUARDED_BY_LOCK(m_swipeStateLock);

    Lock m_pendingScrollUpdatesLock;
    Vector<ScrollUpdate> m_pendingScrollUpdates WTF_GUARDED_BY_LOCK(m_pendingScrollUpdatesLock);

    Lock m_lastWheelEventTimeLock;
    MonotonicTime m_lastWheelEventTime WTF_GUARDED_BY_LOCK(m_lastWheelEventTimeLock);

    RefPtr<WheelEventTestMonitor> m_wheelEventTestMonitor;

protected:
    bool m_allowLatching { true };

private:
    unsigned m_fixedOrStickyNodeCount { 0 };
    bool m_isHandlingProgrammaticScroll { false };
    bool m_isMonitoringWheelEvents { false };
    bool m_scrollingPerformanceTestingEnabled { false };
    bool m_asyncFrameOrOverflowScrollingEnabled { false };
    bool m_wheelEventGesturesBecomeNonBlocking { false };
    bool m_needsApplyLayerPositionsAfterCommit { false };
    bool m_inCommitTreeState { false };
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_SCROLLING_TREE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(ToValueTypeName) \
    static bool isType(const WebCore::ScrollingTree& tree) { return tree.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
#endif // ENABLE(ASYNC_SCROLLING)
