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


#ifndef LayerRendererChromium_h
#define LayerRendererChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "IntRect.h"
#include "LayerChromium.h"
#include "SkBitmap.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class GLES2Context;
class Page;

// Class that handles drawing of composited render layers using GL.
class LayerRendererChromium : public Noncopyable {
public:
    static PassOwnPtr<LayerRendererChromium> create(Page* page);

    LayerRendererChromium(Page* page);
    ~LayerRendererChromium();

    // Updates the contents of the root layer that fall inside the updateRect and recomposites
    // all the layers.
    void drawLayers(const IntRect& updateRect, const IntRect& visibleRect, const IntRect& contentRect, const IntPoint& scrollPosition);

    void setRootLayer(PassRefPtr<LayerChromium> layer) { m_rootLayer = layer; }
    LayerChromium* rootLayer() { return m_rootLayer.get(); }

    void setNeedsDisplay() { m_needsDisplay = true; }

    // Frees the texture associated with the given layer.
    bool freeLayerTexture(LayerChromium*);

    bool hardwareCompositing() const { return m_hardwareCompositing; }

    void setRootLayerCanvasSize(const IntSize&);

    GraphicsContext* rootLayerGraphicsContext() const { return m_rootLayerGraphicsContext.get(); }

private:
    void compositeLayersRecursive(LayerChromium*, const TransformationMatrix&, float opacity, const IntRect& visibleRect);

    void drawDebugBorder(LayerChromium*, const TransformationMatrix&);

    void drawTexturedQuad(const TransformationMatrix& matrix, float width, float height, float opacity, bool scrolling);

    bool isLayerVisible(LayerChromium*, const TransformationMatrix&, const IntRect& visibleRect);

    void bindCommonAttribLocation(int location, char* attribName);

    enum VboIds { Vertices, LayerElements };

    // These are here only temporarily and should be removed once we switch over to GGL
    bool initGL();
    bool makeContextCurrent();

    bool initializeSharedGLObjects();
    int getTextureId(LayerChromium*);
    int assignTextureForLayer(LayerChromium*);

    // GL shader program object IDs.
    unsigned int m_layerProgramObject;
    unsigned int m_borderProgramObject;
    unsigned int m_scrollProgramObject;

    unsigned int m_rootLayerTextureId;
    int m_rootLayerTextureWidth;
    int m_rootLayerTextureHeight;

    // Shader uniform and attribute locations.
    const int m_positionLocation;
    const int m_texCoordLocation;
    int m_samplerLocation;
    int m_matrixLocation;
    int m_alphaLocation;
    int m_scrollMatrixLocation;
    int m_scrollSamplerLocation;

    int m_borderMatrixLocation;
    int m_borderColorLocation;

    unsigned int m_quadVboIds[3];
    TransformationMatrix m_projectionMatrix;

    RefPtr<LayerChromium> m_rootLayer;

    bool m_needsDisplay;
    IntPoint m_scrollPosition;
    bool m_hardwareCompositing;

    // Map associating layers with textures ids used by the GL compositor.
    typedef HashMap<LayerChromium*, unsigned int> TextureIdMap;
    TextureIdMap m_textureIdMap;

#if PLATFORM(SKIA)
    OwnPtr<skia::PlatformCanvas> m_rootLayerCanvas;
    OwnPtr<PlatformContextSkia> m_rootLayerSkiaContext;
    OwnPtr<GraphicsContext> m_rootLayerGraphicsContext;
#endif
    IntSize m_rootLayerCanvasSize;

    OwnPtr<GLES2Context> m_gles2Context;

    // The WebCore Page that the compositor renders into.
    Page* m_page;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif
