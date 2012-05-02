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

#include "Color.h"
#include "GraphicsTypes3D.h"
#include "IntRect.h"
#include "LayerChromium.h"
#include "RateLimiter.h"
#include "TransformationMatrix.h"
#include "cc/CCAnimationEvents.h"
#include "cc/CCLayerTreeHostCommon.h"
#include "cc/CCProxy.h"

#include <limits>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class CCLayerTreeHostImpl;
class CCLayerTreeHostImplClient;
class CCTextureUpdater;
class GraphicsContext3D;
class LayerPainterChromium;
class Region;
class TextureAllocator;
class TextureManager;

class CCLayerTreeHostClient {
public:
    virtual void willBeginFrame() = 0;
    virtual void updateAnimations(double frameBeginTime) = 0;
    virtual void layout() = 0;
    virtual void applyScrollAndScale(const IntSize& scrollDelta, float pageScale) = 0;
    virtual PassRefPtr<GraphicsContext3D> createContext() = 0;
    virtual void didRecreateContext(bool success) = 0;
    virtual void didCommit() = 0;
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
            , showFPSCounter(false)
            , showPlatformLayerTree(false)
            , showPaintRects(false)
            , showPropertyChangedRects(false)
            , showSurfaceDamageRects(false)
            , refreshRate(0)
            , perTilePainting(false)
            , partialSwapEnabled(false)
            , threadedAnimationEnabled(false)
            , maxPartialTextureUpdates(std::numeric_limits<size_t>::max()) { }

    bool acceleratePainting;
    bool showFPSCounter;
    bool showPlatformLayerTree;
    bool showPaintRects;
    bool showPropertyChangedRects;
    bool showSurfaceDamageRects;
    double refreshRate;
    bool perTilePainting;
    bool partialSwapEnabled;
    bool threadedAnimationEnabled;
    size_t maxPartialTextureUpdates;
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
        , usingGpuMemoryManager(false)
        , usingDiscardFramebuffer(false)
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
    bool usingGpuMemoryManager;
    bool usingDiscardFramebuffer;
    int maxTextureSize;
};

class CCLayerTreeHost : public RateLimiterClient {
    WTF_MAKE_NONCOPYABLE(CCLayerTreeHost);
public:
    static PassOwnPtr<CCLayerTreeHost> create(CCLayerTreeHostClient*, const CCSettings&);
    virtual ~CCLayerTreeHost();

    void setSurfaceReady();

    // Returns true if any CCLayerTreeHost is alive.
    static bool anyLayerTreeHostInstanceExists();

    static bool needsFilterContext() { return s_needsFilterContext; }
    static void setNeedsFilterContext(bool needsFilterContext) { s_needsFilterContext = needsFilterContext; }

    // CCLayerTreeHost interface to CCProxy.
    void willBeginFrame() { m_client->willBeginFrame(); }
    void updateAnimations(double monotonicFrameBeginTime);
    void layout();
    void beginCommitOnImplThread(CCLayerTreeHostImpl*);
    void finishCommitOnImplThread(CCLayerTreeHostImpl*);
    void commitComplete();
    PassRefPtr<GraphicsContext3D> createContext();
    virtual PassOwnPtr<CCLayerTreeHostImpl> createLayerTreeHostImpl(CCLayerTreeHostImplClient*);
    void didBecomeInvisibleOnImplThread(CCLayerTreeHostImpl*);
    void didLoseContext();
    enum RecreateResult {
        RecreateSucceeded,
        RecreateFailedButTryAgain,
        RecreateFailedAndGaveUp,
    };
    RecreateResult recreateContext();
    void didCommitAndDrawFrame() { m_client->didCommitAndDrawFrame(); }
    void didCompleteSwapBuffers() { m_client->didCompleteSwapBuffers(); }
    void deleteContentsTexturesOnImplThread(TextureAllocator*);
    void acquireLayerTextures();
    // Returns false if we should abort this frame due to initialization failure.
    bool updateLayers(CCTextureUpdater&);

    CCLayerTreeHostClient* client() { return m_client; }

    int compositorIdentifier() const { return m_compositorIdentifier; }

    // Only used when compositing on the main thread.
    void composite();

    // NOTE: The returned value can only be used to make GL calls or make the
    // context current on the thread the compositor is running on!
    GraphicsContext3D* context();

    // Composites and attempts to read back the result into the provided
    // buffer. If it wasn't possible, e.g. due to context lost, will return
    // false.
    bool compositeAndReadback(void *pixels, const IntRect&);

    void finishAllRendering();

    int frameNumber() const { return m_frameNumber; }

    const LayerRendererCapabilities& layerRendererCapabilities() const;

    // Test only hook
    void loseContext(int numTimes);

    void setNeedsAnimate();
    // virtual for testing
    virtual void setNeedsCommit();
    void setNeedsRedraw();
    bool commitRequested() const;

    void setAnimationEvents(PassOwnPtr<CCAnimationEventsVector>, double wallClockTime);

    LayerChromium* rootLayer() { return m_rootLayer.get(); }
    const LayerChromium* rootLayer() const { return m_rootLayer.get(); }
    void setRootLayer(PassRefPtr<LayerChromium>);

    const CCSettings& settings() const { return m_settings; }

    void setViewportSize(const IntSize&);

    const IntSize& viewportSize() const { return m_viewportSize; }

    void setPageScaleFactorAndLimits(float pageScaleFactor, float minPageScaleFactor, float maxPageScaleFactor);

    void setBackgroundColor(const Color& color) { m_backgroundColor = color; }

    TextureManager* contentsTextureManager() const;
    void setContentsMemoryAllocationLimitBytes(size_t);

    bool visible() const { return m_visible; }
    void setVisible(bool);

    void startPageScaleAnimation(const IntSize& targetPosition, bool useAnchor, float scale, double durationSec);

    void applyScrollAndScale(const CCScrollAndScaleSet&);
    void startRateLimiter(GraphicsContext3D*);
    void stopRateLimiter(GraphicsContext3D*);

    // RateLimitClient implementation
    virtual void rateLimit() OVERRIDE;

    bool bufferedUpdates();
    bool requestPartialTextureUpdate();
    void deleteTextureAfterCommit(PassOwnPtr<ManagedTexture>);

protected:
    CCLayerTreeHost(CCLayerTreeHostClient*, const CCSettings&);
    bool initialize();

private:
    typedef Vector<RefPtr<LayerChromium> > LayerList;
    typedef Vector<OwnPtr<ManagedTexture> > TextureList;

    void initializeLayerRenderer();

    enum PaintType { PaintVisible, PaintIdle };
    static void update(LayerChromium*, PaintType, CCTextureUpdater&, const CCOcclusionTracker*);
    void paintLayerContents(const LayerList&, PaintType, CCTextureUpdater&);
    void paintMasksForRenderSurface(LayerChromium*, PaintType, CCTextureUpdater&);

    void updateLayers(LayerChromium*, CCTextureUpdater&);
    // Pre-reserve textures for any layer marked "always reserve textures"
    void reserveTextures(const LayerList&);

    void animateLayers(double monotonicTime);
    bool animateLayersRecursive(LayerChromium* current, double monotonicTime);
    void setAnimationEventsRecursive(const CCAnimationEventsVector&, LayerChromium*, double wallClockTime);

    int m_compositorIdentifier;

    bool m_animating;
    bool m_needsAnimateLayers;

    CCLayerTreeHostClient* m_client;

    int m_frameNumber;

    OwnPtr<CCProxy> m_proxy;
    bool m_layerRendererInitialized;
    bool m_contextLost;
    int m_numTimesRecreateShouldFail;
    int m_numFailedRecreateAttempts;

    RefPtr<LayerChromium> m_rootLayer;
    OwnPtr<TextureManager> m_contentsTextureManager;

    CCSettings m_settings;

    IntSize m_viewportSize;
    bool m_visible;
    typedef HashMap<GraphicsContext3D*, RefPtr<RateLimiter> > RateLimiterMap;
    RateLimiterMap m_rateLimiters;

    float m_pageScaleFactor;
    float m_minPageScaleFactor, m_maxPageScaleFactor;
    bool m_triggerIdlePaints;
    Color m_backgroundColor;

    TextureList m_deleteTextureAfterCommitList;
    size_t m_partialTextureUpdateRequests;
    static bool s_needsFilterContext;
};

}

#endif
