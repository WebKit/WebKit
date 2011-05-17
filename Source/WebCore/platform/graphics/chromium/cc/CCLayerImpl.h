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
#include "FloatRect.h"
#include "IntRect.h"
#include "TextStream.h"
#include "TransformationMatrix.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class LayerChromium;
class LayerRendererChromium;
class RenderSurfaceChromium;

class CCLayerImpl : public RefCounted<CCLayerImpl> {
public:
    static PassRefPtr<CCLayerImpl> create(LayerChromium* owner, int id)
    {
        return adoptRef(new CCLayerImpl(owner, id));
    }
    virtual ~CCLayerImpl();

    // Tree structure.
    CCLayerImpl* parent() const { return m_parent; }
    const Vector<RefPtr<CCLayerImpl> >& children() const { return m_children; }
    void addChild(PassRefPtr<CCLayerImpl>);
    void removeFromParent();
    void removeAllChildren();

    void setMaskLayer(PassRefPtr<CCLayerImpl> maskLayer) { m_maskLayer = maskLayer; }
    CCLayerImpl* maskLayer() const { return m_maskLayer.get(); }

    void setReplicaLayer(PassRefPtr<CCLayerImpl> replicaLayer) { m_replicaLayer = replicaLayer; }
    CCLayerImpl* replicaLayer() const { return m_replicaLayer.get(); }

    int id() const { return m_layerId; }

#ifndef NDEBUG
    int debugID() const { return m_debugID; }
#endif

    virtual void draw(const IntRect& contentRect);
    virtual void updateCompositorResources();
    void unreserveContentsTexture();
    void bindContentsTexture();

    // Returns true if this layer has content to draw.
    virtual bool drawsContent() const;

    // Returns true if any of the layer's descendants has content to draw.
    bool descendantsDrawsContent();

    void cleanupResources();

    void setAnchorPoint(const FloatPoint& anchorPoint) { m_anchorPoint = anchorPoint; }
    const FloatPoint& anchorPoint() const { return m_anchorPoint; }

    void setAnchorPointZ(float anchorPointZ) { m_anchorPointZ = anchorPointZ; }
    float anchorPointZ() const { return m_anchorPointZ; }

    void setMasksToBounds(bool masksToBounds) { m_masksToBounds = masksToBounds; }
    bool masksToBounds() const { return m_masksToBounds; }

    void setOpacity(float opacity) { m_opacity = opacity; }
    float opacity() const { return m_opacity; }

    void setPosition(const FloatPoint& position) { m_position = position; }
    const FloatPoint& position() const { return m_position; }

    void setPreserves3D(bool preserves3D) { m_preserves3D = preserves3D; }
    bool preserves3D() const { return m_preserves3D; }

    void setSublayerTransform(const TransformationMatrix& sublayerTransform) { m_sublayerTransform = sublayerTransform; }
    const TransformationMatrix& sublayerTransform() const { return m_sublayerTransform; }

    void setTransform(const TransformationMatrix& transform) { m_transform = transform; }
    const TransformationMatrix& transform() const { return m_transform; }

    void setName(const String& name) { m_name = name; }
    const String& name() const { return m_name; }

    // Debug layer border - visual effect only, do not change geometry/clipping/etc.
    void setDebugBorderColor(Color c) { m_debugBorderColor = c; }
    Color debugBorderColor() const { return m_debugBorderColor; }
    void setDebugBorderWidth(float width) { m_debugBorderWidth = width; }
    float debugBorderWidth() const { return m_debugBorderWidth; }

    void drawDebugBorder();

    void setLayerRenderer(LayerRendererChromium*);
    LayerRendererChromium* layerRenderer() const { return m_layerRenderer.get(); }

    RenderSurfaceChromium* createRenderSurface();

    RenderSurfaceChromium* renderSurface() const { return m_renderSurface.get(); }
    void clearRenderSurface() { m_renderSurface.clear(); }
    float drawOpacity() const { return m_drawOpacity; }
    void setDrawOpacity(float opacity) { m_drawOpacity = opacity; }
    const IntRect& scissorRect() const { return m_scissorRect; }
    void setScissorRect(const IntRect& rect) { m_scissorRect = rect; }
    RenderSurfaceChromium* targetRenderSurface() const { return m_targetRenderSurface; }
    void setTargetRenderSurface(RenderSurfaceChromium* surface) { m_targetRenderSurface = surface; }

    bool doubleSided() const { return m_doubleSided; }
    void setDoubleSided(bool doubleSided) { m_doubleSided = doubleSided; }
    const IntSize& bounds() const { return m_bounds; }
    void setBounds(const IntSize& bounds) { m_bounds = bounds; }

    // Returns the rect containtaining this layer in the current view's coordinate system.
    const IntRect getDrawRect() const;

    const TransformationMatrix& drawTransform() const { return m_drawTransform; }
    void setDrawTransform(const TransformationMatrix& matrix) { m_drawTransform = matrix; }
    const IntRect& drawableContentRect() const { return m_drawableContentRect; }
    void setDrawableContentRect(const IntRect& rect) { m_drawableContentRect = rect; }

    virtual void dumpLayerProperties(TextStream&, int indent) const;

    // HACK TODO fix this
    LayerChromium* owner() const { return m_owner; }
    void clearOwner() { m_owner = 0; }
protected:
    // For now, CCLayerImpls have a back pointer to their LayerChromium.
    // FIXME: remove this after https://bugs.webkit.org/show_bug.cgi?id=58833 is fixed.
    LayerChromium* m_owner;
    CCLayerImpl(LayerChromium*, int);

    static void writeIndent(TextStream&, int indent);

private:
    void setParent(CCLayerImpl* parent) { m_parent = parent; }
    friend class TreeSynchronizer;
    void clearChildList(); // Warning: This does not preserve tree structure invariants and so is only exposed to the tree synchronizer.

    // Properties internal to CCLayerImpl
    CCLayerImpl* m_parent;
    Vector<RefPtr<CCLayerImpl> > m_children;
    RefPtr<CCLayerImpl> m_maskLayer;
    RefPtr<CCLayerImpl> m_replicaLayer;
    int m_layerId;

    // Properties synchronized from the associated LayerChromium.
    FloatPoint m_anchorPoint;
    float m_anchorPointZ;
    IntSize m_bounds;

    // Whether the "back" of this layer should draw.
    bool m_doubleSided;

    bool m_masksToBounds;
    float m_opacity;
    FloatPoint m_position;
    bool m_preserves3D;
    TransformationMatrix m_sublayerTransform;
    TransformationMatrix m_transform;

    // Properties owned exclusively by this CCLayerImpl.
    // Debugging.
#ifndef NDEBUG
    int m_debugID;
#endif

    String m_name;

    // Render surface this layer draws into. This is a surface that can belong
    // either to this layer (if m_targetRenderSurface == m_renderSurface) or
    // to an ancestor of this layer. The target render surface determines the
    // coordinate system the layer's transforms are relative to.
    RenderSurfaceChromium* m_targetRenderSurface;

    // The global depth value of the center of the layer. This value is used
    // to sort layers from back to front.
    float m_drawDepth;
    float m_drawOpacity;

    // Debug borders.
    Color m_debugBorderColor;
    float m_debugBorderWidth;

    TransformationMatrix m_drawTransform;

    // The scissor rectangle that should be used when this layer is drawn.
    // Inherited by the parent layer and further restricted if this layer masks
    // to bounds.
    IntRect m_scissorRect;

    // Render surface associated with this layer. The layer and its descendants
    // will render to this surface.
    OwnPtr<RenderSurfaceChromium> m_renderSurface;

    // Hierarchical bounding rect containing the layer and its descendants.
    IntRect m_drawableContentRect;

    // Points to the layer renderer that updates and draws this layer.
    RefPtr<LayerRendererChromium> m_layerRenderer;
};

}

#endif // CCLayerImpl_h
