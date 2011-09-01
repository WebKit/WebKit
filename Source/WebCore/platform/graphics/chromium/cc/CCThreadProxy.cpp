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
    : m_commitPending(false)
    , m_layerTreeHost(layerTreeHost)
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
    ASSERT(!m_layerTreeHostImpl); // Make sure stop() got called.
    ASSERT(!m_layerTreeHost); // Make sure stop() got called.

    numProxies--;
    if (!numProxies) {
        delete ccThread;
        ccThread = 0;
    }
}

bool CCThreadProxy::compositeAndReadback(void *pixels, const IntRect& rect)
{
    ASSERT_NOT_REACHED();
    return false;
}

GraphicsContext3D* CCThreadProxy::context()
{
    ASSERT_NOT_REACHED();
    return 0;
}

void CCThreadProxy::finishAllRendering()
{
    ASSERT_NOT_REACHED();
}

bool CCThreadProxy::isStarted() const
{
    return m_layerTreeHostImpl;
}

bool CCThreadProxy::initializeLayerRenderer(CCLayerTreeHost* ownerHack)
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
    bool initializeSucceeded;
    LayerRendererCapabilities capabilities;
    ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::initializeLayerRendererOnCCThread,
                                          AllowCrossThreadAccess(ownerHack), AllowCrossThreadAccess(contextPtr),
                                          AllowCrossThreadAccess(&completion), AllowCrossThreadAccess(&initializeSucceeded), AllowCrossThreadAccess(&capabilities)));
    completion.wait();

    if (initializeSucceeded)
        m_layerRendererCapabilitiesMainThreadCopy = capabilities;
    return initializeSucceeded;
}

const LayerRendererCapabilities& CCThreadProxy::layerRendererCapabilities() const
{
    return m_layerRendererCapabilitiesMainThreadCopy;
}

void CCThreadProxy::loseCompositorContext()
{
    ASSERT_NOT_REACHED();
}

void CCThreadProxy::setNeedsCommitAndRedraw()
{
    ASSERT(isMainThread());
    if (m_commitPending)
        return;

    TRACE_EVENT("CCThreadProxy::setNeedsCommitAndRedraw", this, 0);
    m_commitPending = true;
    ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::setNeedsCommitAndRedrawOnCCThread));
}

void CCThreadProxy::setNeedsRedraw()
{
    ASSERT(isMainThread());
    ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::setNeedsRedrawOnCCThread));
}

void CCThreadProxy::start()
{
    // Create LayerTreeHostImpl.
    CCCompletionEvent completion;
    ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::initializeImplOnCCThread, AllowCrossThreadAccess(&completion)));
    completion.wait();
}

void CCThreadProxy::stop()
{
    TRACE_EVENT("CCThreadProxy::stop", this, 0);
    ASSERT(isMainThread());
    // Synchronously deletes the impl.
    CCCompletionEvent completion;
    ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::layerTreeHostClosedOnCCThread, AllowCrossThreadAccess(&completion)));
    completion.wait();

    ASSERT(!m_layerTreeHostImpl); // verify that the impl deleted.
    m_layerTreeHost = 0;
}

TextureManager* CCThreadProxy::contentsTextureManager()
{
    ASSERT_NOT_REACHED();
    return 0;
}

void CCThreadProxy::beginFrameAndCommitOnCCThread()
{
    TRACE_EVENT("CCThreadProxy::beginFrameAndCommitOnCCThread", this, 0);
    ASSERT(isImplThread());
    ASSERT_NOT_REACHED();
    // FIXME: call beginFrameAndCommit on main thread
}

void CCThreadProxy::beginFrameAndCommit(double frameBeginTime)
{
    ASSERT(isMainThread());
    if (!m_layerTreeHost)
        return;

    TRACE_EVENT("CCThreadProxy::requestFrameAndCommit", this, 0);
    {
        TRACE_EVENT("CCLayerTreeHost::animateAndLayout", this, 0);
        m_layerTreeHost->animateAndLayout(frameBeginTime);
    }

    m_commitPending = false;

    // Blocking call to CCThreadProxy::performCommit
    CCCompletionEvent completion;
    ccThread->postTask(createCCThreadTask(this, &CCThreadProxy::commitOnCCThread, AllowCrossThreadAccess(&completion)));
    completion.wait();
}

void CCThreadProxy::commitOnCCThread(CCCompletionEvent* completion)
{
    ASSERT(isImplThread());
    TRACE_EVENT("CCThreadProxy::commitOnCCThread", this, 0);
    m_layerTreeHostImpl->beginCommit();
    {
        TRACE_EVENT("CCLayerTreeHost::commit", this, 0);
        m_layerTreeHost->commitTo(m_layerTreeHostImpl.get());
    }
    completion->signal();

    m_layerTreeHostImpl->commitComplete();
    setNeedsRedrawOnCCThread();
}

void CCThreadProxy::drawLayersOnCCThread()
{
    TRACE_EVENT("CCThreadProxy::drawLayersOnCCThread", this, 0);
    ASSERT(isImplThread());
    if (m_layerTreeHostImpl)
        m_layerTreeHostImpl->drawLayers();
}

void CCThreadProxy::setNeedsCommitAndRedrawOnCCThread()
{
    TRACE_EVENT("CCThreadProxy::setNeedsCommitAndRedrawOnCCThread", this, 0);
    ASSERT(isImplThread());
    ASSERT(m_layerTreeHostImpl);
    ASSERT_NOT_REACHED();
}

void CCThreadProxy::setNeedsRedrawOnCCThread()
{
    TRACE_EVENT("CCThreadProxy::setNeedsRedrawOnCCThread", this, 0);
    ASSERT_NOT_REACHED();
}

void CCThreadProxy::initializeImplOnCCThread(CCCompletionEvent* completion)
{
    TRACE_EVENT("CCThreadProxy::initializeImplOnCCThread", this, 0);
    ASSERT(isImplThread());
    m_layerTreeHostImpl = m_layerTreeHost->createLayerTreeHostImpl();
    completion->signal();
}

void CCThreadProxy::initializeLayerRendererOnCCThread(CCLayerTreeHost* ownerHack, GraphicsContext3D* contextPtr, CCCompletionEvent* completion, bool* initializeSucceeded, LayerRendererCapabilities* capabilities)
{
    TRACE_EVENT("CCThreadProxy::initializeLayerRendererOnCCThread", this, 0);
    ASSERT(isImplThread());
    RefPtr<GraphicsContext3D> context(adoptRef(contextPtr));
    *initializeSucceeded = m_layerTreeHostImpl->initializeLayerRenderer(ownerHack, context);
    if (*initializeSucceeded)
        *capabilities = m_layerTreeHostImpl->layerRendererCapabilities();
    completion->signal();
}

void CCThreadProxy::layerTreeHostClosedOnCCThread(CCCompletionEvent* completion)
{
    TRACE_EVENT("CCThreadProxy::layerTreeHostClosedOnCCThread", this, 0);
    ASSERT(isImplThread());
    m_layerTreeHostImpl.clear();
    completion->signal();
}

}
