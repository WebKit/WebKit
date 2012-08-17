/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCLayerImpl_h
#define CCLayerImpl_h

#include "CCInputHandler.h"
#include "CCLayerAnimationController.h"
#include "CCRenderSurface.h"
#include "CCResourceProvider.h"
#include "CCSharedQuadState.h"
#include "FloatRect.h"
#include "IntRect.h"
#include "Region.h"
#include "SkColor.h"
#include "TextStream.h"
#include <public/WebFilterOperations.h>
#include <public/WebTransformationMatrix.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CCLayerSorter;
class CCLayerTreeHostImpl;
class CCRenderer;
class CCQuadSink;
class CCScrollbarAnimationController;
class CCScrollbarLayerImpl;
class LayerChromium;

class CCLayerImpl : public CCLayerAnimationControllerClient {
public:
    static PassOwnPtr<CCLayerImpl> create(int id)
    {
        return adoptPtr(new CCLayerImpl(id));
    }

    virtual ~CCLayerImpl();

    // CCLayerAnimationControllerClient implementation.
    virtual int id() const OVERRIDE { return m_layerId; }
    virtual void setOpacityFromAnimation(float) OVERRIDE;
    virtual float opacity() const OVERRIDE { return m_opacity; }
    virtual void setTransformFromAnimation(const WebKit::WebTransformationMatrix&) OVERRIDE;
    virtual const WebKit::WebTransformationMatrix& transform() const OVERRIDE { return m_transform; }

    // Tree structure.
    CCLayerImpl* parent() const { return m_parent; }
    const Vector<OwnPtr<CCLayerImpl> >& children() const { return m_children; }
    void addChild(PassOwnPtr<CCLayerImpl>);
    void removeFromParent();
    void removeAllChildren();

    void setMaskLayer(PassOwnPtr<CCLayerImpl>);
    CCLayerImpl* maskLayer() const { return m_maskLayer.get(); }

    void setReplicaLayer(PassOwnPtr<CCLayerImpl>);
    CCLayerImpl* replicaLayer() const { return m_replicaLayer.get(); }

    bool hasMask() const { return m_maskLayer; }
    bool hasReplica() const { return m_replicaLayer; }
    bool replicaHasMask() const { return m_replicaLayer && (m_maskLayer || m_replicaLayer->m_maskLayer); }

    CCLayerTreeHostImpl* layerTreeHostImpl() const { return m_layerTreeHostImpl; }
    void setLayerTreeHostImpl(CCLayerTreeHostImpl* hostImpl) { m_layerTreeHostImpl = hostImpl; }

    PassOwnPtr<CCSharedQuadState> createSharedQuadState(int id) const;
    // willDraw must be called before appendQuads. If willDraw is called,
    // didDraw is guaranteed to be called before another willDraw or before
    // the layer is destroyed. To enforce this, any class that overrides
    // willDraw/didDraw must call the base class version.
    virtual void willDraw(CCResourceProvider*);
    virtual void appendQuads(CCQuadSink&, const CCSharedQuadState*, bool& hadMissingTiles) { }
    virtual void didDraw(CCResourceProvider*);
    void appendDebugBorderQuad(CCQuadSink&, const CCSharedQuadState*) const;

    virtual CCResourceProvider::ResourceId contentsResourceId() const;

    // Returns true if this layer has content to draw.
    void setDrawsContent(bool);
    bool drawsContent() const { return m_drawsContent; }

    bool forceRenderSurface() const { return m_forceRenderSurface; }
    void setForceRenderSurface(bool force) { m_forceRenderSurface = force; }

    // Returns true if any of the layer's descendants has content to draw.
    bool descendantDrawsContent();

    void setAnchorPoint(const FloatPoint&);
    const FloatPoint& anchorPoint() const { return m_anchorPoint; }

    void setAnchorPointZ(float);
    float anchorPointZ() const { return m_anchorPointZ; }

    void setBackgroundColor(SkColor);
    SkColor backgroundColor() const { return m_backgroundColor; }

    void setFilters(const WebKit::WebFilterOperations&);
    const WebKit::WebFilterOperations& filters() const { return m_filters; }

    void setBackgroundFilters(const WebKit::WebFilterOperations&);
    const WebKit::WebFilterOperations& backgroundFilters() const { return m_backgroundFilters; }

    void setMasksToBounds(bool);
    bool masksToBounds() const { return m_masksToBounds; }

    void setOpaque(bool);
    bool opaque() const { return m_opaque; }

    void setOpacity(float);
    bool opacityIsAnimating() const;

    void setPosition(const FloatPoint&);
    const FloatPoint& position() const { return m_position; }

    void setIsContainerForFixedPositionLayers(bool isContainerForFixedPositionLayers) { m_isContainerForFixedPositionLayers = isContainerForFixedPositionLayers; }
    bool isContainerForFixedPositionLayers() const { return m_isContainerForFixedPositionLayers; }

    void setFixedToContainerLayer(bool fixedToContainerLayer = true) { m_fixedToContainerLayer = fixedToContainerLayer;}
    bool fixedToContainerLayer() const { return m_fixedToContainerLayer; }

    void setPreserves3D(bool);
    bool preserves3D() const { return m_preserves3D; }

    void setUseParentBackfaceVisibility(bool useParentBackfaceVisibility) { m_useParentBackfaceVisibility = useParentBackfaceVisibility; }
    bool useParentBackfaceVisibility() const { return m_useParentBackfaceVisibility; }

    void setUseLCDText(bool useLCDText) { m_useLCDText = useLCDText; }
    bool useLCDText() const { return m_useLCDText; }

    void setSublayerTransform(const WebKit::WebTransformationMatrix&);
    const WebKit::WebTransformationMatrix& sublayerTransform() const { return m_sublayerTransform; }

    // Debug layer border - visual effect only, do not change geometry/clipping/etc.
    void setDebugBorderColor(SkColor);
    SkColor debugBorderColor() const { return m_debugBorderColor; }
    void setDebugBorderWidth(float);
    float debugBorderWidth() const { return m_debugBorderWidth; }
    bool hasDebugBorders() const;

    // Debug layer name.
    void setDebugName(const String& debugName) { m_debugName = debugName; }
    String debugName() const { return m_debugName; }

    CCRenderSurface* renderSurface() const { return m_renderSurface.get(); }
    void createRenderSurface();
    void clearRenderSurface() { m_renderSurface.clear(); }

    float drawOpacity() const { return m_drawOpacity; }
    void setDrawOpacity(float opacity) { m_drawOpacity = opacity; }

    bool drawOpacityIsAnimating() const { return m_drawOpacityIsAnimating; }
    void setDrawOpacityIsAnimating(bool drawOpacityIsAnimating) { m_drawOpacityIsAnimating = drawOpacityIsAnimating; }

    CCLayerImpl* renderTarget() const { ASSERT(!m_renderTarget || m_renderTarget->renderSurface()); return m_renderTarget; }
    void setRenderTarget(CCLayerImpl* target) { m_renderTarget = target; }

    void setBounds(const IntSize&);
    const IntSize& bounds() const { return m_bounds; }

    const IntSize& contentBounds() const { return m_contentBounds; }
    void setContentBounds(const IntSize&);

    const IntPoint& scrollPosition() const { return m_scrollPosition; }
    void setScrollPosition(const IntPoint&);

    const IntSize& maxScrollPosition() const {return m_maxScrollPosition; }
    void setMaxScrollPosition(const IntSize&);

    const FloatSize& scrollDelta() const { return m_scrollDelta; }
    void setScrollDelta(const FloatSize&);

    float pageScaleDelta() const { return m_pageScaleDelta; }
    void setPageScaleDelta(float);

    const IntSize& sentScrollDelta() const { return m_sentScrollDelta; }
    void setSentScrollDelta(const IntSize& sentScrollDelta) { m_sentScrollDelta = sentScrollDelta; }

    void scrollBy(const FloatSize& scroll);

    bool scrollable() const { return m_scrollable; }
    void setScrollable(bool scrollable) { m_scrollable = scrollable; }

    bool shouldScrollOnMainThread() const { return m_shouldScrollOnMainThread; }
    void setShouldScrollOnMainThread(bool shouldScrollOnMainThread) { m_shouldScrollOnMainThread = shouldScrollOnMainThread; }

    bool haveWheelEventHandlers() const { return m_haveWheelEventHandlers; }
    void setHaveWheelEventHandlers(bool haveWheelEventHandlers) { m_haveWheelEventHandlers = haveWheelEventHandlers; }

    const Region& nonFastScrollableRegion() const { return m_nonFastScrollableRegion; }
    void setNonFastScrollableRegion(const Region& region) { m_nonFastScrollableRegion = region; }

    void setDrawCheckerboardForMissingTiles(bool checkerboard) { m_drawCheckerboardForMissingTiles = checkerboard; }
    bool drawCheckerboardForMissingTiles() const { return m_drawCheckerboardForMissingTiles; }

    CCInputHandlerClient::ScrollStatus tryScroll(const IntPoint& viewportPoint, CCInputHandlerClient::ScrollInputType) const;

    const IntRect& visibleContentRect() const { return m_visibleContentRect; }
    void setVisibleContentRect(const IntRect& visibleContentRect) { m_visibleContentRect = visibleContentRect; }

    bool doubleSided() const { return m_doubleSided; }
    void setDoubleSided(bool);

    void setTransform(const WebKit::WebTransformationMatrix&);
    bool transformIsAnimating() const;

    const WebKit::WebTransformationMatrix& drawTransform() const { return m_drawTransform; }
    void setDrawTransform(const WebKit::WebTransformationMatrix& matrix) { m_drawTransform = matrix; }
    const WebKit::WebTransformationMatrix& screenSpaceTransform() const { return m_screenSpaceTransform; }
    void setScreenSpaceTransform(const WebKit::WebTransformationMatrix& matrix) { m_screenSpaceTransform = matrix; }

    bool drawTransformIsAnimating() const { return m_drawTransformIsAnimating; }
    void setDrawTransformIsAnimating(bool animating) { m_drawTransformIsAnimating = animating; }
    bool screenSpaceTransformIsAnimating() const { return m_screenSpaceTransformIsAnimating; }
    void setScreenSpaceTransformIsAnimating(bool animating) { m_screenSpaceTransformIsAnimating = animating; }

    const IntRect& drawableContentRect() const { return m_drawableContentRect; }
    void setDrawableContentRect(const IntRect& rect) { m_drawableContentRect = rect; }
    const FloatRect& updateRect() const { return m_updateRect; }
    void setUpdateRect(const FloatRect& updateRect) { m_updateRect = updateRect; }

    String layerTreeAsText() const;

    void setStackingOrderChanged(bool);

    bool layerPropertyChanged() const { return m_layerPropertyChanged || layerIsAlwaysDamaged(); }
    bool layerSurfacePropertyChanged() const;

    void resetAllChangeTrackingForSubtree();

    virtual bool layerIsAlwaysDamaged() const { return false; }

    CCLayerAnimationController* layerAnimationController() { return m_layerAnimationController.get(); }

    virtual Region visibleContentOpaqueRegion() const;

    // Indicates that the context previously used to render this layer
    // was lost and that a new one has been created. Won't be called
    // until the new context has been created successfully.
    virtual void didLoseContext();

    CCScrollbarAnimationController* scrollbarAnimationController() const { return m_scrollbarAnimationController.get(); }

    CCScrollbarLayerImpl* horizontalScrollbarLayer() const;
    void setHorizontalScrollbarLayer(CCScrollbarLayerImpl*);

    CCScrollbarLayerImpl* verticalScrollbarLayer() const;
    void setVerticalScrollbarLayer(CCScrollbarLayerImpl*);

protected:
    explicit CCLayerImpl(int);

    virtual void dumpLayerProperties(TextStream&, int indent) const;
    static void writeIndent(TextStream&, int indent);

private:
    void setParent(CCLayerImpl* parent) { m_parent = parent; }
    friend class TreeSynchronizer;
    void clearChildList(); // Warning: This does not preserve tree structure invariants and so is only exposed to the tree synchronizer.

    void noteLayerPropertyChangedForSubtree();

    // Note carefully this does not affect the current layer.
    void noteLayerPropertyChangedForDescendants();

    virtual const char* layerTypeAsString() const { return "LayerChromium"; }

    void dumpLayer(TextStream&, int indent) const;

    // Properties internal to CCLayerImpl
    CCLayerImpl* m_parent;
    Vector<OwnPtr<CCLayerImpl> > m_children;
    // m_maskLayer can be temporarily stolen during tree sync, we need this ID to confirm newly assigned layer is still the previous one
    int m_maskLayerId;
    OwnPtr<CCLayerImpl> m_maskLayer;
    int m_replicaLayerId; // ditto
    OwnPtr<CCLayerImpl> m_replicaLayer;
    int m_layerId;
    CCLayerTreeHostImpl* m_layerTreeHostImpl;

    // Properties synchronized from the associated LayerChromium.
    FloatPoint m_anchorPoint;
    float m_anchorPointZ;
    IntSize m_bounds;
    IntSize m_contentBounds;
    IntPoint m_scrollPosition;
    bool m_scrollable;
    bool m_shouldScrollOnMainThread;
    bool m_haveWheelEventHandlers;
    Region m_nonFastScrollableRegion;
    SkColor m_backgroundColor;

    // Whether the "back" of this layer should draw.
    bool m_doubleSided;

    // Tracks if drawing-related properties have changed since last redraw.
    bool m_layerPropertyChanged;

    // Indicates that a property has changed on this layer that would not
    // affect the pixels on its target surface, but would require redrawing
    // but would require redrawing the targetSurface onto its ancestor targetSurface.
    // For layers that do not own a surface this flag acts as m_layerPropertyChanged.
    bool m_layerSurfacePropertyChanged;

    // Uses layer's content space.
    IntRect m_visibleContentRect;
    bool m_masksToBounds;
    bool m_opaque;
    float m_opacity;
    FloatPoint m_position;
    bool m_preserves3D;
    bool m_useParentBackfaceVisibility;
    bool m_drawCheckerboardForMissingTiles;
    WebKit::WebTransformationMatrix m_sublayerTransform;
    WebKit::WebTransformationMatrix m_transform;
    bool m_useLCDText;

    bool m_drawsContent;
    bool m_forceRenderSurface;

    // Set for the layer that other layers are fixed to.
    bool m_isContainerForFixedPositionLayers;
    // This is true if the layer should be fixed to the closest ancestor container.
    bool m_fixedToContainerLayer;

    FloatSize m_scrollDelta;
    IntSize m_sentScrollDelta;
    IntSize m_maxScrollPosition;
    float m_pageScaleDelta;

    // The layer whose coordinate space this layer draws into. This can be
    // either the same layer (m_renderTarget == this) or an ancestor of this
    // layer.
    CCLayerImpl* m_renderTarget;

    // The global depth value of the center of the layer. This value is used
    // to sort layers from back to front.
    float m_drawDepth;
    float m_drawOpacity;
    bool m_drawOpacityIsAnimating;

    // Debug borders.
    SkColor m_debugBorderColor;
    float m_debugBorderWidth;

    // Debug layer name.
    String m_debugName;

    WebKit::WebFilterOperations m_filters;
    WebKit::WebFilterOperations m_backgroundFilters;

    WebKit::WebTransformationMatrix m_drawTransform;
    WebKit::WebTransformationMatrix m_screenSpaceTransform;
    bool m_drawTransformIsAnimating;
    bool m_screenSpaceTransformIsAnimating;

#ifndef NDEBUG
    bool m_betweenWillDrawAndDidDraw;
#endif

    // Render surface associated with this layer. The layer and its descendants
    // will render to this surface.
    OwnPtr<CCRenderSurface> m_renderSurface;

    // Hierarchical bounding rect containing the layer and its descendants.
    // Uses target surface's space.
    IntRect m_drawableContentRect;

    // Rect indicating what was repainted/updated during update.
    // Note that plugin layers bypass this and leave it empty.
    // Uses layer's content space.
    FloatRect m_updateRect;

    // Manages animations for this layer.
    OwnPtr<CCLayerAnimationController> m_layerAnimationController;

    // Manages scrollbars for this layer
    OwnPtr<CCScrollbarAnimationController> m_scrollbarAnimationController;
};

void sortLayers(Vector<CCLayerImpl*>::iterator first, Vector<CCLayerImpl*>::iterator end, CCLayerSorter*);

}

#endif // CCLayerImpl_h
