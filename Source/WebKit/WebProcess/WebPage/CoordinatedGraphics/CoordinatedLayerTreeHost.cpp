/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Company 100, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedLayerTreeHost.h"

#include "DrawingArea.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/PageOverlayController.h>

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebKit {
using namespace WebCore;

Ref<CoordinatedLayerTreeHost> CoordinatedLayerTreeHost::create(WebPage& webPage)
{
    return adoptRef(*new CoordinatedLayerTreeHost(webPage));
}

CoordinatedLayerTreeHost::~CoordinatedLayerTreeHost()
{
}

CoordinatedLayerTreeHost::CoordinatedLayerTreeHost(WebPage& webPage)
    : LayerTreeHost(webPage)
    , m_coordinator(webPage.corePage(), *this)
    , m_layerFlushTimer(RunLoop::main(), this, &CoordinatedLayerTreeHost::layerFlushTimerFired)
{
#if USE(GLIB_EVENT_LOOP)
    m_layerFlushTimer.setPriority(RunLoopSourcePriority::LayerFlushTimer);
    m_layerFlushTimer.setName("[WebKit] CoordinatedLayerTreeHost");
#endif
    m_coordinator.createRootLayer(m_webPage.size());

    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::scheduleLayerFlush()
{
    if (!m_layerFlushSchedulingEnabled)
        return;

    if (m_isWaitingForRenderer) {
        m_scheduledWhileWaitingForRenderer = true;
        return;
    }

    if (!m_layerFlushTimer.isActive())
        m_layerFlushTimer.startOneShot(0_s);
}

void CoordinatedLayerTreeHost::cancelPendingLayerFlush()
{
    m_layerFlushTimer.stop();
}

void CoordinatedLayerTreeHost::setViewOverlayRootLayer(GraphicsLayer* viewOverlayRootLayer)
{
    LayerTreeHost::setViewOverlayRootLayer(viewOverlayRootLayer);
    m_coordinator.setViewOverlayRootLayer(viewOverlayRootLayer);
}

void CoordinatedLayerTreeHost::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    m_coordinator.setRootCompositingLayer(graphicsLayer);
}

void CoordinatedLayerTreeHost::invalidate()
{
    cancelPendingLayerFlush();

    m_coordinator.invalidate();
    LayerTreeHost::invalidate();
}

void CoordinatedLayerTreeHost::forceRepaint()
{
    // This is necessary for running layout tests. Since in this case we are not waiting for a UIProcess to reply nicely.
    // Instead we are just triggering forceRepaint. But we still want to have the scripted animation callbacks being executed.
    m_coordinator.syncDisplayState();

    // We need to schedule another flush, otherwise the forced paint might cancel a later expected flush.
    // This is aligned with LayerTreeHostCA.
    scheduleLayerFlush();

    if (m_isWaitingForRenderer)
        return;

    m_coordinator.flushPendingLayerChanges();
}

bool CoordinatedLayerTreeHost::forceRepaintAsync(CallbackID callbackID)
{
    scheduleLayerFlush();

    // We want a clean repaint, meaning that if we're currently waiting for the renderer
    // to finish an update, we'll have to schedule another flush when it's done.
    ASSERT(!m_forceRepaintAsync.callbackID);
    m_forceRepaintAsync.callbackID = OptionalCallbackID(callbackID);
    m_forceRepaintAsync.needsFreshFlush = m_scheduledWhileWaitingForRenderer;
    return true;
}

void CoordinatedLayerTreeHost::sizeDidChange(const IntSize& newSize)
{
    m_coordinator.sizeDidChange(newSize);
    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::setVisibleContentsRect(const FloatRect& rect)
{
    m_coordinator.setVisibleContentsRect(rect);
    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::renderNextFrame(bool forceRepaint)
{
    m_isWaitingForRenderer = false;
    bool scheduledWhileWaitingForRenderer = std::exchange(m_scheduledWhileWaitingForRenderer, false);
    m_coordinator.renderNextFrame();

    if (m_forceRepaintAsync.callbackID) {
        // If the asynchronous force-repaint needs a separate fresh flush, it was due to
        // the force-repaint request being registered while CoordinatedLayerTreeHost was
        // waiting for the renderer to finish an update.
        ASSERT(!m_forceRepaintAsync.needsFreshFlush || scheduledWhileWaitingForRenderer);

        // Execute the callback if another layer flush and the subsequent state update
        // aren't needed. If they are, the callback will be executed when this function
        // is called after the next update.
        if (!m_forceRepaintAsync.needsFreshFlush) {
            m_webPage.send(Messages::WebPageProxy::VoidCallback(m_forceRepaintAsync.callbackID.callbackID()));
            m_forceRepaintAsync.callbackID = OptionalCallbackID();
        }
        m_forceRepaintAsync.needsFreshFlush = false;
    }

    if (scheduledWhileWaitingForRenderer || m_layerFlushTimer.isActive() || forceRepaint) {
        m_layerFlushTimer.stop();
        if (forceRepaint)
            m_coordinator.forceFrameSync();
        layerFlushTimerFired();
    }
}

void CoordinatedLayerTreeHost::didFlushRootLayer(const FloatRect& visibleContentRect)
{
    // Because our view-relative overlay root layer is not attached to the FrameView's GraphicsLayer tree, we need to flush it manually.
    if (m_viewOverlayRootLayer)
        m_viewOverlayRootLayer->flushCompositingState(visibleContentRect);
}

void CoordinatedLayerTreeHost::layerFlushTimerFired()
{
    if (m_isSuspended || m_isWaitingForRenderer)
        return;

    m_coordinator.syncDisplayState();
    m_webPage.flushPendingEditorStateUpdate();
    m_webPage.willDisplayPage();

    if (!m_isValid || !m_coordinator.rootCompositingLayer())
        return;

    // If a force-repaint callback was registered, we should force a 'frame sync' that
    // will guarantee us a call to renderNextFrame() once the update is complete.
    if (m_forceRepaintAsync.callbackID)
        m_coordinator.forceFrameSync();

    bool didSync = m_coordinator.flushPendingLayerChanges();

    if (m_notifyAfterScheduledLayerFlush && didSync) {
        m_webPage.drawingArea()->layerHostDidFlushLayers();
        m_notifyAfterScheduledLayerFlush = false;
    }
}

void CoordinatedLayerTreeHost::commitSceneState(const CoordinatedGraphicsState& state)
{
    m_isWaitingForRenderer = true;
}

void CoordinatedLayerTreeHost::flushLayersAndForceRepaint()
{
    m_coordinator.forceFrameSync();
    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::deviceOrPageScaleFactorChanged()
{
    m_coordinator.deviceOrPageScaleFactorChanged();
    m_webPage.corePage()->pageOverlayController().didChangeDeviceScaleFactor();
}

GraphicsLayerFactory* CoordinatedLayerTreeHost::graphicsLayerFactory()
{
    return &m_coordinator;
}

void CoordinatedLayerTreeHost::scheduleAnimation()
{
    if (m_isWaitingForRenderer)
        return;

    if (m_layerFlushTimer.isActive())
        return;

    scheduleLayerFlush();
    m_layerFlushTimer.startOneShot(1_s * m_coordinator.nextAnimationServiceTime());
}

} // namespace WebKit
#endif // USE(COORDINATED_GRAPHICS)
