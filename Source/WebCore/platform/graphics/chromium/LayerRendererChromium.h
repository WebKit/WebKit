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

#include "CCCheckerboardDrawQuad.h"
#include "CCDebugBorderDrawQuad.h"
#include "CCDirectRenderer.h"
#include "CCIOSurfaceDrawQuad.h"
#include "CCRenderPassDrawQuad.h"
#include "CCRenderer.h"
#include "CCSolidColorDrawQuad.h"
#include "CCStreamVideoDrawQuad.h"
#include "CCTextureDrawQuad.h"
#include "CCTileDrawQuad.h"
#include "CCYUVVideoDrawQuad.h"
#include "Extensions3DChromium.h"
#include "TextureCopier.h"
#include <wtf/PassOwnPtr.h>

namespace WebKit {
class WebGraphicsContext3D;
}

namespace WebCore {

class CCScopedTexture;
class GeometryBinding;
class ScopedEnsureFramebufferAllocation;

// Class that handles drawing of composited render layers using GL.
class LayerRendererChromium : public CCDirectRenderer,
                              public WebKit::WebGraphicsContext3D::WebGraphicsSwapBuffersCompleteCallbackCHROMIUM,
                              public WebKit::WebGraphicsContext3D::WebGraphicsMemoryAllocationChangedCallbackCHROMIUM ,
                              public WebKit::WebGraphicsContext3D::WebGraphicsContextLostCallback {
    WTF_MAKE_NONCOPYABLE(LayerRendererChromium);
public:
    static PassOwnPtr<LayerRendererChromium> create(CCRendererClient*, CCResourceProvider*, TextureUploaderOption);

    virtual ~LayerRendererChromium();

    virtual const LayerRendererCapabilities& capabilities() const OVERRIDE { return m_capabilities; }

    WebKit::WebGraphicsContext3D* context();

    virtual void viewportChanged() OVERRIDE;

    const FloatQuad& sharedGeometryQuad() const { return m_sharedGeometryQuad; }

    // waits for rendering to finish
    virtual void finish() OVERRIDE;

    virtual void doNoOp() OVERRIDE;
    // puts backbuffer onscreen
    virtual bool swapBuffers() OVERRIDE;

    static void debugGLCall(WebKit::WebGraphicsContext3D*, const char* command, const char* file, int line);

    const GeometryBinding* sharedGeometry() const { return m_sharedGeometry.get(); }

    virtual void getFramebufferPixels(void *pixels, const IntRect&) OVERRIDE;
    bool getFramebufferTexture(CCScopedTexture*, const IntRect& deviceRect);

    virtual TextureCopier* textureCopier() const OVERRIDE { return m_textureCopier.get(); }
    virtual TextureUploader* textureUploader() const OVERRIDE { return m_textureUploader.get(); }

    virtual bool isContextLost() OVERRIDE;

    virtual void setVisible(bool) OVERRIDE;

protected:
    LayerRendererChromium(CCRendererClient*, CCResourceProvider*, TextureUploaderOption);

    bool isFramebufferDiscarded() const { return m_isFramebufferDiscarded; }
    bool initialize();

    void releaseRenderPassTextures();

    virtual void bindFramebufferToOutputSurface(DrawingFrame&) OVERRIDE;
    virtual bool bindFramebufferToTexture(DrawingFrame&, const CCScopedTexture*, const IntRect& framebufferRect) OVERRIDE;
    virtual void setDrawViewportSize(const IntSize&) OVERRIDE;
    virtual void enableScissorTestRect(const IntRect& scissorRect) OVERRIDE;
    virtual void disableScissorTest() OVERRIDE;
    virtual void clearFramebuffer(DrawingFrame&) OVERRIDE;
    virtual void drawQuad(DrawingFrame&, const CCDrawQuad*) OVERRIDE;
    virtual void beginDrawingFrame(DrawingFrame&) OVERRIDE;
    virtual void finishDrawingFrame(DrawingFrame&) OVERRIDE;

private:
    static void toGLMatrix(float*, const WebKit::WebTransformationMatrix&);

    void drawCheckerboardQuad(const DrawingFrame&, const CCCheckerboardDrawQuad*);
    void drawDebugBorderQuad(const DrawingFrame&, const CCDebugBorderDrawQuad*);
    PassOwnPtr<CCScopedTexture> drawBackgroundFilters(DrawingFrame&, const CCRenderPassDrawQuad*, const WebKit::WebFilterOperations&, const WebKit::WebTransformationMatrix& deviceTransform);
    void drawRenderPassQuad(DrawingFrame&, const CCRenderPassDrawQuad*);
    void drawSolidColorQuad(const DrawingFrame&, const CCSolidColorDrawQuad*);
    void drawStreamVideoQuad(const DrawingFrame&, const CCStreamVideoDrawQuad*);
    void drawTextureQuad(const DrawingFrame&, const CCTextureDrawQuad*);
    void drawIOSurfaceQuad(const DrawingFrame&, const CCIOSurfaceDrawQuad*);
    void drawTileQuad(const DrawingFrame&, const CCTileDrawQuad*);
    void drawYUVVideoQuad(const DrawingFrame&, const CCYUVVideoDrawQuad*);

    void setShaderOpacity(float opacity, int alphaLocation);
    void setShaderFloatQuad(const FloatQuad&, int quadLocation);
    void drawQuadGeometry(const DrawingFrame&, const WebKit::WebTransformationMatrix& drawTransform, const FloatRect& quadRect, int matrixLocation);

    void copyTextureToFramebuffer(const DrawingFrame&, int textureId, const IntRect&, const WebKit::WebTransformationMatrix& drawMatrix);

    bool useScopedTexture(DrawingFrame&, const CCScopedTexture*, const IntRect& viewportRect);

    bool makeContextCurrent();

    bool initializeSharedObjects();
    void cleanupSharedObjects();

    // WebKit::WebGraphicsContext3D::WebGraphicsSwapBuffersCompleteCallbackCHROMIUM implementation.
    virtual void onSwapBuffersComplete() OVERRIDE;

    // WebKit::WebGraphicsContext3D::WebGraphicsMemoryAllocationChangedCallbackCHROMIUM implementation.
    virtual void onMemoryAllocationChanged(WebKit::WebGraphicsMemoryAllocation) OVERRIDE;
    void onMemoryAllocationChangedOnImplThread(WebKit::WebGraphicsMemoryAllocation);
    void discardFramebuffer();
    void ensureFramebuffer();

    // WebGraphicsContext3D::WebGraphicsContextLostCallback implementation.
    virtual void onContextLost() OVERRIDE;

    LayerRendererCapabilities m_capabilities;

    unsigned m_offscreenFramebufferId;

    OwnPtr<GeometryBinding> m_sharedGeometry;
    FloatQuad m_sharedGeometryQuad;

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
    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderRGBATexAlpha> RenderPassProgram;
    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderRGBATexAlphaMask> RenderPassMaskProgram;
    typedef ProgramBinding<VertexShaderQuad, FragmentShaderRGBATexAlphaAA> RenderPassProgramAA;
    typedef ProgramBinding<VertexShaderQuad, FragmentShaderRGBATexAlphaMaskAA> RenderPassMaskProgramAA;

    // Texture shaders.
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexAlpha> TextureProgram;
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexFlipAlpha> TextureProgramFlip;
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexRectAlpha> TextureIOSurfaceProgram;

    // Video shaders.
    typedef ProgramBinding<VertexShaderVideoTransform, FragmentShaderOESImageExternal> VideoStreamTextureProgram;
    typedef ProgramBinding<VertexShaderPosTexYUVStretch, FragmentShaderYUVVideo> VideoYUVProgram;

    // Special purpose / effects shaders.
    typedef ProgramBinding<VertexShaderPos, FragmentShaderColor> SolidColorProgram;

    const TileProgram* tileProgram();
    const TileProgramOpaque* tileProgramOpaque();
    const TileProgramAA* tileProgramAA();
    const TileProgramSwizzle* tileProgramSwizzle();
    const TileProgramSwizzleOpaque* tileProgramSwizzleOpaque();
    const TileProgramSwizzleAA* tileProgramSwizzleAA();
    const TileCheckerboardProgram* tileCheckerboardProgram();

    const RenderPassProgram* renderPassProgram();
    const RenderPassProgramAA* renderPassProgramAA();
    const RenderPassMaskProgram* renderPassMaskProgram();
    const RenderPassMaskProgramAA* renderPassMaskProgramAA();

    const TextureProgram* textureProgram();
    const TextureProgramFlip* textureProgramFlip();
    const TextureIOSurfaceProgram* textureIOSurfaceProgram();

    const VideoYUVProgram* videoYUVProgram();
    const VideoStreamTextureProgram* videoStreamTextureProgram();

    const SolidColorProgram* solidColorProgram();

    OwnPtr<TileProgram> m_tileProgram;
    OwnPtr<TileProgramOpaque> m_tileProgramOpaque;
    OwnPtr<TileProgramAA> m_tileProgramAA;
    OwnPtr<TileProgramSwizzle> m_tileProgramSwizzle;
    OwnPtr<TileProgramSwizzleOpaque> m_tileProgramSwizzleOpaque;
    OwnPtr<TileProgramSwizzleAA> m_tileProgramSwizzleAA;
    OwnPtr<TileCheckerboardProgram> m_tileCheckerboardProgram;

    OwnPtr<RenderPassProgram> m_renderPassProgram;
    OwnPtr<RenderPassProgramAA> m_renderPassProgramAA;
    OwnPtr<RenderPassMaskProgram> m_renderPassMaskProgram;
    OwnPtr<RenderPassMaskProgramAA> m_renderPassMaskProgramAA;

    OwnPtr<TextureProgram> m_textureProgram;
    OwnPtr<TextureProgramFlip> m_textureProgramFlip;
    OwnPtr<TextureIOSurfaceProgram> m_textureIOSurfaceProgram;

    OwnPtr<VideoYUVProgram> m_videoYUVProgram;
    OwnPtr<VideoStreamTextureProgram> m_videoStreamTextureProgram;

    OwnPtr<SolidColorProgram> m_solidColorProgram;

    OwnPtr<AcceleratedTextureCopier> m_textureCopier;
    OwnPtr<TextureUploader> m_textureUploader;

    WebKit::WebGraphicsContext3D* m_context;

    IntRect m_swapBufferRect;
    bool m_isViewportChanged;
    bool m_isFramebufferDiscarded;
    bool m_isUsingBindUniform;
    bool m_visible;
    TextureUploaderOption m_textureUploaderSetting;

    OwnPtr<CCScopedLockResourceForWrite> m_currentFramebufferLock;
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
