/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef LayerChromium_h
#define LayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "FloatPoint.h"
#include "Region.h"
#include "RenderSurfaceChromium.h"
#include "SkColor.h"
#include "cc/CCLayerAnimationController.h"
#include "cc/CCOcclusionTracker.h"
#include "cc/CCPrioritizedTexture.h"

#include <public/WebFilterOperations.h>
#include <public/WebTransformationMatrix.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
class WebAnimationDelegate;
}

namespace WebCore {

class CCActiveAnimation;
struct CCAnimationEvent;
class CCLayerAnimationDelegate;
class CCLayerImpl;
class CCLayerTreeHost;
class CCTextureUpdateQueue;
class ScrollbarLayerChromium;
struct CCAnimationEvent;
struct CCRenderingStats;

// Delegate for handling scroll input for a LayerChromium.
class LayerChromiumScrollDelegate {
public:
    virtual void didScroll(const IntSize&) = 0;

protected:
    virtual ~LayerChromiumScrollDelegate() { }
};

// Base class for composited layers. Special layer types are derived from
// this class.
class LayerChromium : public RefCounted<LayerChromium>, public CCLayerAnimationControllerClient {
public:
    static PassRefPtr<LayerChromium> create();

    virtual ~LayerChromium();

    // CCLayerAnimationControllerClient implementation
    virtual int id() const OVERRIDE { return m_layerId; }
    virtual void setOpacityFromAnimation(float) OVERRIDE;
    virtual float opacity() const OVERRIDE { return m_opacity; }
    virtual void setTransformFromAnimation(const WebKit::WebTransformationMatrix&) OVERRIDE;
    // A layer's transform operates layer space. That is, entirely in logical,
    // non-page-scaled pixels (that is, they have page zoom baked in, but not page scale).
    // The root layer is a special case -- it operates in physical pixels.
    virtual const WebKit::WebTransformationMatrix& transform() const OVERRIDE { return m_transform; }

    const LayerChromium* rootLayer() const;
    LayerChromium* parent() const;
    void addChild(PassRefPtr<LayerChromium>);
    void insertChild(PassRefPtr<LayerChromium>, size_t index);
    void replaceChild(LayerChromium* reference, PassRefPtr<LayerChromium> newLayer);
    void removeFromParent();
    void removeAllChildren();
    void setChildren(const Vector<RefPtr<LayerChromium> >&);
    const Vector<RefPtr<LayerChromium> >& children() const { return m_children; }

    void setAnchorPoint(const FloatPoint&);
    FloatPoint anchorPoint() const { return m_anchorPoint; }

    void setAnchorPointZ(float);
    float anchorPointZ() const { return m_anchorPointZ; }

    void setBackgroundColor(SkColor);
    SkColor backgroundColor() const { return m_backgroundColor; }

    // A layer's bounds are in logical, non-page-scaled pixels (however, the
    // root layer's bounds are in physical pixels).
    void setBounds(const IntSize&);
    const IntSize& bounds() const { return m_bounds; }
    virtual IntSize contentBounds() const { return bounds(); }

    void setMasksToBounds(bool);
    bool masksToBounds() const { return m_masksToBounds; }

    void setMaskLayer(LayerChromium*);
    LayerChromium* maskLayer() const { return m_maskLayer.get(); }

    virtual void setNeedsDisplayRect(const FloatRect& dirtyRect);
    void setNeedsDisplay() { setNeedsDisplayRect(FloatRect(FloatPoint(), bounds())); }
    virtual bool needsDisplay() const { return m_needsDisplay; }

    void setOpacity(float);
    bool opacityIsAnimating() const;

    void setFilters(const WebKit::WebFilterOperations&);
    const WebKit::WebFilterOperations& filters() const { return m_filters; }

    // Background filters are filters applied to what is behind this layer, when they are viewed through non-opaque
    // regions in this layer. They are used through the WebLayer interface, and are not exposed to HTML.
    void setBackgroundFilters(const WebKit::WebFilterOperations&);
    const WebKit::WebFilterOperations& backgroundFilters() const { return m_backgroundFilters; }

    virtual void setOpaque(bool);
    bool opaque() const { return m_opaque; }

    void setPosition(const FloatPoint&);
    FloatPoint position() const { return m_position; }

    void setIsContainerForFixedPositionLayers(bool);
    bool isContainerForFixedPositionLayers() const { return m_isContainerForFixedPositionLayers; }

    void setFixedToContainerLayer(bool);
    bool fixedToContainerLayer() const { return m_fixedToContainerLayer; }

    void setSublayerTransform(const WebKit::WebTransformationMatrix&);
    const WebKit::WebTransformationMatrix& sublayerTransform() const { return m_sublayerTransform; }

    void setTransform(const WebKit::WebTransformationMatrix&);
    bool transformIsAnimating() const;

    const IntRect& visibleContentRect() const { return m_visibleContentRect; }
    void setVisibleContentRect(const IntRect& visibleContentRect) { m_visibleContentRect = visibleContentRect; }

    void setScrollPosition(const IntPoint&);
    const IntPoint& scrollPosition() const { return m_scrollPosition; }

    void setMaxScrollPosition(const IntSize&);
    const IntSize& maxScrollPosition() const { return m_maxScrollPosition; }

    void setScrollable(bool);
    bool scrollable() const { return m_scrollable; }
    void setShouldScrollOnMainThread(bool);
    void setHaveWheelEventHandlers(bool);
    const Region& nonFastScrollableRegion() { return m_nonFastScrollableRegion; }
    void setNonFastScrollableRegion(const Region&);
    void setNonFastScrollableRegionChanged() { m_nonFastScrollableRegionChanged = true; }
    void setLayerScrollDelegate(LayerChromiumScrollDelegate* layerScrollDelegate) { m_layerScrollDelegate = layerScrollDelegate; }
    void scrollBy(const IntSize&);

    void setDrawCheckerboardForMissingTiles(bool);
    bool drawCheckerboardForMissingTiles() const { return m_drawCheckerboardForMissingTiles; }

    bool forceRenderSurface() const { return m_forceRenderSurface; }
    void setForceRenderSurface(bool);

    IntSize scrollDelta() const { return IntSize(); }

    float pageScaleDelta() const { return 1; }

    void setDoubleSided(bool);
    bool doubleSided() const { return m_doubleSided; }

    void setPreserves3D(bool preserve3D) { m_preserves3D = preserve3D; }
    bool preserves3D() const { return m_preserves3D; }

    void setUseParentBackfaceVisibility(bool useParentBackfaceVisibility) { m_useParentBackfaceVisibility = useParentBackfaceVisibility; }
    bool useParentBackfaceVisibility() const { return m_useParentBackfaceVisibility; }

    virtual void setUseLCDText(bool);
    bool useLCDText() const { return m_useLCDText; }

    virtual void setLayerTreeHost(CCLayerTreeHost*);

    void setIsDrawable(bool);

    void setReplicaLayer(LayerChromium*);
    LayerChromium* replicaLayer() const { return m_replicaLayer.get(); }

    bool hasMask() const { return m_maskLayer; }
    bool hasReplica() const { return m_replicaLayer; }
    bool replicaHasMask() const { return m_replicaLayer && (m_maskLayer || m_replicaLayer->m_maskLayer); }

    // These methods typically need to be overwritten by derived classes.
    virtual bool drawsContent() const { return m_isDrawable; }
    virtual void update(CCTextureUpdateQueue&, const CCOcclusionTracker*, CCRenderingStats&) { }
    virtual bool needMoreUpdates() { return false; }
    virtual void setIsMask(bool) { }
    virtual void bindContentsTexture() { }
    virtual bool needsContentsScale() const { return false; }

    void setDebugBorderColor(SkColor);
    void setDebugBorderWidth(float);
    void setDebugName(const String&);

    virtual void pushPropertiesTo(CCLayerImpl*);

    void clearRenderSurface() { m_renderSurface.clear(); }
    RenderSurfaceChromium* renderSurface() const { return m_renderSurface.get(); }
    void createRenderSurface();

    float drawOpacity() const { return m_drawOpacity; }
    void setDrawOpacity(float opacity) { m_drawOpacity = opacity; }

    bool drawOpacityIsAnimating() const { return m_drawOpacityIsAnimating; }
    void setDrawOpacityIsAnimating(bool drawOpacityIsAnimating) { m_drawOpacityIsAnimating = drawOpacityIsAnimating; }

    LayerChromium* renderTarget() const { ASSERT(!m_renderTarget || m_renderTarget->renderSurface()); return m_renderTarget; }
    void setRenderTarget(LayerChromium* target) { m_renderTarget = target; }

    bool drawTransformIsAnimating() const { return m_drawTransformIsAnimating; }
    void setDrawTransformIsAnimating(bool animating) { m_drawTransformIsAnimating = animating; }
    bool screenSpaceTransformIsAnimating() const { return m_screenSpaceTransformIsAnimating; }
    void setScreenSpaceTransformIsAnimating(bool animating) { m_screenSpaceTransformIsAnimating = animating; }

    // This moves from layer space, with origin in the center to target space with origin in the top left.
    // That is, it converts from logical, non-page-scaled, to target pixels (and if the target is the
    // root render surface, then this converts to physical pixels).
    const WebKit::WebTransformationMatrix& drawTransform() const { return m_drawTransform; }
    void setDrawTransform(const WebKit::WebTransformationMatrix& matrix) { m_drawTransform = matrix; }
    // This moves from content space, with origin the top left to screen space with origin in the top left.
    // It converts logical, non-page-scaled pixels to physical pixels.
    const WebKit::WebTransformationMatrix& screenSpaceTransform() const { return m_screenSpaceTransform; }
    void setScreenSpaceTransform(const WebKit::WebTransformationMatrix& matrix) { m_screenSpaceTransform = matrix; }
    const IntRect& drawableContentRect() const { return m_drawableContentRect; }
    void setDrawableContentRect(const IntRect& rect) { m_drawableContentRect = rect; }
    // The contentsScale converts from logical, non-page-scaled pixels to target pixels.
    // The contentsScale is 1 for the root layer as it is already in physical pixels.
    float contentsScale() const { return m_contentsScale; }
    void setContentsScale(float);

    // Returns true if any of the layer's descendants has content to draw.
    bool descendantDrawsContent();

    CCLayerTreeHost* layerTreeHost() const { return m_layerTreeHost; }

    // Set the priority of all desired textures in this layer.
    virtual void setTexturePriorities(const CCPriorityCalculator&) { }

    bool addAnimation(PassOwnPtr<CCActiveAnimation>);
    void pauseAnimation(int animationId, double timeOffset);
    void removeAnimation(int animationId);

    void suspendAnimations(double monotonicTime);
    void resumeAnimations(double monotonicTime);

    CCLayerAnimationController* layerAnimationController() { return m_layerAnimationController.get(); }
    void setLayerAnimationController(PassOwnPtr<CCLayerAnimationController>);
    PassOwnPtr<CCLayerAnimationController> releaseLayerAnimationController();

    void setLayerAnimationDelegate(WebKit::WebAnimationDelegate* layerAnimationDelegate) { m_layerAnimationDelegate = layerAnimationDelegate; }

    bool hasActiveAnimation() const;

    virtual void notifyAnimationStarted(const CCAnimationEvent&, double wallClockTime);
    virtual void notifyAnimationFinished(double wallClockTime);

    virtual Region visibleContentOpaqueRegion() const;

    virtual ScrollbarLayerChromium* toScrollbarLayerChromium() { return 0; }

protected:
    friend class CCLayerImpl;
    friend class TreeSynchronizer;

    LayerChromium();

    void setNeedsCommit();

    // This flag is set when layer need repainting/updating.
    bool m_needsDisplay;

    // Tracks whether this layer may have changed stacking order with its siblings.
    bool m_stackingOrderChanged;

    // The update rect is the region of the compositor resource that was actually updated by the compositor.
    // For layers that may do updating outside the compositor's control (i.e. plugin layers), this information
    // is not available and the update rect will remain empty.
    // Note this rect is in layer space (not content space).
    FloatRect m_updateRect;

    RefPtr<LayerChromium> m_maskLayer;

    // Constructs a CCLayerImpl of the correct runtime type for this LayerChromium type.
    virtual PassOwnPtr<CCLayerImpl> createCCLayerImpl();
    int m_layerId;

private:
    void setParent(LayerChromium*);
    bool hasAncestor(LayerChromium*) const;
    bool descendantIsFixedToContainerLayer() const;

    size_t numChildren() const { return m_children.size(); }

    // Returns the index of the child or -1 if not found.
    int indexOfChild(const LayerChromium*);

    // This should only be called from removeFromParent.
    void removeChild(LayerChromium*);

    Vector<RefPtr<LayerChromium> > m_children;
    LayerChromium* m_parent;

    // LayerChromium instances have a weak pointer to their CCLayerTreeHost.
    // This pointer value is nil when a LayerChromium is not in a tree and is
    // updated via setLayerTreeHost() if a layer moves between trees.
    CCLayerTreeHost* m_layerTreeHost;

    OwnPtr<CCLayerAnimationController> m_layerAnimationController;

    // Layer properties.
    IntSize m_bounds;

    // Uses layer's content space.
    IntRect m_visibleContentRect;

    IntPoint m_scrollPosition;
    IntSize m_maxScrollPosition;
    bool m_scrollable;
    bool m_shouldScrollOnMainThread;
    bool m_haveWheelEventHandlers;
    Region m_nonFastScrollableRegion;
    bool m_nonFastScrollableRegionChanged;
    FloatPoint m_position;
    FloatPoint m_anchorPoint;
    SkColor m_backgroundColor;
    SkColor m_debugBorderColor;
    float m_debugBorderWidth;
    String m_debugName;
    float m_opacity;
    WebKit::WebFilterOperations m_filters;
    WebKit::WebFilterOperations m_backgroundFilters;
    float m_anchorPointZ;
    bool m_isContainerForFixedPositionLayers;
    bool m_fixedToContainerLayer;
    bool m_isDrawable;
    bool m_masksToBounds;
    bool m_opaque;
    bool m_doubleSided;
    bool m_useLCDText;
    bool m_preserves3D;
    bool m_useParentBackfaceVisibility;
    bool m_drawCheckerboardForMissingTiles;
    bool m_forceRenderSurface;

    WebKit::WebTransformationMatrix m_transform;
    WebKit::WebTransformationMatrix m_sublayerTransform;

    // Replica layer used for reflections.
    RefPtr<LayerChromium> m_replicaLayer;

    // Transient properties.
    OwnPtr<RenderSurfaceChromium> m_renderSurface;
    float m_drawOpacity;
    bool m_drawOpacityIsAnimating;

    LayerChromium* m_renderTarget;

    WebKit::WebTransformationMatrix m_drawTransform;
    WebKit::WebTransformationMatrix m_screenSpaceTransform;
    bool m_drawTransformIsAnimating;
    bool m_screenSpaceTransformIsAnimating;

    // Uses target surface space.
    IntRect m_drawableContentRect;
    float m_contentsScale;

    WebKit::WebAnimationDelegate* m_layerAnimationDelegate;
    LayerChromiumScrollDelegate* m_layerScrollDelegate;
};

void sortLayers(Vector<RefPtr<LayerChromium> >::iterator, Vector<RefPtr<LayerChromium> >::iterator, void*);

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
