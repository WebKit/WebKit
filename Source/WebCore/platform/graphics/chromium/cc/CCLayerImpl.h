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

#include "Color.h"
#include "FilterOperations.h"
#include "FloatRect.h"
#include "IntRect.h"
#include "Region.h"
#include "TextStream.h"
#include "TransformationMatrix.h"
#include "cc/CCLayerAnimationController.h"
#include "cc/CCRenderSurface.h"
#include "cc/CCSharedQuadState.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CCLayerSorter;
class CCQuadCuller;
class LayerChromium;
class LayerRendererChromium;

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
    virtual void setTransformFromAnimation(const TransformationMatrix&) OVERRIDE;
    virtual const TransformationMatrix& transform() const OVERRIDE { return m_transform; }
    virtual const IntSize& bounds() const OVERRIDE { return m_bounds; }

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

    PassOwnPtr<CCSharedQuadState> createSharedQuadState() const;
    // willDraw must be called before appendQuads. If willDraw is called,
    // didDraw is guaranteed to be called before another willDraw or before
    // the layer is destroyed. To enforce this, any class that overrides
    // willDraw/didDraw must call the base class version.
    virtual void willDraw(LayerRendererChromium*);
    virtual void appendQuads(CCQuadCuller&, const CCSharedQuadState*, bool& hadMissingTiles) { }
    virtual void didDraw();
    void appendDebugBorderQuad(CCQuadCuller&, const CCSharedQuadState*) const;

    virtual void bindContentsTexture(LayerRendererChromium*);

    // Returns true if this layer has content to draw.
    void setDrawsContent(bool);
    bool drawsContent() const { return m_drawsContent; }

    // Returns true if any of the layer's descendants has content to draw.
    bool descendantDrawsContent();

    void setAnchorPoint(const FloatPoint&);
    const FloatPoint& anchorPoint() const { return m_anchorPoint; }

    void setAnchorPointZ(float);
    float anchorPointZ() const { return m_anchorPointZ; }

    void setBackgroundColor(const Color&);
    Color backgroundColor() const { return m_backgroundColor; }

    void setFilters(const FilterOperations&);
    const FilterOperations& filters() const { return m_filters; }

    void setBackgroundFilters(const FilterOperations&);
    const FilterOperations& backgroundFilters() const { return m_backgroundFilters; }

    void setMasksToBounds(bool);
    bool masksToBounds() const { return m_masksToBounds; }

    void setOpaque(bool);
    bool opaque() const { return m_opaque; }

    void setOpacity(float);
    bool opacityIsAnimating() const;

    void setPosition(const FloatPoint&);
    const FloatPoint& position() const { return m_position; }

    void setPreserves3D(bool);
    bool preserves3D() const { return m_preserves3D; }

    void setUsesLayerClipping(bool usesLayerClipping) { m_usesLayerClipping = usesLayerClipping; }
    bool usesLayerClipping() const { return m_usesLayerClipping; }

    void setIsNonCompositedContent(bool isNonCompositedContent) { m_isNonCompositedContent = isNonCompositedContent; }
    bool isNonCompositedContent() const { return m_isNonCompositedContent; }

    void setSublayerTransform(const TransformationMatrix&);
    const TransformationMatrix& sublayerTransform() const { return m_sublayerTransform; }

    // Debug layer border - visual effect only, do not change geometry/clipping/etc.
    void setDebugBorderColor(Color);
    Color debugBorderColor() const { return m_debugBorderColor; }
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

    // Usage: if this->usesLayerClipping() is false, then this clipRect should not be used.
    const IntRect& clipRect() const { return m_clipRect; }
    void setClipRect(const IntRect& rect) { m_clipRect = rect; }
    CCRenderSurface* targetRenderSurface() const { return m_targetRenderSurface; }
    void setTargetRenderSurface(CCRenderSurface* surface) { m_targetRenderSurface = surface; }

    void setBounds(const IntSize&);

    const IntSize& contentBounds() const { return m_contentBounds; }
    void setContentBounds(const IntSize&);

    const IntPoint& scrollPosition() const { return m_scrollPosition; }
    void setScrollPosition(const IntPoint&);

    const IntSize& maxScrollPosition() const {return m_maxScrollPosition; }
    void setMaxScrollPosition(const IntSize& maxScrollPosition) { m_maxScrollPosition = maxScrollPosition; }

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

    const IntRect& visibleLayerRect() const { return m_visibleLayerRect; }
    void setVisibleLayerRect(const IntRect& visibleLayerRect) { m_visibleLayerRect = visibleLayerRect; }

    bool doubleSided() const { return m_doubleSided; }
    void setDoubleSided(bool);

    // Returns the rect containtaining this layer in the current view's coordinate system.
    const IntRect getDrawRect() const;

    void setTransform(const TransformationMatrix&);
    bool transformIsAnimating() const;

    const TransformationMatrix& drawTransform() const { return m_drawTransform; }
    void setDrawTransform(const TransformationMatrix& matrix) { m_drawTransform = matrix; }
    const TransformationMatrix& screenSpaceTransform() const { return m_screenSpaceTransform; }
    void setScreenSpaceTransform(const TransformationMatrix& matrix) { m_screenSpaceTransform = matrix; }

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

    bool layerPropertyChanged() const { return m_layerPropertyChanged; }
    void resetAllChangeTrackingForSubtree();

    CCLayerAnimationController* layerAnimationController() { return m_layerAnimationController.get(); }

    virtual Region visibleContentOpaqueRegion() const;

    // Indicates that the context previously used to render this layer
    // was lost and that a new one has been created. Won't be called
    // until the new context has been created successfully.
    virtual void didLoseContext();

protected:
    explicit CCLayerImpl(int);

    virtual void dumpLayerProperties(TextStream&, int indent) const;
    static void writeIndent(TextStream&, int indent);

    // Transformation used to transform quads provided in appendQuads.
    virtual TransformationMatrix quadTransform() const;

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
    Color m_backgroundColor;

    // Whether the "back" of this layer should draw.
    bool m_doubleSided;

    // Tracks if drawing-related properties have changed since last redraw.
    bool m_layerPropertyChanged;

    IntRect m_visibleLayerRect;
    bool m_masksToBounds;
    bool m_opaque;
    float m_opacity;
    FloatPoint m_position;
    bool m_preserves3D;
    bool m_drawCheckerboardForMissingTiles;
    TransformationMatrix m_sublayerTransform;
    TransformationMatrix m_transform;
    bool m_usesLayerClipping;
    bool m_isNonCompositedContent;

    bool m_drawsContent;

    FloatSize m_scrollDelta;
    IntSize m_sentScrollDelta;
    IntSize m_maxScrollPosition;
    float m_pageScaleDelta;

    // Render surface this layer draws into. This is a surface that can belong
    // either to this layer (if m_targetRenderSurface == m_renderSurface) or
    // to an ancestor of this layer. The target render surface determines the
    // coordinate system the layer's transforms are relative to.
    CCRenderSurface* m_targetRenderSurface;

    // The global depth value of the center of the layer. This value is used
    // to sort layers from back to front.
    float m_drawDepth;
    float m_drawOpacity;
    bool m_drawOpacityIsAnimating;

    // Debug borders.
    Color m_debugBorderColor;
    float m_debugBorderWidth;

    // Debug layer name.
    String m_debugName;

    FilterOperations m_filters;
    FilterOperations m_backgroundFilters;

    TransformationMatrix m_drawTransform;
    TransformationMatrix m_screenSpaceTransform;
    bool m_drawTransformIsAnimating;
    bool m_screenSpaceTransformIsAnimating;

#ifndef NDEBUG
    bool m_betweenWillDrawAndDidDraw;
#endif

    // The rect that contributes to the scissor when this layer is drawn.
    // Inherited by the parent layer and further restricted if this layer masks
    // to bounds.
    IntRect m_clipRect;

    // Render surface associated with this layer. The layer and its descendants
    // will render to this surface.
    OwnPtr<CCRenderSurface> m_renderSurface;

    // Hierarchical bounding rect containing the layer and its descendants.
    IntRect m_drawableContentRect;

    // Rect indicating what was repainted/updated during update.
    // Note that plugin layers bypass this and leave it empty.
    FloatRect m_updateRect;

    // Manages animations for this layer.
    OwnPtr<CCLayerAnimationController> m_layerAnimationController;
};

void sortLayers(Vector<CCLayerImpl*>::iterator first, Vector<CCLayerImpl*>::iterator end, CCLayerSorter*);

}

#endif // CCLayerImpl_h
