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
#include "cc/CCMainThreadTask.h"
#include <wtf/CurrentTime.h>

using namespace WTF;

namespace WebCore {

class ScopedSetImplThread {
public:
    ScopedSetImplThread()
    {
#ifndef NDEBUG
        CCProxy::setImplThread(true);
#endif
    }
    ~ScopedSetImplThread()
    {
#ifndef NDEBUG
        CCProxy::setImplThread(false);
#endif
    }
};

PassOwnPtr<CCProxy> CCSingleThreadProxy::create(CCLayerTreeHost* layerTreeHost)
{
    return adoptPtr(new CCSingleThreadProxy(layerTreeHost));
}

CCSingleThreadProxy::CCSingleThreadProxy(CCLayerTreeHost* layerTreeHost)
    : m_layerTreeHost(layerTreeHost)
    , m_numFailedRecreateAttempts(0)
    , m_graphicsContextLost(false)
{
    TRACE_EVENT("CCSingleThreadProxy::CCSingleThreadProxy", this, 0);
    ASSERT(isMainThread());
}

void CCSingleThreadProxy::start()
{
    ScopedSetImplThread impl;
    m_layerTreeHostImpl = m_layerTreeHost->createLayerTreeHostImpl();
}

CCSingleThreadProxy::~CCSingleThreadProxy()
{
    TRACE_EVENT("CCSingleThreadProxy::~CCSingleThreadProxy", this, 0);
    ASSERT(isMainThread());
    ASSERT(!m_layerTreeHostImpl && !m_layerTreeHost); // make sure stop() got called.
}

bool CCSingleThreadProxy::compositeAndReadback(void *pixels, const IntRect& rect)
{
    ASSERT(isMainThread());

    if (!recreateContextIfNeeded())
        return false;

    commitIfNeeded();


    if (!doComposite())
        return false;

    m_layerTreeHostImpl->readback(pixels, rect);

    if (m_layerTreeHostImpl->isContextLost())
        return false;

    return true;
}

GraphicsContext3D* CCSingleThreadProxy::context()
{
    ASSERT(isMainThread());
    ScopedSetImplThread impl;
    return m_layerTreeHostImpl->context();
}

void CCSingleThreadProxy::finishAllRendering()
{
    ASSERT(isMainThread());
    ScopedSetImplThread impl;
    m_layerTreeHostImpl->finishAllRendering();
}

bool CCSingleThreadProxy::isStarted() const
{
    ASSERT(isMainThread());
    return m_layerTreeHostImpl;
}

bool CCSingleThreadProxy::initializeLayerRenderer(CCLayerTreeHost* ownerHack)
{
    RefPtr<GraphicsContext3D> context = m_layerTreeHost->createLayerTreeHostContext3D();
    if (!context)
        return false;
    ASSERT(context->hasOneRef());

    {
        ScopedSetImplThread impl;
        return m_layerTreeHostImpl->initializeLayerRenderer(ownerHack, context);
    }
}

const LayerRendererCapabilities& CCSingleThreadProxy::layerRendererCapabilities() const
{
    ScopedSetImplThread impl;
    return m_layerTreeHostImpl->layerRendererCapabilities();
}

void CCSingleThreadProxy::loseCompositorContext()
{
    m_graphicsContextLost = true;
}

void CCSingleThreadProxy::setNeedsCommitAndRedraw()
{
    ASSERT(isMainThread());
#if !USE(THREADED_COMPOSITING)
    m_layerTreeHost->scheduleComposite();
#else
    // Single threaded only works with THREADED_COMPOSITING.
    CRASH();
#endif
}

void CCSingleThreadProxy::setNeedsRedraw()
{
    // FIXME: Once we move render_widget scheduling into this class, we can
    // treat redraw requests more efficiently than commitAndRedraw requests.
    setNeedsCommitAndRedraw();
}

void CCSingleThreadProxy::stop()
{
    TRACE_EVENT("CCSingleThreadProxy::stop", this, 0);
    ASSERT(isMainThread());
    {
        ScopedSetImplThread impl;
        m_layerTreeHostImpl.clear();
    }
    m_layerTreeHost = 0;
}

TextureManager* CCSingleThreadProxy::contentsTextureManager()
{
    return m_layerTreeHostImpl->layerRenderer()->contentsTextureManager();
}

#if !USE(THREADED_COMPOSITING)
// Called by the legacy scheduling path (e.g. where render_widget does the scheduling)
void CCSingleThreadProxy::compositeImmediately()
{
    if (!recreateContextIfNeeded())
        return;

    commitIfNeeded();

    if (doComposite())
        m_layerTreeHostImpl->present();
}
#endif


bool CCSingleThreadProxy::recreateContextIfNeeded()
{
    if (!m_graphicsContextLost)
        return true;
    RefPtr<GraphicsContext3D> context = m_layerTreeHost->createLayerTreeHostContext3D();
    ASSERT(context->hasOneRef());
    if (m_layerTreeHostImpl->initializeLayerRenderer(0, context)) {
        m_layerTreeHost->didRecreateGraphicsContext(true);
        m_graphicsContextLost = false;
        return true;
    }

    // Tolerate a certain number of recreation failures to work around races
    // in the context-lost machinery.
    m_numFailedRecreateAttempts++;
    if (m_numFailedRecreateAttempts < 5) {
        setNeedsCommitAndRedraw();
        return false;
    }

    // We have tried too many times to recreate the context. Tell the host to fall
    // back to software rendering.
    m_layerTreeHost->didRecreateGraphicsContext(false);
    return false;
}

void CCSingleThreadProxy::commitIfNeeded()
{
    // Update
    double frameBeginTime = currentTime();
    m_layerTreeHost->animateAndLayout(frameBeginTime);

    // Precommit is a temporary step we need to perform to push things like
    // the viewport to the LayerTreeHostImpl, because
    // LayerTreeHostImpl::updateLayers() isn't split into host and impl parts.
    {
        ScopedSetImplThread impl;
        m_layerTreeHost->preCommit(m_layerTreeHostImpl.get());
    }

    // This should become m_layerTreeHost->updateLayers() once we split apart
    // LayerTreeHostImpl::updateLayers.
    m_layerTreeHostImpl->updateLayers();

    // Commit
    {
        ScopedSetImplThread impl;
        m_layerTreeHostImpl->beginCommit();
        m_layerTreeHost->commitTo(m_layerTreeHostImpl.get());
        m_layerTreeHostImpl->commitComplete();
    }
}

bool CCSingleThreadProxy::doComposite()
{
    ASSERT(!m_graphicsContextLost);

    {
      ScopedSetImplThread impl;
      m_layerTreeHostImpl->drawLayers();
      if (m_layerTreeHostImpl->isContextLost()) {
          // Trying to recover the context right here will not work if GPU process
          // died. This is because GpuChannelHost::OnErrorMessage will only be
          // called at the next iteration of the message loop, reverting our
          // recovery attempts here. Instead, we detach the root layer from the
          // renderer, recreate the renderer at the next message loop iteration
          // and request a repaint yet again.
          m_graphicsContextLost = true;
          m_numFailedRecreateAttempts = 0;
          setNeedsCommitAndRedraw();
          return false;
      }
    }

    return true;
}

}
