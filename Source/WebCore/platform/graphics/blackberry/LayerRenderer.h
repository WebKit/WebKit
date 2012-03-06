/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
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


#ifndef LayerRenderer_h
#define LayerRenderer_h

#if USE(ACCELERATED_COMPOSITING)

#include "IntRect.h"
#include "LayerData.h"
#include "TransformationMatrix.h"

#include <BlackBerryPlatformGLES2Context.h>
#include <BlackBerryPlatformIntRectRegion.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class LayerCompositingThread;
class LayerRendererSurface;

class LayerRenderingResults {
public:
    LayerRenderingResults() : wasEmpty(true), needsAnimationFrame(false) { }

    void addHolePunchRect(const IntRect&);
    IntRect holePunchRect(size_t index) const;
    size_t holePunchRectSize() { return m_holePunchRects.size(); }

    static const int NumberOfDirtyRects = 3;
    const IntRect& dirtyRect(int i) const { return m_dirtyRects[i]; }
    void addDirtyRect(const IntRect& dirtyRect);
    bool isEmpty() const;

    bool wasEmpty;

    BlackBerry::Platform::IntRectRegion dirtyRegion;

    bool needsAnimationFrame;

private:
    Vector<IntRect> m_holePunchRects; // Rects are in compositing surface coordinates.
    IntRect m_dirtyRects[NumberOfDirtyRects];
};

// Class that handles drawing of composited render layers using GL.
class LayerRenderer {
    WTF_MAKE_NONCOPYABLE(LayerRenderer);
public:
    static PassOwnPtr<LayerRenderer> create(BlackBerry::Platform::Graphics::GLES2Context*);

    LayerRenderer(BlackBerry::Platform::Graphics::GLES2Context*);
    ~LayerRenderer();

    void releaseLayerResources();

    // Recomposites all the layers. Returns true if it needs more draw.
    void drawLayers(const FloatRect& visibleRect, const IntRect& layoutRect, const IntSize& contentsSize, const IntRect& dstRect);

    void setRootLayer(LayerCompositingThread*);
    LayerCompositingThread* rootLayer() { return m_rootLayer.get(); }

    // Keep track of layers that need cleanup when the LayerRenderer is destroyed
    void addLayer(LayerCompositingThread*);
    bool removeLayer(LayerCompositingThread*);

    // Keep track of layers that need to release their textures when we swap buffers
    void addLayerToReleaseTextureResourcesList(LayerCompositingThread*);

    bool hardwareCompositing() const { return m_hardwareCompositing; }

    void setClearSurfaceOnDrawLayers(bool clear) { m_clearSurfaceOnDrawLayers = clear; }
    bool clearSurfaceOnDrawLayers() const { return m_clearSurfaceOnDrawLayers; }

    BlackBerry::Platform::Graphics::GLES2Context* context() const { return m_context; }

    const LayerRenderingResults& lastRenderingResults() const { return m_lastRenderingResults; }

    // Schedule a commit on the WebKit thread at the end of rendering
    // Used when a layer discovers during rendering that it needs a commit.
    void setNeedsCommit() { m_needsCommit = true; }

    IntRect toWebKitDocumentCoordinates(const FloatRect&) const;

    // If the layer has already been drawed on a surface.
    bool layerAlreadyOnSurface(LayerCompositingThread*) const;

private:
    void updateLayersRecursive(LayerCompositingThread*, const TransformationMatrix& parentMatrix, Vector<RefPtr<LayerCompositingThread> >& surfaceLayers, float opacity, FloatRect clipRect, double currentTime);
    void compositeLayersRecursive(LayerCompositingThread*, int stencilValue, FloatRect clipRect);
    void updateScissorIfNeeded(const FloatRect& clipRect);

    bool useSurface(LayerRendererSurface*);
    void drawLayersOnSurfaces(const Vector<RefPtr<LayerCompositingThread> >& surfaceLayers);

    void drawDebugBorder(LayerCompositingThread*);
    void drawHolePunchRect(LayerCompositingThread*);

    IntRect toOpenGLWindowCoordinates(const FloatRect&) const;
    IntRect toWebKitWindowCoordinates(const FloatRect&) const;

    void bindCommonAttribLocation(int location, const char* attribName);

    bool makeContextCurrent();

    bool initializeSharedGLObjects();

    // GL shader program object IDs.
    unsigned m_layerProgramObject[LayerData::NumberOfLayerProgramShaders];
    unsigned m_layerMaskProgramObject[LayerData::NumberOfLayerProgramShaders];
    unsigned m_colorProgramObject;
    unsigned m_checkerProgramObject;

    // Shader uniform and attribute locations.
    const int m_positionLocation;
    const int m_texCoordLocation;
    int m_samplerLocation[LayerData::NumberOfLayerProgramShaders];
    int m_alphaLocation[LayerData::NumberOfLayerProgramShaders];
    int m_maskSamplerLocation[LayerData::NumberOfLayerProgramShaders];
    int m_maskSamplerLocationMask[LayerData::NumberOfLayerProgramShaders];
    int m_maskAlphaLocation[LayerData::NumberOfLayerProgramShaders];

    int m_colorColorLocation;
    int m_checkerScaleLocation;
    int m_checkerOriginLocation;
    int m_checkerSurfaceHeightLocation;

    // Current draw configuration.
    FloatRect m_visibleRect;
    IntRect m_layoutRect;
    IntSize m_contentsSize;

    IntRect m_viewport;
    IntRect m_scissorRect;

    RefPtr<LayerCompositingThread> m_rootLayer;

    unsigned m_fbo;
    LayerRendererSurface* m_currentLayerRendererSurface;

    bool m_hardwareCompositing;
    bool m_clearSurfaceOnDrawLayers;

    // Map associating layers with textures ids used by the GL compositor.
    typedef HashSet<LayerCompositingThread*> LayerSet;
    LayerSet m_layers;
    LayerSet m_layersLockingTextureResources;

    BlackBerry::Platform::Graphics::GLES2Context* m_context;

    LayerRenderingResults m_lastRenderingResults;
    bool m_needsCommit;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif
