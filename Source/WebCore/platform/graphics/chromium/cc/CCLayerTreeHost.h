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

#include "CCAnimationEvents.h"
#include "CCGraphicsContext.h"
#include "CCLayerTreeHostCommon.h"
#include "CCOcclusionTracker.h"
#include "CCPrioritizedTextureManager.h"
#include "CCProxy.h"
#include "CCRenderingStats.h"
#include "IntRect.h"
#include "RateLimiter.h"
#include "SkColor.h"
#include <limits>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class CCFontAtlas;
class CCLayerChromium;
class CCLayerTreeHostImpl;
class CCLayerTreeHostImplClient;
class CCPrioritizedTextureManager;
class CCTextureUpdateQueue;
class HeadsUpDisplayLayerChromium;
class Region;
struct CCScrollAndScaleSet;

class CCLayerTreeHostClient {
public:
    virtual void willBeginFrame() = 0;
    // Marks finishing compositing-related tasks on the main thread. In threaded mode, this corresponds to didCommit().
    virtual void didBeginFrame() = 0;
    virtual void updateAnimations(double frameBeginTime) = 0;
    virtual void layout() = 0;
    virtual void applyScrollAndScale(const IntSize& scrollDelta, float pageScale) = 0;
    virtual PassOwnPtr<WebKit::WebCompositorOutputSurface> createOutputSurface() = 0;
    virtual void didRecreateOutputSurface(bool success) = 0;
    virtual void willCommit() = 0;
    virtual void didCommit() = 0;
    virtual void didCommitAndDrawFrame() = 0;
    virtual void didCompleteSwapBuffers() = 0;

    // Used only in the single-threaded path.
    virtual void scheduleComposite() = 0;

protected:
    virtual ~CCLayerTreeHostClient() { }
};

struct CCLayerTreeSettings {
    CCLayerTreeSettings()
            : acceleratePainting(false)
            , showFPSCounter(false)
            , showPlatformLayerTree(false)
            , showPaintRects(false)
            , showPropertyChangedRects(false)
            , showSurfaceDamageRects(false)
            , showScreenSpaceRects(false)
            , showReplicaScreenSpaceRects(false)
            , showOccludingRects(false)
            , renderVSyncEnabled(true)
            , refreshRate(0)
            , maxPartialTextureUpdates(std::numeric_limits<size_t>::max())
            , defaultTileSize(IntSize(256, 256))
            , maxUntiledLayerSize(IntSize(512, 512))
            , minimumOcclusionTrackingSize(IntSize(160, 160))
    { }

    bool acceleratePainting;
    bool showFPSCounter;
    bool showPlatformLayerTree;
    bool showPaintRects;
    bool showPropertyChangedRects;
    bool showSurfaceDamageRects;
    bool showScreenSpaceRects;
    bool showReplicaScreenSpaceRects;
    bool showOccludingRects;
    bool renderVSyncEnabled;
    double refreshRate;
    size_t maxPartialTextureUpdates;
    IntSize defaultTileSize;
    IntSize maxUntiledLayerSize;
    IntSize minimumOcclusionTrackingSize;

    bool showDebugInfo() const { return showPlatformLayerTree || showFPSCounter || showDebugRects(); }
    bool showDebugRects() const { return showPaintRects || showPropertyChangedRects || showSurfaceDamageRects || showScreenSpaceRects || showReplicaScreenSpaceRects || showOccludingRects; }
};

// Provides information on an Impl's rendering capabilities back to the CCLayerTreeHost
struct LayerRendererCapabilities {
    LayerRendererCapabilities()
        : bestTextureFormat(0)
        , contextHasCachedFrontBuffer(false)
        , usingPartialSwap(false)
        , usingAcceleratedPainting(false)
        , usingSetVisibility(false)
        , usingSwapCompleteCallback(false)
        , usingGpuMemoryManager(false)
        , usingDiscardFramebuffer(false)
        , usingEglImage(false)
        , maxTextureSize(0) { }

    GC3Denum bestTextureFormat;
    bool contextHasCachedFrontBuffer;
    bool usingPartialSwap;
    bool usingAcceleratedPainting;
    bool usingSetVisibility;
    bool usingSwapCompleteCallback;
    bool usingGpuMemoryManager;
    bool usingDiscardFramebuffer;
    bool usingEglImage;
    int maxTextureSize;
};

class CCLayerTreeHost : public RateLimiterClient {
    WTF_MAKE_NONCOPYABLE(CCLayerTreeHost);
public:
    static PassOwnPtr<CCLayerTreeHost> create(CCLayerTreeHostClient*, const CCLayerTreeSettings&);
    virtual ~CCLayerTreeHost();

    void setSurfaceReady();

    // Returns true if any CCLayerTreeHost is alive.
    static bool anyLayerTreeHostInstanceExists();

    static bool needsFilterContext() { return s_needsFilterContext; }
    static void setNeedsFilterContext(bool needsFilterContext) { s_needsFilterContext = needsFilterContext; }
    bool needsSharedContext() const { return needsFilterContext() || settings().acceleratePainting; }

    // CCLayerTreeHost interface to CCProxy.
    void willBeginFrame() { m_client->willBeginFrame(); }
    void didBeginFrame() { m_client->didBeginFrame(); }
    void updateAnimations(double monotonicFrameBeginTime);
    void layout();
    void beginCommitOnImplThread(CCLayerTreeHostImpl*);
    void finishCommitOnImplThread(CCLayerTreeHostImpl*);
    void willCommit();
    void commitComplete();
    PassOwnPtr<CCGraphicsContext> createContext();
    virtual PassOwnPtr<CCLayerTreeHostImpl> createLayerTreeHostImpl(CCLayerTreeHostImplClient*);
    void didLoseContext();
    enum RecreateResult {
        RecreateSucceeded,
        RecreateFailedButTryAgain,
        RecreateFailedAndGaveUp,
    };
    RecreateResult recreateContext();
    void didCommitAndDrawFrame() { m_client->didCommitAndDrawFrame(); }
    void didCompleteSwapBuffers() { m_client->didCompleteSwapBuffers(); }
    void deleteContentsTexturesOnImplThread(CCResourceProvider*);
    virtual void acquireLayerTextures();
    // Returns false if we should abort this frame due to initialization failure.
    bool initializeLayerRendererIfNeeded();
    void updateLayers(CCTextureUpdateQueue&, size_t contentsMemoryLimitBytes);

    CCLayerTreeHostClient* client() { return m_client; }

    int compositorIdentifier() const { return m_compositorIdentifier; }

    // Only used when compositing on the main thread.
    void composite();
    void scheduleComposite();

    // Composites and attempts to read back the result into the provided
    // buffer. If it wasn't possible, e.g. due to context lost, will return
    // false.
    bool compositeAndReadback(void *pixels, const IntRect&);

    void finishAllRendering();

    int commitNumber() const { return m_commitNumber; }

    void renderingStats(CCRenderingStats&) const;

    const LayerRendererCapabilities& layerRendererCapabilities() const;

    // Test only hook
    void loseContext(int numTimes);

    void setNeedsAnimate();
    // virtual for testing
    virtual void setNeedsCommit();
    void setNeedsRedraw();
    bool commitRequested() const;

    void setAnimationEvents(PassOwnPtr<CCAnimationEventsVector>, double wallClockTime);
    virtual void didAddAnimation();

    LayerChromium* rootLayer() { return m_rootLayer.get(); }
    const LayerChromium* rootLayer() const { return m_rootLayer.get(); }
    void setRootLayer(PassRefPtr<LayerChromium>);

    const CCLayerTreeSettings& settings() const { return m_settings; }

    void setViewportSize(const IntSize& layoutViewportSize, const IntSize& deviceViewportSize);

    const IntSize& layoutViewportSize() const { return m_layoutViewportSize; }
    const IntSize& deviceViewportSize() const { return m_deviceViewportSize; }

    void setPageScaleFactorAndLimits(float pageScaleFactor, float minPageScaleFactor, float maxPageScaleFactor);

    void setBackgroundColor(SkColor color) { m_backgroundColor = color; }

    void setHasTransparentBackground(bool transparent) { m_hasTransparentBackground = transparent; }

    CCPrioritizedTextureManager* contentsTextureManager() const;

    // This will cause contents texture manager to evict all textures, but
    // without deleting them. This happens after all content textures have
    // already been deleted on impl, after getting a 0 allocation limit.
    // Set during a commit, but before updateLayers.
    void evictAllContentTextures();

    bool visible() const { return m_visible; }
    void setVisible(bool);

    void startPageScaleAnimation(const IntSize& targetPosition, bool useAnchor, float scale, double durationSec);

    void applyScrollAndScale(const CCScrollAndScaleSet&);

    void startRateLimiter(WebKit::WebGraphicsContext3D*);
    void stopRateLimiter(WebKit::WebGraphicsContext3D*);

    // RateLimitClient implementation
    virtual void rateLimit() OVERRIDE;

    bool bufferedUpdates();
    bool requestPartialTextureUpdate();
    void deleteTextureAfterCommit(PassOwnPtr<CCPrioritizedTexture>);

    void setDeviceScaleFactor(float);
    float deviceScaleFactor() const { return m_deviceScaleFactor; }

    void setFontAtlas(PassOwnPtr<CCFontAtlas>);

protected:
    CCLayerTreeHost(CCLayerTreeHostClient*, const CCLayerTreeSettings&);
    bool initialize();

private:
    typedef Vector<RefPtr<LayerChromium> > LayerList;
    typedef Vector<OwnPtr<CCPrioritizedTexture> > TextureList;

    void initializeLayerRenderer();

    void update(LayerChromium*, CCTextureUpdateQueue&, const CCOcclusionTracker*);
    bool paintLayerContents(const LayerList&, CCTextureUpdateQueue&);
    bool paintMasksForRenderSurface(LayerChromium*, CCTextureUpdateQueue&);

    void updateLayers(LayerChromium*, CCTextureUpdateQueue&);

    void prioritizeTextures(const LayerList&, CCOverdrawMetrics&); 
    void setPrioritiesForSurfaces(size_t surfaceMemoryBytes);
    void setPrioritiesForLayers(const LayerList&);
    size_t calculateMemoryForRenderSurfaces(const LayerList& updateList);

    void animateLayers(double monotonicTime);
    bool animateLayersRecursive(LayerChromium* current, double monotonicTime);
    void setAnimationEventsRecursive(const CCAnimationEventsVector&, LayerChromium*, double wallClockTime);

    int m_compositorIdentifier;

    bool m_animating;
    bool m_needsAnimateLayers;

    CCLayerTreeHostClient* m_client;

    int m_commitNumber;
    CCRenderingStats m_renderingStats;

    OwnPtr<CCProxy> m_proxy;
    bool m_layerRendererInitialized;
    bool m_contextLost;
    int m_numTimesRecreateShouldFail;
    int m_numFailedRecreateAttempts;

    RefPtr<LayerChromium> m_rootLayer;
    RefPtr<HeadsUpDisplayLayerChromium> m_hudLayer;
    OwnPtr<CCFontAtlas> m_fontAtlas;

    OwnPtr<CCPrioritizedTextureManager> m_contentsTextureManager;
    OwnPtr<CCPrioritizedTexture> m_surfaceMemoryPlaceholder;

    CCLayerTreeSettings m_settings;

    IntSize m_layoutViewportSize;
    IntSize m_deviceViewportSize;
    float m_deviceScaleFactor;

    bool m_visible;

    typedef HashMap<WebKit::WebGraphicsContext3D*, RefPtr<RateLimiter> > RateLimiterMap;
    RateLimiterMap m_rateLimiters;

    float m_pageScaleFactor;
    float m_minPageScaleFactor, m_maxPageScaleFactor;
    bool m_triggerIdleUpdates;

    SkColor m_backgroundColor;
    bool m_hasTransparentBackground;

    TextureList m_deleteTextureAfterCommitList;
    size_t m_partialTextureUpdateRequests;

    static bool s_needsFilterContext;
};

}

#endif
