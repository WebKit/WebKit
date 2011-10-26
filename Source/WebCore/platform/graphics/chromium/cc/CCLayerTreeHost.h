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
#include "RateLimiter.h"
#include "TransformationMatrix.h"
#include "cc/CCLayerTreeHostCommon.h"
#include "cc/CCProxy.h"

#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

#if USE(SKIA)
class GrContext;
#endif

namespace WebCore {

class CCLayerTreeHostImpl;
class CCTextureUpdater;
class GraphicsContext3D;
class LayerChromium;
class LayerPainterChromium;
class TextureAllocator;
class TextureManager;

class CCLayerTreeHostClient {
public:
    virtual void animateAndLayout(double frameBeginTime) = 0;
    virtual void applyScrollDelta(const IntSize&) = 0;
    virtual PassRefPtr<GraphicsContext3D> createLayerTreeHostContext3D() = 0;
    virtual void didRecreateGraphicsContext(bool success) = 0;

    // Used only in the single-threaded path.
    virtual void scheduleComposite() = 0;

protected:
    virtual ~CCLayerTreeHostClient() { }
};

struct CCSettings {
    CCSettings()
            : acceleratePainting(false)
            , compositeOffscreen(false)
            , enableCompositorThread(false)
            , showFPSCounter(false)
            , showPlatformLayerTree(false) { }

    bool acceleratePainting;
    bool compositeOffscreen;
    bool enableCompositorThread;
    bool showFPSCounter;
    bool showPlatformLayerTree;
};

// Provides information on an Impl's rendering capabilities back to the CCLayerTreeHost
struct LayerRendererCapabilities {
    LayerRendererCapabilities()
        : bestTextureFormat(0)
        , usingMapSub(false)
        , usingAcceleratedPainting(false)
        , usingSetVisibility(false)
        , usingSwapCompleteCallback(false)
        , maxTextureSize(0) { }

    GC3Denum bestTextureFormat;
    bool usingMapSub;
    bool usingAcceleratedPainting;
    bool usingSetVisibility;
    bool usingSwapCompleteCallback;
    int maxTextureSize;
};

class CCLayerTreeHost : public RefCounted<CCLayerTreeHost> {
public:
    static PassRefPtr<CCLayerTreeHost> create(CCLayerTreeHostClient*, PassRefPtr<LayerChromium> rootLayer, const CCSettings&);
    virtual ~CCLayerTreeHost();

    // CCLayerTreeHost interface to CCProxy.
    void animateAndLayout(double frameBeginTime);
    void beginCommitOnImplThread(CCLayerTreeHostImpl*);
    void finishCommitOnImplThread(CCLayerTreeHostImpl*);
    void commitComplete();
    PassRefPtr<GraphicsContext3D> createLayerTreeHostContext3D();
    virtual PassOwnPtr<CCLayerTreeHostImpl> createLayerTreeHostImpl(CCLayerTreeHostImplClient*);
    void didRecreateGraphicsContext(bool success);
    void deleteContentsTexturesOnImplThread(TextureAllocator*);

    // CCLayerTreeHost interface to WebView.
    bool animating() const { return m_animating; }
    void setAnimating(bool animating) { m_animating = animating; } // Can be removed when non-threaded scheduling moves inside.

    CCLayerTreeHostClient* client() { return m_client; }

    int compositorIdentifier() const { return m_compositorIdentifier; }

    // Only used when compositing on the main thread.
    void composite();

    GraphicsContext3D* context();

    // Composites and attempts to read back the result into the provided
    // buffer. If it wasn't possible, e.g. due to context lost, will return
    // false.
    bool compositeAndReadback(void *pixels, const IntRect&);

    void finishAllRendering();

    int frameNumber() const { return m_frameNumber; }

    void setZoomAnimatorTransform(const TransformationMatrix&);

    const LayerRendererCapabilities& layerRendererCapabilities() const;

    // Test-only hook
    void loseCompositorContext(int numTimes);

    void setNeedsAnimate();
    void setNeedsCommitThenRedraw();
    void setNeedsRedraw();

    LayerChromium* rootLayer() { return m_rootLayer.get(); }
    const LayerChromium* rootLayer() const { return m_rootLayer.get(); }

    const CCSettings& settings() const { return m_settings; }

    void setViewport(const IntSize& viewportSize);

    const IntSize& viewportSize() const { return m_viewportSize; }
    TextureManager* contentsTextureManager() const;

    void setVisible(bool);

    void updateLayers();

    void updateCompositorResources(GraphicsContext3D*, CCTextureUpdater&);
    void applyScrollDeltas(const CCScrollUpdateSet&);
    void startRateLimiter(GraphicsContext3D*);
    void stopRateLimiter(GraphicsContext3D*);
protected:
    CCLayerTreeHost(CCLayerTreeHostClient*, PassRefPtr<LayerChromium> rootLayer, const CCSettings&);
    bool initialize();

private:
    typedef Vector<RefPtr<LayerChromium> > LayerList;

    void paintLayerContents(const LayerList&);
    void paintMaskAndReplicaForRenderSurface(LayerChromium*);

    void updateLayers(LayerChromium*);
    void updateCompositorResources(LayerChromium*, GraphicsContext3D*, CCTextureUpdater&);
    void clearPendingUpdate();

    int m_compositorIdentifier;

    bool m_animating;

    CCLayerTreeHostClient* m_client;

    int m_frameNumber;

    OwnPtr<CCProxy> m_proxy;

    RefPtr<LayerChromium> m_rootLayer;
    OwnPtr<TextureManager> m_contentsTextureManager;

    LayerList m_updateList;

    CCSettings m_settings;

    IntSize m_viewportSize;
    TransformationMatrix m_zoomAnimatorTransform;
    bool m_visible;
    typedef HashMap<GraphicsContext3D*, RefPtr<RateLimiter> > RateLimiterMap;
    RateLimiterMap m_rateLimiters;
};

}

#endif
