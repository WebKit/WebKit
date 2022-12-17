/*
 * Copyright (C) 2011, 2015 Apple Inc. All rights reserved.
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

#include "EventTrackingRegions.h"
#include "LayoutRect.h"
#include "PlatformWheelEvent.h"
#include "ScrollSnapOffsetsInfo.h"
#include "ScrollTypes.h"
#include "ScrollingCoordinatorTypes.h"
#include "WheelEventTestMonitor.h"
#include <variant>
#include <wtf/Forward.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>
#include <wtf/TypeCasts.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class AbsolutePositionConstraints;
class Document;
class Frame;
class FrameView;
class GraphicsLayer;
struct KeyboardScroll;
class Page;
class Region;
class RenderObject;
class RenderLayer;
class ScrollableArea;
class ViewportConstraints;

using FramesPerSecond = unsigned;
using PlatformDisplayID = uint32_t;

class ScrollingCoordinator : public ThreadSafeRefCounted<ScrollingCoordinator> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ScrollingCoordinator> create(Page*);
    virtual ~ScrollingCoordinator();

    WEBCORE_EXPORT virtual void pageDestroyed();
    
    virtual bool isAsyncScrollingCoordinator() const { return false; }
    virtual bool isRemoteScrollingCoordinator() const { return false; }

    // Return whether this scrolling coordinator handles scrolling for the given frame view.
    WEBCORE_EXPORT virtual bool coordinatesScrollingForFrameView(const FrameView&) const;

    // Return whether this scrolling coordinator handles scrolling for the given overflow scroll layer.
    WEBCORE_EXPORT virtual bool coordinatesScrollingForOverflowLayer(const RenderLayer&) const;

    // Returns the ScrollingNodeID of the innermost scrolling node that scrolls the renderer.
    WEBCORE_EXPORT virtual ScrollingNodeID scrollableContainerNodeID(const RenderObject&) const;

    // Should be called whenever the given frame view has been laid out.
    virtual void frameViewLayoutUpdated(FrameView&) { }

    using LayoutViewportOriginOrOverrideRect = std::variant<std::optional<FloatPoint>, std::optional<FloatRect>>;
    virtual void reconcileScrollingState(FrameView&, const FloatPoint&, const LayoutViewportOriginOrOverrideRect&, ScrollType, ViewportRectStability, ScrollingLayerPositionAction) { }

    // Should be called whenever the set of fixed objects changes.
    void frameViewFixedObjectsDidChange(FrameView&);

    // Should be called whenever the FrameView's visual viewport changed.
    virtual void frameViewVisualViewportChanged(FrameView&) { }

    // Called whenever the non-fast scrollable region changes for reasons other than layout.
    virtual void frameViewEventTrackingRegionsChanged(FrameView&) { }

    // Should be called whenever the root layer for the given frame view changes.
    virtual void frameViewRootLayerDidChange(FrameView&);

    virtual void frameViewWillBeDetached(FrameView&) { }

    // Traverses the scrolling tree, setting layer positions to represent the current scrolled state.
    virtual void applyScrollingTreeLayerPositions() { }

    virtual void didScheduleRenderingUpdate() { }
    virtual void willStartRenderingUpdate() { }
    virtual void didCompleteRenderingUpdate() { }

    virtual void willStartPlatformRenderingUpdate() { }
    virtual void didCompletePlatformRenderingUpdate() { }

#if ENABLE(KINETIC_SCROLLING)
    // Dispatched by the scrolling tree during handleWheelEvent. This is required as long as scrollbars are painted on the main thread.
    virtual void handleWheelEventPhase(ScrollingNodeID, PlatformWheelEventPhase) { }
#endif

    // Force all scroll layer position updates to happen on the main thread.
    WEBCORE_EXPORT void setForceSynchronousScrollLayerPositionUpdates(bool);

    // These virtual functions are currently unique to the threaded scrolling architecture. 
    virtual void commitTreeStateIfNeeded() { }

    virtual bool requestStartKeyboardScrollAnimation(ScrollableArea&, const KeyboardScroll&) { return false; }
    virtual bool requestStopKeyboardScrollAnimation(ScrollableArea&, bool) { return false; }

    virtual bool requestScrollPositionUpdate(ScrollableArea&, const ScrollPosition&, ScrollType = ScrollType::Programmatic, ScrollClamping = ScrollClamping::Clamped) { return false; }
    virtual bool requestAnimatedScrollToPosition(ScrollableArea&, const ScrollPosition&, ScrollClamping) { return false; }
    virtual void stopAnimatedScroll(ScrollableArea&) { }

    virtual bool handleWheelEventForScrolling(const PlatformWheelEvent&, ScrollingNodeID, std::optional<WheelScrollGestureState>) { return false; }
    virtual void wheelEventWasProcessedByMainThread(const PlatformWheelEvent&, std::optional<WheelScrollGestureState>) { }

    // Create an unparented node.
    virtual ScrollingNodeID createNode(ScrollingNodeType, ScrollingNodeID newNodeID) { return newNodeID; }
    // Parent a node in the scrolling tree. This may return a new nodeID if the node type changed. parentID = 0 sets the root node.
    virtual ScrollingNodeID insertNode(ScrollingNodeType, ScrollingNodeID newNodeID, ScrollingNodeID /*parentID*/, size_t /*childIndex*/ = notFound) { return newNodeID; }
    // Node will be unparented, but not destroyed. It's the client's responsibility to either re-parent or destroy this node.
    virtual void unparentNode(ScrollingNodeID) { }
    // Node will be destroyed, and its children left unparented.
    virtual void unparentChildrenAndDestroyNode(ScrollingNodeID) { }
    // Node will be unparented, and it and its children destroyed.
    virtual void detachAndDestroySubtree(ScrollingNodeID) { }
    // Destroy the tree, including both parented and unparented nodes.
    virtual void clearAllNodes() { }

    virtual ScrollingNodeID parentOfNode(ScrollingNodeID) const { return 0; }
    virtual Vector<ScrollingNodeID> childrenOfNode(ScrollingNodeID) const { return { }; }

    virtual void scrollBySimulatingWheelEventForTesting(ScrollingNodeID, FloatSize) { }

    struct NodeLayers {
        GraphicsLayer* layer { nullptr };
        GraphicsLayer* scrollContainerLayer { nullptr };
        GraphicsLayer* scrolledContentsLayer { nullptr };
        GraphicsLayer* counterScrollingLayer { nullptr };
        GraphicsLayer* insetClipLayer { nullptr };
        GraphicsLayer* rootContentsLayer { nullptr };
        GraphicsLayer* horizontalScrollbarLayer { nullptr };
        GraphicsLayer* verticalScrollbarLayer { nullptr };
    };
    virtual void setNodeLayers(ScrollingNodeID, const NodeLayers&) { }

    virtual void setScrollingNodeScrollableAreaGeometry(ScrollingNodeID, ScrollableArea&) { }
    virtual void setFrameScrollingNodeState(ScrollingNodeID, const FrameView&) { }
    virtual void setViewportConstraintedNodeConstraints(ScrollingNodeID, const ViewportConstraints&) { }
    virtual void setPositionedNodeConstraints(ScrollingNodeID, const AbsolutePositionConstraints&) { }
    virtual void setRelatedOverflowScrollingNodes(ScrollingNodeID, Vector<ScrollingNodeID>&&) { }
    virtual void setSynchronousScrollingReasons(ScrollingNodeID, OptionSet<SynchronousScrollingReason>) { }
    virtual OptionSet<SynchronousScrollingReason> synchronousScrollingReasons(ScrollingNodeID) const { return { }; };
    bool hasSynchronousScrollingReasons(ScrollingNodeID nodeID) const { return !!synchronousScrollingReasons(nodeID); }

    virtual void reconcileViewportConstrainedLayerPositions(ScrollingNodeID, const LayoutRect&, ScrollingLayerPositionAction) { }
    virtual String scrollingStateTreeAsText(OptionSet<ScrollingStateTreeAsTextBehavior> = { }) const;
    virtual String scrollingTreeAsText(OptionSet<ScrollingStateTreeAsTextBehavior> = { }) const;
    virtual bool isRubberBandInProgress(ScrollingNodeID) const { return false; }
    virtual bool isUserScrollInProgress(ScrollingNodeID) const { return false; }
    virtual bool isScrollSnapInProgress(ScrollingNodeID) const { return false; }
    virtual void updateScrollSnapPropertiesWithFrameView(const FrameView&) { }
    virtual void setScrollPinningBehavior(ScrollPinningBehavior) { }
    virtual bool hasSubscrollers() const { return false; }

    // Generated a unique id for scrolling nodes.
    ScrollingNodeID uniqueScrollingNodeID();

    bool shouldUpdateScrollLayerPositionSynchronously(const FrameView&) const;

    virtual void willDestroyScrollableArea(ScrollableArea&) { }
    virtual void scrollableAreaScrollbarLayerDidChange(ScrollableArea&, ScrollbarOrientation) { }

    virtual void windowScreenDidChange(PlatformDisplayID, std::optional<FramesPerSecond> /* nominalFramesPerSecond */) { }

    static String synchronousScrollingReasonsAsText(OptionSet<SynchronousScrollingReason>);
    String synchronousScrollingReasonsAsText() const;

    EventTrackingRegions absoluteEventTrackingRegions() const;
    virtual void updateIsMonitoringWheelEventsForFrameView(const FrameView&) { }

    virtual void startMonitoringWheelEvents(bool /* clearLatchingState */) { }
    virtual void stopMonitoringWheelEvents() { }

    void receivedWheelEventWithPhases(PlatformWheelEventPhase phase, PlatformWheelEventPhase momentumPhase);
    void deferWheelEventTestCompletionForReason(ScrollingNodeID, WheelEventTestMonitor::DeferReason);
    void removeWheelEventTestCompletionDeferralForReason(ScrollingNodeID, WheelEventTestMonitor::DeferReason);

protected:
    explicit ScrollingCoordinator(Page*);

    GraphicsLayer* scrollContainerLayerForFrameView(FrameView&);
    GraphicsLayer* scrolledContentsLayerForFrameView(FrameView&);
    GraphicsLayer* counterScrollingLayerForFrameView(FrameView&);
    GraphicsLayer* insetClipLayerForFrameView(FrameView&);
    GraphicsLayer* rootContentsLayerForFrameView(FrameView&);
    GraphicsLayer* contentShadowLayerForFrameView(FrameView&);
    GraphicsLayer* headerLayerForFrameView(FrameView&);
    GraphicsLayer* footerLayerForFrameView(FrameView&);

    virtual void willCommitTree() { }

    Page* m_page; // FIXME: ideally this would be a reference but it gets nulled on async teardown.

private:
    virtual bool hasVisibleSlowRepaintViewportConstrainedObjects(const FrameView&) const;

    void updateSynchronousScrollingReasons(FrameView&);
    void updateSynchronousScrollingReasonsForAllFrames();

    EventTrackingRegions absoluteEventTrackingRegionsForFrame(const Frame&) const;

    bool m_forceSynchronousScrollLayerPositionUpdates { false };
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_SCROLLING_COORDINATOR(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(ToValueTypeName) \
    static bool isType(const WebCore::ScrollingCoordinator& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
