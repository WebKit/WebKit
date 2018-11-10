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

#pragma once

#include "ChromeClient.h"
#include "GraphicsLayerClient.h"
#include "GraphicsLayerUpdater.h"
#include "RenderLayer.h"
#include <wtf/HashMap.h>
#include <wtf/OptionSet.h>

namespace WebCore {

class FixedPositionViewportConstraints;
class GraphicsLayer;
class GraphicsLayerUpdater;
class RenderEmbeddedObject;
class RenderVideo;
class RenderWidget;
class ScrollingCoordinator;
class StickyPositionViewportConstraints;
class TiledBacking;

typedef unsigned LayerTreeFlags;

enum class CompositingUpdateType {
    AfterStyleChange,
    AfterLayout,
    OnHitTest,
    OnScroll,
    OnCompositedScroll
};

enum class CompositingReason {
    Transform3D                            = 1 << 0,
    Video                                  = 1 << 1,
    Canvas                                 = 1 << 2,
    Plugin                                 = 1 << 3,
    IFrame                                 = 1 << 4,
    BackfaceVisibilityHidden               = 1 << 5,
    ClipsCompositingDescendants            = 1 << 6,
    Animation                              = 1 << 7,
    Filters                                = 1 << 8,
    PositionFixed                          = 1 << 9,
    PositionSticky                         = 1 << 10,
    OverflowScrollingTouch                 = 1 << 11,
    Stacking                               = 1 << 12,
    Overlap                                = 1 << 13,
    NegativeZIndexChildren                 = 1 << 14,
    TransformWithCompositedDescendants     = 1 << 15,
    OpacityWithCompositedDescendants       = 1 << 16,
    MaskWithCompositedDescendants          = 1 << 17,
    ReflectionWithCompositedDescendants    = 1 << 18,
    FilterWithCompositedDescendants        = 1 << 19,
    BlendingWithCompositedDescendants      = 1 << 20,
    Perspective                            = 1 << 21,
    Preserve3D                             = 1 << 22,
    WillChange                             = 1 << 23,
    Root                                   = 1 << 24,
    IsolatesCompositedBlendingDescendants  = 1 << 25,
    EmbeddedView                           = 1 << 26,
};

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
    void updateCompositingDescendantGeometry(RenderLayer& compositingAncestor, RenderLayer&);
    
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
    void rootBackgroundColorOrTransparencyChanged();
    
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

    GraphicsLayer* scrollLayer() const { return m_scrollLayer.get(); }
    GraphicsLayer* clipLayer() const { return m_clipLayer.get(); }
    GraphicsLayer* rootContentLayer() const { return m_rootContentLayer.get(); }


#if ENABLE(RUBBER_BANDING)
    GraphicsLayer* headerLayer() const { return m_layerForHeader.get(); }
    GraphicsLayer* footerLayer() const { return m_layerForFooter.get(); }
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

    enum class ScrollingNodeChangeFlags {
        Layer           = 1 << 0,
        LayerGeometry   = 1 << 1,
    };

    void updateScrollCoordinatedStatus(RenderLayer&, OptionSet<ScrollingNodeChangeFlags>);
    void removeFromScrollCoordinatedLayers(RenderLayer&);

    void willRemoveScrollingLayerWithBacking(RenderLayer&, RenderLayerBacking&);
    void didAddScrollingLayer(RenderLayer&);

#if PLATFORM(IOS_FAMILY)
    void registerAllViewportConstrainedLayers();
    void unregisterAllViewportConstrainedLayers();

    void registerAllScrollingLayers();
    void unregisterAllScrollingLayers();
#endif

    void resetTrackedRepaintRects();
    void setTracksRepaints(bool tracksRepaints) { m_isTrackingRepaints = tracksRepaints; }


    void setShouldReevaluateCompositingAfterLayout() { m_reevaluateCompositingAfterLayout = true; }

    bool viewHasTransparentBackground(Color* backgroundColor = nullptr) const;

    bool hasNonMainLayersWithTiledBacking() const { return m_layersWithTiledBackingCount; }

    OptionSet<CompositingReason> reasonsForCompositing(const RenderLayer&) const;

    void setLayerFlushThrottlingEnabled(bool);
    void disableLayerFlushThrottlingTemporarilyForInteraction();
    
    void didPaintBacking(RenderLayerBacking*);

    const Color& rootExtendedBackgroundColor() const { return m_rootExtendedBackgroundColor; }

    void updateRootContentLayerClipping();

#if ENABLE(CSS_SCROLL_SNAP)
    void updateScrollSnapPropertiesWithFrameView(const FrameView&);
#endif

    // For testing.
    void startTrackingLayerFlushes() { m_layerFlushCount = 0; }
    unsigned layerFlushCount() const { return m_layerFlushCount; }

    void startTrackingCompositingUpdates() { m_compositingUpdateCount = 0; }
    unsigned compositingUpdateCount() const { return m_compositingUpdateCount; }

private:
    class OverlapMap;
    struct CompositingState;
    struct OverlapExtent;

    // Returns true if the policy changed.
    bool updateCompositingPolicy();
    
    // GraphicsLayerClient implementation
    void notifyFlushRequired(const GraphicsLayer*) override;
    void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const FloatRect&, GraphicsLayerPaintBehavior) override;
    void customPositionForVisibleRectComputation(const GraphicsLayer*, FloatPoint&) const override;
    bool isTrackingRepaints() const override { return m_isTrackingRepaints; }
    
    // GraphicsLayerUpdaterClient implementation
    void flushLayersSoon(GraphicsLayerUpdater&) override;

    // Copy the accelerated compositing related flags from Settings
    void cacheAcceleratedCompositingFlags();
    void cacheAcceleratedCompositingFlagsAfterLayout();

    // Whether the given RL needs a compositing layer.
    bool needsToBeComposited(const RenderLayer&, RenderLayer::ViewportConstrainedNotCompositedReason* = nullptr) const;
    // Whether the layer has an intrinsic need for compositing layer.
    bool requiresCompositingLayer(const RenderLayer&, RenderLayer::ViewportConstrainedNotCompositedReason* = nullptr) const;
    // Whether the layer could ever be composited.
    bool canBeComposited(const RenderLayer&) const;
    bool needsCompositingUpdateForStyleChangeOnNonCompositedLayer(RenderLayer&, const RenderStyle* oldStyle) const;

    // Make or destroy the backing for this layer; returns true if backing changed.
    enum class BackingRequired { No, Yes, Unknown };
    bool updateBacking(RenderLayer&, CompositingChangeRepaint shouldRepaint, BackingRequired = BackingRequired::Unknown);

    void clearBackingForLayerIncludingDescendants(RenderLayer&);

    // Repaint this and its child layers.
    void recursiveRepaintLayer(RenderLayer&);

    void computeExtent(const OverlapMap&, const RenderLayer&, OverlapExtent&) const;
    void addToOverlapMap(OverlapMap&, const RenderLayer&, OverlapExtent&);
    void addToOverlapMapRecursive(OverlapMap&, const RenderLayer&, const RenderLayer* ancestorLayer = nullptr);

    void updateCompositingLayersTimerFired();

    // Returns true if any layer's compositing changed
    void computeCompositingRequirements(RenderLayer* ancestorLayer, RenderLayer&, OverlapMap&, CompositingState&, bool& layersChanged, bool& descendantHas3DTransform);

    // Recurses down the tree, parenting descendant compositing layers and collecting an array of child layers for the current compositing layer.
    void rebuildCompositingLayerTree(RenderLayer&, Vector<Ref<GraphicsLayer>>& childGraphicsLayersOfEnclosingLayer, int depth);

    // Recurses down the tree, updating layer geometry only.
    void updateLayerTreeGeometry(RenderLayer&, int depth);
    
    // Hook compositing layers together
    void setCompositingParent(RenderLayer& childLayer, RenderLayer* parentLayer);
    void removeCompositedChildren(RenderLayer&);

    bool layerHas3DContent(const RenderLayer&) const;
    bool isRunningTransformAnimation(RenderLayerModelObject&) const;

    void appendDocumentOverlayLayers(Vector<Ref<GraphicsLayer>>&);
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

    void updateScrollCoordinatedLayersAfterFlushIncludingSubframes();
    void updateScrollCoordinatedLayersAfterFlush();

    FloatRect visibleRectForLayerFlushing() const;
    
    Page& page() const;
    
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
    bool requiresCompositingForEditableImage(RenderLayerModelObject&) const;
    bool requiresCompositingForScrollableFrame() const;
    bool requiresCompositingForPosition(RenderLayerModelObject&, const RenderLayer&, RenderLayer::ViewportConstrainedNotCompositedReason* = nullptr) const;
    bool requiresCompositingForOverflowScrolling(const RenderLayer&) const;
    bool requiresCompositingForIndirectReason(RenderLayerModelObject&, bool hasCompositedDescendants, bool has3DTransformedDescendants, RenderLayer::IndirectCompositingReason&) const;
    static bool styleChangeMayAffectIndirectCompositingReasons(const RenderLayerModelObject& renderer, const RenderStyle& oldStyle);

    void updateCustomLayersAfterFlush();

    void updateScrollCoordinationForThisFrame(ScrollingNodeID);
    ScrollingNodeID attachScrollingNode(RenderLayer&, ScrollingNodeType, ScrollingNodeID parentNodeID);
    void updateScrollCoordinatedLayer(RenderLayer&, OptionSet<LayerScrollCoordinationRole>, OptionSet<ScrollingNodeChangeFlags>);
    void detachScrollCoordinatedLayer(RenderLayer&, OptionSet<LayerScrollCoordinationRole>);
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
    bool useCoordinatedScrollingForLayer(const RenderLayer&) const;

    bool isAsyncScrollableStickyLayer(const RenderLayer&, const RenderLayer** enclosingAcceleratedOverflowLayer = nullptr) const;
    bool isViewportConstrainedFixedOrStickyLayer(const RenderLayer&) const;
    
    bool shouldCompositeOverflowControls() const;

    void scheduleLayerFlushNow();
    bool isThrottlingLayerFlushes() const;
    void startInitialLayerFlushTimerIfNeeded();
    void startLayerFlushTimerIfNeeded();
    void layerFlushTimerFired();

#if !LOG_DISABLED
    const char* logReasonsForCompositing(const RenderLayer&);
    void logLayerInfo(const RenderLayer&, int depth);
#endif

    bool documentUsesTiledBacking() const;
    bool isMainFrameCompositor() const;
    
    void setRootLayerConfigurationNeedsUpdate() { m_rootLayerConfigurationNeedsUpdate = true; }

private:
    RenderView& m_renderView;
    RefPtr<GraphicsLayer> m_rootContentLayer;
    Timer m_updateCompositingLayersTimer;

    ChromeClient::CompositingTriggerFlags m_compositingTriggers { static_cast<ChromeClient::CompositingTriggerFlags>(ChromeClient::AllTriggers) };
    bool m_hasAcceleratedCompositing { true };
    
    CompositingPolicy m_compositingPolicy { CompositingPolicy::Normal };

    bool m_showDebugBorders { false };
    bool m_showRepaintCounter { false };
    bool m_acceleratedDrawingEnabled { false };
    bool m_displayListDrawingEnabled { false };

    // When true, we have to wait until layout has happened before we can decide whether to enter compositing mode,
    // because only then do we know the final size of plugins and iframes.
    mutable bool m_reevaluateCompositingAfterLayout { false };

    bool m_compositing { false };
    bool m_compositingLayersNeedRebuild { false };
    bool m_rootLayerConfigurationNeedsUpdate { false };
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
    RefPtr<GraphicsLayer> m_clipLayer;
    RefPtr<GraphicsLayer> m_scrollLayer;

#if PLATFORM(IOS_FAMILY)
    HashSet<RenderLayer*> m_scrollingLayers;
    HashSet<RenderLayer*> m_scrollingLayersNeedingUpdate;
#endif
    HashSet<RenderLayer*> m_scrollCoordinatedLayers;
    HashSet<RenderLayer*> m_scrollCoordinatedLayersNeedingUpdate;

    // Enclosing layer for overflow controls and the clipping layer
    RefPtr<GraphicsLayer> m_overflowControlsHostLayer;

    // Layers for overflow controls
    RefPtr<GraphicsLayer> m_layerForHorizontalScrollbar;
    RefPtr<GraphicsLayer> m_layerForVerticalScrollbar;
    RefPtr<GraphicsLayer> m_layerForScrollCorner;
#if ENABLE(RUBBER_BANDING)
    RefPtr<GraphicsLayer> m_layerForOverhangAreas;
    RefPtr<GraphicsLayer> m_contentShadowLayer;
    RefPtr<GraphicsLayer> m_layerForTopOverhangArea;
    RefPtr<GraphicsLayer> m_layerForBottomOverhangArea;
    RefPtr<GraphicsLayer> m_layerForHeader;
    RefPtr<GraphicsLayer> m_layerForFooter;
#endif

    std::unique_ptr<GraphicsLayerUpdater> m_layerUpdater; // Updates tiled layer visible area periodically while animations are running.

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

    Color m_viewBackgroundColor;
    Color m_rootExtendedBackgroundColor;

    HashMap<ScrollingNodeID, RenderLayer*> m_scrollingNodeToLayerMap;
};

void paintScrollbar(Scrollbar*, GraphicsContext&, const IntRect& clip);

WTF::TextStream& operator<<(WTF::TextStream&, CompositingUpdateType);
WTF::TextStream& operator<<(WTF::TextStream&, CompositingPolicy);

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
// Outside the WebCore namespace for ease of invocation from the debugger.
void showGraphicsLayerTreeForCompositor(WebCore::RenderLayerCompositor&);
#endif
