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

#include "cc/CCLayerTreeHostImplProxy.h"

#include "TraceEvent.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCMainThreadTask.h"
#include "cc/CCThreadTask.h"
#include <wtf/MainThread.h>

using namespace WTF;

namespace WebCore {

namespace {
CCThread* ccThread;
int numProxies = 0;
}

CCLayerTreeHostImplProxy::CCLayerTreeHostImplProxy(CCLayerTreeHost* layerTreeHost)
    : m_commitPending(false)
    , m_layerTreeHost(layerTreeHost)
{
    TRACE_EVENT("CCLayerTreeHostImplProxy::CCLayerTreeHostImplProxy", this, 0);
    ASSERT(isMainThread());
    numProxies++;
    if (!ccThread)
        ccThread = CCThread::create().leakPtr();
}


void CCLayerTreeHostImplProxy::start()
{
    // Create LayerTreeHostImpl.
    CCCompletionEvent completion;
    ccThread->postTask(createCCThreadTask(this, &CCLayerTreeHostImplProxy::initImplOnCCThread, AllowCrossThreadAccess(&completion)));
    completion.wait();
}

CCLayerTreeHostImplProxy::~CCLayerTreeHostImplProxy()
{
    TRACE_EVENT("CCLayerTreeHostImplProxy::~CCLayerTreeHostImplProxy", this, 0);
    ASSERT(isMainThread());
    ASSERT(!m_layerTreeHostImpl && !m_layerTreeHost); // make sure stop() got called.

    numProxies--;
    if (!numProxies) {
        delete ccThread;
        ccThread = 0;
    }
}

bool CCLayerTreeHostImplProxy::isStarted() const
{
    return m_layerTreeHostImpl;
}

void CCLayerTreeHostImplProxy::setNeedsCommitAndRedraw()
{
    ASSERT(isMainThread());
    if (m_commitPending)
        return;

    TRACE_EVENT("CCLayerTreeHostImplProxy::setNeedsCommitAndRedraw", this, 0);
    m_commitPending = true;
    ccThread->postTask(createCCThreadTask(this, &CCLayerTreeHostImplProxy::setNeedsCommitAndRedrawOnCCThread));
}

void CCLayerTreeHostImplProxy::setNeedsRedraw()
{
    ASSERT(isMainThread());
    ccThread->postTask(createCCThreadTask(this, &CCLayerTreeHostImplProxy::setNeedsRedrawOnCCThread));
}

void CCLayerTreeHostImplProxy::stop()
{
    TRACE_EVENT("CCLayerTreeHostImplProxy::stop", this, 0);
    ASSERT(isMainThread());
    // Synchronously deletes the impl.
    CCCompletionEvent completion;
    ccThread->postTask(createCCThreadTask(this, &CCLayerTreeHostImplProxy::layerTreeHostClosedOnCCThread, AllowCrossThreadAccess(&completion)));
    completion.wait();

    ASSERT(!m_layerTreeHostImpl); // verify that the impl deleted.
    m_layerTreeHost = 0;
}

void CCLayerTreeHostImplProxy::postDrawLayersTaskOnCCThread()
{
    ASSERT(isCCThread());
    if (m_layerTreeHostImpl)
        ccThread->postTask(createCCThreadTask(this, &CCLayerTreeHostImplProxy::drawLayersOnCCThread));
}

void CCLayerTreeHostImplProxy::requestFrameAndCommitOnCCThread(double frameBeginTime)
{
    TRACE_EVENT("CCLayerTreeHostImplProxy::requestFrameAndCommitOnCCThread", this, 0);
    ASSERT(isCCThread());
    if (m_layerTreeHostImpl)
        CCMainThread::postTask(createMainThreadTask(this, &CCLayerTreeHostImplProxy::requestFrameAndCommit, frameBeginTime));
}

bool CCLayerTreeHostImplProxy::isMainThread() const
{
    return ::isMainThread();
}

bool CCLayerTreeHostImplProxy::isCCThread() const
{
    return currentThread() == ccThread->threadID();
}

void CCLayerTreeHostImplProxy::requestFrameAndCommit(double frameBeginTime)
{
    ASSERT(isMainThread());
    if (!m_layerTreeHost)
        return;

    TRACE_EVENT("CCLayerTreeHostImplProxy::requestFrameAndCommit", this, 0);
    {
        TRACE_EVENT("CCLayerTreeHost::animateAndLayout", this, 0);
        m_layerTreeHost->animateAndLayout(frameBeginTime);
    }

    m_commitPending = false;

    OwnPtr<CCLayerTreeHostCommitter> committer = m_layerTreeHost->createLayerTreeHostCommitter();
    m_layerTreeHost->beginCommit();

    // Blocking call to CCLayerTreeHostImplProxy::performCommit
    CCCompletionEvent completion;
    ccThread->postTask(createCCThreadTask(this, &CCLayerTreeHostImplProxy::commitOnCCThread, AllowCrossThreadAccess(committer.get()), AllowCrossThreadAccess(&completion)));
    completion.wait();

    committer.clear();

    m_layerTreeHost->commitComplete();
}

void CCLayerTreeHostImplProxy::commitOnCCThread(CCLayerTreeHostCommitter* committer, CCCompletionEvent* completion)
{
    ASSERT(isCCThread());
    TRACE_EVENT("CCLayerTreeHostImplProxy::commitOnCCThread", this, 0);
    m_layerTreeHostImpl->beginCommit();
    {
        TRACE_EVENT("CCLayerTreeHost::commit", this, 0);
        committer->commit(m_layerTreeHost, m_layerTreeHostImpl.get());
    }
    completion->signal();

    m_layerTreeHostImpl->commitComplete();
}

void CCLayerTreeHostImplProxy::drawLayersOnCCThread()
{
    TRACE_EVENT("CCLayerTreeHostImplProxy::drawLayersOnCCThread", this, 0);
    ASSERT(isCCThread());
    if (m_layerTreeHostImpl)
        m_layerTreeHostImpl->drawLayers();
}

void CCLayerTreeHostImplProxy::setNeedsCommitAndRedrawOnCCThread()
{
    TRACE_EVENT("CCLayerTreeHostImplProxy::setNeedsCommitAndRedrawOnCCThread", this, 0);
    ASSERT(isCCThread() && m_layerTreeHostImpl);
    m_layerTreeHostImpl->setNeedsCommitAndRedraw();
}

void CCLayerTreeHostImplProxy::setNeedsRedrawOnCCThread()
{
    TRACE_EVENT("CCLayerTreeHostImplProxy::setNeedsRedrawOnCCThread", this, 0);
    ASSERT(isCCThread() && m_layerTreeHostImpl);
    m_layerTreeHostImpl->setNeedsRedraw();
}

void CCLayerTreeHostImplProxy::initImplOnCCThread(CCCompletionEvent* completion)
{
    TRACE_EVENT("CCLayerTreeHostImplProxy::initImplOnCCThread", this, 0);
    ASSERT(isCCThread());
    m_layerTreeHostImpl = createLayerTreeHostImpl();
    completion->signal();
}

void CCLayerTreeHostImplProxy::layerTreeHostClosedOnCCThread(CCCompletionEvent* completion)
{
    TRACE_EVENT("CCLayerTreeHostImplProxy::layerTreeHostClosedOnCCThread", this, 0);
    ASSERT(isCCThread());
    m_layerTreeHostImpl.clear();
    completion->signal();
}

}
