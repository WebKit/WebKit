/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef ScrollingCoordinator_h
#define ScrollingCoordinator_h

#include "IntRect.h"
#include "LayoutRect.h"
#include "PlatformWheelEvent.h"
#include "RenderObject.h"
#include "ScrollTypes.h"
#include <wtf/Forward.h>

#if ENABLE(ASYNC_SCROLLING)
#include <wtf/HashMap.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>
#endif

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {

typedef unsigned SynchronousScrollingReasons;
typedef uint64_t ScrollingNodeID;

enum ScrollingNodeType { ScrollingNode, FixedNode, StickyNode };

class Document;
class Frame;
class FrameView;
class GraphicsLayer;
class Page;
class Region;
class ScrollableArea;
class ViewportConstraints;

#if ENABLE(ASYNC_SCROLLING)
class ScrollingTree;
#endif

enum SetOrSyncScrollingLayerPosition {
    SetScrollingLayerPosition,
    SyncScrollingLayerPosition
};

struct ScrollableAreaParameters {
    ScrollElasticity horizontalScrollElasticity;
    ScrollElasticity verticalScrollElasticity;

    ScrollbarMode horizontalScrollbarMode;
    ScrollbarMode verticalScrollbarMode;

    bool hasEnabledHorizontalScrollbar;
    bool hasEnabledVerticalScrollbar;
    
    ScrollableAreaParameters()
        : horizontalScrollElasticity(ScrollElasticityNone)
        , verticalScrollElasticity(ScrollElasticityNone)
        , horizontalScrollbarMode(ScrollbarAuto)
        , verticalScrollbarMode(ScrollbarAuto)
        , hasEnabledHorizontalScrollbar(false)
        , hasEnabledVerticalScrollbar(false)
    {
    }

    bool operator==(const ScrollableAreaParameters& other) const
    {
        return horizontalScrollElasticity == other.horizontalScrollElasticity
            && verticalScrollElasticity == other.verticalScrollElasticity
            && horizontalScrollbarMode == other.horizontalScrollbarMode
            && verticalScrollbarMode == other.verticalScrollbarMode
            && hasEnabledHorizontalScrollbar == other.hasEnabledHorizontalScrollbar
            && hasEnabledVerticalScrollbar == other.hasEnabledVerticalScrollbar;
    }
};

class ScrollingCoordinator : public ThreadSafeRefCounted<ScrollingCoordinator> {
public:
    static PassRefPtr<ScrollingCoordinator> create(Page*);
    virtual ~ScrollingCoordinator();

    virtual void pageDestroyed();
    
    virtual bool isAsyncScrollingCoordinator() const { return false; }
    virtual bool isRemoteScrollingCoordinator() const { return false; }

    // Return whether this scrolling coordinator handles scrolling for the given frame view.
    bool coordinatesScrollingForFrameView(FrameView*) const;

    // Should be called whenever the given frame view has been laid out.
    virtual void frameViewLayoutUpdated(FrameView*) { }

    // Should be called whenever a wheel event handler is added or removed in the 
    // frame view's underlying document.
    void frameViewWheelEventHandlerCountChanged(FrameView*);

    // Should be called whenever the slow repaint objects counter changes between zero and one.
    void frameViewHasSlowRepaintObjectsDidChange(FrameView*);

    // Should be called whenever the set of fixed objects changes.
    void frameViewFixedObjectsDidChange(FrameView*);

    // Called whenever the non-fast scrollable region changes for reasons other than layout.
    virtual void frameViewNonFastScrollableRegionChanged(FrameView*) { }

    // Should be called whenever the root layer for the given frame view changes.
    virtual void frameViewRootLayerDidChange(FrameView*);

    // Return whether this scrolling coordinator can keep fixed position layers fixed to their
    // containers while scrolling.
    virtual bool supportsFixedPositionLayers() const { return false; }

#if PLATFORM(COCOA)
    // Dispatched by the scrolling tree during handleWheelEvent. This is required as long as scrollbars are painted on the main thread.
    void handleWheelEventPhase(PlatformWheelEventPhase);
#endif

    // Force all scroll layer position updates to happen on the main thread.
    void setForceSynchronousScrollLayerPositionUpdates(bool);

    // These virtual functions are currently unique to the threaded scrolling architecture. 
    // Their meaningful implementations are in ScrollingCoordinatorMac.
    virtual void commitTreeStateIfNeeded() { }
    virtual bool requestScrollPositionUpdate(FrameView*, const IntPoint&) { return false; }
    virtual bool handleWheelEvent(FrameView*, const PlatformWheelEvent&) { return true; }
    virtual ScrollingNodeID attachToStateTree(ScrollingNodeType, ScrollingNodeID newNodeID, ScrollingNodeID /*parentID*/) { return newNodeID; }
    virtual void detachFromStateTree(ScrollingNodeID) { }
    virtual void clearStateTree() { }
    virtual void updateViewportConstrainedNode(ScrollingNodeID, const ViewportConstraints&, GraphicsLayer*) { }
    virtual void updateScrollingNode(ScrollingNodeID, GraphicsLayer* /*scrollLayer*/, GraphicsLayer* /*scrolledContentsLayer*/, GraphicsLayer* /*counterScrollingLayer*/) { }
    virtual void syncChildPositions(const LayoutRect&) { }
    virtual String scrollingStateTreeAsText() const;
    virtual bool isRubberBandInProgress() const { return false; }
    virtual void setScrollPinningBehavior(ScrollPinningBehavior) { }

    // Generated a unique id for scroll layers.
    ScrollingNodeID uniqueScrollLayerID();

    enum MainThreadScrollingReasonFlags {
        ForcedOnMainThread = 1 << 0,
        HasSlowRepaintObjects = 1 << 1,
        HasViewportConstrainedObjectsWithoutSupportingFixedLayers = 1 << 2,
        HasNonLayerViewportConstrainedObjects = 1 << 3,
        IsImageDocument = 1 << 4
    };

    SynchronousScrollingReasons synchronousScrollingReasons() const;
    bool shouldUpdateScrollLayerPositionSynchronously() const { return synchronousScrollingReasons(); }

    virtual void willDestroyScrollableArea(ScrollableArea*) { }
    virtual void scrollableAreaScrollLayerDidChange(ScrollableArea*) { }
    virtual void scrollableAreaScrollbarLayerDidChange(ScrollableArea*, ScrollbarOrientation) { }

    static String synchronousScrollingReasonsAsText(SynchronousScrollingReasons);
    String synchronousScrollingReasonsAsText() const;

    Region computeNonFastScrollableRegion(const Frame*, const IntPoint& frameLocation) const;

protected:
    explicit ScrollingCoordinator(Page*);

    static GraphicsLayer* scrollLayerForScrollableArea(ScrollableArea*);
    static GraphicsLayer* horizontalScrollbarLayerForScrollableArea(ScrollableArea*);
    static GraphicsLayer* verticalScrollbarLayerForScrollableArea(ScrollableArea*);

    unsigned computeCurrentWheelEventHandlerCount();
    GraphicsLayer* scrollLayerForFrameView(FrameView*);
    GraphicsLayer* counterScrollingLayerForFrameView(FrameView*);
    GraphicsLayer* headerLayerForFrameView(FrameView*);
    GraphicsLayer* footerLayerForFrameView(FrameView*);

    Page* m_page; // FIXME: ideally this would be a reference but it gets nulled on async teardown.

private:
    virtual void recomputeWheelEventHandlerCountForFrameView(FrameView*) { }
    virtual void setSynchronousScrollingReasons(SynchronousScrollingReasons) { }

    virtual bool hasVisibleSlowRepaintViewportConstrainedObjects(FrameView*) const;
    void updateSynchronousScrollingReasons();
    
    bool m_forceSynchronousScrollLayerPositionUpdates;
};

#define SCROLLING_COORDINATOR_TYPE_CASTS(ToValueTypeName, predicate) \
    TYPE_CASTS_BASE(ToValueTypeName, WebCore::ScrollingCoordinator, value, value->predicate, value.predicate)

} // namespace WebCore

#endif // ScrollingCoordinator_h
