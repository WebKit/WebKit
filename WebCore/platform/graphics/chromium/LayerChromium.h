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
#include "TransformationMatrix.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>


namespace skia {
class PlatformCanvas;
}

namespace WebCore {

class GraphicsContext3D;
class LayerRendererChromium;

// Base class for composited layers. Special layer types are derived from
// this class.
class LayerChromium : public RefCounted<LayerChromium> {
    friend class LayerRendererChromium;
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

    void setBorderColor(const Color& color) { m_borderColor = color; setNeedsCommit(); }
    Color borderColor() const { return m_borderColor; }

    void setBorderWidth(float width) { m_borderWidth = width; setNeedsCommit(); }
    float borderWidth() const { return m_borderWidth; }

    void setBounds(const IntSize&);
    IntSize bounds() const { return m_bounds; }

    void setClearsContext(bool clears) { m_clearsContext = clears; setNeedsCommit(); }
    bool clearsContext() const { return m_clearsContext; }

    void setDoubleSided(bool doubleSided) { m_doubleSided = doubleSided; setNeedsCommit(); }
    bool doubleSided() const { return m_doubleSided; }

    void setFrame(const FloatRect&);
    FloatRect frame() const { return m_frame; }

    void setHidden(bool hidden) { m_hidden = hidden; setNeedsCommit(); }
    bool isHidden() const { return m_hidden; }

    void setMasksToBounds(bool masksToBounds) { m_masksToBounds = masksToBounds; }
    bool masksToBounds() const { return m_masksToBounds; }

    void setName(const String& name) { m_name = name; }
    String name() const { return m_name; }

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

    void setDrawTransform(const TransformationMatrix& transform) { m_drawTransform = transform; }
    const TransformationMatrix& drawTransform() const { return m_drawTransform; }

    void setDrawOpacity(float opacity) { m_drawOpacity = opacity; }
    float drawOpacity() const { return m_drawOpacity; }

    bool preserves3D() { return m_owner && m_owner->preserves3D(); }

    void setLayerRenderer(LayerRendererChromium*);

    void setOwner(GraphicsLayerChromium* owner) { m_owner = owner; }

    bool contentsDirty() { return m_contentsDirty; }

    // Returns the rect containtaining this layer in the current view's coordinate system.
    const FloatRect getDrawRect() const;

    // These methods typically need to be overwritten by derived classes.
    virtual bool drawsContent() { return false; }
    virtual void updateContents() { };
    virtual void draw() { };

    void drawDebugBorder();

    // Draws the layer without a texture. This is used for stencil operations.
    void drawAsMask();

    // Stores values that are shared between instances of this class that are
    // associated with the same LayerRendererChromium (and hence the same GL
    // context).
    class SharedValues {
    public:
        explicit SharedValues(GraphicsContext3D*);
        ~SharedValues();

        GraphicsContext3D* context() const { return m_context; }
        unsigned quadVerticesVbo() const { return m_quadVerticesVbo; }
        unsigned quadElementsVbo() const { return m_quadElementsVbo; }
        int maxTextureSize() const { return m_maxTextureSize; }
        unsigned borderShaderProgram() const { return m_borderShaderProgram; }
        int borderShaderMatrixLocation() const { return m_borderShaderMatrixLocation; }
        int borderShaderColorLocation() const { return m_borderShaderColorLocation; }
        bool initialized() const { return m_initialized; }

    private:
        GraphicsContext3D* m_context;
        unsigned m_quadVerticesVbo;
        unsigned m_quadElementsVbo;
        int m_maxTextureSize;
        unsigned m_borderShaderProgram;
        int m_borderShaderMatrixLocation;
        int m_borderShaderColorLocation;
        bool m_initialized;
    };

    static void prepareForDraw(const SharedValues*);

protected:
    GraphicsLayerChromium* m_owner;
    LayerChromium(GraphicsLayerChromium* owner);

    LayerRendererChromium* layerRenderer() const { return m_layerRenderer; }
    GraphicsContext3D* layerRendererContext() const;

    static void drawTexturedQuad(GraphicsContext3D*, const TransformationMatrix& projectionMatrix, const TransformationMatrix& layerMatrix,
                                 float width, float height, float opacity,
                                 int matrixLocation, int alphaLocation);

    static void toGLMatrix(float*, const TransformationMatrix&);

    static unsigned createShaderProgram(GraphicsContext3D*, const char* vertexShaderSource, const char* fragmentShaderSource);

    IntSize m_bounds;
    FloatRect m_dirtyRect;
    bool m_contentsDirty;

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

    Vector<RefPtr<LayerChromium> > m_sublayers;
    LayerChromium* m_superlayer;

    // Layer properties.
    IntSize m_backingStoreSize;
    FloatPoint m_position;
    FloatPoint m_anchorPoint;
    Color m_backgroundColor;
    Color m_borderColor;
    float m_opacity;
    float m_zPosition;
    float m_anchorPointZ;
    float m_borderWidth;
    float m_drawOpacity;
    bool m_clearsContext;
    bool m_doubleSided;
    bool m_hidden;
    bool m_masksToBounds;
    bool m_opaque;
    bool m_geometryFlipped;
    bool m_needsDisplayOnBoundsChange;

    // Points to the layer renderer that updates and draws this layer.
    LayerRendererChromium* m_layerRenderer;

    FloatRect m_frame;
    TransformationMatrix m_transform;
    TransformationMatrix m_sublayerTransform;
    TransformationMatrix m_drawTransform;

    String m_name;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
