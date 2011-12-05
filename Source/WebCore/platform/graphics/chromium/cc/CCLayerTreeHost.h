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
#include "LayerChromium.h"
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
class LayerPainterChromium;
class TextureAllocator;
class TextureManager;

class CCLayerTreeHostClient {
public:
    virtual void animateAndLayout(double frameBeginTime) = 0;
    virtual void applyScrollAndScale(const IntSize& scrollDelta, float pageScale) = 0;
    virtual PassRefPtr<GraphicsContext3D> createLayerTreeHostContext3D() = 0;
    virtual void didRecreateGraphicsContext(bool success) = 0;
    virtual void didCommitAndDrawFrame() = 0;
    virtual void didCompleteSwapBuffers() = 0;

    // Used only in the single-threaded path.
    virtual void scheduleComposite() = 0;

protected:
    virtual ~CCLayerTreeHostClient() { }
};

struct CCSettings {
    CCSettings()
            : acceleratePainting(false)
            , compositeOffscreen(false)
            , showFPSCounter(false)
            , showPlatformLayerTree(false)
            , refreshRate(0) { }

    bool acceleratePainting;
    bool compositeOffscreen;
    bool showFPSCounter;
    bool showPlatformLayerTree;
    double refreshRate;
};

// Provides information on an Impl's rendering capabilities back to the CCLayerTreeHost
struct LayerRendererCapabilities {
    LayerRendererCapabilities()
        : bestTextureFormat(0)
        , contextHasCachedFrontBuffer(false)
        , usingPartialSwap(false)
        , usingMapSub(false)
        , usingAcceleratedPainting(false)
        , usingSetVisibility(false)
        , usingSwapCompleteCallback(false)
        , usingTextureUsageHint(false)
        , usingTextureStorageExtension(false)
        , maxTextureSize(0) { }

    GC3Denum bestTextureFormat;
    bool contextHasCachedFrontBuffer;
    bool usingPartialSwap;
    bool usingMapSub;
    bool usingAcceleratedPainting;
    bool usingSetVisibility;
    bool usingSwapCompleteCallback;
    bool usingTextureUsageHint;
    bool usingTextureStorageExtension;
    int maxTextureSize;
};

class CCLayerTreeHost : public RefCounted<CCLayerTreeHost> {
public:
    static PassRefPtr<CCLayerTreeHost> create(CCLayerTreeHostClient*, const CCSettings&);
    virtual ~CCLayerTreeHost();

    // Returns true if any CCLayerTreeHost is alive.
    static bool anyLayerTreeHostInstanceExists();

    // CCLayerTreeHost interface to CCProxy.
    void animateAndLayout(double frameBeginTime);
    void beginCommitOnImplThread(CCLayerTreeHostImpl*);
    void finishCommitOnImplThread(CCLayerTreeHostImpl*);
    void commitComplete();
    PassRefPtr<GraphicsContext3D> createLayerTreeHostContext3D();
    virtual PassOwnPtr<CCLayerTreeHostImpl> createLayerTreeHostImpl(CCLayerTreeHostImplClient*);
    void didBecomeInvisibleOnImplThread(CCLayerTreeHostImpl*);
    void didRecreateGraphicsContext(bool success);
    void didCommitAndDrawFrame() { m_client->didCommitAndDrawFrame(); }
    void didCompleteSwapBuffers() { m_client->didCompleteSwapBuffers(); }
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
    void setNeedsCommit();
    void setNeedsRedraw();

    LayerChromium* rootLayer() { return m_rootLayer.get(); }
    const LayerChromium* rootLayer() const { return m_rootLayer.get(); }
    void setRootLayer(PassRefPtr<LayerChromium> rootLayer) { m_rootLayer = rootLayer; }

    const CCSettings& settings() const { return m_settings; }

    void setViewport(const IntSize& viewportSize);

    const IntSize& viewportSize() const { return m_viewportSize; }

    void setPageScale(float);
    float pageScale() const { return m_pageScale; }

    void setPageScaleFactorLimits(float minScale, float maxScale);

    TextureManager* contentsTextureManager() const;

    bool visible() const { return m_visible; }
    void setVisible(bool);

    void setHaveWheelEventHandlers(bool);

    void updateLayers();

    void updateCompositorResources(GraphicsContext3D*, CCTextureUpdater&);
    void applyScrollAndScale(const CCScrollAndScaleSet&);
    void startRateLimiter(GraphicsContext3D*);
    void stopRateLimiter(GraphicsContext3D*);

protected:
    CCLayerTreeHost(CCLayerTreeHostClient*, const CCSettings&);
    bool initialize();

private:
    typedef Vector<RefPtr<LayerChromium> > LayerList;

    void paintLayerContents(const LayerList&);
    void paintMaskAndReplicaForRenderSurface(LayerChromium*);

    void updateLayers(LayerChromium*);
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
    bool m_haveWheelEventHandlers;
    typedef HashMap<GraphicsContext3D*, RefPtr<RateLimiter> > RateLimiterMap;
    RateLimiterMap m_rateLimiters;

    float m_pageScale;
    float m_minPageScale, m_maxPageScale;
};

}

#endif
