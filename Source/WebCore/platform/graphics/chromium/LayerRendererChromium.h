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

#include "CanvasLayerChromium.h"
#include "ContentLayerChromium.h"
#include "IntRect.h"
#include "LayerChromium.h"
#include "LayerTilerChromium.h"
#include "PluginLayerChromium.h"
#include "RenderSurfaceChromium.h"
#include "SkBitmap.h"
#include "VideoLayerChromium.h"
#include "cc/CCHeadsUpDisplay.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

#if PLATFORM(CG)
#include <CoreGraphics/CGContext.h>
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {

class CCLayerImpl;
class GeometryBinding;
class GraphicsContext3D;
class CCHeadsUpDisplay;

// Class that handles drawing of composited render layers using GL.
class LayerRendererChromium : public RefCounted<LayerRendererChromium> {
public:
    static PassRefPtr<LayerRendererChromium> create(PassRefPtr<GraphicsContext3D> graphicsContext3D);

    ~LayerRendererChromium();

    GraphicsContext3D* context();

    void invalidateRootLayerRect(const IntRect& dirtyRect, const IntRect& visibleRect, const IntRect& contentRect);

    // updates and draws the current layers onto the backbuffer
    void updateAndDrawLayers(const IntRect& visibleRect, const IntRect& contentRect, const IntPoint& scrollPosition,
                             TilePaintInterface&, TilePaintInterface& scrollbarPaint);

    // waits for rendering to finish
    void finish();

    // puts backbuffer onscreen
    void present();

    IntSize visibleRectSize() const { return m_visibleRect.size(); }

    void setRootLayer(PassRefPtr<LayerChromium> layer);
    LayerChromium* rootLayer() { return m_rootLayer.get(); }
    void transferRootLayer(LayerRendererChromium* other) { other->m_rootLayer = m_rootLayer.release(); }

    bool hardwareCompositing() const { return m_hardwareCompositing; }

    void setCompositeOffscreen(bool);
    bool isCompositingOffscreen() const { return m_compositeOffscreen; }
    LayerTexture* getOffscreenLayerTexture();
    void copyOffscreenTextureToDisplay();

    unsigned createLayerTexture();
    void deleteLayerTexture(unsigned);

    static void debugGLCall(GraphicsContext3D*, const char* command, const char* file, int line);

    const TransformationMatrix& projectionMatrix() const { return m_projectionMatrix; }

    void useShader(unsigned);

    bool checkTextureSize(const IntSize&);

    const GeometryBinding* sharedGeometry() const { return m_sharedGeometry.get(); }
    const LayerChromium::BorderProgram* borderProgram() const { return m_borderProgram.get(); }
    const ContentLayerChromium::Program* contentLayerProgram() const { return m_contentLayerProgram.get(); }
    const CanvasLayerChromium::Program* canvasLayerProgram() const { return m_canvasLayerProgram.get(); }
    const VideoLayerChromium::RGBAProgram* videoLayerRGBAProgram() const { return m_videoLayerRGBAProgram.get(); }
    const VideoLayerChromium::YUVProgram* videoLayerYUVProgram() const { return m_videoLayerYUVProgram.get(); }
    const PluginLayerChromium::Program* pluginLayerProgram() const { return m_pluginLayerProgram.get(); }
    const RenderSurfaceChromium::Program* renderSurfaceProgram() const { return m_renderSurfaceProgram.get(); }
    const RenderSurfaceChromium::MaskProgram* renderSurfaceMaskProgram() const { return m_renderSurfaceMaskProgram.get(); }
    const LayerTilerChromium::Program* tilerProgram() const { return m_tilerProgram.get(); }

    void resizeOnscreenContent(const IntSize&);

    void getFramebufferPixels(void *pixels, const IntRect& rect);

    TextureManager* textureManager() const { return m_textureManager.get(); }

    CCHeadsUpDisplay* headsUpDisplay() { return m_headsUpDisplay.get(); }

    void setScissorToRect(const IntRect&);

    String layerTreeAsText() const;

private:
    explicit LayerRendererChromium(PassRefPtr<GraphicsContext3D> graphicsContext3D);

    void updateLayers(const IntRect& visibleRect, const IntRect& contentRect, const IntPoint& scrollPosition,
                     Vector<CCLayerImpl*>& renderSurfaceLayerList);
    void updateRootLayerContents(TilePaintInterface&, const IntRect& visibleRect);
    void updateRootLayerScrollbars(TilePaintInterface& scrollbarPaint, const IntRect& visibleRect, const IntRect& contentRect);
    void updatePropertiesAndRenderSurfaces(LayerChromium*, const TransformationMatrix& parentMatrix, Vector<CCLayerImpl*>& renderSurfaceLayerList, Vector<CCLayerImpl*>& layerList);
    void updateContentsRecursive(LayerChromium*);

    void drawLayers(const Vector<CCLayerImpl*>& renderSurfaceLayerList);
    void drawLayer(CCLayerImpl*, RenderSurfaceChromium*);

    void drawRootLayer();

    bool isLayerVisible(LayerChromium*, const TransformationMatrix&, const IntRect& visibleRect);

    void setDrawViewportRect(const IntRect&, bool flipY);

    bool useRenderSurface(RenderSurfaceChromium*);

    bool makeContextCurrent();

    static bool compareLayerZ(const CCLayerImpl*, const CCLayerImpl*);

    void dumpRenderSurfaces(TextStream&, int indent, LayerChromium*) const;

    bool initializeSharedObjects();
    void cleanupSharedObjects();

    static IntRect verticalScrollbarRect(const IntRect& visibleRect, const IntRect& contentRect);
    static IntRect horizontalScrollbarRect(const IntRect& visibleRect, const IntRect& contentRect);

    IntRect m_visibleRect;

    TransformationMatrix m_projectionMatrix;

    RefPtr<LayerChromium> m_rootLayer;
    OwnPtr<LayerTilerChromium> m_rootLayerTiler;
    OwnPtr<LayerTilerChromium> m_horizontalScrollbarTiler;
    OwnPtr<LayerTilerChromium> m_verticalScrollbarTiler;

    IntPoint m_scrollPosition;
    bool m_hardwareCompositing;

    unsigned m_currentShader;
    RenderSurfaceChromium* m_currentRenderSurface;

    unsigned m_offscreenFramebufferId;
    bool m_compositeOffscreen;

#if USE(SKIA)
    OwnPtr<skia::PlatformCanvas> m_rootLayerCanvas;
    OwnPtr<PlatformContextSkia> m_rootLayerSkiaContext;
    OwnPtr<GraphicsContext> m_rootLayerGraphicsContext;
#elif PLATFORM(CG)
    Vector<uint8_t> m_rootLayerBackingStore;
    RetainPtr<CGContextRef> m_rootLayerCGContext;
    OwnPtr<GraphicsContext> m_rootLayerGraphicsContext;
#endif

    // Maximum texture dimensions supported.
    int m_maxTextureSize;

    // Store values that are shared between instances of each layer type
    // associated with this instance of the compositor. Since there can be
    // multiple instances of the compositor running in the same renderer process
    // we cannot store these values in static variables.
    OwnPtr<GeometryBinding> m_sharedGeometry;
    OwnPtr<LayerChromium::BorderProgram> m_borderProgram;
    OwnPtr<ContentLayerChromium::Program> m_contentLayerProgram;
    OwnPtr<CanvasLayerChromium::Program> m_canvasLayerProgram;
    OwnPtr<VideoLayerChromium::RGBAProgram> m_videoLayerRGBAProgram;
    OwnPtr<VideoLayerChromium::YUVProgram> m_videoLayerYUVProgram;
    OwnPtr<PluginLayerChromium::Program> m_pluginLayerProgram;
    OwnPtr<RenderSurfaceChromium::Program> m_renderSurfaceProgram;
    OwnPtr<RenderSurfaceChromium::MaskProgram> m_renderSurfaceMaskProgram;
    OwnPtr<LayerTilerChromium::Program> m_tilerProgram;

    OwnPtr<TextureManager> m_textureManager;

    OwnPtr<CCHeadsUpDisplay> m_headsUpDisplay;

    RefPtr<GraphicsContext3D> m_context;

    RenderSurfaceChromium* m_defaultRenderSurface;
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
