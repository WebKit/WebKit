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
#include "FloatQuad.h"
#include "IntRect.h"
#include "LayerChromium.h"
#include "TrackingTextureAllocator.h"
#include "VideoLayerChromium.h"
#include "cc/CCCanvasLayerImpl.h"
#include "cc/CCHeadsUpDisplay.h"
#include "cc/CCLayerTreeHostImpl.h"
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

namespace WebCore {

class CCHeadsUpDisplay;
class CCLayerImpl;
class CCLayerTreeHostImpl;
class CCRenderPass;
class GeometryBinding;
class GraphicsContext3D;
class TrackingTextureAllocator;
class LayerRendererSwapBuffersCompleteCallbackAdapter;

// Class that handles drawing of composited render layers using GL.
class LayerRendererChromium {
    WTF_MAKE_NONCOPYABLE(LayerRendererChromium);
public:
    static PassOwnPtr<LayerRendererChromium> create(CCLayerTreeHostImpl*, PassRefPtr<GraphicsContext3D>);

    // Must be called in order to allow the LayerRendererChromium to destruct
    void close();

    ~LayerRendererChromium();

    const CCSettings& settings() const { return m_owner->settings(); }
    const LayerRendererCapabilities& capabilities() const { return m_capabilities; }

    CCLayerImpl* rootLayer() { return m_owner->rootLayer(); }
    const CCLayerImpl* rootLayer() const { return m_owner->rootLayer(); }

    GraphicsContext3D* context();
    bool contextSupportsMapSub() const { return m_capabilities.usingMapSub; }

    const IntSize& viewportSize() { return m_owner->viewportSize(); }
    int viewportWidth() { return viewportSize().width(); }
    int viewportHeight() { return viewportSize().height(); }

    void viewportChanged();

    void beginDrawingFrame();
    void drawRenderPass(const CCRenderPass*);
    void finishDrawingFrame();

    // waits for rendering to finish
    void finish();

    // puts backbuffer onscreen
    void swapBuffers(const IntRect& subBuffer);

    static void debugGLCall(GraphicsContext3D*, const char* command, const char* file, int line);

    const TransformationMatrix& projectionMatrix() const { return m_projectionMatrix; }
    const TransformationMatrix& windowMatrix() const { return m_windowMatrix; }

    const GeometryBinding* sharedGeometry() const { return m_sharedGeometry.get(); }
    const FloatQuad& sharedGeometryQuad() const { return m_sharedGeometryQuad; }
    const LayerChromium::BorderProgram* borderProgram();
    const CCHeadsUpDisplay::Program* headsUpDisplayProgram();
    const CCRenderSurface::Program* renderSurfaceProgram();
    const CCRenderSurface::ProgramAA* renderSurfaceProgramAA();
    const CCRenderSurface::MaskProgram* renderSurfaceMaskProgram();
    const CCRenderSurface::MaskProgramAA* renderSurfaceMaskProgramAA();
    const CCTiledLayerImpl::Program* tilerProgram();
    const CCTiledLayerImpl::ProgramOpaque* tilerProgramOpaque();
    const CCTiledLayerImpl::ProgramAA* tilerProgramAA();
    const CCTiledLayerImpl::ProgramSwizzle* tilerProgramSwizzle();
    const CCTiledLayerImpl::ProgramSwizzleOpaque* tilerProgramSwizzleOpaque();
    const CCTiledLayerImpl::ProgramSwizzleAA* tilerProgramSwizzleAA();
    const CCCanvasLayerImpl::Program* canvasLayerProgram();
    const CCPluginLayerImpl::Program* pluginLayerProgram();
    const CCPluginLayerImpl::ProgramFlip* pluginLayerProgramFlip();
    const CCPluginLayerImpl::TexRectProgram* pluginLayerTexRectProgram();
    const CCPluginLayerImpl::TexRectProgramFlip* pluginLayerTexRectProgramFlip();
    const CCVideoLayerImpl::RGBAProgram* videoLayerRGBAProgram();
    const CCVideoLayerImpl::YUVProgram* videoLayerYUVProgram();
    const CCVideoLayerImpl::NativeTextureProgram* videoLayerNativeTextureProgram();

    void getFramebufferPixels(void *pixels, const IntRect&);

    TextureManager* renderSurfaceTextureManager() const { return m_renderSurfaceTextureManager.get(); }
    TextureAllocator* renderSurfaceTextureAllocator() const { return m_renderSurfaceTextureAllocator.get(); }
    TextureAllocator* contentsTextureAllocator() const { return m_contentsTextureAllocator.get(); }

    CCHeadsUpDisplay* headsUpDisplay() { return m_headsUpDisplay.get(); }

    void setScissorToRect(const IntRect&);

    String layerTreeAsText() const;

    bool isContextLost();

    void setVisible(bool);

    GC3Denum bestTextureFormat();

    static void toGLMatrix(float*, const TransformationMatrix&);
    void drawTexturedQuad(const TransformationMatrix& layerMatrix,
                          float width, float height, float opacity, const FloatQuad&,
                          int matrixLocation, int alphaLocation, int quadLocation);

private:
    LayerRendererChromium(CCLayerTreeHostImpl*, PassRefPtr<GraphicsContext3D>);
    bool initialize();

    void drawQuad(const CCDrawQuad*, const FloatRect& surfaceDamageRect);
    void drawDebugBorderQuad(const CCDebugBorderDrawQuad*);
    void drawRenderSurfaceQuad(const CCRenderSurfaceDrawQuad*);
    void drawSolidColorQuad(const CCSolidColorDrawQuad*);
    void drawTileQuad(const CCTileDrawQuad*);
    void drawCanvasQuad(const CCCanvasDrawQuad*);
    void drawVideoQuad(const CCVideoDrawQuad*);
    void drawPluginQuad(const CCPluginDrawQuad*);

    ManagedTexture* getOffscreenLayerTexture();
    void copyOffscreenTextureToDisplay();

    void setDrawViewportRect(const IntRect&, bool flipY);

    bool useRenderSurface(CCRenderSurface*);
    void clearSurfaceForDebug(CCRenderSurface*, CCRenderSurface* rootRenderSurface, const FloatRect& surfaceDamageRect);

    void releaseRenderSurfaceTextures();

    bool makeContextCurrent();

    static bool compareLayerZ(const RefPtr<CCLayerImpl>&, const RefPtr<CCLayerImpl>&);

    void dumpRenderSurfaces(TextStream&, int indent, const CCLayerImpl*) const;

    bool initializeSharedObjects();
    void cleanupSharedObjects();

    void clearRenderSurfacesOnCCLayerImplRecursive(CCLayerImpl*);

    friend class LayerRendererSwapBuffersCompleteCallbackAdapter;
    void onSwapBuffersComplete();

    CCLayerTreeHostImpl* m_owner;

    LayerRendererCapabilities m_capabilities;

    TransformationMatrix m_projectionMatrix;
    TransformationMatrix m_windowMatrix;

    CCRenderSurface* m_currentRenderSurface;
    unsigned m_offscreenFramebufferId;

    // Store values that are shared between instances of each layer type
    // associated with this instance of the compositor. Since there can be
    // multiple instances of the compositor running in the same renderer process
    // we cannot store these values in static variables.
    OwnPtr<GeometryBinding> m_sharedGeometry;
    OwnPtr<LayerChromium::BorderProgram> m_borderProgram;
    OwnPtr<CCHeadsUpDisplay::Program> m_headsUpDisplayProgram;
    OwnPtr<CCTiledLayerImpl::Program> m_tilerProgram;
    OwnPtr<CCTiledLayerImpl::ProgramOpaque> m_tilerProgramOpaque;
    OwnPtr<CCTiledLayerImpl::ProgramSwizzle> m_tilerProgramSwizzle;
    OwnPtr<CCTiledLayerImpl::ProgramSwizzleOpaque> m_tilerProgramSwizzleOpaque;
    OwnPtr<CCTiledLayerImpl::ProgramAA> m_tilerProgramAA;
    OwnPtr<CCTiledLayerImpl::ProgramSwizzleAA> m_tilerProgramSwizzleAA;
    OwnPtr<CCCanvasLayerImpl::Program> m_canvasLayerProgram;
    OwnPtr<CCPluginLayerImpl::Program> m_pluginLayerProgram;
    OwnPtr<CCPluginLayerImpl::ProgramFlip> m_pluginLayerProgramFlip;
    OwnPtr<CCPluginLayerImpl::TexRectProgram> m_pluginLayerTexRectProgram;
    OwnPtr<CCPluginLayerImpl::TexRectProgramFlip> m_pluginLayerTexRectProgramFlip;
    OwnPtr<CCRenderSurface::MaskProgram> m_renderSurfaceMaskProgram;
    OwnPtr<CCRenderSurface::Program> m_renderSurfaceProgram;
    OwnPtr<CCRenderSurface::MaskProgramAA> m_renderSurfaceMaskProgramAA;
    OwnPtr<CCRenderSurface::ProgramAA> m_renderSurfaceProgramAA;
    OwnPtr<CCVideoLayerImpl::RGBAProgram> m_videoLayerRGBAProgram;
    OwnPtr<CCVideoLayerImpl::YUVProgram> m_videoLayerYUVProgram;
    OwnPtr<CCVideoLayerImpl::NativeTextureProgram> m_videoLayerNativeTextureProgram;

    OwnPtr<TextureManager> m_renderSurfaceTextureManager;
    OwnPtr<TrackingTextureAllocator> m_contentsTextureAllocator;
    OwnPtr<TrackingTextureAllocator> m_renderSurfaceTextureAllocator;

    OwnPtr<CCHeadsUpDisplay> m_headsUpDisplay;

    RefPtr<GraphicsContext3D> m_context;

    CCRenderSurface* m_defaultRenderSurface;

    FloatQuad m_sharedGeometryQuad;

    bool m_isViewportChanged;
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
