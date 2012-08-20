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

#ifndef CCThreadProxy_h
#define CCThreadProxy_h

#include "CCAnimationEvents.h"
#include "CCCompletionEvent.h"
#include "CCLayerTreeHostImpl.h"
#include "CCProxy.h"
#include "CCScheduler.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class CCInputHandler;
class CCLayerTreeHost;
class CCScheduler;
class CCScopedThreadProxy;
class CCTextureUpdateQueue;
class CCTextureUpdateController;
class CCThread;
class CCThreadProxyContextRecreationTimer;

class CCThreadProxy : public CCProxy, CCLayerTreeHostImplClient, CCSchedulerClient {
public:
    static PassOwnPtr<CCProxy> create(CCLayerTreeHost*);

    virtual ~CCThreadProxy();

    // CCProxy implementation
    virtual bool compositeAndReadback(void *pixels, const IntRect&) OVERRIDE;
    virtual void startPageScaleAnimation(const IntSize& targetPosition, bool useAnchor, float scale, double duration) OVERRIDE;
    virtual void finishAllRendering() OVERRIDE;
    virtual bool isStarted() const OVERRIDE;
    virtual bool initializeContext() OVERRIDE;
    virtual void setSurfaceReady() OVERRIDE;
    virtual void setVisible(bool) OVERRIDE;
    virtual bool initializeLayerRenderer() OVERRIDE;
    virtual bool recreateContext() OVERRIDE;
    virtual int compositorIdentifier() const OVERRIDE;
    virtual void implSideRenderingStats(CCRenderingStats&) OVERRIDE;
    virtual const LayerRendererCapabilities& layerRendererCapabilities() const OVERRIDE;
    virtual void loseContext() OVERRIDE;
    virtual void setNeedsAnimate() OVERRIDE;
    virtual void setNeedsCommit() OVERRIDE;
    virtual void setNeedsRedraw() OVERRIDE;
    virtual bool commitRequested() const OVERRIDE;
    virtual void didAddAnimation() OVERRIDE { }
    virtual void start() OVERRIDE;
    virtual void stop() OVERRIDE;
    virtual size_t maxPartialTextureUpdates() const OVERRIDE;
    virtual void acquireLayerTextures() OVERRIDE;
    virtual void forceSerializeOnSwapBuffers() OVERRIDE;

    // CCLayerTreeHostImplClient implementation
    virtual void didLoseContextOnImplThread() OVERRIDE;
    virtual void onSwapBuffersCompleteOnImplThread() OVERRIDE;
    virtual void onVSyncParametersChanged(double monotonicTimebase, double intervalInSeconds) OVERRIDE;
    virtual void setNeedsRedrawOnImplThread() OVERRIDE;
    virtual void setNeedsCommitOnImplThread() OVERRIDE;
    virtual void postAnimationEventsToMainThreadOnImplThread(PassOwnPtr<CCAnimationEventsVector>, double wallClockTime) OVERRIDE;

    // CCSchedulerClient implementation
    virtual bool canDraw() OVERRIDE;
    virtual bool hasMoreResourceUpdates() const OVERRIDE;
    virtual void scheduledActionBeginFrame() OVERRIDE;
    virtual CCScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapIfPossible() OVERRIDE;
    virtual CCScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapForced() OVERRIDE;
    virtual void scheduledActionUpdateMoreResources(double monotonicTimeLimit) OVERRIDE;
    virtual void scheduledActionCommit() OVERRIDE;
    virtual void scheduledActionBeginContextRecreation() OVERRIDE;
    virtual void scheduledActionAcquireLayerTexturesForMainThread() OVERRIDE;

private:
    explicit CCThreadProxy(CCLayerTreeHost*);
    friend class CCThreadProxyContextRecreationTimer;

    // Set on impl thread, read on main thread.
    struct BeginFrameAndCommitState {
        BeginFrameAndCommitState()
            : monotonicFrameBeginTime(0)
        {
        }

        double monotonicFrameBeginTime;
        OwnPtr<CCScrollAndScaleSet> scrollInfo;
        bool contentsTexturesWereDeleted;
        size_t memoryAllocationLimitBytes;
    };
    OwnPtr<BeginFrameAndCommitState> m_pendingBeginFrameRequest;

    // Called on main thread
    void beginFrame();
    void didCommitAndDrawFrame();
    void didCompleteSwapBuffers();
    void setAnimationEvents(PassOwnPtr<CCAnimationEventsVector>, double wallClockTime);
    void beginContextRecreation();
    void tryToRecreateContext();

    // Called on impl thread
    struct ReadbackRequest {
        CCCompletionEvent completion;
        bool success;
        void* pixels;
        IntRect rect;
    };
    void forceBeginFrameOnImplThread(CCCompletionEvent*);
    void beginFrameCompleteOnImplThread(CCCompletionEvent*, PassOwnPtr<CCTextureUpdateQueue>, bool contentsTexturesWereDeleted);
    void beginFrameAbortedOnImplThread();
    void requestReadbackOnImplThread(ReadbackRequest*);
    void requestStartPageScaleAnimationOnImplThread(IntSize targetPosition, bool useAnchor, float scale, double durationSec);
    void finishAllRenderingOnImplThread(CCCompletionEvent*);
    void initializeImplOnImplThread(CCCompletionEvent*);
    void setSurfaceReadyOnImplThread();
    void setVisibleOnImplThread(CCCompletionEvent*, bool);
    void initializeContextOnImplThread(CCGraphicsContext*);
    void initializeLayerRendererOnImplThread(CCCompletionEvent*, bool* initializeSucceeded, LayerRendererCapabilities*);
    void layerTreeHostClosedOnImplThread(CCCompletionEvent*);
    void setFullRootLayerDamageOnImplThread();
    void acquireLayerTexturesForMainThreadOnImplThread(CCCompletionEvent*);
    void recreateContextOnImplThread(CCCompletionEvent*, CCGraphicsContext*, bool* recreateSucceeded, LayerRendererCapabilities*);
    void implSideRenderingStatsOnImplThread(CCCompletionEvent*, CCRenderingStats*);
    CCScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapInternal(bool forcedDraw);
    void forceSerializeOnSwapBuffersOnImplThread(CCCompletionEvent*);
    void setNeedsForcedCommitOnImplThread();

    // Accessed on main thread only.
    bool m_animateRequested;
    bool m_commitRequested;
    bool m_forcedCommitRequested;
    OwnPtr<CCThreadProxyContextRecreationTimer> m_contextRecreationTimer;
    CCLayerTreeHost* m_layerTreeHost;
    int m_compositorIdentifier;
    bool m_layerRendererInitialized;
    LayerRendererCapabilities m_layerRendererCapabilitiesMainThreadCopy;
    bool m_started;
    bool m_texturesAcquired;
    bool m_inCompositeAndReadback;

    OwnPtr<CCLayerTreeHostImpl> m_layerTreeHostImpl;

    OwnPtr<CCInputHandler> m_inputHandlerOnImplThread;

    OwnPtr<CCScheduler> m_schedulerOnImplThread;

    RefPtr<CCScopedThreadProxy> m_mainThreadProxy;

    // Holds on to the context we might use for compositing in between initializeContext()
    // and initializeLayerRenderer() calls.
    OwnPtr<CCGraphicsContext> m_contextBeforeInitializationOnImplThread;

    // Set when the main thread is waiting on a scheduledActionBeginFrame to be issued.
    CCCompletionEvent* m_beginFrameCompletionEventOnImplThread;

    // Set when the main thread is waiting on a readback.
    ReadbackRequest* m_readbackRequestOnImplThread;

    // Set when the main thread is waiting on a commit to complete.
    CCCompletionEvent* m_commitCompletionEventOnImplThread;

    // Set when the main thread is waiting on layers to be drawn.
    CCCompletionEvent* m_textureAcquisitionCompletionEventOnImplThread;

    OwnPtr<CCTextureUpdateController> m_currentTextureUpdateControllerOnImplThread;

    // Set when we need to reset the contentsTexturesPurged flag after the
    // commit.
    bool m_resetContentsTexturesPurgedAfterCommitOnImplThread;

    // Set when the next draw should post didCommitAndDrawFrame to the main thread.
    bool m_nextFrameIsNewlyCommittedFrameOnImplThread;

    bool m_renderVSyncEnabled;
};

}

#endif
