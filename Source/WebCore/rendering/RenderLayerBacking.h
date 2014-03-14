/*
 * Copyright (C) 2009, 2010, 2011 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef RenderLayerBacking_h
#define RenderLayerBacking_h

#include "FloatPoint.h"
#include "FloatPoint3D.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerClient.h"
#include "RenderLayer.h"
#include "ScrollingCoordinator.h"

namespace WebCore {

class KeyframeList;
class RenderLayerCompositor;
class TiledBacking;
class TransformationMatrix;

enum CompositingLayerType {
    NormalCompositingLayer, // non-tiled layer with backing store
    TiledCompositingLayer, // tiled layer (always has backing store)
    MediaCompositingLayer, // layer that contains an image, video, webGL or plugin
    ContainerCompositingLayer // layer with no backing store
};

// RenderLayerBacking controls the compositing behavior for a single RenderLayer.
// It holds the various GraphicsLayers, and makes decisions about intra-layer rendering
// optimizations.
// 
// There is one RenderLayerBacking for each RenderLayer that is composited.

class RenderLayerBacking : public GraphicsLayerClient {
    WTF_MAKE_NONCOPYABLE(RenderLayerBacking); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RenderLayerBacking(RenderLayer&);
    ~RenderLayerBacking();

#if PLATFORM(IOS)
    void layerWillBeDestroyed();
#endif

    RenderLayer& owningLayer() const { return m_owningLayer; }

    enum UpdateAfterLayoutFlag {
        CompositingChildrenOnly = 1 << 0,
        NeedsFullRepaint = 1 << 1,
        IsUpdateRoot = 1 << 2
    };
    typedef unsigned UpdateAfterLayoutFlags;
    void updateAfterLayout(UpdateAfterLayoutFlags);
    
    // Returns true if layer configuration changed.
    bool updateGraphicsLayerConfiguration();
    // Update graphics layer position and bounds.
    void updateGraphicsLayerGeometry(); // make private
    // Update contents and clipping structure.
    void updateDrawsContent();
    
    GraphicsLayer* graphicsLayer() const { return m_graphicsLayer.get(); }

    // Layer to clip children
    bool hasClippingLayer() const { return (m_childContainmentLayer && !m_usingTiledCacheLayer); }
    GraphicsLayer* clippingLayer() const { return !m_usingTiledCacheLayer ? m_childContainmentLayer.get() : 0; }

    // Layer to get clipped by ancestor
    bool hasAncestorClippingLayer() const { return m_ancestorClippingLayer != 0; }
    GraphicsLayer* ancestorClippingLayer() const { return m_ancestorClippingLayer.get(); }

    GraphicsLayer* contentsContainmentLayer() const { return m_contentsContainmentLayer.get(); }

    bool hasContentsLayer() const { return m_foregroundLayer != 0; }
    GraphicsLayer* foregroundLayer() const { return m_foregroundLayer.get(); }

    GraphicsLayer* backgroundLayer() const { return m_backgroundLayer.get(); }
    bool backgroundLayerPaintsFixedRootBackground() const { return m_backgroundLayerPaintsFixedRootBackground; }
    
    bool hasScrollingLayer() const { return m_scrollingLayer != nullptr; }
    GraphicsLayer* scrollingLayer() const { return m_scrollingLayer.get(); }
    GraphicsLayer* scrollingContentsLayer() const { return m_scrollingContentsLayer.get(); }

    void detachFromScrollingCoordinator();

    ScrollingNodeID viewportConstrainedNodeID() const { return m_viewportConstrainedNodeID; }
    void setViewportConstrainedNodeID(ScrollingNodeID nodeID) { m_viewportConstrainedNodeID = nodeID; }

    ScrollingNodeID scrollingNodeID() const { return m_scrollingNodeID; }
    void setScrollingNodeID(ScrollingNodeID nodeID) { m_scrollingNodeID = nodeID; }
    
    ScrollingNodeID scrollingNodeIDForChildren() const { return m_scrollingNodeID ? m_scrollingNodeID : m_viewportConstrainedNodeID; }

    bool hasMaskLayer() const { return m_maskLayer != 0; }

    GraphicsLayer* parentForSublayers() const;
    GraphicsLayer* childForSuperlayers() const;

    // RenderLayers with backing normally short-circuit paintLayer() because
    // their content is rendered via callbacks from GraphicsLayer. However, the document
    // layer is special, because it has a GraphicsLayer to act as a container for the GraphicsLayers
    // for descendants, but its contents usually render into the window (in which case this returns true).
    // This returns false for other layers, and when the document layer actually needs to paint into its backing store
    // for some reason.
    bool paintsIntoWindow() const;
    
    // Returns true for a composited layer that has no backing store of its own, so
    // paints into some ancestor layer.
    bool paintsIntoCompositedAncestor() const { return !m_requiresOwnBackingStore; }

    void setRequiresOwnBackingStore(bool);

    void setContentsNeedDisplay(GraphicsLayer::ShouldClipToLayer = GraphicsLayer::ClipToLayer);
    // r is in the coordinate space of the layer's render object
    void setContentsNeedDisplayInRect(const LayoutRect&, GraphicsLayer::ShouldClipToLayer = GraphicsLayer::ClipToLayer);

    // Notification from the renderer that its content changed.
    void contentChanged(ContentChangeType);

    // Interface to start, finish, suspend and resume animations and transitions
    bool startTransition(double, CSSPropertyID, const RenderStyle* fromStyle, const RenderStyle* toStyle);
    void transitionPaused(double timeOffset, CSSPropertyID);
    void transitionFinished(CSSPropertyID);

    bool startAnimation(double timeOffset, const Animation* anim, const KeyframeList& keyframes);
    void animationPaused(double timeOffset, const String& name);
    void animationFinished(const String& name);

    void suspendAnimations(double time = 0);
    void resumeAnimations();

    LayoutRect compositedBounds() const;
    void setCompositedBounds(const LayoutRect&);
    void updateCompositedBounds();
    
    void updateAfterWidgetResize();
    void positionOverflowControlsLayers();
    bool hasUnpositionedOverflowControlsLayers() const;

    bool usingTiledBacking() const { return m_usingTiledCacheLayer; }
    TiledBacking* tiledBacking() const;
    void adjustTiledBackingCoverage();
    void setTiledBackingHasMargins(bool);
    
    void updateDebugIndicators(bool showBorder, bool showRepaintCounter);

    // GraphicsLayerClient interface
    virtual bool shouldUseTiledBacking(const GraphicsLayer*) const override;
    virtual void tiledBackingUsageChanged(const GraphicsLayer*, bool /*usingTiledBacking*/) override;
    virtual void notifyAnimationStarted(const GraphicsLayer*, double startTime) override;
    virtual void notifyFlushRequired(const GraphicsLayer*) override;
    virtual void notifyFlushBeforeDisplayRefresh(const GraphicsLayer*) override;

    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const FloatRect& clip) override;

    virtual float deviceScaleFactor() const override;
    virtual float contentsScaleMultiplierForNewTiles(const GraphicsLayer*) const override;

    virtual float pageScaleFactor() const override;
    virtual void didCommitChangesForLayer(const GraphicsLayer*) const override;
    virtual bool getCurrentTransform(const GraphicsLayer*, TransformationMatrix&) const override;

    virtual bool isTrackingRepaints() const override;
    virtual bool shouldSkipLayerInDump(const GraphicsLayer*) const override;
    virtual bool shouldDumpPropertyForLayer(const GraphicsLayer*, const char* propertyName) const override;

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    virtual bool mediaLayerMustBeUpdatedOnMainThread() const override;
#endif

#ifndef NDEBUG
    virtual void verifyNotPainting();
#endif

    LayoutRect contentsBox() const;
    
    // For informative purposes only.
    CompositingLayerType compositingLayerType() const;
    
    GraphicsLayer* layerForHorizontalScrollbar() const { return m_layerForHorizontalScrollbar.get(); }
    GraphicsLayer* layerForVerticalScrollbar() const { return m_layerForVerticalScrollbar.get(); }
    GraphicsLayer* layerForScrollCorner() const { return m_layerForScrollCorner.get(); }

#if ENABLE(CSS_FILTERS)
    bool canCompositeFilters() const { return m_canCompositeFilters; }
#endif

    // Return an estimate of the backing store area (in pixels) allocated by this object's GraphicsLayers.
    double backingStoreMemoryEstimate() const;

#if ENABLE(CSS_COMPOSITING)
    void setBlendMode(BlendMode);
#endif

private:
    FloatRect backgroundBoxForPainting() const;

    void createPrimaryGraphicsLayer();
    void destroyGraphicsLayers();
    
    void willDestroyLayer(const GraphicsLayer*);

    LayoutRect compositedBoundsIncludingMargin() const;
    
    std::unique_ptr<GraphicsLayer> createGraphicsLayer(const String&);

    RenderLayerModelObject& renderer() const { return m_owningLayer.renderer(); }
    RenderLayerCompositor& compositor() const { return m_owningLayer.compositor(); }

    void updateInternalHierarchy();
    bool updateAncestorClippingLayer(bool needsAncestorClip);
    bool updateDescendantClippingLayer(bool needsDescendantClip);
    bool updateOverflowControlsLayers(bool needsHorizontalScrollbarLayer, bool needsVerticalScrollbarLayer, bool needsScrollCornerLayer);
    bool updateForegroundLayer(bool needsForegroundLayer);
    bool updateBackgroundLayer(bool needsBackgroundLayer);
    bool updateMaskLayer(bool needsMaskLayer);
    bool requiresHorizontalScrollbarLayer() const;
    bool requiresVerticalScrollbarLayer() const;
    bool requiresScrollCornerLayer() const;
    bool updateScrollingLayers(bool scrollingLayers);
    void updateDrawsContent(bool isSimpleContainer);
    
    void updateRootLayerConfiguration();

    void setBackgroundLayerPaintsFixedRootBackground(bool);

    GraphicsLayerPaintingPhase paintingPhaseForPrimaryLayer() const;
    
    LayoutSize contentOffsetInCompostingLayer() const;
    // Result is transform origin in pixels.
    FloatPoint3D computeTransformOrigin(const LayoutRect& borderBox) const;
    // Result is perspective origin in pixels.
    FloatPoint computePerspectiveOrigin(const LayoutRect& borderBox) const;

    void updateOpacity(const RenderStyle*);
    void updateTransform(const RenderStyle*);
#if ENABLE(CSS_FILTERS)
    void updateFilters(const RenderStyle*);
#endif
#if ENABLE(CSS_COMPOSITING)
    void updateBlendMode(const RenderStyle*);
#endif
    // Return the opacity value that this layer should use for compositing.
    float compositingOpacity(float rendererOpacity) const;
    
    bool isMainFrameRenderViewLayer() const;
    
    bool paintsBoxDecorations() const;
    bool paintsChildren() const;

    // Returns true if this compositing layer has no visible content.
    bool isSimpleContainerCompositingLayer() const;
    // Returns true if this layer has content that needs to be rendered by painting into the backing store.
    bool containsPaintedContent(bool isSimpleContainer) const;
    // Returns true if the RenderLayer just contains an image that we can composite directly.
    bool isDirectlyCompositedImage() const;
    void updateImageContents();

    Color rendererBackgroundColor() const;
    void updateDirectlyCompositedBackgroundColor(bool isSimpleContainer, bool& didUpdateContentsRect);
    void updateDirectlyCompositedBackgroundImage(bool isSimpleContainer, bool& didUpdateContentsRect);
    void updateDirectlyCompositedContents(bool isSimpleContainer, bool& didUpdateContentsRect);

    void resetContentsRect();

    bool hasVisibleNonCompositingDescendantLayers() const;

    bool shouldClipCompositedBounds() const;

    bool hasTiledBackingFlatteningLayer() const { return (m_childContainmentLayer && m_usingTiledCacheLayer); }
    GraphicsLayer* tileCacheFlatteningLayer() const { return m_usingTiledCacheLayer ? m_childContainmentLayer.get() : 0; }

    void paintIntoLayer(const GraphicsLayer*, GraphicsContext*, const IntRect& paintDirtyRect, PaintBehavior, GraphicsLayerPaintingPhase);

    // Helper function for updateGraphicsLayerGeometry.
    void adjustAncestorCompositingBoundsForFlowThread(LayoutRect& ancestorCompositingBounds, const RenderLayer* compositingAncestor) const;

    static CSSPropertyID graphicsLayerToCSSProperty(AnimatedPropertyID);
    static AnimatedPropertyID cssToGraphicsLayerProperty(CSSPropertyID);

    RenderLayer& m_owningLayer;

    std::unique_ptr<GraphicsLayer> m_ancestorClippingLayer; // Only used if we are clipped by an ancestor which is not a stacking context.
    std::unique_ptr<GraphicsLayer> m_contentsContainmentLayer; // Only used if we have a background layer; takes the transform.
    std::unique_ptr<GraphicsLayer> m_graphicsLayer;
    std::unique_ptr<GraphicsLayer> m_foregroundLayer; // Only used in cases where we need to draw the foreground separately.
    std::unique_ptr<GraphicsLayer> m_backgroundLayer; // Only used in cases where we need to draw the background separately.
    std::unique_ptr<GraphicsLayer> m_childContainmentLayer; // Only used if we have clipping on a stacking context with compositing children, or if the layer has a tile cache.
    std::unique_ptr<GraphicsLayer> m_maskLayer; // Only used if we have a mask.

    std::unique_ptr<GraphicsLayer> m_layerForHorizontalScrollbar;
    std::unique_ptr<GraphicsLayer> m_layerForVerticalScrollbar;
    std::unique_ptr<GraphicsLayer> m_layerForScrollCorner;

    std::unique_ptr<GraphicsLayer> m_scrollingLayer; // Only used if the layer is using composited scrolling.
    std::unique_ptr<GraphicsLayer> m_scrollingContentsLayer; // Only used if the layer is using composited scrolling.

    ScrollingNodeID m_viewportConstrainedNodeID;
    ScrollingNodeID m_scrollingNodeID;

    LayoutRect m_compositedBounds;
    LayoutSize m_devicePixelFractionFromRenderer;

    bool m_artificiallyInflatedBounds; // bounds had to be made non-zero to make transform-origin work
    bool m_isMainFrameRenderViewLayer;
    bool m_usingTiledCacheLayer;
    bool m_requiresOwnBackingStore;
#if ENABLE(CSS_FILTERS)
    bool m_canCompositeFilters;
#endif
    bool m_backgroundLayerPaintsFixedRootBackground;

    static bool m_creatingPrimaryGraphicsLayer;
};

enum CanvasCompositingStrategy {
    UnacceleratedCanvas,
    CanvasPaintedToLayer,
    CanvasAsLayerContents
};
CanvasCompositingStrategy canvasCompositingStrategy(const RenderObject&);

} // namespace WebCore

#endif // RenderLayerBacking_h
