/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2016-2019 Igalia S.L.
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
#include "DrawingAreaProxyCoordinatedGraphics.h"

#include "DrawingAreaMessages.h"
#include "DrawingAreaProxyMessages.h"
#include "LayerTreeContext.h"
#include "MessageSenderInlines.h"
#include "UpdateInfo.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessProxy.h"
#include <WebCore/Region.h>

#if PLATFORM(GTK)
#include <gtk/gtk.h>
#endif

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebKit {
using namespace WebCore;

DrawingAreaProxyCoordinatedGraphics::DrawingAreaProxyCoordinatedGraphics(WebPageProxy& webPageProxy)
    : DrawingAreaProxy(DrawingAreaType::CoordinatedGraphics, webPageProxy)
#if !PLATFORM(WPE)
    , m_discardBackingStoreTimer(RunLoop::current(), this, &DrawingAreaProxyCoordinatedGraphics::discardBackingStore)
#endif
{
#if USE(GLIB_EVENT_LOOP) && !PLATFORM(WPE)
    m_discardBackingStoreTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
#endif
}

DrawingAreaProxyCoordinatedGraphics::~DrawingAreaProxyCoordinatedGraphics()
{
    // Make sure to exit accelerated compositing mode.
    if (isInAcceleratedCompositingMode())
        exitAcceleratedCompositingMode();
}

#if !PLATFORM(WPE)
void DrawingAreaProxyCoordinatedGraphics::paint(BackingStore::PlatformGraphicsContext context, const IntRect& rect, Region& unpaintedRegion)
{
    unpaintedRegion = rect;

    if (isInAcceleratedCompositingMode())
        return;

    if (!m_backingStore && !forceUpdateIfNeeded())
        return;

    m_backingStore->paint(context, rect);
    unpaintedRegion.subtract(IntRect(IntPoint(), m_backingStore->size()));

    discardBackingStoreSoon();
}

bool DrawingAreaProxyCoordinatedGraphics::forceUpdateIfNeeded()
{
    ASSERT(!isInAcceleratedCompositingMode());

    if (!m_webPageProxy.hasRunningProcess())
        return false;

    if (m_webPageProxy.process().state() == WebProcessProxy::State::Launching)
        return false;

    if (m_isWaitingForDidUpdateGeometry)
        return false;

    if (!m_webPageProxy.isViewVisible())
        return false;

    SetForScope inForceUpdate(m_inForceUpdate, true);
    m_webPageProxy.send(Messages::DrawingArea::ForceUpdate(), m_identifier);
    m_webPageProxy.process().connection()->waitForAndDispatchImmediately<Messages::DrawingAreaProxy::Update>(m_identifier, Seconds::fromMilliseconds(500));
    return !!m_backingStore;
}

void DrawingAreaProxyCoordinatedGraphics::incorporateUpdate(UpdateInfo&& updateInfo)
{
    ASSERT(!isInAcceleratedCompositingMode());

    if (updateInfo.updateRectBounds.isEmpty())
        return;

    if (!m_backingStore || m_backingStore->size() != updateInfo.viewSize || m_backingStore->deviceScaleFactor() != updateInfo.deviceScaleFactor)
        m_backingStore = makeUnique<BackingStore>(updateInfo.viewSize, updateInfo.deviceScaleFactor, m_webPageProxy);

    if (m_inForceUpdate) {
        m_backingStore->incorporateUpdate(WTFMove(updateInfo));
        return;
    }

    Region damageRegion;
    if (updateInfo.scrollRect.isEmpty()) {
        for (const auto& rect : updateInfo.updateRects)
            damageRegion.unite(rect);
    } else
        damageRegion = IntRect(IntPoint(), m_webPageProxy.viewSize());

    m_backingStore->incorporateUpdate(WTFMove(updateInfo));
    m_webPageProxy.setViewNeedsDisplay(damageRegion);
}
#endif

void DrawingAreaProxyCoordinatedGraphics::sizeDidChange()
{
    if (!m_webPageProxy.hasRunningProcess())
        return;

    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void DrawingAreaProxyCoordinatedGraphics::deviceScaleFactorDidChange()
{
    m_webPageProxy.send(Messages::DrawingArea::SetDeviceScaleFactor(m_webPageProxy.deviceScaleFactor()), m_identifier);
}

void DrawingAreaProxyCoordinatedGraphics::waitForBackingStoreUpdateOnNextPaint()
{
    m_hasReceivedFirstUpdate = true;
}

void DrawingAreaProxyCoordinatedGraphics::setBackingStoreIsDiscardable(bool isBackingStoreDiscardable)
{
#if !PLATFORM(WPE)
    if (m_isBackingStoreDiscardable == isBackingStoreDiscardable)
        return;

    m_isBackingStoreDiscardable = isBackingStoreDiscardable;
    if (m_isBackingStoreDiscardable)
        discardBackingStoreSoon();
    else
        m_discardBackingStoreTimer.stop();
#endif
}

#if PLATFORM(GTK)
void DrawingAreaProxyCoordinatedGraphics::adjustTransientZoom(double scale, FloatPoint origin)
{
    m_webPageProxy.send(Messages::DrawingArea::AdjustTransientZoom(scale, origin), m_identifier);
}

void DrawingAreaProxyCoordinatedGraphics::commitTransientZoom(double scale, FloatPoint origin)
{
    m_webPageProxy.send(Messages::DrawingArea::CommitTransientZoom(scale, origin), m_identifier);
}
#endif

void DrawingAreaProxyCoordinatedGraphics::update(uint64_t, UpdateInfo&& updateInfo)
{
    if (m_isWaitingForDidUpdateGeometry && updateInfo.viewSize != m_lastSentSize) {
        m_webPageProxy.send(Messages::DrawingArea::DisplayDidRefresh(), m_identifier);
        return;
    }

    // FIXME: Handle the case where the view is hidden.

#if !PLATFORM(WPE)
    incorporateUpdate(WTFMove(updateInfo));
#endif

    if (!m_isWaitingForDidUpdateGeometry)
        m_webPageProxy.send(Messages::DrawingArea::DisplayDidRefresh(), m_identifier);
}

void DrawingAreaProxyCoordinatedGraphics::enterAcceleratedCompositingMode(uint64_t, const LayerTreeContext& layerTreeContext)
{
    enterAcceleratedCompositingMode(layerTreeContext);
}

void DrawingAreaProxyCoordinatedGraphics::exitAcceleratedCompositingMode(uint64_t, UpdateInfo&& updateInfo)
{
    exitAcceleratedCompositingMode();
#if !PLATFORM(WPE)
    incorporateUpdate(WTFMove(updateInfo));
#endif
}

void DrawingAreaProxyCoordinatedGraphics::updateAcceleratedCompositingMode(uint64_t, const LayerTreeContext& layerTreeContext)
{
    updateAcceleratedCompositingMode(layerTreeContext);
}

void DrawingAreaProxyCoordinatedGraphics::targetRefreshRateDidChange(unsigned rate)
{
    m_webPageProxy.send(Messages::DrawingArea::TargetRefreshRateDidChange(rate), m_identifier);
}

bool DrawingAreaProxyCoordinatedGraphics::alwaysUseCompositing() const
{
    return m_webPageProxy.preferences().acceleratedCompositingEnabled() && m_webPageProxy.preferences().forceCompositingMode();
}

void DrawingAreaProxyCoordinatedGraphics::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    ASSERT(!isInAcceleratedCompositingMode());
#if !PLATFORM(WPE)
    m_backingStore = nullptr;
#endif
    m_layerTreeContext = layerTreeContext;
    m_webPageProxy.enterAcceleratedCompositingMode(layerTreeContext);
}

void DrawingAreaProxyCoordinatedGraphics::exitAcceleratedCompositingMode()
{
    ASSERT(isInAcceleratedCompositingMode());

    m_layerTreeContext = { };
    m_webPageProxy.exitAcceleratedCompositingMode();
}

void DrawingAreaProxyCoordinatedGraphics::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    ASSERT(isInAcceleratedCompositingMode());

    m_layerTreeContext = layerTreeContext;
    m_webPageProxy.updateAcceleratedCompositingMode(layerTreeContext);
}

void DrawingAreaProxyCoordinatedGraphics::sendUpdateGeometry()
{
    ASSERT(!m_isWaitingForDidUpdateGeometry);
    m_lastSentSize = m_size;
    m_isWaitingForDidUpdateGeometry = true;

    m_webPageProxy.sendWithAsyncReply(Messages::DrawingArea::UpdateGeometry(m_size), [weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;
        weakThis->didUpdateGeometry();
    }, m_identifier);
}

void DrawingAreaProxyCoordinatedGraphics::didUpdateGeometry()
{
    ASSERT(m_isWaitingForDidUpdateGeometry);

    m_isWaitingForDidUpdateGeometry = false;

    // If the view was resized while we were waiting for a DidUpdateGeometry reply from the web process,
    // we need to resend the new size here.
    if (m_lastSentSize != m_size)
        sendUpdateGeometry();
}

#if !PLATFORM(WPE)
void DrawingAreaProxyCoordinatedGraphics::discardBackingStoreSoon()
{
    if (!m_backingStore || !m_isBackingStoreDiscardable || m_discardBackingStoreTimer.isActive())
        return;

    // We'll wait this many seconds after the last paint before throwing away our backing store to save memory.
    // FIXME: It would be smarter to make this delay based on how expensive painting is. See <http://webkit.org/b/55733>.
    static const Seconds discardBackingStoreDelay = 10_s;

    m_discardBackingStoreTimer.startOneShot(discardBackingStoreDelay);
}

void DrawingAreaProxyCoordinatedGraphics::discardBackingStore()
{
    if (!m_backingStore)
        return;

    m_backingStore = nullptr;
    m_webPageProxy.send(Messages::DrawingArea::DidDiscardBackingStore(), m_identifier);
}
#endif

DrawingAreaProxyCoordinatedGraphics::DrawingMonitor::DrawingMonitor(WebPageProxy& webPage)
    : m_timer(RunLoop::main(), this, &DrawingMonitor::stop)
#if PLATFORM(GTK)
    , m_webPage(webPage)
#endif
{
#if USE(GLIB_EVENT_LOOP)
#if PLATFORM(GTK)
    // Give redraws more priority.
    m_timer.setPriority(GDK_PRIORITY_REDRAW - 10);
#else
    m_timer.setPriority(RunLoopSourcePriority::RunLoopDispatcher);
#endif
#endif
}

DrawingAreaProxyCoordinatedGraphics::DrawingMonitor::~DrawingMonitor()
{
    if (m_callback)
        m_callback();
    stop();
}

int DrawingAreaProxyCoordinatedGraphics::DrawingMonitor::webViewDrawCallback(DrawingAreaProxyCoordinatedGraphics::DrawingMonitor* monitor)
{
    monitor->didDraw();
    return false;
}

void DrawingAreaProxyCoordinatedGraphics::DrawingMonitor::start(CompletionHandler<void()>&& callback)
{
    m_startTime = MonotonicTime::now();
    m_callback = WTFMove(callback);
#if PLATFORM(GTK)
    gtk_widget_queue_draw(m_webPage.viewWidget());
#if USE(GTK4)
    m_timer.startOneShot(16_ms);
#else
    g_signal_connect_swapped(m_webPage.viewWidget(), "draw", reinterpret_cast<GCallback>(webViewDrawCallback), this);
    m_timer.startOneShot(100_ms);
#endif
#else
    m_timer.startOneShot(0_s);
#endif
}

void DrawingAreaProxyCoordinatedGraphics::DrawingMonitor::stop()
{
    m_timer.stop();
#if PLATFORM(GTK) && !USE(GTK4)
    g_signal_handlers_disconnect_by_func(m_webPage.viewWidget(), reinterpret_cast<gpointer>(webViewDrawCallback), this);
#endif
    m_startTime = MonotonicTime();
    if (m_callback)
        m_callback();
}

void DrawingAreaProxyCoordinatedGraphics::DrawingMonitor::didDraw()
{
    // We wait up to 100 milliseconds for draw events. If there are several draw events queued quickly,
    // we want to wait until all of them have been processed, so after receiving a draw, we wait
    // for the next frame or stop.
    if (MonotonicTime::now() - m_startTime > 100_ms)
        stop();
    else
        m_timer.startOneShot(16_ms);
}

void DrawingAreaProxyCoordinatedGraphics::dispatchAfterEnsuringDrawing(CompletionHandler<void()>&& callbackFunction)
{
    if (!m_webPageProxy.hasRunningProcess()) {
        callbackFunction();
        return;
    }

    if (!m_drawingMonitor)
        m_drawingMonitor = makeUnique<DrawingAreaProxyCoordinatedGraphics::DrawingMonitor>(m_webPageProxy);
    m_drawingMonitor->start(WTFMove(callbackFunction));
}

} // namespace WebKit
