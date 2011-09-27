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
#include "cc/CCLayerTreeHost.h"
#include "cc/CCMainThreadTask.h"
#include "cc/CCThreadTask.h"
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>

using namespace WTF;

namespace WebCore {

namespace {
CCThread* ccThread;
int numProxies = 0;
}

PassOwnPtr<CCProxy> CCThreadProxy::create(CCLayerTreeHost* layerTreeHost)
{
    return adoptPtr(new CCThreadProxy(layerTreeHost));
}

CCThreadProxy::CCThreadProxy(CCLayerTreeHost* layerTreeHost)
    : m_commitRequested(false)
    , m_layerTreeHost(layerTreeHost)
    , m_started(false)
    , m_lastExecutedBeginFrameAndCommitSequenceNumber(-1)
    , m_numBeginFrameAndCommitsIssuedOnCCThread(0)
    , m_beginFrameAndCommitPendingOnCCThread(false)
    , m_drawTaskPostedOnCCThread(false)
    , m_redrawRequestedOnCCThread(false)
{
    TRACE_EVENT("CCThreadProxy::CCThreadProxy", this, 0);
    ASSERT(isMainThread());
    numProxies++;
    if (!ccThread) {
        ccThread = m_layerTreeHost->createCompositorThread().leakPtr();
#ifndef NDEBUG
        CCProxy::setImplThread(ccThread->threadID());
#endif
    }
}

CCThreadProxy::~CCThreadProxy()
{
    TRACE_EVENT("CCThreadProxy::~CCThreadProxy", this, 0);
    ASSERT(isMainThread());
    ASSERT(!m_started);

    numProxies--;
    if (!numProxies) {
        delete ccThread;
        ccThread = 0;
    }
}

bool CCThreadProxy::compositeAndReadback(void *pixels, const IntRect& rect)
{
    ASSERT(isMainThread());
    ASSERT(m_layerTreeHost);

    // If a commit is pending, perform the commit first.
    if (m_commitRequested)  {
        // This bit of code is uglier than it should be because returning
        // pointers via the CCThread task model is really messy. Effectively, we
        // are making a blocking call to createBeginFrameAndCommitTaskOnCCThread,
        // and trying to get the CCMainThread::Task it returns so we can run it.
        OwnPtr<CCMainThread::Task> beginFrameAndCommitTask;
        {
            CCMainThread::Task* taskPtr = 0;
            CCCompletionEvent completion;
            ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::obtainBeginFrameAndCommitTaskFromCCThread, AllowCrossThreadAccess(&completion), AllowCrossThreadAccess(&taskPtr)));
            completion.wait();
            beginFrameAndCommitTask = adoptPtr(taskPtr);
        }

        beginFrameAndCommitTask->performTask();
    }

    // Draw using the new tree and read back the results.
    bool success = false;
    CCCompletionEvent completion;
    ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::drawLayersAndReadbackOnCCThread, AllowCrossThreadAccess(&completion), AllowCrossThreadAccess(&success), AllowCrossThreadAccess(pixels), rect));
    completion.wait();
    return success;
}

void CCThreadProxy::drawLayersAndReadbackOnCCThread(CCCompletionEvent* completion, bool* success, void* pixels, const IntRect& rect)
{
    ASSERT(CCProxy::isImplThread());
    if (!m_layerTreeHostImpl) {
        *success = false;
        completion->signal();
        return;
    }
    drawLayersOnCCThread();
    m_layerTreeHostImpl->readback(pixels, rect);
    *success = m_layerTreeHostImpl->isContextLost();
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
    ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::finishAllRenderingOnCCThread, AllowCrossThreadAccess(&completion)));
    completion.wait();
}

bool CCThreadProxy::isStarted() const
{
    ASSERT(CCProxy::isMainThread());
    return m_started;
}

bool CCThreadProxy::initializeLayerRenderer()
{
    RefPtr<GraphicsContext3D> context = m_layerTreeHost->createLayerTreeHostContext3D();
    if (!context)
        return false;
    ASSERT(context->hasOneRef());

    // Leak the context pointer so we can transfer ownership of it to the other side...
    GraphicsContext3D* contextPtr = context.release().leakRef();
    ASSERT(contextPtr->hasOneRef());

    // Make a blocking call to initializeLayerRendererOnCCThread. The results of that call
    // are pushed into the initializeSucceeded and capabilities local variables.
    CCCompletionEvent completion;
    bool initializeSucceeded = false;
    LayerRendererCapabilities capabilities;
    ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::initializeLayerRendererOnCCThread,
                                          AllowCrossThreadAccess(contextPtr), AllowCrossThreadAccess(&completion), AllowCrossThreadAccess(&initializeSucceeded), AllowCrossThreadAccess(&capabilities)));
    completion.wait();

    if (initializeSucceeded)
        m_layerRendererCapabilitiesMainThreadCopy = capabilities;
    return initializeSucceeded;
}

const LayerRendererCapabilities& CCThreadProxy::layerRendererCapabilities() const
{
    return m_layerRendererCapabilitiesMainThreadCopy;
}

void CCThreadProxy::loseCompositorContext(int numTimes)
{
    ASSERT_NOT_REACHED();
}

void CCThreadProxy::setNeedsCommit()
{
    ASSERT(isMainThread());
    if (m_commitRequested)
        return;

    TRACE_EVENT("CCThreadProxy::setNeedsCommit", this, 0);
    m_commitRequested = true;
    ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::updateSchedulerStateOnCCThread, m_commitRequested, true));
}

void CCThreadProxy::setNeedsCommitAndRedraw()
{
    ASSERT(isMainThread());
    if (m_commitRequested)
        return;
    m_commitRequested = true;

    TRACE_EVENT("CCThreadProxy::setNeedsCommitAndRedraw", this, 0);
    ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::updateSchedulerStateOnCCThread, m_commitRequested, true));
}

void CCThreadProxy::setNeedsRedraw()
{
    ASSERT(isMainThread());
    if (m_commitRequested) // Implies that a commit is in flight.
        return;
    // Unlike setNeedsCommit that tracks whether a commit message has been sent,
    // setNeedsRedraw always sends a message to the compositor thread. This is
    // because the compositor thread can draw without telling the main
    // thread. This should not be much of a problem because calls to
    // setNeedsRedraw messages are uncommon (only triggered by WM_PAINT/etc),
    // compared to setNeedsCommitAndRedraw messages.
    TRACE_EVENT("CCThreadProxy::setNeedsRedraw", this, 0);
    ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::updateSchedulerStateOnCCThread, false, true));
}

void CCThreadProxy::start()
{
    ASSERT(isMainThread());
    // Create LayerTreeHostImpl.
    CCCompletionEvent completion;
    ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::initializeImplOnCCThread, AllowCrossThreadAccess(&completion)));
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
    ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::layerTreeHostClosedOnCCThread, AllowCrossThreadAccess(&completion)));
    completion.wait();

    ASSERT(!m_layerTreeHostImpl); // verify that the impl deleted.
    m_layerTreeHost = 0;
    m_started = false;
}

void CCThreadProxy::finishAllRenderingOnCCThread(CCCompletionEvent* completion)
{
    TRACE_EVENT("CCThreadProxy::finishAllRenderingOnCCThread", this, 0);
    ASSERT(isImplThread());
    ASSERT(!m_beginFrameAndCommitPendingOnCCThread);
    if (m_redrawRequestedOnCCThread) {
        drawLayersOnCCThread();
        m_layerTreeHostImpl->present();
        m_redrawRequestedOnCCThread = false;
    }
    m_layerTreeHostImpl->finishAllRendering();
    completion->signal();
}

void CCThreadProxy::obtainBeginFrameAndCommitTaskFromCCThread(CCCompletionEvent* completion, CCMainThread::Task** taskPtr)
{
    OwnPtr<CCMainThread::Task> task = createBeginFrameAndCommitTaskOnCCThread();
    *taskPtr = task.leakPtr();
    completion->signal();
}

PassOwnPtr<CCMainThread::Task> CCThreadProxy::createBeginFrameAndCommitTaskOnCCThread()
{
    TRACE_EVENT("CCThreadProxy::createBeginFrameAndCommitTaskOnCCThread", this, 0);
    ASSERT(isImplThread());
    double frameBeginTime = currentTime();
    m_beginFrameAndCommitPendingOnCCThread = true;

    // NOTE, it is possible to receieve a request for a
    // beginFrameAndCommitOnCCThread from finishAllRendering while a
    // beginFrameAndCommitOnCCThread is enqueued. Since CCMainThread doesn't
    // provide a threadsafe way to cancel tasks, it is important that
    // beginFrameAndCommit be structured to understand that it may get called at
    // a point that it shouldn't. We do this by assigning a sequence number to
    // every new beginFrameAndCommit task. Then, beginFrameAndCommit tracks the
    // last executed sequence number, dropping beginFrameAndCommit with sequence
    // numbers below the last executed one.
    int thisTaskSequenceNumber = m_numBeginFrameAndCommitsIssuedOnCCThread;
    m_numBeginFrameAndCommitsIssuedOnCCThread++;
    return createMainThreadTask(this, &CCThreadProxy::beginFrameAndCommit, thisTaskSequenceNumber, frameBeginTime);
}

void CCThreadProxy::beginFrameAndCommit(int sequenceNumber, double frameBeginTime)
{
    TRACE_EVENT("CCThreadProxy::beginFrameAndCommit", this, 0);
    ASSERT(isMainThread());
    if (!m_layerTreeHost)
        return;

    // Drop beginFrameAndCommit calls that occur out of sequence. See createBeginFrameAndCommitTaskOnCCThread for
    // an explanation of how out-of-sequence beginFrameAndCommit tasks can occur.
    if (sequenceNumber < m_lastExecutedBeginFrameAndCommitSequenceNumber) {
        TRACE_EVENT("EarlyOut_StaleBeginFrameAndCommit", this, 0);
        return;
    }
    m_lastExecutedBeginFrameAndCommitSequenceNumber = sequenceNumber;

    ASSERT(m_commitRequested);

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
        // Blocking call to CCThreadProxy::commitOnCCThread
        TRACE_EVENT("commit", this, 0);
        CCCompletionEvent completion;
        ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::commitOnCCThread, AllowCrossThreadAccess(&completion)));
        completion.wait();
    }

    m_layerTreeHost->commitComplete();

    ASSERT(m_lastExecutedBeginFrameAndCommitSequenceNumber == sequenceNumber);
}

void CCThreadProxy::commitOnCCThread(CCCompletionEvent* completion)
{
    TRACE_EVENT("CCThreadProxy::beginFrameAndCommitOnCCThread", this, 0);
    ASSERT(isImplThread());
    ASSERT(m_beginFrameAndCommitPendingOnCCThread);
    m_beginFrameAndCommitPendingOnCCThread = false;
    if (!m_layerTreeHostImpl) {
        completion->signal();
        return;
    }
    m_layerTreeHostImpl->beginCommit();
    m_layerTreeHost->commitTo(m_layerTreeHostImpl.get());
    m_layerTreeHostImpl->commitComplete();

    completion->signal();

    if (m_redrawRequestedOnCCThread)
        scheduleDrawTaskOnCCThread();
}

void CCThreadProxy::scheduleDrawTaskOnCCThread()
{
    ASSERT(isImplThread());
    if (m_drawTaskPostedOnCCThread)
        return;
    TRACE_EVENT("CCThreadProxy::scheduleDrawTaskOnCCThread", this, 0);
    ASSERT(m_layerTreeHostImpl);
    m_drawTaskPostedOnCCThread = true;
    ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::drawLayersAndPresentOnCCThread));
}

void CCThreadProxy::drawLayersAndPresentOnCCThread()
{
    TRACE_EVENT("CCThreadProxy::drawLayersOnCCThread", this, 0);
    ASSERT(isImplThread());
    if (!m_layerTreeHostImpl)
        return;

    drawLayersOnCCThread();
    m_layerTreeHostImpl->present();
    m_redrawRequestedOnCCThread = false;
    m_drawTaskPostedOnCCThread = false;
}

void CCThreadProxy::drawLayersOnCCThread()
{
    TRACE_EVENT("CCThreadProxy::drawLayersOnCCThread", this, 0);
    ASSERT(isImplThread());
    ASSERT(m_layerTreeHostImpl);

    m_layerTreeHostImpl->drawLayers();
    ASSERT(!m_layerTreeHostImpl->isContextLost());
}

void CCThreadProxy::updateSchedulerStateOnCCThread(bool commitRequested, bool redrawRequested)
{
    TRACE_EVENT("CCThreadProxy::updateSchedulerStateOnCCThread", this, 0);
    ASSERT(isImplThread());
    ASSERT(m_layerTreeHostImpl);

    // FIXME: use CCScheduler to decide when to manage the conversion of this
    // commit request into an actual createBeginFrameAndCommitTaskOnCCThread call.
    m_redrawRequestedOnCCThread |= redrawRequested;
    if (!m_beginFrameAndCommitPendingOnCCThread) {
        CCMainThread::postTask(createBeginFrameAndCommitTaskOnCCThread());
        return;
    }

    // If no commit is pending, but a redraw is requested, then post a redraw right away
    if (m_redrawRequestedOnCCThread)
        scheduleDrawTaskOnCCThread();
}

void CCThreadProxy::initializeImplOnCCThread(CCCompletionEvent* completion)
{
    TRACE_EVENT("CCThreadProxy::initializeImplOnCCThread", this, 0);
    ASSERT(isImplThread());
    m_layerTreeHostImpl = m_layerTreeHost->createLayerTreeHostImpl();
    completion->signal();
}

void CCThreadProxy::initializeLayerRendererOnCCThread(GraphicsContext3D* contextPtr, CCCompletionEvent* completion, bool* initializeSucceeded, LayerRendererCapabilities* capabilities)
{
    TRACE_EVENT("CCThreadProxy::initializeLayerRendererOnCCThread", this, 0);
    ASSERT(isImplThread());
    RefPtr<GraphicsContext3D> context(adoptRef(contextPtr));
    *initializeSucceeded = m_layerTreeHostImpl->initializeLayerRenderer(context);
    if (*initializeSucceeded)
        *capabilities = m_layerTreeHostImpl->layerRendererCapabilities();
    completion->signal();
}

void CCThreadProxy::layerTreeHostClosedOnCCThread(CCCompletionEvent* completion)
{
    TRACE_EVENT("CCThreadProxy::layerTreeHostClosedOnCCThread", this, 0);
    ASSERT(isImplThread());
    m_layerTreeHost->deleteContentsTextures(m_layerTreeHostImpl->context());
    m_layerTreeHostImpl.clear();
    completion->signal();
}

} // namespace WebCore
