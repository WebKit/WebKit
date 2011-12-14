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
#include "LayerTilerChromium.h"
#include "RenderSurfaceChromium.h"
#include "SkBitmap.h"
#include "VideoLayerChromium.h"
#include "cc/CCCanvasLayerImpl.h"
#include "cc/CCHeadsUpDisplay.h"
#include "cc/CCLayerSorter.h"
#include "cc/CCPluginLayerImpl.h"
#include "cc/CCVideoLayerImpl.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

#if USE(CG)
#include <CoreGraphics/CGContext.h>
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {

class CCHeadsUpDisplay;
class CCLayerImpl;
class GeometryBinding;
class GraphicsContext3D;

// Class that handles drawing of composited render layers using GL.
class LayerRendererChromium : public RefCounted<LayerRendererChromium> {
public:
    static PassRefPtr<LayerRendererChromium> create(PassRefPtr<GraphicsContext3D>, PassOwnPtr<TilePaintInterface> contentPaint);

    ~LayerRendererChromium();

    GraphicsContext3D* context();
    bool contextSupportsMapSub() const { return m_contextSupportsMapSub; }

    void invalidateRootLayerRect(const IntRect& dirtyRect);

    void setViewport(const IntRect& visibleRect, const IntRect& contentRect, const IntPoint& scrollPosition);

    // updates and draws the current layers onto the backbuffer
    void updateAndDrawLayers();

    // Set by WebViewImpl when animation callbacks are running.
    // FIXME: When we move scheduling into the compositor, we can remove this flag.
    void setIsAnimating(bool animating) { m_animating = animating; }
    bool isAnimating() const { return m_animating; }

    // waits for rendering to finish
    void finish();

    // puts backbuffer onscreen
    void present();

    IntSize viewportSize() const { return m_viewportVisibleRect.size(); }

    void setRootLayer(PassRefPtr<LayerChromium>);
    LayerChromium* rootLayer() { return m_rootLayer.get(); }
    void transferRootLayer(LayerRendererChromium* other);

    bool hardwareCompositing() const { return m_hardwareCompositing; }

    void setCompositeOffscreen(bool);
    bool isCompositingOffscreen() const { return m_compositeOffscreen; }

    unsigned createLayerTexture();
    void deleteLayerTexture(unsigned);

    static void debugGLCall(GraphicsContext3D*, const char* command, const char* file, int line);

    const TransformationMatrix& projectionMatrix() const { return m_projectionMatrix; }

    void useShader(unsigned);

    bool checkTextureSize(const IntSize&);

    const GeometryBinding* sharedGeometry() const { return m_sharedGeometry.get(); }
    const LayerChromium::BorderProgram* borderProgram() const { return m_borderProgram.get(); }
    const CCHeadsUpDisplay::Program* headsUpDisplayProgram() const { return m_headsUpDisplayProgram.get(); }
    const RenderSurfaceChromium::Program* renderSurfaceProgram() const { return m_renderSurfaceProgram.get(); }
    const RenderSurfaceChromium::MaskProgram* renderSurfaceMaskProgram() const { return m_renderSurfaceMaskProgram.get(); }
    const LayerTilerChromium::Program* tilerProgram() const { return m_tilerProgram.get(); }
    const CCCanvasLayerImpl::Program* canvasLayerProgram() const { return m_canvasLayerProgram.get(); }
    const CCPluginLayerImpl::Program* pluginLayerProgram() const { return m_pluginLayerProgram.get(); }
    const CCVideoLayerImpl::RGBAProgram* videoLayerRGBAProgram() const { return m_videoLayerRGBAProgram.get(); }
    const CCVideoLayerImpl::YUVProgram* videoLayerYUVProgram() const { return m_videoLayerYUVProgram.get(); }

    void resizeOnscreenContent(const IntSize&);

    void getFramebufferPixels(void *pixels, const IntRect& rect);

    TextureManager* textureManager() const { return m_textureManager.get(); }

    CCHeadsUpDisplay* headsUpDisplay() { return m_headsUpDisplay.get(); }

    void setScissorToRect(const IntRect&);

    String layerTreeAsText() const;

    void addChildContext(GraphicsContext3D*);
    void removeChildContext(GraphicsContext3D*);

#ifndef NDEBUG
    static bool s_inPaintLayerContents;
#endif
private:
    typedef Vector<RefPtr<CCLayerImpl> > LayerList;
    typedef HashMap<GraphicsContext3D*, int> ChildContextMap;

    // FIXME: This needs to be moved to the CCViewImpl when that class exists.
    RefPtr<CCLayerImpl> m_rootCCLayerImpl;

    LayerRendererChromium(PassRefPtr<GraphicsContext3D>, PassOwnPtr<TilePaintInterface> contentPaint);

    void updateLayers(LayerList& renderSurfaceLayerList);
    void updateRootLayerContents();
    void updatePropertiesAndRenderSurfaces(CCLayerImpl*, const TransformationMatrix& parentMatrix, LayerList& renderSurfaceLayerList, LayerList& layers);

    void paintLayerContents(const LayerList&);
    void updateCompositorResourcesRecursive(LayerChromium*);

    void drawLayers(const LayerList& renderSurfaceLayerList);
    void drawLayer(CCLayerImpl*, RenderSurfaceChromium*);

    void drawRootLayer();
    LayerTexture* getOffscreenLayerTexture();
    void copyOffscreenTextureToDisplay();

    bool isLayerVisible(LayerChromium*, const TransformationMatrix&, const IntRect& visibleRect);

    void setDrawViewportRect(const IntRect&, bool flipY);

    bool useRenderSurface(RenderSurfaceChromium*);

    bool makeContextCurrent();

    static bool compareLayerZ(const RefPtr<CCLayerImpl>&, const RefPtr<CCLayerImpl>&);

    void dumpRenderSurfaces(TextStream&, int indent, LayerChromium*) const;

    bool initializeSharedObjects();
    void cleanupSharedObjects();

    void setLayerRendererRecursive(LayerChromium*);

    IntRect m_viewportVisibleRect;
    IntRect m_viewportContentRect;
    IntPoint m_viewportScrollPosition;

    TransformationMatrix m_projectionMatrix;

    RefPtr<LayerChromium> m_rootLayer;
    OwnPtr<TilePaintInterface> m_rootLayerContentPaint;
    OwnPtr<LayerTilerChromium> m_rootLayerContentTiler;

    bool m_hardwareCompositing;

    unsigned m_currentShader;
    RenderSurfaceChromium* m_currentRenderSurface;

    unsigned m_offscreenFramebufferId;
    bool m_compositeOffscreen;

    // Maximum texture dimensions supported.
    int m_maxTextureSize;

    // Store values that are shared between instances of each layer type
    // associated with this instance of the compositor. Since there can be
    // multiple instances of the compositor running in the same renderer process
    // we cannot store these values in static variables.
    OwnPtr<GeometryBinding> m_sharedGeometry;
    OwnPtr<LayerChromium::BorderProgram> m_borderProgram;
    OwnPtr<CCHeadsUpDisplay::Program> m_headsUpDisplayProgram;
    OwnPtr<RenderSurfaceChromium::Program> m_renderSurfaceProgram;
    OwnPtr<RenderSurfaceChromium::MaskProgram> m_renderSurfaceMaskProgram;
    OwnPtr<LayerTilerChromium::Program> m_tilerProgram;
    OwnPtr<CCCanvasLayerImpl::Program> m_canvasLayerProgram;
    OwnPtr<CCVideoLayerImpl::RGBAProgram> m_videoLayerRGBAProgram;
    OwnPtr<CCVideoLayerImpl::YUVProgram> m_videoLayerYUVProgram;
    OwnPtr<CCPluginLayerImpl::Program> m_pluginLayerProgram;

    OwnPtr<TextureManager> m_textureManager;

    OwnPtr<CCHeadsUpDisplay> m_headsUpDisplay;

    RefPtr<GraphicsContext3D> m_context;
    ChildContextMap m_childContexts;

    // If true, the child contexts were copied to the compositor texture targets
    // and the compositor will need to wait on the proper latches before using
    // the target textures. If false, the compositor is reusing the textures
    // from last frame.
    bool m_childContextsWereCopied;

    bool m_contextSupportsLatch;
    bool m_contextSupportsMapSub;

    bool m_animating;

    RenderSurfaceChromium* m_defaultRenderSurface;

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
