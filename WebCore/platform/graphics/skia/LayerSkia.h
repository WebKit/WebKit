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


#ifndef LayerSkia_h
#define LayerSkia_h

#if USE(ACCELERATED_COMPOSITING)


#include "GraphicsContext.h"
#include "GraphicsLayerSkia.h"
#include "PlatformString.h"
#include "SkColor.h"
#include "SkPoint.h"
#include "StringHash.h"
#include "TransformationMatrix.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>


namespace skia {
class PlatformCanvas;
}

namespace WebCore {

class LayerSkia : public RefCounted<LayerSkia> {
public:
    enum LayerType { Layer, TransformLayer };
    enum FilterType { Linear, Nearest, Trilinear, Lanczos };
    enum ContentsGravityType { Center, Top, Bottom, Left, Right, TopLeft, TopRight,
                               BottomLeft, BottomRight, Resize, ResizeAspect, ResizeAspectFill };

    static PassRefPtr<LayerSkia> create(LayerType, GraphicsLayerSkia* owner = 0);

    ~LayerSkia();

    void display(PlatformGraphicsContext*);

    void addSublayer(PassRefPtr<LayerSkia>);
    void insertSublayer(PassRefPtr<LayerSkia>, size_t index);
    void removeFromSuperlayer();

    void setAnchorPoint(const SkPoint& anchorPoint) { m_anchorPoint = anchorPoint; setNeedsCommit(); }
    SkPoint anchorPoint() const { return m_anchorPoint; }

    void setAnchorPointZ(float anchorPointZ) { m_anchorPointZ = anchorPointZ; setNeedsCommit(); }
    float anchorPointZ() const { return m_anchorPointZ; }

    void setBackgroundColor(const Color& color) { m_backgroundColor = color; setNeedsCommit(); }
    Color backgroundColor() const { return m_backgroundColor; }

    void setBorderColor(const Color& color) { m_borderColor = color; setNeedsCommit(); }
    Color borderColor() const { return m_borderColor; }

    void setBorderWidth(float width) { m_borderWidth = width; setNeedsCommit(); }
    SkScalar borderWidth() const { return m_borderWidth; }

    void setBounds(const SkIRect&);
    SkIRect bounds() const { return m_bounds; }

    void setClearsContext(bool clears) { m_clearsContext = clears; setNeedsCommit(); }
    bool clearsContext() const { return m_clearsContext; }

    void setContentsGravity(ContentsGravityType gravityType) { m_contentsGravity = gravityType; setNeedsCommit(); }
    ContentsGravityType contentsGravity() const { return m_contentsGravity; }

    void setDoubleSided(bool doubleSided) { m_doubleSided = doubleSided; setNeedsCommit(); }
    bool doubleSided() const { return m_doubleSided; }

    void setEdgeAntialiasingMask(uint32_t mask) { m_edgeAntialiasingMask = mask; setNeedsCommit(); }
    uint32_t edgeAntialiasingMask() const { return m_edgeAntialiasingMask; }

    void setFrame(const SkRect&);
    SkRect frame() const { return m_frame; }

    void setHidden(bool hidden) { m_hidden = hidden; setNeedsCommit(); }
    bool isHidden() const { return m_hidden; }

    void setMasksToBounds(bool masksToBounds) { m_masksToBounds = masksToBounds; }
    bool masksToBounds() const { return m_masksToBounds; }

    void setName(const String& name) { m_name = name; }
    String name() const { return m_name; }

    void setNeedsDisplay(const SkRect& dirtyRect);
    void setNeedsDisplay();

    void setNeedsDisplayOnBoundsChange(bool needsDisplay) { m_needsDisplayOnBoundsChange = needsDisplay; }

    void setOpacity(float opacity) { m_opacity = opacity; setNeedsCommit(); }
    float opacity() const { return m_opacity; }

    void setOpaque(bool opaque) { m_opaque = opaque; setNeedsCommit(); }
    bool opaque() const { return m_opaque; }

    void setPosition(const SkPoint& position) { m_position = position;  setNeedsCommit(); }

    SkPoint position() const { return m_position; }

    void setZPosition(float zPosition) { m_zPosition = zPosition; setNeedsCommit(); }
    SkScalar zPosition() const {  return m_zPosition; }

    const LayerSkia* rootLayer() const;

    void removeAllSublayers();

    void setSublayers(const Vector<RefPtr<LayerSkia> >&);

    const Vector<RefPtr<LayerSkia> >& getSublayers() const { return m_sublayers; }

    void setSublayerTransform(const SkMatrix& transform) { m_sublayerTransform = transform; setNeedsCommit(); }
    const SkMatrix& sublayerTransform() const { return m_sublayerTransform; }

    void setSuperlayer(LayerSkia* superlayer);
    LayerSkia* superlayer() const;


    void setTransform(const SkMatrix& transform) { m_transform = transform; setNeedsCommit(); }
    const SkMatrix& transform() const { return m_transform; }

    void setGeometryFlipped(bool flipped) { m_geometryFlipped = flipped; setNeedsCommit(); }
    bool geometryFlipped() const { return m_geometryFlipped; }

    void updateContents();

    skia::PlatformCanvas* platformCanvas() { return m_canvas.get(); }
    GraphicsContext* graphicsContext() { return m_graphicsContext.get(); }

    void setBackingStoreRect(const SkIRect&);

    void drawDebugBorder();

private:
    LayerSkia(LayerType, GraphicsLayerSkia* owner);

    void setNeedsCommit();

    void paintMe();

    size_t numSublayers() const
    {
        return m_sublayers.size();
    }

    // Returns the index of the sublayer or -1 if not found.
    int indexOfSublayer(const LayerSkia*);

    // This should only be called from removeFromSuperlayer.
    void removeSublayer(LayerSkia*);

    // Re-create the canvas and graphics context. This method
    // must be called every time the layer is resized.
    void updateGraphicsContext(const SkIRect&);

    Vector<RefPtr<LayerSkia> > m_sublayers;
    LayerSkia* m_superlayer;

    GraphicsLayerSkia* m_owner;
    OwnPtr<skia::PlatformCanvas> m_canvas;
    OwnPtr<PlatformContextSkia> m_skiaContext;
    OwnPtr<GraphicsContext> m_graphicsContext;

    LayerType m_layerType;

    SkIRect m_bounds;
    SkIRect m_backingStoreRect;
    SkPoint m_position;
    SkPoint m_anchorPoint;
    Color m_backgroundColor;
    Color m_borderColor;

    SkRect m_frame;
    SkMatrix m_transform;
    SkMatrix m_sublayerTransform;

    uint32_t m_edgeAntialiasingMask;
    float m_opacity;
    float m_zPosition;
    float m_anchorPointZ;
    float m_borderWidth;

    bool m_clearsContext;
    bool m_doubleSided;
    bool m_hidden;
    bool m_masksToBounds;
    bool m_opaque;
    bool m_geometryFlipped;
    bool m_needsDisplayOnBoundsChange;

    ContentsGravityType m_contentsGravity;
    String m_name;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif
