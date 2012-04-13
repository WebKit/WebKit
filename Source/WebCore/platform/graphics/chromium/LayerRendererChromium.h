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
#include "TextureCopier.h"
#include "TrackingTextureAllocator.h"
#include "VideoLayerChromium.h"
#include "cc/CCDrawQuad.h"
#include "cc/CCHeadsUpDisplay.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCTextureLayerImpl.h"
#include "cc/CCVideoLayerImpl.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CCHeadsUpDisplay;
class CCLayerImpl;
class CCRenderPass;
class CCTextureDrawQuad;
class GeometryBinding;
class GraphicsContext3D;
class LayerRendererSwapBuffersCompleteCallbackAdapter;
class LayerRendererGpuMemoryAllocationChangedCallbackAdapter;
class ScopedEnsureFramebufferAllocation;

class LayerRendererChromiumClient {
public:
    virtual const IntSize& viewportSize() const = 0;
    virtual const CCSettings& settings() const = 0;
    virtual CCLayerImpl* rootLayer() = 0;
    virtual const CCLayerImpl* rootLayer() const = 0;
    virtual void didLoseContext() = 0;
    virtual void onSwapBuffersComplete() = 0;
    virtual void setFullRootLayerDamage() = 0;
};

// Class that handles drawing of composited render layers using GL.
class LayerRendererChromium {
    WTF_MAKE_NONCOPYABLE(LayerRendererChromium);
public:
    static PassOwnPtr<LayerRendererChromium> create(LayerRendererChromiumClient*, PassRefPtr<GraphicsContext3D>);

    ~LayerRendererChromium();

    const CCSettings& settings() const { return m_client->settings(); }
    const LayerRendererCapabilities& capabilities() const { return m_capabilities; }

    CCLayerImpl* rootLayer() { return m_client->rootLayer(); }
    const CCLayerImpl* rootLayer() const { return m_client->rootLayer(); }

    GraphicsContext3D* context();
    bool contextSupportsMapSub() const { return m_capabilities.usingMapSub; }

    const IntSize& viewportSize() { return m_client->viewportSize(); }
    int viewportWidth() { return viewportSize().width(); }
    int viewportHeight() { return viewportSize().height(); }

    void viewportChanged();

    void beginDrawingFrame();
    void drawRenderPass(const CCRenderPass*);
    void finishDrawingFrame();

    // waits for rendering to finish
    void finish();

    // puts backbuffer onscreen
    bool swapBuffers(const IntRect& subBuffer);

    static void debugGLCall(GraphicsContext3D*, const char* command, const char* file, int line);

    const TransformationMatrix& projectionMatrix() const { return m_projectionMatrix; }
    const TransformationMatrix& windowMatrix() const { return m_windowMatrix; }

    const GeometryBinding* sharedGeometry() const { return m_sharedGeometry.get(); }
    const FloatQuad& sharedGeometryQuad() const { return m_sharedGeometryQuad; }

    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderCheckerboard> CheckerboardProgram;

    const CheckerboardProgram* checkerboardProgram();
    const LayerChromium::BorderProgram* borderProgram();
    const CCHeadsUpDisplay::Program* headsUpDisplayProgram();
    const CCRenderSurface::Program* renderSurfaceProgram();
    const CCRenderSurface::ProgramAA* renderSurfaceProgramAA();
    const CCRenderSurface::MaskProgram* renderSurfaceMaskProgram();
    const CCRenderSurface::MaskProgramAA* renderSurfaceMaskProgramAA();
    const CCTextureLayerImpl::ProgramFlip* textureLayerProgramFlip();
    const CCTextureLayerImpl::ProgramStretch* textureLayerProgramStretch();
    const CCTextureLayerImpl::ProgramStretchFlip* textureLayerProgramStretchFlip();
    const CCTextureLayerImpl::TexRectProgram* textureLayerTexRectProgram();
    const CCTextureLayerImpl::TexRectProgramFlip* textureLayerTexRectProgramFlip();
    const CCTiledLayerImpl::Program* tilerProgram();
    const CCTiledLayerImpl::ProgramOpaque* tilerProgramOpaque();
    const CCTiledLayerImpl::ProgramAA* tilerProgramAA();
    const CCTiledLayerImpl::ProgramSwizzle* tilerProgramSwizzle();
    const CCTiledLayerImpl::ProgramSwizzleOpaque* tilerProgramSwizzleOpaque();
    const CCTiledLayerImpl::ProgramSwizzleAA* tilerProgramSwizzleAA();
    const CCVideoLayerImpl::RGBAProgram* videoLayerRGBAProgram();
    const CCVideoLayerImpl::YUVProgram* videoLayerYUVProgram();
    const CCVideoLayerImpl::NativeTextureProgram* videoLayerNativeTextureProgram();
    const CCVideoLayerImpl::StreamTextureProgram* streamTextureLayerProgram();

    void getFramebufferPixels(void *pixels, const IntRect&);
    bool getFramebufferTexture(ManagedTexture*, const IntRect& deviceRect);

    TextureManager* renderSurfaceTextureManager() const { return m_renderSurfaceTextureManager.get(); }
    TextureCopier* textureCopier() const { return m_textureCopier.get(); }
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

protected:
    friend class LayerRendererGpuMemoryAllocationChangedCallbackAdapter;
    void discardFramebuffer();
    void ensureFramebuffer();
    bool isFramebufferDiscarded() const { return m_isFramebufferDiscarded; }

    LayerRendererChromium(LayerRendererChromiumClient*, PassRefPtr<GraphicsContext3D>);
    bool initialize();

private:
    void drawQuad(const CCDrawQuad*, const FloatRect& surfaceDamageRect);
    void drawCheckerboardQuad(const CCCheckerboardDrawQuad*);
    void drawDebugBorderQuad(const CCDebugBorderDrawQuad*);
    void drawBackgroundFilters(const CCRenderSurfaceDrawQuad*);
    void drawRenderSurfaceQuad(const CCRenderSurfaceDrawQuad*);
    void drawSolidColorQuad(const CCSolidColorDrawQuad*);
    void drawTextureQuad(const CCTextureDrawQuad*);
    void drawTileQuad(const CCTileDrawQuad*);
    void drawVideoQuad(const CCVideoDrawQuad*);

    ManagedTexture* getOffscreenLayerTexture();
    void copyPlaneToTexture(const CCVideoDrawQuad*, const void* plane, int index);
    bool copyFrameToTextures(const CCVideoDrawQuad*);
    template<class Program> void drawSingleTextureVideoQuad(const CCVideoDrawQuad*, Program*, float widthScaleFactor, Platform3DObject textureId, GC3Denum target);
    void drawNativeTexture2D(const CCVideoDrawQuad*);
    void drawStreamTexture(const CCVideoDrawQuad*);
    void drawRGBA(const CCVideoDrawQuad*);
    void drawYUV(const CCVideoDrawQuad*);

    void copyOffscreenTextureToDisplay();

    void setDrawViewportRect(const IntRect&, bool flipY);

    // The current drawing target is either a RenderSurface or ManagedTexture. Use these functions to switch to a new drawing target.
    bool useRenderSurface(CCRenderSurface*);
    bool useManagedTexture(ManagedTexture*, const IntRect& viewportRect);
    bool isCurrentRenderSurface(CCRenderSurface*);

    bool bindFramebufferToTexture(ManagedTexture*, const IntRect& viewportRect);

    void clearRenderSurface(CCRenderSurface*, CCRenderSurface* rootRenderSurface, const FloatRect& surfaceDamageRect);

    void releaseRenderSurfaceTextures();

    bool makeContextCurrent();

    static bool compareLayerZ(const RefPtr<CCLayerImpl>&, const RefPtr<CCLayerImpl>&);

    void dumpRenderSurfaces(TextStream&, int indent, const CCLayerImpl*) const;

    bool initializeSharedObjects();
    void cleanupSharedObjects();

    friend class LayerRendererSwapBuffersCompleteCallbackAdapter;
    void onSwapBuffersComplete();

    LayerRendererChromiumClient* m_client;

    LayerRendererCapabilities m_capabilities;

    TransformationMatrix m_projectionMatrix;
    TransformationMatrix m_windowMatrix;

    CCRenderSurface* m_currentRenderSurface;
    ManagedTexture* m_currentManagedTexture;
    unsigned m_offscreenFramebufferId;

    // Store values that are shared between instances of each layer type
    // associated with this instance of the compositor. Since there can be
    // multiple instances of the compositor running in the same renderer process
    // we cannot store these values in static variables.
    OwnPtr<GeometryBinding> m_sharedGeometry;
    OwnPtr<CheckerboardProgram> m_checkerboardProgram;
    OwnPtr<LayerChromium::BorderProgram> m_borderProgram;
    OwnPtr<CCHeadsUpDisplay::Program> m_headsUpDisplayProgram;
    OwnPtr<CCTextureLayerImpl::ProgramFlip> m_textureLayerProgramFlip;
    OwnPtr<CCTextureLayerImpl::ProgramStretch> m_textureLayerProgramStretch;
    OwnPtr<CCTextureLayerImpl::ProgramStretchFlip> m_textureLayerProgramStretchFlip;
    OwnPtr<CCTextureLayerImpl::TexRectProgram> m_textureLayerTexRectProgram;
    OwnPtr<CCTextureLayerImpl::TexRectProgramFlip> m_textureLayerTexRectProgramFlip;
    OwnPtr<CCTiledLayerImpl::Program> m_tilerProgram;
    OwnPtr<CCTiledLayerImpl::ProgramOpaque> m_tilerProgramOpaque;
    OwnPtr<CCTiledLayerImpl::ProgramSwizzle> m_tilerProgramSwizzle;
    OwnPtr<CCTiledLayerImpl::ProgramSwizzleOpaque> m_tilerProgramSwizzleOpaque;
    OwnPtr<CCTiledLayerImpl::ProgramAA> m_tilerProgramAA;
    OwnPtr<CCTiledLayerImpl::ProgramSwizzleAA> m_tilerProgramSwizzleAA;
    OwnPtr<CCRenderSurface::MaskProgram> m_renderSurfaceMaskProgram;
    OwnPtr<CCRenderSurface::Program> m_renderSurfaceProgram;
    OwnPtr<CCRenderSurface::MaskProgramAA> m_renderSurfaceMaskProgramAA;
    OwnPtr<CCRenderSurface::ProgramAA> m_renderSurfaceProgramAA;
    OwnPtr<CCVideoLayerImpl::RGBAProgram> m_videoLayerRGBAProgram;
    OwnPtr<CCVideoLayerImpl::YUVProgram> m_videoLayerYUVProgram;
    OwnPtr<CCVideoLayerImpl::NativeTextureProgram> m_videoLayerNativeTextureProgram;
    OwnPtr<CCVideoLayerImpl::StreamTextureProgram> m_streamTextureLayerProgram;

    OwnPtr<TextureManager> m_renderSurfaceTextureManager;
    OwnPtr<AcceleratedTextureCopier> m_textureCopier;
    OwnPtr<TrackingTextureAllocator> m_contentsTextureAllocator;
    OwnPtr<TrackingTextureAllocator> m_renderSurfaceTextureAllocator;

    OwnPtr<CCHeadsUpDisplay> m_headsUpDisplay;

    RefPtr<GraphicsContext3D> m_context;

    CCRenderSurface* m_defaultRenderSurface;

    FloatQuad m_sharedGeometryQuad;

    bool m_isViewportChanged;
    bool m_isFramebufferDiscarded;
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
