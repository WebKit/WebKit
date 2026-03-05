/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameRenderer.h"

#if USE(COORDINATED_GRAPHICS)
#include <WebCore/RunLoopObserver.h>
#include <WebCore/WindowEventLoop.h>
#include <wtf/SystemTracing.h>

namespace WebKit {
using namespace WebCore;

FrameRenderer::FrameRenderer()
{
    m_renderingUpdateRunLoopObserver = makeUnique<RunLoopObserver>(RunLoopObserver::WellKnownOrder::RenderingUpdate, [this] {
        renderingUpdateRunLoopObserverFired();
    });
}

FrameRenderer::~FrameRenderer()
{
    if (m_forcedRepaintAsyncCallback)
        m_forcedRepaintAsyncCallback();

    invalidateRenderingUpdateRunLoopObserver();
}

void FrameRenderer::scheduleRenderingUpdateRunLoopObserver()
{
    if (m_renderingUpdateRunLoopObserver->isScheduled())
        return;

    if (m_isUpdatingRendering)
        return;

    tracePoint(RenderingUpdateRunLoopObserverStart);
    m_renderingUpdateRunLoopObserver->schedule();

    // Avoid running any more tasks before the runloop observer fires.
    WindowEventLoop::breakToAllowRenderingUpdate();
}

void FrameRenderer::invalidateRenderingUpdateRunLoopObserver()
{
    if (!m_renderingUpdateRunLoopObserver->isScheduled())
        return;

    tracePoint(RenderingUpdateRunLoopObserverEnd);
    m_renderingUpdateRunLoopObserver->invalidate();
}

void FrameRenderer::renderingUpdateRunLoopObserverFired()
{
    WTFEmitSignpost(this, RenderingUpdateRunLoopObserverFired, "canUpdateRendering %s", canUpdateRendering() ? "yes" : "no");

    invalidateRenderingUpdateRunLoopObserver();

    if (m_layerTreeStateIsFrozen || m_isSuspended)
        return;

    if (canUpdateRendering())
        updateRendering();
}

void FrameRenderer::setLayerTreeStateIsFrozen(bool isFrozen)
{
    if (m_layerTreeStateIsFrozen == isFrozen)
        return;

    m_layerTreeStateIsFrozen = isFrozen;

    if (m_layerTreeStateIsFrozen)
        invalidateRenderingUpdateRunLoopObserver();
    else
        scheduleRenderingUpdate();
}

void FrameRenderer::suspend()
{
    m_isSuspended = true;
}

void FrameRenderer::resume()
{
    m_isSuspended = false;
    scheduleRenderingUpdate();
}

void FrameRenderer::updateRenderingWithForcedRepaintAsync(CompletionHandler<void()>&& callback)
{
    ASSERT(!m_forcedRepaintAsyncCallback);
    m_forcedRepaintAsyncCallback = WTF::move(callback);
    updateRenderingWithForcedRepaint();
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
