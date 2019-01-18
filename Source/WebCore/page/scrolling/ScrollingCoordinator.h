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
#include <wtf/Forward.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TypeCasts.h>
#include <wtf/Variant.h>

#if ENABLE(ASYNC_SCROLLING)
#include <wtf/HashMap.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>
#endif

#if ENABLE(CSS_SCROLL_SNAP)
#include "AxisScrollSnapOffsets.h"
#endif

namespace WTF {
class TextStream;
}

namespace WebCore {

typedef unsigned SynchronousScrollingReasons;
typedef uint64_t ScrollingNodeID;

enum class ScrollingNodeType : uint8_t {
    MainFrame,
    Subframe,
    Overflow,
    Fixed,
    Sticky
};

enum ScrollingStateTreeAsTextBehaviorFlags {
    ScrollingStateTreeAsTextBehaviorNormal                  = 0,
    ScrollingStateTreeAsTextBehaviorIncludeLayerIDs         = 1 << 0,
    ScrollingStateTreeAsTextBehaviorIncludeNodeIDs          = 1 << 1,
    ScrollingStateTreeAsTextBehaviorIncludeLayerPositions   = 1 << 2,
    ScrollingStateTreeAsTextBehaviorDebug                   = ScrollingStateTreeAsTextBehaviorIncludeLayerIDs | ScrollingStateTreeAsTextBehaviorIncludeNodeIDs | ScrollingStateTreeAsTextBehaviorIncludeLayerPositions
};
typedef unsigned ScrollingStateTreeAsTextBehavior;

class Document;
class Frame;
class FrameView;
class GraphicsLayer;
class Page;
class Region;
class RenderLayer;
class ScrollableArea;
class ViewportConstraints;

#if ENABLE(ASYNC_SCROLLING)
class ScrollingTree;
#endif

enum class ScrollingLayerPositionAction {
    Set,
    SetApproximate,
    Sync
};

struct ScrollableAreaParameters {
    ScrollElasticity horizontalScrollElasticity { ScrollElasticityNone };
    ScrollElasticity verticalScrollElasticity { ScrollElasticityNone };

    ScrollbarMode horizontalScrollbarMode { ScrollbarAuto };
    ScrollbarMode verticalScrollbarMode { ScrollbarAuto };

    bool hasEnabledHorizontalScrollbar { false };
    bool hasEnabledVerticalScrollbar { false };

    bool useDarkAppearanceForScrollbars { false };

    bool operator==(const ScrollableAreaParameters& other) const
    {
        return horizontalScrollElasticity == other.horizontalScrollElasticity
            && verticalScrollElasticity == other.verticalScrollElasticity
            && horizontalScrollbarMode == other.horizontalScrollbarMode
            && verticalScrollbarMode == other.verticalScrollbarMode
            && hasEnabledHorizontalScrollbar == other.hasEnabledHorizontalScrollbar
            && hasEnabledVerticalScrollbar == other.hasEnabledVerticalScrollbar
            && useDarkAppearanceForScrollbars == other.useDarkAppearanceForScrollbars;
    }
};

enum class ViewportRectStability {
    Stable,
    Unstable,
    ChangingObscuredInsetsInteractively // This implies Unstable.
};

class ScrollingCoordinator : public ThreadSafeRefCounted<ScrollingCoordinator> {
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

    // Should be called whenever the given frame view has been laid out.
    virtual void frameViewLayoutUpdated(FrameView&) { }

    using LayoutViewportOriginOrOverrideRect = WTF::Variant<Optional<FloatPoint>, Optional<FloatRect>>;
    virtual void reconcileScrollingState(FrameView&, const FloatPoint&, const LayoutViewportOriginOrOverrideRect&, bool /* programmaticScroll */, ViewportRectStability, ScrollingLayerPositionAction) { }

    // Should be called whenever the slow repaint objects counter changes between zero and one.
    void frameViewHasSlowRepaintObjectsDidChange(FrameView&);

    // Should be called whenever the set of fixed objects changes.
    void frameViewFixedObjectsDidChange(FrameView&);

    // Called whenever the non-fast scrollable region changes for reasons other than layout.
    virtual void frameViewEventTrackingRegionsChanged(FrameView&) { }

    // Should be called whenever the root layer for the given frame view changes.
    virtual void frameViewRootLayerDidChange(FrameView&);

#if PLATFORM(COCOA)
    // Dispatched by the scrolling tree during handleWheelEvent. This is required as long as scrollbars are painted on the main thread.
    void handleWheelEventPhase(PlatformWheelEventPhase);
#endif

    // Force all scroll layer position updates to happen on the main thread.
    WEBCORE_EXPORT void setForceSynchronousScrollLayerPositionUpdates(bool);

    // These virtual functions are currently unique to the threaded scrolling architecture. 
    virtual void commitTreeStateIfNeeded() { }
    virtual bool requestScrollPositionUpdate(FrameView&, const IntPoint&) { return false; }
    virtual bool handleWheelEvent(FrameView&, const PlatformWheelEvent&) { return true; }
    virtual ScrollingNodeID attachToStateTree(ScrollingNodeType, ScrollingNodeID newNodeID, ScrollingNodeID /*parentID*/, size_t /*childIndex*/ = notFound) { return newNodeID; }

    virtual void detachFromStateTree(ScrollingNodeID) { }
    virtual void clearStateTree() { }

    virtual void setNodeLayers(ScrollingNodeID, GraphicsLayer* /*layer*/, GraphicsLayer* /*scrolledContentsLayer*/ = nullptr, GraphicsLayer* /*counterScrollingLayer*/ = nullptr, GraphicsLayer* /*insetClipLayer*/ = nullptr) { }

    struct ScrollingGeometry {
        LayoutRect parentRelativeScrollableRect;
        FloatSize scrollableAreaSize;
        FloatSize contentSize;
        FloatSize reachableContentSize; // Smaller than contentSize when overflow is hidden on one axis.
        FloatPoint scrollPosition;
        IntPoint scrollOrigin;
#if ENABLE(CSS_SCROLL_SNAP)
        Vector<LayoutUnit> horizontalSnapOffsets;
        Vector<LayoutUnit> verticalSnapOffsets;
        Vector<ScrollOffsetRange<LayoutUnit>> horizontalSnapOffsetRanges;
        Vector<ScrollOffsetRange<LayoutUnit>> verticalSnapOffsetRanges;
        unsigned currentHorizontalSnapPointIndex;
        unsigned currentVerticalSnapPointIndex;
#endif
    };

    virtual void setScrollingNodeGeometry(ScrollingNodeID, const ScrollingGeometry&) { }
    virtual void setViewportConstraintedNodeGeometry(ScrollingNodeID, const ViewportConstraints&) { }

    virtual void reconcileViewportConstrainedLayerPositions(ScrollingNodeID, const LayoutRect&, ScrollingLayerPositionAction) { }
    virtual String scrollingStateTreeAsText(ScrollingStateTreeAsTextBehavior = ScrollingStateTreeAsTextBehaviorNormal) const;
    virtual bool isRubberBandInProgress() const { return false; }
    virtual bool isScrollSnapInProgress() const { return false; }
    virtual void updateScrollSnapPropertiesWithFrameView(const FrameView&) { }
    virtual void setScrollPinningBehavior(ScrollPinningBehavior) { }

    // Generated a unique id for scrolling nodes.
    ScrollingNodeID uniqueScrollingNodeID();

    enum MainThreadScrollingReasonFlags {
        ForcedOnMainThread                                          = 1 << 0,
        HasSlowRepaintObjects                                       = 1 << 1,
        HasViewportConstrainedObjectsWithoutSupportingFixedLayers   = 1 << 2,
        HasNonLayerViewportConstrainedObjects                       = 1 << 3,
        IsImageDocument                                             = 1 << 4
    };

    SynchronousScrollingReasons synchronousScrollingReasons(const FrameView&) const;
    bool shouldUpdateScrollLayerPositionSynchronously(const FrameView&) const;

    virtual void willDestroyScrollableArea(ScrollableArea&) { }
    virtual void scrollableAreaScrollbarLayerDidChange(ScrollableArea&, ScrollbarOrientation) { }

    static String synchronousScrollingReasonsAsText(SynchronousScrollingReasons);
    String synchronousScrollingReasonsAsText() const;

    EventTrackingRegions absoluteEventTrackingRegions() const;
    virtual void updateExpectsWheelEventTestTriggerWithFrameView(const FrameView&) { }

protected:
    explicit ScrollingCoordinator(Page*);

    static GraphicsLayer* scrollLayerForScrollableArea(ScrollableArea&);

    GraphicsLayer* scrollLayerForFrameView(FrameView&);
    GraphicsLayer* counterScrollingLayerForFrameView(FrameView&);
    GraphicsLayer* insetClipLayerForFrameView(FrameView&);
    GraphicsLayer* rootContentLayerForFrameView(FrameView&);
    GraphicsLayer* contentShadowLayerForFrameView(FrameView&);
    GraphicsLayer* headerLayerForFrameView(FrameView&);
    GraphicsLayer* footerLayerForFrameView(FrameView&);

    virtual void willCommitTree() { }

    Page* m_page; // FIXME: ideally this would be a reference but it gets nulled on async teardown.

private:
    virtual void setSynchronousScrollingReasons(FrameView&, SynchronousScrollingReasons) { }

    virtual bool hasVisibleSlowRepaintViewportConstrainedObjects(const FrameView&) const;
    void updateSynchronousScrollingReasons(FrameView&);
    void updateSynchronousScrollingReasonsForAllFrames();

    EventTrackingRegions absoluteEventTrackingRegionsForFrame(const Frame&) const;

    bool m_forceSynchronousScrollLayerPositionUpdates { false };
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ScrollableAreaParameters);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ScrollingNodeType);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ScrollingLayerPositionAction);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, ViewportRectStability);

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_SCROLLING_COORDINATOR(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(ToValueTypeName) \
    static bool isType(const WebCore::ScrollingCoordinator& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
