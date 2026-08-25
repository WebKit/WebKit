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
    : m_prioritizedRenderingUpdateTimer([this] { prioritizedRenderingUpdateTimerFired(); })
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

void FrameRenderer::prioritizeRenderingUpdate()
{
    if (m_layerTreeStateIsFrozen || m_isSuspended || m_isUpdatingRendering)
        return;

    // Interaction priority only reorders a rendering update that is already pending; it must never create one.
    if (!m_renderingUpdateRunLoopObserver->isScheduled())
        return;

    // A new frame cannot be produced while the compositor is still rendering the previous one, so a pending
    // update cannot run ahead of the already-due timers; keep the existing scheduling.
    if (!canUpdateRendering())
        return;

    // All main thread timers, including the event loop's DOM timers, share the ThreadTimers heap and fire
    // in fire-time order. Arming a timer with a far-past fire time runs the pending update ahead of any
    // already-due timer without stopping, holding, or reordering the timers themselves. The run loop
    // observer stays scheduled as the ordinary fallback in case the update cannot run when the timer fires.
    // FIXME: Add a Timer interface for running ahead of all already-due timers instead of encoding
    // priority as a far-past fire time.
    m_prioritizedRenderingUpdateTimer.startOneShot(-1_h);
    WindowEventLoop::breakToAllowRenderingUpdate();
}

void FrameRenderer::prioritizedRenderingUpdateTimerFired()
{
    // The update already ran or was invalidated through the ordinary paths since prioritization.
    if (!m_renderingUpdateRunLoopObserver->isScheduled())
        return;

    // Leave the pending update to the still-scheduled run loop observer if it cannot run right now.
    if (m_layerTreeStateIsFrozen || m_isSuspended || m_isUpdatingRendering || !canUpdateRendering())
        return;

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
