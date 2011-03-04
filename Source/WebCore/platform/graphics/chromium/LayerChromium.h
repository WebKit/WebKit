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
#include "GraphicsContext.h"
#include "GraphicsLayerChromium.h"
#include "PlatformString.h"
#include "ProgramBinding.h"
#include "RenderSurfaceChromium.h"
#include "ShaderChromium.h"
#include "TransformationMatrix.h"

#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>


namespace skia {
class PlatformCanvas;
}

namespace WebCore {

class CCLayerImpl;
class GraphicsContext3D;
class LayerRendererChromium;

// Base class for composited layers. Special layer types are derived from
// this class.
class LayerChromium : public RefCounted<LayerChromium> {
    friend class LayerTilerChromium;
public:
    static PassRefPtr<LayerChromium> create(GraphicsLayerChromium* owner = 0);

    virtual ~LayerChromium();

    const LayerChromium* rootLayer() const;
    LayerChromium* superlayer() const;
    void addSublayer(PassRefPtr<LayerChromium>);
    void insertSublayer(PassRefPtr<LayerChromium>, size_t index);
    void replaceSublayer(LayerChromium* reference, PassRefPtr<LayerChromium> newLayer);
    void removeFromSuperlayer();
    void removeAllSublayers();
    void setSublayers(const Vector<RefPtr<LayerChromium> >&);
    const Vector<RefPtr<LayerChromium> >& getSublayers() const { return m_sublayers; }

    void setAnchorPoint(const FloatPoint& anchorPoint) { m_anchorPoint = anchorPoint; setNeedsCommit(); }
    FloatPoint anchorPoint() const { return m_anchorPoint; }

    void setAnchorPointZ(float anchorPointZ) { m_anchorPointZ = anchorPointZ; setNeedsCommit(); }
    float anchorPointZ() const { return m_anchorPointZ; }

    void setBackgroundColor(const Color& color) { m_backgroundColor = color; setNeedsCommit(); }
    Color backgroundColor() const { return m_backgroundColor; }

    void setClearsContext(bool clears) { m_clearsContext = clears; setNeedsCommit(); }
    bool clearsContext() const { return m_clearsContext; }

    void setFrame(const FloatRect&);
    FloatRect frame() const { return m_frame; }

    void setHidden(bool hidden) { m_hidden = hidden; setNeedsCommit(); }
    bool isHidden() const { return m_hidden; }

    void setMasksToBounds(bool masksToBounds) { m_masksToBounds = masksToBounds; }
    bool masksToBounds() const { return m_masksToBounds; }

    void setName(const String&);
    const String& name() const { return m_name; }

    void setMaskLayer(LayerChromium* maskLayer) { m_maskLayer = maskLayer; }
    CCLayerImpl* maskDrawLayer() const { return m_maskLayer ? m_maskLayer->ccLayerImpl() : 0; }
    LayerChromium* maskLayer() const { return m_maskLayer.get(); }

    void setNeedsDisplay(const FloatRect& dirtyRect);
    void setNeedsDisplay();
    const FloatRect& dirtyRect() const { return m_dirtyRect; }
    void resetNeedsDisplay();

    void setNeedsDisplayOnBoundsChange(bool needsDisplay) { m_needsDisplayOnBoundsChange = needsDisplay; }

    void setOpacity(float opacity) { m_opacity = opacity; setNeedsCommit(); }
    float opacity() const { return m_opacity; }

    void setOpaque(bool opaque) { m_opaque = opaque; setNeedsCommit(); }
    bool opaque() const { return m_opaque; }

    void setPosition(const FloatPoint& position) { m_position = position;  setNeedsCommit(); }
    FloatPoint position() const { return m_position; }

    void setZPosition(float zPosition) { m_zPosition = zPosition; setNeedsCommit(); }
    float zPosition() const {  return m_zPosition; }

    void setSublayerTransform(const TransformationMatrix& transform) { m_sublayerTransform = transform; setNeedsCommit(); }
    const TransformationMatrix& sublayerTransform() const { return m_sublayerTransform; }

    void setTransform(const TransformationMatrix& transform) { m_transform = transform; setNeedsCommit(); }
    const TransformationMatrix& transform() const { return m_transform; }

    // FIXME: This setting is currently ignored.
    void setGeometryFlipped(bool flipped) { m_geometryFlipped = flipped; setNeedsCommit(); }
    bool geometryFlipped() const { return m_geometryFlipped; }

    bool preserves3D() { return m_owner && m_owner->preserves3D(); }

    // Derived types must override this method if they need to react to a change
    // in the LayerRendererChromium.
    virtual void setLayerRenderer(LayerRendererChromium*);

    // Returns true if any of the layer's descendants has content to draw.
    bool descendantsDrawContent();

    void setOwner(GraphicsLayerChromium* owner) { m_owner = owner; }

    void setReplicaLayer(LayerChromium* layer) { m_replicaLayer = layer; }
    LayerChromium* replicaLayer() { return m_replicaLayer; }

    // These methods typically need to be overwritten by derived classes.
    virtual bool drawsContent() const { return false; }
    virtual void updateContentsIfDirty() { }
    virtual void unreserveContentsTexture() { }
    virtual void bindContentsTexture() { }
    virtual void draw() { }

    // These exists just for debugging (via drawDebugBorder()).
    void setBorderColor(const Color&);
    Color borderColor() const;

#ifndef NDEBUG
    int debugID() const { return m_debugID; }
#endif

    void drawDebugBorder();
    String layerTreeAsText() const;

    void setBorderWidth(float);
    float borderWidth() const;

    // Everything from here down in the public section will move to CCLayerImpl.

    CCLayerImpl* ccLayerImpl() const { return m_ccLayerImpl.get(); }

    static void drawTexturedQuad(GraphicsContext3D*, const TransformationMatrix& projectionMatrix, const TransformationMatrix& layerMatrix,
                                 float width, float height, float opacity,
                                 int matrixLocation, int alphaLocation);

    // Begin calls that forward to the CCLayerImpl.
    LayerRendererChromium* layerRenderer() const;
    void setDoubleSided(bool);
    void setBounds(const IntSize&);
    const IntSize& bounds() const;
    // End calls that forward to the CCLayerImpl.

    typedef ProgramBinding<VertexShaderPos, FragmentShaderColor> BorderProgram;
protected:
    GraphicsLayerChromium* m_owner;
    explicit LayerChromium(GraphicsLayerChromium* owner);

    // This is called to clean up resources being held in the same context as
    // layerRendererContext(). Subclasses should override this method if they
    // hold context-dependent resources such as textures.
    virtual void cleanupResources();

    GraphicsContext3D* layerRendererContext() const;

    static void toGLMatrix(float*, const TransformationMatrix&);

    void dumpLayer(TextStream&, int indent) const;

    virtual const char* layerTypeAsString() const { return "LayerChromium"; }
    virtual void dumpLayerProperties(TextStream&, int indent) const;

    FloatRect m_dirtyRect;
    bool m_contentsDirty;

    RefPtr<LayerChromium> m_maskLayer;

    // All layer shaders share the same attribute locations for the vertex positions
    // and texture coordinates. This allows switching shaders without rebinding attribute
    // arrays.
    static const unsigned s_positionAttribLocation;
    static const unsigned s_texCoordAttribLocation;

private:
    void setNeedsCommit();

    void setSuperlayer(LayerChromium* superlayer) { m_superlayer = superlayer; }

    size_t numSublayers() const
    {
        return m_sublayers.size();
    }

    // Returns the index of the sublayer or -1 if not found.
    int indexOfSublayer(const LayerChromium*);

    // This should only be called from removeFromSuperlayer.
    void removeSublayer(LayerChromium*);

    bool descendantsDrawContentRecursive();

    Vector<RefPtr<LayerChromium> > m_sublayers;
    LayerChromium* m_superlayer;

#ifndef NDEBUG
    int m_debugID;
#endif

    // Layer properties.
    FloatPoint m_position;
    FloatPoint m_anchorPoint;
    Color m_backgroundColor;
    float m_opacity;
    float m_zPosition;
    float m_anchorPointZ;
    bool m_clearsContext;
    bool m_hidden;
    bool m_masksToBounds;
    bool m_opaque;
    bool m_geometryFlipped;
    bool m_needsDisplayOnBoundsChange;

    TransformationMatrix m_transform;
    TransformationMatrix m_sublayerTransform;

    FloatRect m_frame;
    // For now, the LayerChromium directly owns its CCLayerImpl.
    RefPtr<CCLayerImpl> m_ccLayerImpl;

    // Replica layer used for reflections.
    LayerChromium* m_replicaLayer;

    String m_name;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
