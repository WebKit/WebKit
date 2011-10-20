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
#include "cc/CCMainThread.h"
#include "cc/CCProxy.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class CCInputHandler;
class CCLayerTreeHost;
class CCScheduler;
class CCScopedMainThreadProxy;
class CCThread;
class CCThreadProxySchedulerClient;

class CCThreadProxy : public CCProxy, CCLayerTreeHostImplClient {
    friend class CCThreadProxySchedulerClient;
public:
    static void setThread(CCThread*);

    static PassOwnPtr<CCProxy> create(CCLayerTreeHost*);

    virtual ~CCThreadProxy();

    // CCProxy implementation
    virtual bool compositeAndReadback(void *pixels, const IntRect&);
    virtual GraphicsContext3D* context();
    virtual void finishAllRendering();
    virtual bool isStarted() const;
    virtual bool initializeLayerRenderer();
    virtual int compositorIdentifier() const;
    virtual const LayerRendererCapabilities& layerRendererCapabilities() const;
    virtual void loseCompositorContext(int numTimes);
    virtual void setNeedsAnimate();
    virtual void setNeedsCommit();
    virtual void setNeedsCommitThenRedraw();
    virtual void setNeedsRedraw();
    virtual void start();
    virtual void stop();

    // CCLayerTreeHostImplClient implementation
    virtual void setNeedsRedrawOnImplThread();
    virtual void setNeedsCommitOnImplThread();

private:
    explicit CCThreadProxy(CCLayerTreeHost*);

    // Called on CCMainThread
    void beginFrameAndCommit(int sequenceNumber, double frameBeginTime, PassOwnPtr<CCScrollUpdateSet>);

    // Called on CCThread
    void postBeginFrameAndCommitOnImplThread();
    PassOwnPtr<CCMainThread::Task> createBeginFrameAndCommitTaskOnImplThread();
    void obtainBeginFrameAndCommitTaskFromCCThread(CCCompletionEvent*, CCMainThread::Task**);
    void commitOnImplThread(CCCompletionEvent*);
    void drawLayersAndPresentOnImplThread();
    void drawLayersOnImplThread();
    void drawLayersAndReadbackOnImplThread(CCCompletionEvent*, bool* success, void* pixels, const IntRect&);
    void finishAllRenderingOnImplThread(CCCompletionEvent*);
    void initializeImplOnImplThread(CCCompletionEvent*);
    void initializeLayerRendererOnImplThread(GraphicsContext3D*, CCCompletionEvent*, bool* initializeSucceeded, LayerRendererCapabilities*, int* compositorIdentifier);
    void setNeedsAnimateOnImplThread();
    void setNeedsCommitThenRedrawOnImplThread();
    void layerTreeHostClosedOnImplThread(CCCompletionEvent*);

    // Accessed on main thread only.
    bool m_animationRequested;
    bool m_commitRequested;
    bool m_redrawAfterCommit;
    CCLayerTreeHost* m_layerTreeHost;
    int m_compositorIdentifier;
    LayerRendererCapabilities m_layerRendererCapabilitiesMainThreadCopy;
    bool m_started;
    int m_lastExecutedBeginFrameAndCommitSequenceNumber;

    // Used on the CCThread only
    OwnPtr<CCLayerTreeHostImpl> m_layerTreeHostImpl;
    int m_numBeginFrameAndCommitsIssuedOnImplThread;

    OwnPtr<CCInputHandler> m_inputHandlerOnImplThread;

    OwnPtr<CCScheduler> m_schedulerOnImplThread;
    OwnPtr<CCThreadProxySchedulerClient> m_schedulerClientOnImplThread;

    RefPtr<CCScopedMainThreadProxy> m_mainThreadProxy;

    static CCThread* s_ccThread;
};

}

#endif
