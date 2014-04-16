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

#include "CoordinatedDrawingArea.h"
#include "CoordinatedGraphicsArgumentCoders.h"
#include "CoordinatedLayerTreeHostProxyMessages.h"
#include "GraphicsContext.h"
#include "WebCoordinatedSurface.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/Settings.h>
#include <wtf/CurrentTime.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<CoordinatedLayerTreeHost> CoordinatedLayerTreeHost::create(WebPage* webPage)
{
    return adoptRef(new CoordinatedLayerTreeHost(webPage));
}

CoordinatedLayerTreeHost::~CoordinatedLayerTreeHost()
{
}

CoordinatedLayerTreeHost::CoordinatedLayerTreeHost(WebPage* webPage)
    : LayerTreeHost(webPage)
    , m_notifyAfterScheduledLayerFlush(false)
    , m_isValid(true)
    , m_isSuspended(false)
    , m_isWaitingForRenderer(true)
    , m_layerFlushTimer(this, &CoordinatedLayerTreeHost::layerFlushTimerFired)
    , m_layerFlushSchedulingEnabled(true)
    , m_forceRepaintAsyncCallbackID(0)
{
    m_coordinator = std::make_unique<CompositingCoordinator>(webPage->corePage(), this);

    m_coordinator->createRootLayer(webPage->size());
    m_layerTreeContext.contextID = toCoordinatedGraphicsLayer(m_coordinator->rootLayer())->id();

    CoordinatedSurface::setFactory(createCoordinatedSurface);

    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::setLayerFlushSchedulingEnabled(bool layerFlushingEnabled)
{
    if (m_layerFlushSchedulingEnabled == layerFlushingEnabled)
        return;

    m_layerFlushSchedulingEnabled = layerFlushingEnabled;

    if (m_layerFlushSchedulingEnabled) {
        scheduleLayerFlush();
        return;
    }

    cancelPendingLayerFlush();
}

void CoordinatedLayerTreeHost::scheduleLayerFlush()
{
    if (!m_layerFlushSchedulingEnabled)
        return;

    if (!m_layerFlushTimer.isActive() || m_layerFlushTimer.nextFireInterval() > 0)
        m_layerFlushTimer.startOneShot(0);
}

void CoordinatedLayerTreeHost::cancelPendingLayerFlush()
{
    m_layerFlushTimer.stop();
}

void CoordinatedLayerTreeHost::setShouldNotifyAfterNextScheduledLayerFlush(bool notifyAfterScheduledLayerFlush)
{
    m_notifyAfterScheduledLayerFlush = notifyAfterScheduledLayerFlush;
}

void CoordinatedLayerTreeHost::setRootCompositingLayer(WebCore::GraphicsLayer* graphicsLayer)
{
    m_coordinator->setRootCompositingLayer(graphicsLayer, m_webPage->pageOverlayController().viewOverlayRootLayer());
}

void CoordinatedLayerTreeHost::invalidate()
{
    cancelPendingLayerFlush();

    ASSERT(m_isValid);
    m_coordinator->clearRootLayer();
    m_isValid = false;
}

void CoordinatedLayerTreeHost::forceRepaint()
{
    // This is necessary for running layout tests. Since in this case we are not waiting for a UIProcess to reply nicely.
    // Instead we are just triggering forceRepaint. But we still want to have the scripted animation callbacks being executed.
    m_coordinator->syncDisplayState();

    // We need to schedule another flush, otherwise the forced paint might cancel a later expected flush.
    // This is aligned with LayerTreeHostCA.
    scheduleLayerFlush();

    if (m_isWaitingForRenderer)
        return;

    m_coordinator->flushPendingLayerChanges();
}

bool CoordinatedLayerTreeHost::forceRepaintAsync(uint64_t callbackID)
{
    // We expect the UI process to not require a new repaint until the previous one has finished.
    ASSERT(!m_forceRepaintAsyncCallbackID);
    m_forceRepaintAsyncCallbackID = callbackID;
    scheduleLayerFlush();
    return true;
}

void CoordinatedLayerTreeHost::sizeDidChange(const WebCore::IntSize& newSize)
{
    m_coordinator->sizeDidChange(newSize);
    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::setVisibleContentsRect(const FloatRect& rect, const FloatPoint& trajectoryVector)
{
    m_coordinator->setVisibleContentsRect(rect, trajectoryVector);
    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::renderNextFrame()
{
    m_isWaitingForRenderer = false;
    scheduleLayerFlush();
    m_coordinator->renderNextFrame();
}

void CoordinatedLayerTreeHost::purgeBackingStores()
{
    m_coordinator->purgeBackingStores();
}

void CoordinatedLayerTreeHost::didFlushRootLayer(const FloatRect& visibleContentRect)
{
    m_webPage->pageOverlayController().flushPageOverlayLayers(visibleContentRect);
}

void CoordinatedLayerTreeHost::performScheduledLayerFlush()
{
    if (m_isSuspended || m_isWaitingForRenderer)
        return;

    m_coordinator->syncDisplayState();

    if (!m_isValid)
        return;

    bool didSync = m_coordinator->flushPendingLayerChanges();

    if (m_forceRepaintAsyncCallbackID) {
        m_webPage->send(Messages::WebPageProxy::VoidCallback(m_forceRepaintAsyncCallbackID));
        m_forceRepaintAsyncCallbackID = 0;
    }

    if (m_notifyAfterScheduledLayerFlush && didSync) {
        static_cast<CoordinatedDrawingArea*>(m_webPage->drawingArea())->layerHostDidFlushLayers();
        m_notifyAfterScheduledLayerFlush = false;
    }
}

void CoordinatedLayerTreeHost::layerFlushTimerFired(Timer<CoordinatedLayerTreeHost>*)
{
    performScheduledLayerFlush();
}

void CoordinatedLayerTreeHost::paintLayerContents(const GraphicsLayer* graphicsLayer, GraphicsContext& graphicsContext, const IntRect& clipRect)
{
}

void CoordinatedLayerTreeHost::commitSceneState(const WebCore::CoordinatedGraphicsState& state)
{
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::CommitCoordinatedGraphicsState(state));
    m_isWaitingForRenderer = true;
}

PassRefPtr<CoordinatedSurface> CoordinatedLayerTreeHost::createCoordinatedSurface(const IntSize& size, CoordinatedSurface::Flags flags)
{
    return WebCoordinatedSurface::create(size, flags);
}

void CoordinatedLayerTreeHost::deviceOrPageScaleFactorChanged()
{
    m_coordinator->deviceOrPageScaleFactorChanged();
    m_webPage->pageOverlayController().didChangeDeviceScaleFactor();
}

void CoordinatedLayerTreeHost::pageBackgroundTransparencyChanged()
{
}

GraphicsLayerFactory* CoordinatedLayerTreeHost::graphicsLayerFactory()
{
    return m_coordinator.get();
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
void CoordinatedLayerTreeHost::scheduleAnimation()
{
    if (m_isWaitingForRenderer)
        return;

    if (m_layerFlushTimer.isActive())
        return;

    scheduleLayerFlush();
    m_layerFlushTimer.startOneShot(m_coordinator->nextAnimationServiceTime());
}
#endif

void CoordinatedLayerTreeHost::setBackgroundColor(const WebCore::Color& color)
{
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::SetBackgroundColor(color));
}

void CoordinatedLayerTreeHost::commitScrollOffset(uint32_t layerID, const WebCore::IntSize& offset)
{
    m_coordinator->commitScrollOffset(layerID, offset);
}

} // namespace WebKit
#endif // USE(COORDINATED_GRAPHICS)
