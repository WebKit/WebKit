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

#include "cc/CCCompletionEvent.h"
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCProxy.h"
#include "cc/CCScheduler.h"
#include "cc/CCThread.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class CCInputHandler;
class CCLayerTreeHost;
class CCScheduler;
class CCScopedThreadProxy;
class CCTextureUpdater;
class CCThread;

class CCThreadProxy : public CCProxy, CCLayerTreeHostImplClient, CCSchedulerClient {
public:
    static PassOwnPtr<CCProxy> create(CCLayerTreeHost*);

    virtual ~CCThreadProxy();

    // CCProxy implementation
    virtual bool compositeAndReadback(void *pixels, const IntRect&);
    virtual GraphicsContext3D* context();
    virtual void finishAllRendering();
    virtual bool isStarted() const;
    virtual bool initializeContext();
    virtual bool initializeLayerRenderer();
    virtual int compositorIdentifier() const;
    virtual const LayerRendererCapabilities& layerRendererCapabilities() const;
    virtual void loseCompositorContext(int numTimes);
    virtual void setNeedsAnimate();
    virtual void setNeedsCommit();
    virtual void setNeedsRedraw();
    virtual void setVisible(bool);
    virtual void start();
    virtual void stop();
    virtual bool partialTextureUpdateCapability() const;

    // CCLayerTreeHostImplClient implementation
    virtual void onSwapBuffersCompleteOnImplThread();
    virtual void setNeedsRedrawOnImplThread();
    virtual void setNeedsCommitOnImplThread();

    // CCSchedulerClient implementation
    virtual bool canDraw();
    virtual bool hasMoreResourceUpdates() const;
    virtual void scheduledActionBeginFrame();
    virtual void scheduledActionDrawAndSwap();
    virtual void scheduledActionUpdateMoreResources();
    virtual void scheduledActionCommit();

private:
    explicit CCThreadProxy(CCLayerTreeHost*);

    // Called on main thread
    void beginFrameAndCommit(int sequenceNumber, double frameBeginTime, PassOwnPtr<CCScrollAndScaleSet>);
    void didCommitAndDrawFrame();
    void didCompleteSwapBuffers();

    // Called on impl thread
    struct ReadbackRequest {
        CCCompletionEvent completion;
        bool success;
        void* pixels;
        IntRect rect;
    };
    PassOwnPtr<CCThread::Task> createBeginFrameAndCommitTaskOnImplThread();
    void obtainBeginFrameAndCommitTaskFromCCThread(CCCompletionEvent*, CCThread::Task**);
    void beginFrameCompleteOnImplThread(CCCompletionEvent*);
    void requestReadbackOnImplThread(ReadbackRequest*);
    void finishAllRenderingOnImplThread(CCCompletionEvent*);
    void initializeImplOnImplThread(CCCompletionEvent*);
    void initializeContextOnImplThread(GraphicsContext3D*);
    void initializeLayerRendererOnImplThread(CCCompletionEvent*, bool* initializeSucceeded, LayerRendererCapabilities*);
    void setVisibleOnImplThread(CCCompletionEvent*, bool visible);
    void layerTreeHostClosedOnImplThread(CCCompletionEvent*);

    // Accessed on main thread only.
    bool m_animateRequested;
    bool m_commitRequested;
    CCLayerTreeHost* m_layerTreeHost;
    int m_compositorIdentifier;
    bool m_layerRendererInitialized;
    LayerRendererCapabilities m_layerRendererCapabilitiesMainThreadCopy;
    bool m_started;
    int m_lastExecutedBeginFrameAndCommitSequenceNumber;

    // Used on the CCThread only.
    OwnPtr<CCLayerTreeHostImpl> m_layerTreeHostImpl;
    int m_numBeginFrameAndCommitsIssuedOnImplThread;

    OwnPtr<CCInputHandler> m_inputHandlerOnImplThread;

    OwnPtr<CCScheduler> m_schedulerOnImplThread;

    RefPtr<CCScopedThreadProxy> m_mainThreadProxy;

    // Holds on to the GraphicsContext3D we might use for compositing in between initializeContext()
    // and initializeLayerRenderer() calls.
    RefPtr<GraphicsContext3D> m_contextBeforeInitializationOnImplThread;

    // Set when the main thread is waiing on a readback.
    ReadbackRequest* m_readbackRequestOnImplThread;

    // Set when the main thread is waiting on a finishAllRendering call.
    CCCompletionEvent* m_finishAllRenderingCompletionEventOnImplThread;

    // Set when the main thread is waiting on a commit to complete.
    CCCompletionEvent* m_commitCompletionEventOnImplThread;
    OwnPtr<CCTextureUpdater> m_currentTextureUpdaterOnImplThread;

    // Set when the next draw should post didCommitAndDrawFrame to the main thread.
    bool m_nextFrameIsNewlyCommittedFrameOnImplThread;
};

}

#endif
