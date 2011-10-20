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

#include "config.h"

#include "cc/CCThreadProxy.h"

#include "GraphicsContext3D.h"
#include "TraceEvent.h"
#include "cc/CCInputHandler.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCMainThreadTask.h"
#include "cc/CCScheduler.h"
#include "cc/CCScopedMainThreadProxy.h"
#include "cc/CCScrollController.h"
#include "cc/CCThreadTask.h"
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>

using namespace WTF;

namespace WebCore {

CCThread* CCThreadProxy::s_ccThread = 0;

void CCThreadProxy::setThread(CCThread* ccThread)
{
    s_ccThread = ccThread;
#ifndef NDEBUG
    CCProxy::setImplThread(s_ccThread->threadID());
#endif
}

class CCThreadProxySchedulerClient : public CCSchedulerClient {
public:
    static PassOwnPtr<CCThreadProxySchedulerClient> create(CCThreadProxy* proxy)
    {
        return adoptPtr(new CCThreadProxySchedulerClient(proxy));
    }
    virtual ~CCThreadProxySchedulerClient() { }

    virtual void scheduleBeginFrameAndCommit()
    {
        m_proxy->postBeginFrameAndCommitOnImplThread();
    }

    virtual void scheduleDrawAndSwap()
    {
        m_proxy->drawLayersAndSwapOnImplThread();
    }

private:
    explicit CCThreadProxySchedulerClient(CCThreadProxy* proxy)
    {
        m_proxy = proxy;
    }

    CCThreadProxy* m_proxy;
};

PassOwnPtr<CCProxy> CCThreadProxy::create(CCLayerTreeHost* layerTreeHost)
{
    return adoptPtr(new CCThreadProxy(layerTreeHost));
}

CCThreadProxy::CCThreadProxy(CCLayerTreeHost* layerTreeHost)
    : m_commitRequested(false)
    , m_layerTreeHost(layerTreeHost)
    , m_compositorIdentifier(-1)
    , m_started(false)
    , m_lastExecutedBeginFrameAndCommitSequenceNumber(-1)
    , m_numBeginFrameAndCommitsIssuedOnImplThread(0)
    , m_mainThreadProxy(CCScopedMainThreadProxy::create())
{
    TRACE_EVENT("CCThreadProxy::CCThreadProxy", this, 0);
    ASSERT(isMainThread());
}

CCThreadProxy::~CCThreadProxy()
{
    TRACE_EVENT("CCThreadProxy::~CCThreadProxy", this, 0);
    ASSERT(isMainThread());
    ASSERT(!m_started);
}

bool CCThreadProxy::compositeAndReadback(void *pixels, const IntRect& rect)
{
    TRACE_EVENT("CCThreadPRoxy::compositeAndReadback", this, 0);
    ASSERT(isMainThread());
    ASSERT(m_layerTreeHost);

    // If a commit is pending, perform the commit first.
    if (m_commitRequested)  {
        // This bit of code is uglier than it should be because returning
        // pointers via the CCThread task model is really messy. Effectively, we
        // are making a blocking call to createBeginFrameAndCommitTaskOnImplThread,
        // and trying to get the CCMainThread::Task it returns so we can run it.
        OwnPtr<CCMainThread::Task> beginFrameAndCommitTask;
        {
            CCMainThread::Task* taskPtr = 0;
            CCCompletionEvent completion;
            s_ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::obtainBeginFrameAndCommitTaskFromCCThread, AllowCrossThreadAccess(&completion), AllowCrossThreadAccess(&taskPtr)));
            completion.wait();
            beginFrameAndCommitTask = adoptPtr(taskPtr);
        }

        beginFrameAndCommitTask->performTask();
    }

    // Draw using the new tree and read back the results.
    bool success = false;
    CCCompletionEvent completion;
    s_ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::drawLayersAndReadbackOnImplThread, AllowCrossThreadAccess(&completion), AllowCrossThreadAccess(&success), AllowCrossThreadAccess(pixels), rect));
    completion.wait();
    return success;
}

void CCThreadProxy::drawLayersAndReadbackOnImplThread(CCCompletionEvent* completion, bool* success, void* pixels, const IntRect& rect)
{
    ASSERT(CCProxy::isImplThread());
    if (!m_layerTreeHostImpl) {
        *success = false;
        completion->signal();
        return;
    }
    drawLayersOnImplThread();
    m_layerTreeHostImpl->readback(pixels, rect);
    bool lost = m_layerTreeHostImpl->isContextLost();
    if (lost)
        m_schedulerOnImplThread->didSwapBuffersAbort();
    *success = !lost;
    completion->signal();
}

GraphicsContext3D* CCThreadProxy::context()
{
    return 0;
}

void CCThreadProxy::finishAllRendering()
{
    ASSERT(CCProxy::isMainThread());

    // Make sure all GL drawing is finished on the impl thread.
    CCCompletionEvent completion;
    s_ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::finishAllRenderingOnImplThread, AllowCrossThreadAccess(&completion)));
    completion.wait();
}

bool CCThreadProxy::isStarted() const
{
    ASSERT(CCProxy::isMainThread());
    return m_started;
}

bool CCThreadProxy::initializeLayerRenderer()
{
    TRACE_EVENT("CCThreadProxy::initializeLayerRenderer", this, 0);
    RefPtr<GraphicsContext3D> context = m_layerTreeHost->createLayerTreeHostContext3D();
    if (!context)
        return false;
    ASSERT(context->hasOneRef());

    // Leak the context pointer so we can transfer ownership of it to the other side...
    GraphicsContext3D* contextPtr = context.release().leakRef();
    ASSERT(contextPtr->hasOneRef());

    // Make a blocking call to initializeLayerRendererOnImplThread. The results of that call
    // are pushed into the initializeSucceeded and capabilities local variables.
    CCCompletionEvent completion;
    bool initializeSucceeded = false;
    LayerRendererCapabilities capabilities;
    s_ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::initializeLayerRendererOnImplThread,
                                          AllowCrossThreadAccess(contextPtr), AllowCrossThreadAccess(&completion),
                                          AllowCrossThreadAccess(&initializeSucceeded), AllowCrossThreadAccess(&capabilities),
                                          AllowCrossThreadAccess(&m_compositorIdentifier)));
    completion.wait();

    if (initializeSucceeded)
        m_layerRendererCapabilitiesMainThreadCopy = capabilities;
    return initializeSucceeded;
}

int CCThreadProxy::compositorIdentifier() const
{
    ASSERT(isMainThread());
    return m_compositorIdentifier;
}

const LayerRendererCapabilities& CCThreadProxy::layerRendererCapabilities() const
{
    return m_layerRendererCapabilitiesMainThreadCopy;
}

void CCThreadProxy::loseCompositorContext(int numTimes)
{
    ASSERT_NOT_REACHED();
}

void CCThreadProxy::setNeedsAnimate()
{
    ASSERT(isMainThread());
    if (m_commitRequested)
        return;

    TRACE_EVENT("CCThreadProxy::setNeedsAnimation", this, 0);
    m_commitRequested = true;
    s_ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::setNeedsAnimateOnImplThread));
}


void CCThreadProxy::setNeedsCommit()
{
    ASSERT(isMainThread());
    if (m_commitRequested)
        return;

    TRACE_EVENT("CCThreadProxy::setNeedsCommit", this, 0);
    m_commitRequested = true;
    s_ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::setNeedsCommitOnImplThread));
}

void CCThreadProxy::setNeedsCommitThenRedraw()
{
    ASSERT(isMainThread());
    m_redrawAfterCommit = true;
    setNeedsCommit();
}

void CCThreadProxy::setNeedsAnimateOnImplThread()
{
    ASSERT(isImplThread());
    TRACE_EVENT("CCThreadProxy::setNeedsCommitOnImplThread", this, 0);
    m_schedulerOnImplThread->requestAnimate();
}

void CCThreadProxy::onSwapBuffersCompleteOnImplThread()
{
    ASSERT(isImplThread());
    TRACE_EVENT("CCThreadProxy::onSwapBuffersCompleteOnImplThread", this, 0);
    m_schedulerOnImplThread->didSwapBuffersComplete();
}

void CCThreadProxy::setNeedsCommitOnImplThread()
{
    ASSERT(isImplThread());
    TRACE_EVENT("CCThreadProxy::setNeedsCommitOnImplThread", this, 0);
    m_schedulerOnImplThread->requestCommit();
}

void CCThreadProxy::setNeedsRedraw()
{
    ASSERT(isMainThread());
    TRACE_EVENT("CCThreadProxy::setNeedsRedraw", this, 0);
    s_ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::setNeedsRedrawOnImplThread));
}

void CCThreadProxy::setNeedsRedrawOnImplThread()
{
    ASSERT(isImplThread());
    TRACE_EVENT("CCThreadProxy::setNeedsRedrawOnImplThread", this, 0);
    m_schedulerOnImplThread->requestRedraw();
}

void CCThreadProxy::start()
{
    ASSERT(isMainThread());
    ASSERT(s_ccThread);
    // Create LayerTreeHostImpl.
    CCCompletionEvent completion;
    s_ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::initializeImplOnImplThread, AllowCrossThreadAccess(&completion)));
    completion.wait();

    m_started = true;
}

void CCThreadProxy::stop()
{
    TRACE_EVENT("CCThreadProxy::stop", this, 0);
    ASSERT(isMainThread());
    ASSERT(m_started);

    // Synchronously deletes the impl.
    CCCompletionEvent completion;
    s_ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::layerTreeHostClosedOnImplThread, AllowCrossThreadAccess(&completion)));
    completion.wait();

    m_mainThreadProxy->shutdown(); // Stop running tasks posted to us.

    ASSERT(!m_layerTreeHostImpl); // verify that the impl deleted.
    m_layerTreeHost = 0;
    m_started = false;
}

void CCThreadProxy::finishAllRenderingOnImplThread(CCCompletionEvent* completion)
{
    TRACE_EVENT("CCThreadProxy::finishAllRenderingOnImplThread", this, 0);
    ASSERT(isImplThread());
    if (m_schedulerOnImplThread->redrawPending()) {
        drawLayersOnImplThread();
        m_layerTreeHostImpl->swapBuffers();
        m_schedulerOnImplThread->didDrawAndSwap();
    }
    m_layerTreeHostImpl->finishAllRendering();
    completion->signal();
}

void CCThreadProxy::postBeginFrameAndCommitOnImplThread()
{
    m_mainThreadProxy->postTask(createBeginFrameAndCommitTaskOnImplThread());
}

void CCThreadProxy::obtainBeginFrameAndCommitTaskFromCCThread(CCCompletionEvent* completion, CCMainThread::Task** taskPtr)
{
    OwnPtr<CCMainThread::Task> task = createBeginFrameAndCommitTaskOnImplThread();
    *taskPtr = task.leakPtr();
    completion->signal();
}

PassOwnPtr<CCMainThread::Task> CCThreadProxy::createBeginFrameAndCommitTaskOnImplThread()
{
    TRACE_EVENT("CCThreadProxy::createBeginFrameAndCommitTaskOnImplThread", this, 0);
    ASSERT(isImplThread());
    double frameBeginTime = currentTime();

    // NOTE, it is possible to receieve a request for a
    // beginFrameAndCommitOnImplThread from finishAllRendering while a
    // beginFrameAndCommitOnImplThread is enqueued. Since CCMainThread doesn't
    // provide a threadsafe way to cancel tasks, it is important that
    // beginFrameAndCommit be structured to understand that it may get called at
    // a point that it shouldn't. We do this by assigning a sequence number to
    // every new beginFrameAndCommit task. Then, beginFrameAndCommit tracks the
    // last executed sequence number, dropping beginFrameAndCommit with sequence
    // numbers below the last executed one.
    int thisTaskSequenceNumber = m_numBeginFrameAndCommitsIssuedOnImplThread;
    m_numBeginFrameAndCommitsIssuedOnImplThread++;
    OwnPtr<CCScrollUpdateSet> scrollInfo = m_layerTreeHostImpl->processScrollDeltas();
    return createMainThreadTask(this, &CCThreadProxy::beginFrameAndCommit, thisTaskSequenceNumber, frameBeginTime, scrollInfo.release());
}

void CCThreadProxy::beginFrameAndCommit(int sequenceNumber, double frameBeginTime, PassOwnPtr<CCScrollUpdateSet> scrollInfo)
{
    TRACE_EVENT("CCThreadProxy::beginFrameAndCommit", this, 0);
    ASSERT(isMainThread());
    if (!m_layerTreeHost)
        return;

    // Scroll deltas need to be applied even if the commit will be dropped.
    m_layerTreeHost->applyScrollDeltas(*scrollInfo.get());

    // Drop beginFrameAndCommit calls that occur out of sequence. See createBeginFrameAndCommitTaskOnImplThread for
    // an explanation of how out-of-sequence beginFrameAndCommit tasks can occur.
    if (sequenceNumber < m_lastExecutedBeginFrameAndCommitSequenceNumber) {
        TRACE_EVENT("EarlyOut_StaleBeginFrameAndCommit", this, 0);
        return;
    }
    m_lastExecutedBeginFrameAndCommitSequenceNumber = sequenceNumber;

    // FIXME: recreate the context if it was requested by the impl thread
    {
        TRACE_EVENT("CCLayerTreeHost::animateAndLayout", this, 0);
        m_layerTreeHost->animateAndLayout(frameBeginTime);
    }

    ASSERT(m_lastExecutedBeginFrameAndCommitSequenceNumber == sequenceNumber);

    // Clear the commit flag after animateAndLayout here --- objects that only
    // layout when painted will trigger another setNeedsCommit inside
    // updateLayers.
    m_commitRequested = false;

    m_layerTreeHost->updateLayers();

    {
        // Blocking call to CCThreadProxy::commitOnImplThread
        TRACE_EVENT("commit", this, 0);
        CCCompletionEvent completion;
        s_ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::commitOnImplThread, AllowCrossThreadAccess(&completion)));
        completion.wait();
    }

    m_layerTreeHost->commitComplete();

    if (m_redrawAfterCommit)
        setNeedsRedraw();
    m_redrawAfterCommit = false;

    ASSERT(m_lastExecutedBeginFrameAndCommitSequenceNumber == sequenceNumber);
}

void CCThreadProxy::commitOnImplThread(CCCompletionEvent* completion)
{
    TRACE_EVENT("CCThreadProxy::beginFrameAndCommitOnImplThread", this, 0);
    ASSERT(isImplThread());
    ASSERT(m_schedulerOnImplThread->commitPending());
    if (!m_layerTreeHostImpl) {
        completion->signal();
        return;
    }
    m_layerTreeHostImpl->beginCommit();
    m_layerTreeHost->commitToOnImplThread(m_layerTreeHostImpl.get());
    m_layerTreeHostImpl->commitComplete();

    completion->signal();

    m_schedulerOnImplThread->didCommit();
}

void CCThreadProxy::drawLayersAndSwapOnImplThread()
{
    TRACE_EVENT("CCThreadProxy::drawLayersAndSwapOnImplThread", this, 0);
    ASSERT(isImplThread());
    if (!m_layerTreeHostImpl)
        return;

    drawLayersOnImplThread();
    m_layerTreeHostImpl->swapBuffers();
    m_schedulerOnImplThread->didDrawAndSwap();
}

void CCThreadProxy::drawLayersOnImplThread()
{
    TRACE_EVENT("CCThreadProxy::drawLayersOnImplThread", this, 0);
    ASSERT(isImplThread());
    ASSERT(m_layerTreeHostImpl);

    m_layerTreeHostImpl->drawLayers();
    // FIXME: handle case where m_layerTreeHostImpl->isContextLost.
    // FIXME: pass didSwapBuffersAbort if m_layerTreeHostImpl->isContextLost.
    ASSERT(!m_layerTreeHostImpl->isContextLost());
}

void CCThreadProxy::initializeImplOnImplThread(CCCompletionEvent* completion)
{
    TRACE_EVENT("CCThreadProxy::initializeImplOnImplThread", this, 0);
    ASSERT(isImplThread());
    m_layerTreeHostImpl = m_layerTreeHost->createLayerTreeHostImpl(this);
    m_schedulerClientOnImplThread = CCThreadProxySchedulerClient::create(this);
    m_schedulerOnImplThread = CCScheduler::create(m_schedulerClientOnImplThread.get());
    completion->signal();
}

void CCThreadProxy::initializeLayerRendererOnImplThread(GraphicsContext3D* contextPtr, CCCompletionEvent* completion, bool* initializeSucceeded, LayerRendererCapabilities* capabilities, int* compositorIdentifier)
{
    TRACE_EVENT("CCThreadProxy::initializeLayerRendererOnImplThread", this, 0);
    ASSERT(isImplThread());
    RefPtr<GraphicsContext3D> context(adoptRef(contextPtr));
    *initializeSucceeded = m_layerTreeHostImpl->initializeLayerRenderer(context);
    if (*initializeSucceeded)
        *capabilities = m_layerTreeHostImpl->layerRendererCapabilities();

    m_inputHandlerOnImplThread = CCInputHandler::create(m_layerTreeHostImpl.get());
    *compositorIdentifier = m_inputHandlerOnImplThread->identifier();

    completion->signal();
}

void CCThreadProxy::layerTreeHostClosedOnImplThread(CCCompletionEvent* completion)
{
    TRACE_EVENT("CCThreadProxy::layerTreeHostClosedOnImplThread", this, 0);
    ASSERT(isImplThread());
    m_layerTreeHost->deleteContentsTexturesOnImplThread(m_layerTreeHostImpl->contentsTextureAllocator());
    m_layerTreeHostImpl.clear();
    m_inputHandlerOnImplThread.clear();
    completion->signal();
}

} // namespace WebCore
