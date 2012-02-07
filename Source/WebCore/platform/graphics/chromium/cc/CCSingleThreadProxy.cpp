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

#include "cc/CCSingleThreadProxy.h"

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "TraceEvent.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCTextureUpdater.h"
#include <wtf/CurrentTime.h>

using namespace WTF;

namespace WebCore {

PassOwnPtr<CCProxy> CCSingleThreadProxy::create(CCLayerTreeHost* layerTreeHost)
{
    return adoptPtr(new CCSingleThreadProxy(layerTreeHost));
}

CCSingleThreadProxy::CCSingleThreadProxy(CCLayerTreeHost* layerTreeHost)
    : m_layerTreeHost(layerTreeHost)
    , m_compositorIdentifier(-1)
    , m_layerRendererInitialized(false)
    , m_numFailedRecreateAttempts(0)
    , m_graphicsContextLost(false)
    , m_timesRecreateShouldFail(0)
    , m_nextFrameIsNewlyCommittedFrame(false)
{
    TRACE_EVENT("CCSingleThreadProxy::CCSingleThreadProxy", this, 0);
    ASSERT(CCProxy::isMainThread());
}

void CCSingleThreadProxy::start()
{
    DebugScopedSetImplThread impl;
    m_layerTreeHostImpl = m_layerTreeHost->createLayerTreeHostImpl(this);
}

CCSingleThreadProxy::~CCSingleThreadProxy()
{
    TRACE_EVENT("CCSingleThreadProxy::~CCSingleThreadProxy", this, 0);
    ASSERT(CCProxy::isMainThread());
    ASSERT(!m_layerTreeHostImpl && !m_layerTreeHost); // make sure stop() got called.
}

bool CCSingleThreadProxy::compositeAndReadback(void *pixels, const IntRect& rect)
{
    TRACE_EVENT("CCSingleThreadProxy::compositeAndReadback", this, 0);
    ASSERT(CCProxy::isMainThread());

    if (!recreateContextIfNeeded()) {
        TRACE_EVENT("compositeAndReadback_EarlyOut_ContextLost", this, 0);
        return false;
    }

    if (!commitIfNeeded())
        return false;

    if (!doComposite())
        return false;

    m_layerTreeHostImpl->readback(pixels, rect);

    if (m_layerTreeHostImpl->isContextLost())
        return false;

    return true;
}

GraphicsContext3D* CCSingleThreadProxy::context()
{
    ASSERT(CCProxy::isMainThread());
    DebugScopedSetImplThread impl;
    return m_layerTreeHostImpl->context();
}

void CCSingleThreadProxy::finishAllRendering()
{
    ASSERT(CCProxy::isMainThread());
    {
        DebugScopedSetImplThread impl;
        m_layerTreeHostImpl->finishAllRendering();
    }
}

bool CCSingleThreadProxy::isStarted() const
{
    ASSERT(CCProxy::isMainThread());
    return m_layerTreeHostImpl;
}

bool CCSingleThreadProxy::initializeContext()
{
    ASSERT(CCProxy::isMainThread());
    RefPtr<GraphicsContext3D> context = m_layerTreeHost->createLayerTreeHostContext3D();
    if (!context)
        return false;
    ASSERT(context->hasOneRef());
    m_contextBeforeInitialization = context;
    return true;
}

bool CCSingleThreadProxy::initializeLayerRenderer()
{
    ASSERT(CCProxy::isMainThread());
    ASSERT(m_contextBeforeInitialization);
    {
        DebugScopedSetImplThread impl;
        bool ok = m_layerTreeHostImpl->initializeLayerRenderer(m_contextBeforeInitialization.release());
        if (ok) {
            m_layerRendererInitialized = true;
            m_layerRendererCapabilitiesForMainThread = m_layerTreeHostImpl->layerRendererCapabilities();
        }
        return ok;
    }
}

const LayerRendererCapabilities& CCSingleThreadProxy::layerRendererCapabilities() const
{
    ASSERT(m_layerRendererInitialized);
    // Note: this gets called during the commit by the "impl" thread
    return m_layerRendererCapabilitiesForMainThread;
}

void CCSingleThreadProxy::loseCompositorContext(int numTimes)
{
    m_graphicsContextLost = true;
    m_timesRecreateShouldFail = numTimes - 1;
}

void CCSingleThreadProxy::setNeedsAnimate()
{
    // CCThread-only feature
    ASSERT_NOT_REACHED();
}

void CCSingleThreadProxy::doCommit()
{
    ASSERT(CCProxy::isMainThread());
    // Commit immediately
    {
        DebugScopedSetImplThread impl;
        m_layerTreeHostImpl->beginCommit();

        m_layerTreeHost->beginCommitOnImplThread(m_layerTreeHostImpl.get());
        CCTextureUpdater updater(m_layerTreeHostImpl->contentsTextureAllocator());
        m_layerTreeHost->updateCompositorResources(m_layerTreeHostImpl->context(), updater);
        while (updater.update(m_layerTreeHostImpl->context(), 1)) { }
        m_layerTreeHostImpl->setVisible(m_layerTreeHost->visible());
        m_layerTreeHost->finishCommitOnImplThread(m_layerTreeHostImpl.get());

        m_layerTreeHostImpl->commitComplete();

#if !ASSERT_DISABLED
        // In the single-threaded case, the scroll deltas should never be
        // touched on the impl layer tree.
        OwnPtr<CCScrollAndScaleSet> scrollInfo = m_layerTreeHostImpl->processScrollDeltas();
        ASSERT(!scrollInfo->scrolls.size());
#endif
    }
    m_layerTreeHost->commitComplete();
    m_nextFrameIsNewlyCommittedFrame = true;
}

void CCSingleThreadProxy::setNeedsCommit()
{
    ASSERT(CCProxy::isMainThread());
    m_layerTreeHost->setNeedsCommit();
}

void CCSingleThreadProxy::setNeedsRedraw()
{
    // FIXME: Once we move render_widget scheduling into this class, we can
    // treat redraw requests more efficiently than commitAndRedraw requests.
    setNeedsCommit();
}

void CCSingleThreadProxy::setVisible(bool visible)
{
    if (!visible) {
        DebugScopedSetImplThread impl;
        m_layerTreeHost->didBecomeInvisibleOnImplThread(m_layerTreeHostImpl.get());
        m_layerTreeHostImpl->setVisible(false);
        return;
    }
    setNeedsCommit();
}

void CCSingleThreadProxy::stop()
{
    TRACE_EVENT("CCSingleThreadProxy::stop", this, 0);
    ASSERT(CCProxy::isMainThread());
    {
        DebugScopedSetImplThread impl;
        m_layerTreeHost->deleteContentsTexturesOnImplThread(m_layerTreeHostImpl->contentsTextureAllocator());
        m_layerTreeHostImpl.clear();
    }
    m_layerTreeHost = 0;
}

// Called by the legacy scheduling path (e.g. where render_widget does the scheduling)
void CCSingleThreadProxy::compositeImmediately()
{
    if (!recreateContextIfNeeded())
        return;

    if (!commitIfNeeded())
        return;

    if (doComposite())
        m_layerTreeHostImpl->swapBuffers();
}

bool CCSingleThreadProxy::recreateContextIfNeeded()
{
    ASSERT(CCProxy::isMainThread());

    if (!m_graphicsContextLost && m_layerTreeHostImpl->isContextLost()) {
        m_graphicsContextLost = true;
        m_numFailedRecreateAttempts = 0;
    }

    if (!m_graphicsContextLost)
        return true;
    RefPtr<GraphicsContext3D> context;
    if (!m_timesRecreateShouldFail)
        context = m_layerTreeHost->createLayerTreeHostContext3D();
    else
        m_timesRecreateShouldFail--;

    if (context) {
        ASSERT(context->hasOneRef());
        bool ok;
        {
            DebugScopedSetImplThread impl;
            m_layerTreeHost->deleteContentsTexturesOnImplThread(m_layerTreeHostImpl->contentsTextureAllocator());
            ok = m_layerTreeHostImpl->initializeLayerRenderer(context);
            if (ok)
                m_layerRendererCapabilitiesForMainThread = m_layerTreeHostImpl->layerRendererCapabilities();
        }
        if (ok) {
            m_layerTreeHost->didRecreateGraphicsContext(true);
            m_graphicsContextLost = false;
            return true;
        }
    }

    // Tolerate a certain number of recreation failures to work around races
    // in the context-lost machinery.
    m_numFailedRecreateAttempts++;
    if (m_numFailedRecreateAttempts < 5) {
        setNeedsCommit();
        return false;
    }

    // We have tried too many times to recreate the context. Tell the host to fall
    // back to software rendering.
    m_layerTreeHost->didRecreateGraphicsContext(false);
    return false;
}

bool CCSingleThreadProxy::commitIfNeeded()
{
    ASSERT(CCProxy::isMainThread());

    if (!m_layerTreeHost->updateLayers())
        return false;

    doCommit();
    return true;
}

bool CCSingleThreadProxy::doComposite()
{
    ASSERT(!m_graphicsContextLost);

    {
      DebugScopedSetImplThread impl;
      double frameDisplayTimeMs = monotonicallyIncreasingTime() * 1000.0;
      m_layerTreeHostImpl->animate(frameDisplayTimeMs);
      m_layerTreeHostImpl->drawLayers();
    }

    if (m_layerTreeHostImpl->isContextLost()) {
        // Trying to recover the context right here will not work if GPU process
        // died. This is because GpuChannelHost::OnErrorMessage will only be
        // called at the next iteration of the message loop, reverting our
        // recovery attempts here. Instead, we detach the root layer from the
        // renderer, recreate the renderer at the next message loop iteration
        // and request a repaint yet again.
        m_graphicsContextLost = true;
        m_numFailedRecreateAttempts = 0;
        setNeedsCommit();
        return false;
    }

    if (m_nextFrameIsNewlyCommittedFrame) {
        m_nextFrameIsNewlyCommittedFrame = false;
        m_layerTreeHost->didCommitAndDrawFrame();
    }

    return true;
}

}
