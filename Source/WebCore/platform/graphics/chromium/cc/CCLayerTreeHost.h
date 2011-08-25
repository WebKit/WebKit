/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCLayerTreeHost_h
#define CCLayerTreeHost_h

#include "GraphicsTypes3D.h"
#include "IntRect.h"
#include "cc/CCLayerTreeHostCommitter.h"
#include "cc/CCLayerTreeHostImplProxy.h"

#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class CCLayerTreeHostImpl;
class CCLayerTreeHostImplClient;
class CCThread;
class GraphicsContext3D;
class LayerChromium;
class LayerPainterChromium;
class LayerRendererChromium;
class GraphicsLayer;
class NonCompositedContentHost;
class TextureManager;

class CCLayerTreeHostClient {
public:
    virtual void animateAndLayout(double frameBeginTime) = 0;
    virtual PassOwnPtr<CCThread> createCompositorThread() = 0;
    virtual PassRefPtr<GraphicsContext3D> createLayerTreeHostContext3D() = 0;
    virtual PassOwnPtr<LayerPainterChromium> createRootLayerPainter() = 0;
    virtual void didRecreateGraphicsContext(bool success) = 0;
#if !USE(THREADED_COMPOSITING)
    virtual void scheduleComposite() = 0;
#endif
protected:
    virtual ~CCLayerTreeHostClient() { }
};

struct CCSettings {
    CCSettings()
            : acceleratePainting(false)
            , compositeOffscreen(false)
            , showFPSCounter(false)
            , showPlatformLayerTree(false) { }

    bool acceleratePainting;
    bool compositeOffscreen;
    bool showFPSCounter;
    bool showPlatformLayerTree;
};

class CCLayerTreeHost : public RefCounted<CCLayerTreeHost> {
public:
    static PassRefPtr<CCLayerTreeHost> create(CCLayerTreeHostClient*, const CCSettings&);
    virtual ~CCLayerTreeHost();

    virtual void animateAndLayout(double frameBeginTime);
    virtual void beginCommit();
    virtual void commitComplete();

    virtual PassOwnPtr<CCLayerTreeHostImpl> createLayerTreeHostImpl(CCLayerTreeHostImplClient*);
    virtual PassOwnPtr<CCLayerTreeHostCommitter> createLayerTreeHostCommitter();

    bool animating() const { return m_animating; }
    void setAnimating(bool animating) { m_animating = animating; } // Can be removed when non-threaded scheduling moves inside.

    CCLayerTreeHostClient* client() { return m_client; }

    GraphicsContext3D* context();

    void compositeAndReadback(void *pixels, const IntRect&);

    void finishAllRendering();

    int frameNumber() const { return m_frameNumber; }

    void invalidateRootLayerRect(const IntRect& dirtyRect);

    void setNeedsCommitAndRedraw();
    void setNeedsRedraw();

    void setRootLayer(GraphicsLayer*);
    GraphicsLayer* rootLayer() { return m_rootLayer.get(); }
    const GraphicsLayer* rootLayer() const { return m_rootLayer.get(); }

    const CCSettings& settings() const { return m_settings; }

    void setViewport(const IntSize& viewportSize, const IntSize& contentsSize, const IntPoint& scrollPosition);

    const IntSize& viewportSize() const { return m_viewportSize; }
    TextureManager* contentsTextureManager() const;

    void setVisible(bool);

    // Temporary home for the non-threaded rendering path.
#if !USE(THREADED_COMPOSITING)
    void composite(bool finish);

    void loseCompositorContext();

    LayerRendererChromium* layerRenderer() const { return m_layerRenderer.get(); }
#endif

    NonCompositedContentHost* nonCompositedContentHost() const { return m_nonCompositedContentHost.get(); }

    bool contextSupportsMapSub() const { return m_contextSupportsMapSub; }
    int maxTextureSize() const { return m_maxTextureSize; }
    GC3Denum bestTextureFormat() { return m_bestTextureFormat; }

protected:
    CCLayerTreeHost(CCLayerTreeHostClient*, const CCSettings&);

private:
    bool initialize();

    PassRefPtr<LayerRendererChromium> createLayerRenderer();

    // Temporary home for the non-threaded rendering path.
#if !USE(THREADED_COMPOSITING)
    void doComposite();
    void reallocateRenderer();

    bool m_recreatingGraphicsContext;
    RefPtr<LayerRendererChromium> m_layerRenderer;
#endif

    int m_maxTextureSize;
    GC3Denum m_bestTextureFormat;
    bool m_contextSupportsMapSub;

    bool m_animating;

    CCLayerTreeHostClient* m_client;

    int m_frameNumber;

    OwnPtr<CCLayerTreeHostImplProxy> m_proxy;

    OwnPtr<GraphicsLayer> m_rootLayer;
    OwnPtr<NonCompositedContentHost> m_nonCompositedContentHost;

    CCSettings m_settings;

    IntSize m_viewportSize;
};

}

#endif
