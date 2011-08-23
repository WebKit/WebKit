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

#include "ContentLayerChromium.h"
#include "IntRect.h"
#include "LayerChromium.h"
#include "VideoLayerChromium.h"
#include "cc/CCCanvasLayerImpl.h"
#include "cc/CCHeadsUpDisplay.h"
#include "cc/CCLayerSorter.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCPluginLayerImpl.h"
#include "cc/CCVideoLayerImpl.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

#if USE(CG)
#include <CoreGraphics/CGContext.h>
#include <wtf/RetainPtr.h>
#endif

#if USE(SKIA)
class GrContext;
#endif

namespace WebCore {

class CCHeadsUpDisplay;
class CCLayerImpl;
class CCLayerTreeHostCommitter;
class CCLayerTreeHostImpl;
class GeometryBinding;
class GraphicsContext3D;
class NonCompositedContentHost;

// Class that handles drawing of composited render layers using GL.
class LayerRendererChromium : public RefCounted<LayerRendererChromium> {
public:
    static PassRefPtr<LayerRendererChromium> create(CCLayerTreeHost*, PassRefPtr<GraphicsContext3D>);

    virtual ~LayerRendererChromium();

    const CCSettings& settings() const { return m_owner->settings(); }

    CCLayerTreeHost* owner() { return m_owner; }
    const CCLayerTreeHost* owner() const { return m_owner; }

    GraphicsLayer* rootLayer() { return m_owner->rootLayer(); }
    const GraphicsLayer* rootLayer() const { return m_owner->rootLayer(); }

    GraphicsContext3D* context();
    bool contextSupportsMapSub() const { return m_contextSupportsMapSub; }

#if USE(SKIA)
    GrContext* skiaContext() { return m_skiaContext.get(); }
#endif

    const IntSize& viewportSize() { return m_owner->viewportSize(); }
    int viewportWidth() { return viewportSize().width(); }
    int viewportHeight() { return viewportSize().height(); }

    void viewportChanged();

    // updates and draws the current layers onto the backbuffer
    void updateLayers();
    void drawLayers();

    // waits for rendering to finish
    void finish();

    // puts backbuffer onscreen
    void present();

    unsigned createLayerTexture();
    void deleteLayerTexture(unsigned);

    static void debugGLCall(GraphicsContext3D*, const char* command, const char* file, int line);

    const TransformationMatrix& projectionMatrix() const { return m_projectionMatrix; }
    const TransformationMatrix& windowMatrix() const { return m_windowMatrix; }

    bool checkTextureSize(const IntSize&);
    int maxTextureSize() const { return m_maxTextureSize; }

    const GeometryBinding* sharedGeometry() const { return m_sharedGeometry.get(); }
    const LayerChromium::BorderProgram* borderProgram();
    const CCHeadsUpDisplay::Program* headsUpDisplayProgram();
    const CCRenderSurface::Program* renderSurfaceProgram();
    const CCRenderSurface::MaskProgram* renderSurfaceMaskProgram();
    const CCTiledLayerImpl::Program* tilerProgram();
    const CCTiledLayerImpl::ProgramSwizzle* tilerProgramSwizzle();
    const CCTiledLayerImpl::ProgramAA* tilerProgramAA();
    const CCTiledLayerImpl::ProgramSwizzleAA* tilerProgramSwizzleAA();
    const CCCanvasLayerImpl::Program* canvasLayerProgram();
    const CCPluginLayerImpl::Program* pluginLayerProgram();
    const CCVideoLayerImpl::RGBAProgram* videoLayerRGBAProgram();
    const CCVideoLayerImpl::YUVProgram* videoLayerYUVProgram();

    void getFramebufferPixels(void *pixels, const IntRect& rect);

    TextureManager* contentsTextureManager() const { return m_contentsTextureManager.get(); }
    TextureManager* renderSurfaceTextureManager() const { return m_renderSurfaceTextureManager.get(); }

    CCHeadsUpDisplay* headsUpDisplay() { return m_headsUpDisplay.get(); }

    void setScissorToRect(const IntRect&);

    String layerTreeAsText() const;

    // Return true if the compositor context has an error.
    bool isCompositorContextLost();

    void releaseTextures();

    void setLayerRendererRecursive(LayerChromium*);

    GC3Denum bestTextureFormat();

    typedef Vector<RefPtr<LayerChromium> > LayerList;
    typedef Vector<RefPtr<CCLayerImpl> > CCLayerList;

private:
    // FIXME: This needs to be moved to the CCLayerTreeHostImpl when that class exists.
    RefPtr<CCLayerImpl> m_rootCCLayerImpl;

    LayerRendererChromium(CCLayerTreeHost*, PassRefPtr<GraphicsContext3D>);
    bool initialize();

    void updateLayers(LayerChromium*);
    void updateRootLayerContents();

    void paintLayerContents(const LayerList&);
    void updateCompositorResources(const LayerList& renderSurfaceLayerList);
    void updateCompositorResources(LayerChromium*);

    void drawLayersInternal();
    void drawLayer(CCLayerImpl*, CCRenderSurface*);

    ManagedTexture* getOffscreenLayerTexture();
    void copyOffscreenTextureToDisplay();

    bool isLayerVisible(LayerChromium*, const TransformationMatrix&, const IntRect& visibleRect);

    void setDrawViewportRect(const IntRect&, bool flipY);

    bool useRenderSurface(CCRenderSurface*);

    bool makeContextCurrent();

    static bool compareLayerZ(const RefPtr<CCLayerImpl>&, const RefPtr<CCLayerImpl>&);

    void dumpRenderSurfaces(TextStream&, int indent, const LayerChromium*) const;

    bool initializeSharedObjects();
    void cleanupSharedObjects();

    // FIXME: Change this to CCLayerTreeHostImpl
    CCLayerTreeHost* m_owner;

    TransformationMatrix m_projectionMatrix;
    TransformationMatrix m_windowMatrix;

    OwnPtr<LayerList> m_computedRenderSurfaceLayerList;

    CCRenderSurface* m_currentRenderSurface;
    unsigned m_offscreenFramebufferId;

    // Maximum texture dimensions supported.
    int m_maxTextureSize;

    // Store values that are shared between instances of each layer type
    // associated with this instance of the compositor. Since there can be
    // multiple instances of the compositor running in the same renderer process
    // we cannot store these values in static variables.
    OwnPtr<GeometryBinding> m_sharedGeometry;
    OwnPtr<LayerChromium::BorderProgram> m_borderProgram;
    OwnPtr<CCHeadsUpDisplay::Program> m_headsUpDisplayProgram;
    OwnPtr<CCTiledLayerImpl::Program> m_tilerProgram;
    OwnPtr<CCTiledLayerImpl::ProgramSwizzle> m_tilerProgramSwizzle;
    OwnPtr<CCTiledLayerImpl::ProgramAA> m_tilerProgramAA;
    OwnPtr<CCTiledLayerImpl::ProgramSwizzleAA> m_tilerProgramSwizzleAA;
    OwnPtr<CCCanvasLayerImpl::Program> m_canvasLayerProgram;
    OwnPtr<CCPluginLayerImpl::Program> m_pluginLayerProgram;
    OwnPtr<CCRenderSurface::MaskProgram> m_renderSurfaceMaskProgram;
    OwnPtr<CCRenderSurface::Program> m_renderSurfaceProgram;
    OwnPtr<CCVideoLayerImpl::RGBAProgram> m_videoLayerRGBAProgram;
    OwnPtr<CCVideoLayerImpl::YUVProgram> m_videoLayerYUVProgram;

    OwnPtr<TextureManager> m_contentsTextureManager;
    OwnPtr<TextureManager> m_renderSurfaceTextureManager;

    OwnPtr<CCHeadsUpDisplay> m_headsUpDisplay;

    RefPtr<GraphicsContext3D> m_context;
#if USE(SKIA)
    OwnPtr<GrContext> m_skiaContext;
#endif

    bool m_contextSupportsMapSub;

    CCRenderSurface* m_defaultRenderSurface;

    CCLayerSorter m_layerSorter;
};

// Setting DEBUG_GL_CALLS to 1 will call glGetError() after almost every GL
// call made by the compositor. Useful for debugging rendering issues but
// will significantly degrade performance.
#define DEBUG_GL_CALLS 0

#if DEBUG_GL_CALLS && !defined ( NDEBUG )
#define GLC(context, x) { (x), LayerRendererChromium::debugGLCall(context, #x, __FILE__, __LINE__); }
#else
#define GLC(context, x) (x)
#endif


}

#endif // USE(ACCELERATED_COMPOSITING)

#endif
