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

#include "FloatQuad.h"
#include "IntRect.h"
#include "TextureCopier.h"
#include "TextureUploader.h"
#include "TrackingTextureAllocator.h"
#include "cc/CCLayerTreeHost.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CCCheckerboardDrawQuad;
class CCDebugBorderDrawQuad;
class CCDrawQuad;
class CCIOSurfaceDrawQuad;
class CCRenderPass;
class CCRenderSurfaceDrawQuad;
class CCSolidColorDrawQuad;
class CCTextureDrawQuad;
class CCTileDrawQuad;
class CCVideoDrawQuad;
class GeometryBinding;
class GraphicsContext3D;
class LayerRendererGpuMemoryAllocationChangedCallbackAdapter;
class LayerRendererSwapBuffersCompleteCallbackAdapter;
class ScopedEnsureFramebufferAllocation;

class LayerRendererChromiumClient {
public:
    virtual const IntSize& viewportSize() const = 0;
    virtual const CCSettings& settings() const = 0;
    virtual void didLoseContext() = 0;
    virtual void onSwapBuffersComplete() = 0;
    virtual void setFullRootLayerDamage() = 0;
    virtual void setContentsMemoryAllocationLimitBytes(size_t) = 0;
};

// Class that handles drawing of composited render layers using GL.
class LayerRendererChromium {
    WTF_MAKE_NONCOPYABLE(LayerRendererChromium);
public:
    static PassOwnPtr<LayerRendererChromium> create(LayerRendererChromiumClient*, PassRefPtr<GraphicsContext3D>);

    ~LayerRendererChromium();

    const CCSettings& settings() const { return m_client->settings(); }
    const LayerRendererCapabilities& capabilities() const { return m_capabilities; }

    GraphicsContext3D* context();
    bool contextSupportsMapSub() const { return m_capabilities.usingMapSub; }

    const IntSize& viewportSize() { return m_client->viewportSize(); }
    int viewportWidth() { return viewportSize().width(); }
    int viewportHeight() { return viewportSize().height(); }

    void viewportChanged();

    void beginDrawingFrame(CCRenderSurface* defaultRenderSurface);
    void drawRenderPass(const CCRenderPass*);
    void finishDrawingFrame();

    void drawHeadsUpDisplay(ManagedTexture*, const IntSize& hudSize);

    // waits for rendering to finish
    void finish();

    void doNoOp();
    // puts backbuffer onscreen
    bool swapBuffers(const IntRect& subBuffer);

    static void debugGLCall(GraphicsContext3D*, const char* command, const char* file, int line);

    const TransformationMatrix& projectionMatrix() const { return m_projectionMatrix; }
    const TransformationMatrix& windowMatrix() const { return m_windowMatrix; }

    const GeometryBinding* sharedGeometry() const { return m_sharedGeometry.get(); }
    const FloatQuad& sharedGeometryQuad() const { return m_sharedGeometryQuad; }


    void getFramebufferPixels(void *pixels, const IntRect&);
    bool getFramebufferTexture(ManagedTexture*, const IntRect& deviceRect);

    TextureManager* renderSurfaceTextureManager() const { return m_renderSurfaceTextureManager.get(); }
    TextureCopier* textureCopier() const { return m_textureCopier.get(); }
    TextureUploader* textureUploader() const { return m_textureUploader.get(); }
    TextureAllocator* renderSurfaceTextureAllocator() const { return m_renderSurfaceTextureAllocator.get(); }
    TextureAllocator* contentsTextureAllocator() const { return m_contentsTextureAllocator.get(); }

    void setScissorToRect(const IntRect&);

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
    void drawIOSurfaceQuad(const CCIOSurfaceDrawQuad*);
    void drawTileQuad(const CCTileDrawQuad*);
    void drawVideoQuad(const CCVideoDrawQuad*);

    void copyPlaneToTexture(const CCVideoDrawQuad*, const void* plane, int index);
    bool copyFrameToTextures(const CCVideoDrawQuad*);
    template<class Program> void drawSingleTextureVideoQuad(const CCVideoDrawQuad*, Program*, float widthScaleFactor, Platform3DObject textureId, GC3Denum target);
    void drawNativeTexture2D(const CCVideoDrawQuad*);
    void drawStreamTexture(const CCVideoDrawQuad*);
    void drawRGBA(const CCVideoDrawQuad*);
    void drawYUV(const CCVideoDrawQuad*);

    void setDrawViewportRect(const IntRect&, bool flipY);

    // The current drawing target is either a RenderSurface or ManagedTexture. Use these functions to switch to a new drawing target.
    bool useRenderSurface(CCRenderSurface*);
    bool useManagedTexture(ManagedTexture*, const IntRect& viewportRect);
    bool isCurrentRenderSurface(CCRenderSurface*);

    bool bindFramebufferToTexture(ManagedTexture*, const IntRect& viewportRect);

    void clearRenderSurface(CCRenderSurface*, CCRenderSurface* rootRenderSurface, const FloatRect& surfaceDamageRect);

    void releaseRenderSurfaceTextures();

    bool makeContextCurrent();

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

    OwnPtr<GeometryBinding> m_sharedGeometry;

    // This block of bindings defines all of the programs used by the compositor itself.

    // Tiled layer shaders.
    typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexAlpha> TileProgram;
    typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexClampAlphaAA> TileProgramAA;
    typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexClampSwizzleAlphaAA> TileProgramSwizzleAA;
    typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexOpaque> TileProgramOpaque;
    typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexSwizzleAlpha> TileProgramSwizzle;
    typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexSwizzleOpaque> TileProgramSwizzleOpaque;
    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderCheckerboard> TileCheckerboardProgram;

    // Render surface shaders.
    // CCRenderSurface::drawLayers() needs to see these programs currently.
    // FIXME: Draw with a quad type for render surfaces and get rid of this friendlyness.
    friend class CCRenderSurface;
    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderRGBATexAlpha> RenderSurfaceProgram;
    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderRGBATexAlphaMask> RenderSurfaceMaskProgram;
    typedef ProgramBinding<VertexShaderQuad, FragmentShaderRGBATexAlphaAA> RenderSurfaceProgramAA;
    typedef ProgramBinding<VertexShaderQuad, FragmentShaderRGBATexAlphaMaskAA> RenderSurfaceMaskProgramAA;

    // Texture shaders.
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexAlpha> TextureProgram;
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexFlipAlpha> TextureProgramFlip;
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexRectFlipAlpha> TextureIOSurfaceProgram;

    // Video shaders.
    typedef ProgramBinding<VertexShaderVideoTransform, FragmentShaderOESImageExternal> VideoStreamTextureProgram;
    typedef ProgramBinding<VertexShaderPosTexYUVStretch, FragmentShaderYUVVideo> VideoYUVProgram;

    // Special purpose / effects shaders.
    typedef ProgramBinding<VertexShaderPos, FragmentShaderColor> SolidColorProgram;

    // Debugging shaders.
    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderRGBATexSwizzleAlpha> HeadsUpDisplayProgram;


    const TileProgram* tileProgram();
    const TileProgramOpaque* tileProgramOpaque();
    const TileProgramAA* tileProgramAA();
    const TileProgramSwizzle* tileProgramSwizzle();
    const TileProgramSwizzleOpaque* tileProgramSwizzleOpaque();
    const TileProgramSwizzleAA* tileProgramSwizzleAA();
    const TileCheckerboardProgram* tileCheckerboardProgram();

    const RenderSurfaceProgram* renderSurfaceProgram();
    const RenderSurfaceProgramAA* renderSurfaceProgramAA();
    const RenderSurfaceMaskProgram* renderSurfaceMaskProgram();
    const RenderSurfaceMaskProgramAA* renderSurfaceMaskProgramAA();

    const TextureProgram* textureProgram();
    const TextureProgramFlip* textureProgramFlip();
    const TextureIOSurfaceProgram* textureIOSurfaceProgram();

    const VideoYUVProgram* videoYUVProgram();
    const VideoStreamTextureProgram* videoStreamTextureProgram();

    const SolidColorProgram* solidColorProgram();

    const HeadsUpDisplayProgram* headsUpDisplayProgram();

    OwnPtr<TileProgram> m_tileProgram;
    OwnPtr<TileProgramOpaque> m_tileProgramOpaque;
    OwnPtr<TileProgramAA> m_tileProgramAA;
    OwnPtr<TileProgramSwizzle> m_tileProgramSwizzle;
    OwnPtr<TileProgramSwizzleOpaque> m_tileProgramSwizzleOpaque;
    OwnPtr<TileProgramSwizzleAA> m_tileProgramSwizzleAA;
    OwnPtr<TileCheckerboardProgram> m_tileCheckerboardProgram;

    OwnPtr<RenderSurfaceProgram> m_renderSurfaceProgram;
    OwnPtr<RenderSurfaceProgramAA> m_renderSurfaceProgramAA;
    OwnPtr<RenderSurfaceMaskProgram> m_renderSurfaceMaskProgram;
    OwnPtr<RenderSurfaceMaskProgramAA> m_renderSurfaceMaskProgramAA;

    OwnPtr<TextureProgram> m_textureProgram;
    OwnPtr<TextureProgramFlip> m_textureProgramFlip;
    OwnPtr<TextureIOSurfaceProgram> m_textureIOSurfaceProgram;

    OwnPtr<VideoYUVProgram> m_videoYUVProgram;
    OwnPtr<VideoStreamTextureProgram> m_videoStreamTextureProgram;

    OwnPtr<SolidColorProgram> m_solidColorProgram;
    OwnPtr<HeadsUpDisplayProgram> m_headsUpDisplayProgram;

    OwnPtr<TextureManager> m_renderSurfaceTextureManager;
    OwnPtr<AcceleratedTextureCopier> m_textureCopier;
    OwnPtr<AcceleratedTextureUploader> m_textureUploader;
    OwnPtr<TrackingTextureAllocator> m_contentsTextureAllocator;
    OwnPtr<TrackingTextureAllocator> m_renderSurfaceTextureAllocator;

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
#define GLC(context, x) (x, LayerRendererChromium::debugGLCall(&*context, #x, __FILE__, __LINE__))
#else
#define GLC(context, x) (x)
#endif


}

#endif // USE(ACCELERATED_COMPOSITING)

#endif
