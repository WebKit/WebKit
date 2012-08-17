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

#ifndef CCLayerTreeHostImpl_h
#define CCLayerTreeHostImpl_h

#include "CCAnimationEvents.h"
#include "CCInputHandler.h"
#include "CCLayerSorter.h"
#include "CCRenderPass.h"
#include "CCRenderer.h"
#include "SkColor.h"
#include <public/WebCompositorOutputSurfaceClient.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CCActiveGestureAnimation;
class CCCompletionEvent;
class CCDebugRectHistory;
class CCFrameRateCounter;
class CCLayerImpl;
class CCLayerTreeHostImplTimeSourceAdapter;
class CCPageScaleAnimation;
class CCRenderPassDrawQuad;
class CCResourceProvider;
class LayerRendererChromium;
struct LayerRendererCapabilities;
struct CCRenderingStats;

// CCLayerTreeHost->CCProxy callback interface.
class CCLayerTreeHostImplClient {
public:
    virtual void didLoseContextOnImplThread() = 0;
    virtual void onSwapBuffersCompleteOnImplThread() = 0;
    virtual void onVSyncParametersChanged(double monotonicTimebase, double intervalInSeconds) = 0;
    virtual void setNeedsRedrawOnImplThread() = 0;
    virtual void setNeedsCommitOnImplThread() = 0;
    virtual void postAnimationEventsToMainThreadOnImplThread(PassOwnPtr<CCAnimationEventsVector>, double wallClockTime) = 0;
};

// CCLayerTreeHostImpl owns the CCLayerImpl tree as well as associated rendering state
class CCLayerTreeHostImpl : public CCInputHandlerClient,
                            public CCRendererClient,
                            public WebKit::WebCompositorOutputSurfaceClient {
    WTF_MAKE_NONCOPYABLE(CCLayerTreeHostImpl);
    typedef Vector<CCLayerImpl*> CCLayerList;

public:
    static PassOwnPtr<CCLayerTreeHostImpl> create(const CCLayerTreeSettings&, CCLayerTreeHostImplClient*);
    virtual ~CCLayerTreeHostImpl();

    // CCInputHandlerClient implementation
    virtual CCInputHandlerClient::ScrollStatus scrollBegin(const IntPoint&, CCInputHandlerClient::ScrollInputType) OVERRIDE;
    virtual void scrollBy(const IntSize&) OVERRIDE;
    virtual void scrollEnd() OVERRIDE;
    virtual void pinchGestureBegin() OVERRIDE;
    virtual void pinchGestureUpdate(float, const IntPoint&) OVERRIDE;
    virtual void pinchGestureEnd() OVERRIDE;
    virtual void startPageScaleAnimation(const IntSize& targetPosition, bool anchorPoint, float pageScale, double startTime, double duration) OVERRIDE;
    virtual CCActiveGestureAnimation* activeGestureAnimation() OVERRIDE { return m_activeGestureAnimation.get(); }
    // To clear an active animation, pass nullptr.
    virtual void setActiveGestureAnimation(PassOwnPtr<CCActiveGestureAnimation>) OVERRIDE;
    virtual void scheduleAnimation() OVERRIDE;

    struct FrameData {
        Vector<IntRect> occludingScreenSpaceRects;
        CCRenderPassList renderPasses;
        CCRenderPassIdHashMap renderPassesById;
        CCLayerList* renderSurfaceLayerList;
        CCLayerList willDrawLayers;
    };

    // Virtual for testing.
    virtual void beginCommit();
    virtual void commitComplete();
    virtual void animate(double monotonicTime, double wallClockTime);

    // Returns false if problems occured preparing the frame, and we should try
    // to avoid displaying the frame. If prepareToDraw is called,
    // didDrawAllLayers must also be called, regardless of whether drawLayers is
    // called between the two.
    virtual bool prepareToDraw(FrameData&);
    virtual void drawLayers(const FrameData&);
    // Must be called if and only if prepareToDraw was called.
    void didDrawAllLayers(const FrameData&);

    // CCRendererClient implementation
    virtual const IntSize& deviceViewportSize() const OVERRIDE { return m_deviceViewportSize; }
    virtual const CCLayerTreeSettings& settings() const OVERRIDE { return m_settings; }
    virtual void didLoseContext() OVERRIDE;
    virtual void onSwapBuffersComplete() OVERRIDE;
    virtual void setFullRootLayerDamage() OVERRIDE;
    virtual void releaseContentsTextures() OVERRIDE;
    virtual void setMemoryAllocationLimitBytes(size_t) OVERRIDE;

    // WebCompositorOutputSurfaceClient implementation.
    virtual void onVSyncParametersChanged(double monotonicTimebase, double intervalInSeconds) OVERRIDE;

    // Implementation
    bool canDraw();
    CCGraphicsContext* context() const;

    String layerTreeAsText() const;

    void finishAllRendering();
    int sourceAnimationFrameNumber() const;

    bool initializeLayerRenderer(PassOwnPtr<CCGraphicsContext>, TextureUploaderOption);
    bool isContextLost();
    CCRenderer* layerRenderer() { return m_layerRenderer.get(); }
    const LayerRendererCapabilities& layerRendererCapabilities() const;

    bool swapBuffers();

    void readback(void* pixels, const IntRect&);

    void setRootLayer(PassOwnPtr<CCLayerImpl>);
    CCLayerImpl* rootLayer() { return m_rootLayerImpl.get(); }

    // Release ownership of the current layer tree and replace it with an empty
    // tree. Returns the root layer of the detached tree.
    PassOwnPtr<CCLayerImpl> detachLayerTree();

    CCLayerImpl* rootScrollLayer() const { return m_rootScrollLayerImpl; }

    bool visible() const { return m_visible; }
    void setVisible(bool);

    int sourceFrameNumber() const { return m_sourceFrameNumber; }
    void setSourceFrameNumber(int frameNumber) { m_sourceFrameNumber = frameNumber; }

    bool contentsTexturesPurged() const { return m_contentsTexturesPurged; }
    void resetContentsTexturesPurged() { m_contentsTexturesPurged = false; }
    size_t memoryAllocationLimitBytes() const { return m_memoryAllocationLimitBytes; }

    void setViewportSize(const IntSize& layoutViewportSize, const IntSize& deviceViewportSize);
    const IntSize& layoutViewportSize() const { return m_layoutViewportSize; }

    float deviceScaleFactor() const { return m_deviceScaleFactor; }
    void setDeviceScaleFactor(float);

    float pageScale() const { return m_pageScale; }
    void setPageScaleFactorAndLimits(float pageScale, float minPageScale, float maxPageScale);

    PassOwnPtr<CCScrollAndScaleSet> processScrollDeltas();

    void startPageScaleAnimation(const IntSize& tragetPosition, bool useAnchor, float scale, double durationSec);

    SkColor backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(SkColor color) { m_backgroundColor = color; }

    bool hasTransparentBackground() const { return m_hasTransparentBackground; }
    void setHasTransparentBackground(bool transparent) { m_hasTransparentBackground = transparent; }

    bool needsAnimateLayers() const { return m_needsAnimateLayers; }
    void setNeedsAnimateLayers() { m_needsAnimateLayers = true; }

    void setNeedsRedraw();

    void renderingStats(CCRenderingStats&) const;

    CCFrameRateCounter* fpsCounter() const { return m_fpsCounter.get(); }
    CCDebugRectHistory* debugRectHistory() const { return m_debugRectHistory.get(); }
    CCResourceProvider* resourceProvider() const { return m_resourceProvider.get(); }

    class CullRenderPassesWithCachedTextures {
    public:
        bool shouldRemoveRenderPass(const CCRenderPassDrawQuad&, const FrameData&) const;

        // Iterates from the root first, in order to remove the surfaces closest
        // to the root with cached textures, and all surfaces that draw into
        // them.
        size_t renderPassListBegin(const CCRenderPassList& list) const { return list.size() - 1; }
        size_t renderPassListEnd(const CCRenderPassList&) const { return 0 - 1; }
        size_t renderPassListNext(size_t it) const { return it - 1; }

        CullRenderPassesWithCachedTextures(CCRenderer& renderer) : m_renderer(renderer) { }
    private:
        CCRenderer& m_renderer;
    };

    class CullRenderPassesWithNoQuads {
    public:
        bool shouldRemoveRenderPass(const CCRenderPassDrawQuad&, const FrameData&) const;

        // Iterates in draw order, so that when a surface is removed, and its
        // target becomes empty, then its target can be removed also.
        size_t renderPassListBegin(const CCRenderPassList&) const { return 0; }
        size_t renderPassListEnd(const CCRenderPassList& list) const { return list.size(); }
        size_t renderPassListNext(size_t it) const { return it + 1; }
    };

    template<typename RenderPassCuller>
    static void removeRenderPasses(RenderPassCuller, FrameData&);

protected:
    CCLayerTreeHostImpl(const CCLayerTreeSettings&, CCLayerTreeHostImplClient*);

    void animatePageScale(double monotonicTime);
    void animateGestures(double monotonicTime);
    void animateScrollbars(double monotonicTime);

    // Exposed for testing.
    void calculateRenderSurfaceLayerList(CCLayerList&);

    // Virtual for testing.
    virtual void animateLayers(double monotonicTime, double wallClockTime);

    // Virtual for testing. Measured in seconds.
    virtual double lowFrequencyAnimationInterval() const;

    CCLayerTreeHostImplClient* m_client;
    int m_sourceFrameNumber;

private:
    void computeDoubleTapZoomDeltas(CCScrollAndScaleSet* scrollInfo);
    void computePinchZoomDeltas(CCScrollAndScaleSet* scrollInfo);
    void makeScrollAndScaleSet(CCScrollAndScaleSet* scrollInfo, const IntSize& scrollOffset, float pageScale);

    void setPageScaleDelta(float);
    void updateMaxScrollPosition();
    void trackDamageForAllSurfaces(CCLayerImpl* rootDrawLayer, const CCLayerList& renderSurfaceLayerList);

    // Returns false if the frame should not be displayed. This function should
    // only be called from prepareToDraw, as didDrawAllLayers must be called
    // if this helper function is called.
    bool calculateRenderPasses(FrameData&);
    void animateLayersRecursive(CCLayerImpl*, double monotonicTime, double wallClockTime, CCAnimationEventsVector*, bool& didAnimate, bool& needsAnimateLayers);
    void setBackgroundTickingEnabled(bool);
    IntSize contentSize() const;

    void sendDidLoseContextRecursive(CCLayerImpl*);
    void clearRenderSurfaces();
    bool ensureRenderSurfaceLayerList();
    void clearCurrentlyScrollingLayer();

    void animateScrollbarsRecursive(CCLayerImpl*, double monotonicTime);

    void dumpRenderSurfaces(TextStream&, int indent, const CCLayerImpl*) const;

    OwnPtr<CCGraphicsContext> m_context;
    OwnPtr<CCResourceProvider> m_resourceProvider;
    OwnPtr<CCRenderer> m_layerRenderer;
    OwnPtr<CCLayerImpl> m_rootLayerImpl;
    CCLayerImpl* m_rootScrollLayerImpl;
    CCLayerImpl* m_currentlyScrollingLayerImpl;
    int m_scrollingLayerIdFromPreviousTree;
    CCLayerTreeSettings m_settings;
    IntSize m_layoutViewportSize;
    IntSize m_deviceViewportSize;
    float m_deviceScaleFactor;
    bool m_visible;
    bool m_contentsTexturesPurged;
    size_t m_memoryAllocationLimitBytes;

    float m_pageScale;
    float m_pageScaleDelta;
    float m_sentPageScaleDelta;
    float m_minPageScale, m_maxPageScale;

    SkColor m_backgroundColor;
    bool m_hasTransparentBackground;

    // If this is true, it is necessary to traverse the layer tree ticking the animators.
    bool m_needsAnimateLayers;
    bool m_pinchGestureActive;
    IntPoint m_previousPinchAnchor;

    OwnPtr<CCPageScaleAnimation> m_pageScaleAnimation;
    OwnPtr<CCActiveGestureAnimation> m_activeGestureAnimation;

    // This is used for ticking animations slowly when hidden.
    OwnPtr<CCLayerTreeHostImplTimeSourceAdapter> m_timeSourceClientAdapter;

    CCLayerSorter m_layerSorter;

    // List of visible layers for the most recently prepared frame. Used for
    // rendering and input event hit testing.
    CCLayerList m_renderSurfaceLayerList;

    OwnPtr<CCFrameRateCounter> m_fpsCounter;
    OwnPtr<CCDebugRectHistory> m_debugRectHistory;
};

};

#endif
