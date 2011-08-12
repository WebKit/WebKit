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

#include "cc/CCLayerTreeHostImpl.h"

#include "Extensions3D.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "TraceEvent.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCMainThread.h"
#include "cc/CCMainThreadTask.h"
#include "cc/CCThreadTask.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

PassOwnPtr<CCLayerTreeHostImpl> CCLayerTreeHostImpl::create(CCLayerTreeHostImplClient* client, PassRefPtr<LayerRendererChromium> renderer)
{
    return adoptPtr(new CCLayerTreeHostImpl(client, renderer));
}

CCLayerTreeHostImpl::CCLayerTreeHostImpl(CCLayerTreeHostImplClient* client, PassRefPtr<LayerRendererChromium> renderer)
    : m_sourceFrameNumber(-1)
    , m_frameNumber(0)
    , m_client(client)
    , m_commitPending(false)
    , m_layerRenderer(renderer)
    , m_redrawPending(false)
{
}

CCLayerTreeHostImpl::~CCLayerTreeHostImpl()
{
    TRACE_EVENT("CCLayerTreeHostImpl::~CCLayerTreeHostImpl()", this, 0);
}

void CCLayerTreeHostImpl::beginCommit()
{
}

void CCLayerTreeHostImpl::commitComplete()
{
    m_commitPending = false;
    setNeedsRedraw();
}

void CCLayerTreeHostImpl::drawLayers()
{
    // If a commit is pending, do not draw. This is a temporary restriction that
    // is necessary because drawLayers is currently a blocking operation on the main thread.
    if (m_commitPending)
        return;

    TRACE_EVENT("CCLayerTreeHostImpl::drawLayers", this, 0);
    ASSERT(m_redrawPending);
    m_redrawPending = false;

    {
        TRACE_EVENT("CCLayerTreeHostImpl::drawLayersAndPresent", this, 0);
        CCCompletionEvent completion;
        bool contextLost;
        CCMainThread::postTask(createMainThreadTask(this, &CCLayerTreeHostImpl::drawLayersOnMainThread, AllowCrossThreadAccess(&completion), AllowCrossThreadAccess(&contextLost)));
        completion.wait();

        // FIXME: Send the "UpdateRect" message up to the RenderWidget [or moveplugin equivalents...]

        // FIXME: handle context lost
        if (contextLost)
            FATAL("LayerRendererChromiumImpl does not handle context lost yet.");
    }

    ++m_frameNumber;
}

void CCLayerTreeHostImpl::setNeedsCommitAndRedraw()
{
    TRACE_EVENT("CCLayerTreeHostImpl::setNeedsCommitAndRedraw", this, 0);

    // FIXME: move the requestFrameAndCommit out from here once we add framerate throttling/animation
    double frameBeginTime = currentTime();
    m_commitPending = true;
    m_client->requestFrameAndCommitOnCCThread(frameBeginTime);
}

void CCLayerTreeHostImpl::setNeedsRedraw()
{
    if (m_redrawPending || m_commitPending)
        return;

    TRACE_EVENT("CCLayerTreeHostImpl::setNeedsRedraw", this, 0);
    m_redrawPending = true;
    m_client->postDrawLayersTaskOnCCThread();
}

void CCLayerTreeHostImpl::drawLayersOnMainThread(CCCompletionEvent* completion, bool* contextLost)
{
    ASSERT(isMainThread());

    if (m_layerRenderer->owner()->rootLayer()) {
        m_layerRenderer->drawLayers();
        m_layerRenderer->present();

        GraphicsContext3D* context = m_layerRenderer->context();
        *contextLost = context->getExtensions()->getGraphicsResetStatusARB() != GraphicsContext3D::NO_ERROR;
    } else
        *contextLost = false;
    completion->signal();
}

}
