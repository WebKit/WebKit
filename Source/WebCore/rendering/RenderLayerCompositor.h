/*
 * Copyright (C) 2009, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef RenderLayerCompositor_h
#define RenderLayerCompositor_h

#include "ChromeClient.h"
#include "GraphicsLayerClient.h"
#include "GraphicsLayerUpdater.h"
#include "RenderLayer.h"
#include <wtf/HashMap.h>

namespace WebCore {

class FixedPositionViewportConstraints;
class GraphicsLayer;
class GraphicsLayerUpdater;
class RenderEmbeddedObject;
class RenderNamedFlowFragment;
class RenderVideo;
class RenderWidget;
class ScrollingCoordinator;
class StickyPositionViewportConstraints;
class TiledBacking;

typedef unsigned LayerTreeFlags;

enum CompositingUpdateType {
    CompositingUpdateAfterStyleChange,
    CompositingUpdateAfterLayout,
    CompositingUpdateOnHitTest,
    CompositingUpdateOnScroll,
    CompositingUpdateOnCompositedScroll
};

enum {
    CompositingReasonNone                                   = 0,
    CompositingReason3DTransform                            = 1 << 0,
    CompositingReasonVideo                                  = 1 << 1,
    CompositingReasonCanvas                                 = 1 << 2,
    CompositingReasonPlugin                                 = 1 << 3,
    CompositingReasonIFrame                                 = 1 << 4,
    CompositingReasonBackfaceVisibilityHidden               = 1 << 5,
    CompositingReasonClipsCompositingDescendants            = 1 << 6,
    CompositingReasonAnimation                              = 1 << 7,
    CompositingReasonFilters                                = 1 << 8,
    CompositingReasonPositionFixed                          = 1 << 9,
    CompositingReasonPositionSticky                         = 1 << 10,
    CompositingReasonOverflowScrollingTouch                 = 1 << 11,
    CompositingReasonStacking                               = 1 << 12,
    CompositingReasonOverlap                                = 1 << 13,
    CompositingReasonNegativeZIndexChildren                 = 1 << 14,
    CompositingReasonTransformWithCompositedDescendants     = 1 << 15,
    CompositingReasonOpacityWithCompositedDescendants       = 1 << 16,
    CompositingReasonMaskWithCompositedDescendants          = 1 << 17,
    CompositingReasonReflectionWithCompositedDescendants    = 1 << 18,
    CompositingReasonFilterWithCompositedDescendants        = 1 << 19,
    CompositingReasonBlendingWithCompositedDescendants      = 1 << 20,
    CompositingReasonPerspective                            = 1 << 21,
    CompositingReasonPreserve3D                             = 1 << 22,
    CompositingReasonWillChange                             = 1 << 23,
    CompositingReasonRoot                                   = 1 << 24,
    CompositingReasonIsolatesCompositedBlendingDescendants  = 1 << 25,
};
typedef unsigned CompositingReasons;

// RenderLayerCompositor manages the hierarchy of
// composited RenderLayers. It determines which RenderLayers
// become compositing, and creates and maintains a hierarchy of
// GraphicsLayers based on the RenderLayer painting order.
// 
// There is one RenderLayerCompositor per RenderView.

class RenderLayerCompositor final : public GraphicsLayerClient, public GraphicsLayerUpdaterClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RenderLayerCompositor(RenderView&);
    virtual ~RenderLayerCompositor();

    // Return true if this RenderView is in "compositing mode" (i.e. has one or more
    // composited RenderLayers)
    bool inCompositingMode() const { return m_compositing; }
    // This will make a compositing layer at the root automatically, and hook up to
    // the native view/window system.
    void enableCompositingMode(bool enable = true);

    bool inForcedCompositingMode() const { return m_forceCompositingMode; }

    // Returns true if the accelerated compositing is enabled
    bool hasAcceleratedCompositing() const { return m_hasAcceleratedCompositing; }

    bool canRender3DTransforms() const;

    // Copy the accelerated compositing related flags from Settings
    void cacheAcceleratedCompositingFlags();

    // Called when the layer hierarchy needs to be updated (compositing layers have been
    // created, destroyed or re-parented).
    void setCompositingLayersNeedRebuild(bool needRebuild = true);
    bool compositingLayersNeedRebuild() const { return m_compositingLayersNeedRebuild; }
    
    void willRecalcStyle();

    // Returns true if the composited layers were actually updated.
    bool didRecalcStyleWithNoPendingLayout();

    // GraphicsLayers buffer state, which gets pushed to the underlying platform layers
    // at specific times.
    void scheduleLayerFlush(bool canThrottle);
    void flushPendingLayerChanges(bool isFlushRoot = true);
    
    // flushPendingLayerChanges() flushes the entire GraphicsLayer tree, which can cross frame boundaries.
    // This call returns the rootmost compositor that is being flushed (including self).
    RenderLayerCompositor* enclosingCompositorFlushingLayers() const;

    // Called when the GraphicsLayer for the given RenderLayer has flushed changes inside of flushPendingLayerChanges().
    void didFlushChangesForLayer(RenderLayer&, const GraphicsLayer*);

    // Called when something outside WebKit affects the visible rect (e.g. delegated scrolling). Might schedule a layer flush.
    void didChangeVisibleRect();
    
    // Rebuild the tree of compositing layers
    bool updateCompositingLayers(CompositingUpdateType, RenderLayer* updateRoot = nullptr);
    // This is only used when state changes and we do not exepect a style update or layout to happen soon (e.g. when
    // we discover that an iframe is overlapped during painting).
    void scheduleCompositingLayerUpdate();
    // This is used to cancel any pending update timers when the document goes into page cache.
    void cancelCompositingLayerUpdate();

    // Update the compositing state of the given layer. Returns true if that state changed.
    enum CompositingChangeRepaint { CompositingChangeRepaintNow, CompositingChangeWillRepaintLater };
    bool updateLayerCompositingState(RenderLayer&, CompositingChangeRepaint = CompositingChangeRepaintNow);

    // Update the geometry for compositing children of compositingAncestor.
    void updateCompositingDescendantGeometry(RenderLayer& compositingAncestor, RenderLayer&, bool compositedChildrenOnly);
    
    // Whether layer's backing needs a graphics layer to do clipping by an ancestor (non-stacking-context parent with overflow).
    bool clippedByAncestor(RenderLayer&) const;
    // Whether layer's backing needs a graphics layer to clip z-order children of the given layer.
    bool clipsCompositingDescendants(const RenderLayer&) const;

    // Whether the given layer needs an extra 'contents' layer.
    bool needsContentsCompositingLayer(const RenderLayer&) const;

    bool supportsFixedRootBackgroundCompositing() const;
    bool needsFixedRootBackgroundLayer(const RenderLayer&) const;
    GraphicsLayer* fixedRootBackgroundLayer() const;

    void rootOrBodyStyleChanged(RenderElement&, const RenderStyle* oldStyle);

    // Called after the view transparency, or the document or base background color change.
    void rootBackgroundTransparencyChanged();
    
    // Repaint the appropriate layers when the given RenderLayer starts or stops being composited.
    void repaintOnCompositingChange(RenderLayer&);
    
    void repaintInCompositedAncestor(RenderLayer&, const LayoutRect&);
    
    // Notify us that a layer has been added or removed
    void layerWasAdded(RenderLayer& parent, RenderLayer& child);
    void layerWillBeRemoved(RenderLayer& parent, RenderLayer& child);

    void layerStyleChanged(StyleDifference, RenderLayer&, const RenderStyle* oldStyle);

    static bool canCompositeClipPath(const RenderLayer&);

    // Get the nearest ancestor layer that has overflow or clip, but is not a stacking context
    RenderLayer* enclosingNonStackingClippingLayer(const RenderLayer&) const;

    // Repaint all composited layers.
    void repaintCompositedLayers();

    // Returns true if the given layer needs it own backing store.
    bool requiresOwnBackingStore(const RenderLayer&, const RenderLayer* compositingAncestorLayer, const LayoutRect& layerCompositedBoundsInAncestor, const LayoutRect& ancestorCompositedBounds) const;

    WEBCORE_EXPORT RenderLayer& rootRenderLayer() const;
    GraphicsLayer* rootGraphicsLayer() const;
    WEBCORE_EXPORT GraphicsLayer* scrollLayer() const;
    GraphicsLayer* clipLayer() const;
    GraphicsLayer* rootContentLayer() const;

#if ENABLE(RUBBER_BANDING)
    GraphicsLayer* headerLayer() const;
    GraphicsLayer* footerLayer() const;
#endif

    enum RootLayerAttachment {
        RootLayerUnattached,
        RootLayerAttachedViaChromeClient,
        RootLayerAttachedViaEnclosingFrame
    };

    RootLayerAttachment rootLayerAttachment() const { return m_rootLayerAttachment; }
    void updateRootLayerAttachment();
    void updateRootLayerPosition();
    
    void setIsInWindow(bool);

    void clearBackingForAllLayers();
    
    void layerBecameComposited(const RenderLayer&) { ++m_compositedLayerCount; }
    void layerBecameNonComposited(const RenderLayer&);
    
#if ENABLE(VIDEO)
    // Use by RenderVideo to ask if it should try to use accelerated compositing.
    bool canAccelerateVideoRendering(RenderVideo&) const;
#endif

    // Walk the tree looking for layers with 3d transforms. Useful in case you need
    // to know if there is non-affine content, e.g. for drawing into an image.
    bool has3DContent() const;
    
    static RenderLayerCompositor* frameContentsCompositor(RenderWidget*);
    // Return true if the layers changed.
    static bool parentFrameContentLayers(RenderWidget*);

    // Update the geometry of the layers used for clipping and scrolling in frames.
    void frameViewDidChangeLocation(const IntPoint& contentsOffset);
    void frameViewDidChangeSize();
    void frameViewDidScroll();
    void frameViewDidAddOrRemoveScrollbars();
    void frameViewDidLayout();
    void rootFixedBackgroundsChanged();

    void scrollingLayerDidChange(RenderLayer&);
    void fixedRootBackgroundLayerChanged();

    String layerTreeAsText(LayerTreeFlags);

    float deviceScaleFactor() const override;
    float contentsScaleMultiplierForNewTiles(const GraphicsLayer*) const override;
    float pageScaleFactor() const override;
    float zoomedOutPageScaleFactor() const override;

    void didCommitChangesForLayer(const GraphicsLayer*) const override;
    void notifyFlushBeforeDisplayRefresh(const GraphicsLayer*) override;

    void layerTiledBackingUsageChanged(const GraphicsLayer*, bool /*usingTiledBacking*/);
    
    bool acceleratedDrawingEnabled() const { return m_acceleratedDrawingEnabled; }
    bool displayListDrawingEnabled() const { return m_displayListDrawingEnabled; }

    void deviceOrPageScaleFactorChanged();

    void windowScreenDidChange(PlatformDisplayID);

    GraphicsLayer* layerForHorizontalScrollbar() const { return m_layerForHorizontalScrollbar.get(); }
    GraphicsLayer* layerForVerticalScrollbar() const { return m_layerForVerticalScrollbar.get(); }
    GraphicsLayer* layerForScrollCorner() const { return m_layerForScrollCorner.get(); }
#if ENABLE(RUBBER_BANDING)
    GraphicsLayer* layerForOverhangAreas() const { return m_layerForOverhangAreas.get(); }
    GraphicsLayer* layerForContentShadow() const { return m_contentShadowLayer.get(); }

    GraphicsLayer* updateLayerForTopOverhangArea(bool wantsLayer);
    GraphicsLayer* updateLayerForBottomOverhangArea(bool wantsLayer);
    GraphicsLayer* updateLayerForHeader(bool wantsLayer);
    GraphicsLayer* updateLayerForFooter(bool wantsLayer);
#endif

    ScrollableArea* scrollableAreaForScrollLayerID(ScrollingNodeID) const;

    void updateScrollCoordinatedStatus(RenderLayer&);
    void removeFromScrollCoordinatedLayers(RenderLayer&);

    void willRemoveScrollingLayerWithBacking(RenderLayer&, RenderLayerBacking&);
    void didAddScrollingLayer(RenderLayer&);

#if PLATFORM(IOS)
    void registerAllViewportConstrainedLayers();
    void unregisterAllViewportConstrainedLayers();

    void registerAllScrollingLayers();
    void unregisterAllScrollingLayers();
#endif

    void resetTrackedRepaintRects();
    void setTracksRepaints(bool);

    void setShouldReevaluateCompositingAfterLayout() { m_reevaluateCompositingAfterLayout = true; }

    bool viewHasTransparentBackground(Color* backgroundColor = nullptr) const;

    bool hasNonMainLayersWithTiledBacking() const { return m_layersWithTiledBackingCount; }

    CompositingReasons reasonsForCompositing(const RenderLayer&) const;

    void setLayerFlushThrottlingEnabled(bool);
    void disableLayerFlushThrottlingTemporarilyForInteraction();
    
    void didPaintBacking(RenderLayerBacking*);

    void setRootExtendedBackgroundColor(const Color&);
    Color rootExtendedBackgroundColor() const { return m_rootExtendedBackgroundColor; }

#if ENABLE(CSS_SCROLL_SNAP)
    void updateScrollSnapPropertiesWithFrameView(const FrameView&);
#endif

    // For testing.
    WEBCORE_EXPORT void startTrackingLayerFlushes();
    WEBCORE_EXPORT unsigned layerFlushCount() const;

    WEBCORE_EXPORT void startTrackingCompositingUpdates();
    WEBCORE_EXPORT unsigned compositingUpdateCount() const;

private:
    class OverlapMap;
    struct CompositingState;
    struct OverlapExtent;

    // GraphicsLayerClient implementation
    void notifyFlushRequired(const GraphicsLayer*) override;
    void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const FloatRect&) override;
    void customPositionForVisibleRectComputation(const GraphicsLayer*, FloatPoint&) const override;
    bool isTrackingRepaints() const override;
    
    // GraphicsLayerUpdaterClient implementation
    void flushLayersSoon(GraphicsLayerUpdater&) override;

    // Whether the given RL needs a compositing layer.
    bool needsToBeComposited(const RenderLayer&, RenderLayer::ViewportConstrainedNotCompositedReason* = nullptr) const;
    // Whether the layer has an intrinsic need for compositing layer.
    bool requiresCompositingLayer(const RenderLayer&, RenderLayer::ViewportConstrainedNotCompositedReason* = nullptr) const;
    // Whether the layer could ever be composited.
    bool canBeComposited(const RenderLayer&) const;

    // Make or destroy the backing for this layer; returns true if backing changed.
    enum class BackingRequired { No, Yes, Unknown };
    bool updateBacking(RenderLayer&, CompositingChangeRepaint shouldRepaint, BackingRequired = BackingRequired::Unknown);

    void clearBackingForLayerIncludingDescendants(RenderLayer&);
    void setIsInWindowForLayerIncludingDescendants(RenderLayer&, bool isInWindow);

    // Repaint this and its child layers.
    void recursiveRepaintLayer(RenderLayer&);

    void computeExtent(const OverlapMap&, const RenderLayer&, OverlapExtent&) const;
    void addToOverlapMap(OverlapMap&, const RenderLayer&, OverlapExtent&);
    void addToOverlapMapRecursive(OverlapMap&, const RenderLayer&, const RenderLayer* ancestorLayer = nullptr);

    void updateCompositingLayersTimerFired();

    // Returns true if any layer's compositing changed
    void computeCompositingRequirements(RenderLayer* ancestorLayer, RenderLayer&, OverlapMap&, CompositingState&, bool& layersChanged, bool& descendantHas3DTransform);

    void computeRegionCompositingRequirements(RenderNamedFlowFragment*, OverlapMap&, CompositingState&, bool& layersChanged, bool& anyDescendantHas3DTransform);

    void computeCompositingRequirementsForNamedFlowFixed(RenderLayer&, OverlapMap&, CompositingState&, bool& layersChanged, bool& anyDescendantHas3DTransform);

    // Recurses down the tree, parenting descendant compositing layers and collecting an array of child layers for the current compositing layer.
    void rebuildCompositingLayerTree(RenderLayer&, Vector<GraphicsLayer*>& childGraphicsLayersOfEnclosingLayer, int depth);

    // Recurses down the RenderFlowThread tree, parenting descendant compositing layers and collecting an array of child 
    // layers for the current compositing layer corresponding to the anonymous region (that belongs to the region's parent).
    void rebuildRegionCompositingLayerTree(RenderNamedFlowFragment*, Vector<GraphicsLayer*>& childList, int depth);

    void rebuildCompositingLayerTreeForNamedFlowFixed(RenderLayer&, Vector<GraphicsLayer*>& childList, int depth);

    // Recurses down the tree, updating layer geometry only.
    void updateLayerTreeGeometry(RenderLayer&, int depth);
    
    // Hook compositing layers together
    void setCompositingParent(RenderLayer& childLayer, RenderLayer* parentLayer);
    void removeCompositedChildren(RenderLayer&);

    bool layerHas3DContent(const RenderLayer&) const;
    bool isRunningTransformAnimation(RenderLayerModelObject&) const;

    void appendDocumentOverlayLayers(Vector<GraphicsLayer*>&);
    bool hasAnyAdditionalCompositedLayers(const RenderLayer& rootLayer) const;

    void ensureRootLayer();
    void destroyRootLayer();

    void attachRootLayer(RootLayerAttachment);
    void detachRootLayer();
    
    void rootLayerAttachmentChanged();

    void updateOverflowControlsLayers();

    void updateScrollLayerPosition();

    FloatPoint positionForClipLayer() const;

    void notifyIFramesOfCompositingChange();

    bool isFlushingLayers() const { return m_flushingLayers; }
    void updateScrollCoordinatedLayersAfterFlushIncludingSubframes();
    void updateScrollCoordinatedLayersAfterFlush();
    
    Page* page() const;
    
    GraphicsLayerFactory* graphicsLayerFactory() const;
    ScrollingCoordinator* scrollingCoordinator() const;

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    RefPtr<DisplayRefreshMonitor> createDisplayRefreshMonitor(PlatformDisplayID) const override;
#endif

    bool requiresCompositingForAnimation(RenderLayerModelObject&) const;
    bool requiresCompositingForTransform(RenderLayerModelObject&) const;
    bool requiresCompositingForBackfaceVisibility(RenderLayerModelObject&) const;
    bool requiresCompositingForVideo(RenderLayerModelObject&) const;
    bool requiresCompositingForCanvas(RenderLayerModelObject&) const;
    bool requiresCompositingForPlugin(RenderLayerModelObject&) const;
    bool requiresCompositingForFrame(RenderLayerModelObject&) const;
    bool requiresCompositingForFilters(RenderLayerModelObject&) const;
    bool requiresCompositingForWillChange(RenderLayerModelObject&) const;
    bool requiresCompositingForScrollableFrame() const;
    bool requiresCompositingForPosition(RenderLayerModelObject&, const RenderLayer&, RenderLayer::ViewportConstrainedNotCompositedReason* = nullptr) const;
    bool requiresCompositingForOverflowScrolling(const RenderLayer&) const;
    bool requiresCompositingForIndirectReason(RenderLayerModelObject&, bool hasCompositedDescendants, bool has3DTransformedDescendants, RenderLayer::IndirectCompositingReason&) const;

#if PLATFORM(IOS)
    bool requiresCompositingForScrolling(const RenderLayer&) const;

    void updateCustomLayersAfterFlush();

    ChromeClient* chromeClient() const;

#endif

    void updateScrollCoordinationForThisFrame(ScrollingNodeID);
    ScrollingNodeID attachScrollingNode(RenderLayer&, ScrollingNodeType, ScrollingNodeID parentNodeID);
    void updateScrollCoordinatedLayer(RenderLayer&, LayerScrollCoordinationRoles);
    void detachScrollCoordinatedLayer(RenderLayer&, LayerScrollCoordinationRoles);
    void reattachSubframeScrollLayers();
    
    FixedPositionViewportConstraints computeFixedViewportConstraints(RenderLayer&) const;
    StickyPositionViewportConstraints computeStickyViewportConstraints(RenderLayer&) const;

    bool requiresScrollLayer(RootLayerAttachment) const;
    bool requiresHorizontalScrollbarLayer() const;
    bool requiresVerticalScrollbarLayer() const;
    bool requiresScrollCornerLayer() const;
#if ENABLE(RUBBER_BANDING)
    bool requiresOverhangAreasLayer() const;
    bool requiresContentShadowLayer() const;
#endif

    // True if the FrameView uses a ScrollingCoordinator.
    bool hasCoordinatedScrolling() const;

    bool isAsyncScrollableStickyLayer(const RenderLayer&, const RenderLayer** enclosingAcceleratedOverflowLayer = nullptr) const;
    bool isViewportConstrainedFixedOrStickyLayer(const RenderLayer&) const;
    
    bool shouldCompositeOverflowControls() const;

    void scheduleLayerFlushNow();
    bool isThrottlingLayerFlushes() const;
    void startInitialLayerFlushTimerIfNeeded();
    void startLayerFlushTimerIfNeeded();
    void layerFlushTimerFired();

    void paintRelatedMilestonesTimerFired();

#if !LOG_DISABLED
    const char* logReasonsForCompositing(const RenderLayer&);
    void logLayerInfo(const RenderLayer&, int depth);
#endif

    bool documentUsesTiledBacking() const;
    bool isMainFrameCompositor() const;

private:
    RenderView& m_renderView;
    std::unique_ptr<GraphicsLayer> m_rootContentLayer;
    Timer m_updateCompositingLayersTimer;

    ChromeClient::CompositingTriggerFlags m_compositingTriggers { static_cast<ChromeClient::CompositingTriggerFlags>(ChromeClient::AllTriggers) };
    bool m_hasAcceleratedCompositing { true };

    bool m_showDebugBorders { false };
    bool m_showRepaintCounter { false };
    bool m_acceleratedDrawingEnabled { false };
    bool m_displayListDrawingEnabled { false };

    // When true, we have to wait until layout has happened before we can decide whether to enter compositing mode,
    // because only then do we know the final size of plugins and iframes.
    mutable bool m_reevaluateCompositingAfterLayout { false };

    bool m_compositing { false };
    bool m_compositingLayersNeedRebuild { false };
    bool m_flushingLayers { false };
    bool m_shouldFlushOnReattach { false };
    bool m_forceCompositingMode { false };
    bool m_inPostLayoutUpdate { false }; // true when it's OK to trust layout information (e.g. layer sizes and positions)
    bool m_subframeScrollLayersNeedReattach { false };

    bool m_isTrackingRepaints { false }; // Used for testing.

    int m_compositedLayerCount { 0 };
    unsigned m_layersWithTiledBackingCount { 0 };
    unsigned m_layerFlushCount { 0 };
    unsigned m_compositingUpdateCount { 0 };

    RootLayerAttachment m_rootLayerAttachment { RootLayerUnattached };

    // Enclosing clipping layer for iframe content
    std::unique_ptr<GraphicsLayer> m_clipLayer;
    std::unique_ptr<GraphicsLayer> m_scrollLayer;

#if PLATFORM(IOS)
    HashSet<RenderLayer*> m_scrollingLayers;
    HashSet<RenderLayer*> m_scrollingLayersNeedingUpdate;
#endif
    HashSet<RenderLayer*> m_scrollCoordinatedLayers;
    HashSet<RenderLayer*> m_scrollCoordinatedLayersNeedingUpdate;

    // Enclosing layer for overflow controls and the clipping layer
    std::unique_ptr<GraphicsLayer> m_overflowControlsHostLayer;

    // Layers for overflow controls
    std::unique_ptr<GraphicsLayer> m_layerForHorizontalScrollbar;
    std::unique_ptr<GraphicsLayer> m_layerForVerticalScrollbar;
    std::unique_ptr<GraphicsLayer> m_layerForScrollCorner;
#if ENABLE(RUBBER_BANDING)
    std::unique_ptr<GraphicsLayer> m_layerForOverhangAreas;
    std::unique_ptr<GraphicsLayer> m_contentShadowLayer;
    std::unique_ptr<GraphicsLayer> m_layerForTopOverhangArea;
    std::unique_ptr<GraphicsLayer> m_layerForBottomOverhangArea;
    std::unique_ptr<GraphicsLayer> m_layerForHeader;
    std::unique_ptr<GraphicsLayer> m_layerForFooter;
#endif

    std::unique_ptr<GraphicsLayerUpdater> m_layerUpdater; // Updates tiled layer visible area periodically while animations are running.

    Timer m_paintRelatedMilestonesTimer;
    Timer m_layerFlushTimer;

    bool m_layerFlushThrottlingEnabled { false };
    bool m_layerFlushThrottlingTemporarilyDisabledForInteraction { false };
    bool m_hasPendingLayerFlush { false };
    bool m_layerNeedsCompositingUpdate { false };
    bool m_viewBackgroundIsTransparent { false };

#if !LOG_DISABLED
    int m_rootLayerUpdateCount { 0 };
    int m_obligateCompositedLayerCount { 0 }; // count of layer that have to be composited.
    int m_secondaryCompositedLayerCount { 0 }; // count of layers that have to be composited because of stacking or overlap.
    double m_obligatoryBackingStoreBytes { 0 };
    double m_secondaryBackingStoreBytes { 0 };
#endif

    Color m_rootExtendedBackgroundColor;

    HashMap<ScrollingNodeID, RenderLayer*> m_scrollingNodeToLayerMap;
};

void paintScrollbar(Scrollbar*, GraphicsContext&, const IntRect& clip);

} // namespace WebCore

#endif // RenderLayerCompositor_h
