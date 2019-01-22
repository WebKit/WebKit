/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "DrawingAreaProxyImpl.h"

#include "DrawingAreaMessages.h"
#include "DrawingAreaProxyMessages.h"
#include "LayerTreeContext.h"
#include "UpdateInfo.h"
#include "WebPageGroup.h"
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

DrawingAreaProxyImpl::DrawingAreaProxyImpl(WebPageProxy& webPageProxy, WebProcessProxy& process)
    : AcceleratedDrawingAreaProxy(webPageProxy, process)
    , m_discardBackingStoreTimer(RunLoop::current(), this, &DrawingAreaProxyImpl::discardBackingStore)
{
#if USE(GLIB_EVENT_LOOP)
    m_discardBackingStoreTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
#endif
}

DrawingAreaProxyImpl::~DrawingAreaProxyImpl()
{
}

void DrawingAreaProxyImpl::paint(BackingStore::PlatformGraphicsContext context, const IntRect& rect, Region& unpaintedRegion)
{
    unpaintedRegion = rect;

    if (isInAcceleratedCompositingMode())
        return;

    ASSERT(m_currentBackingStoreStateID <= m_nextBackingStoreStateID);
    if (m_currentBackingStoreStateID < m_nextBackingStoreStateID) {
        // Tell the web process to do a full backing store update now, in case we previously told
        // it about our next state but didn't request an immediate update.
        sendUpdateBackingStoreState(RespondImmediately);

        // If we haven't yet received our first bits from the WebProcess then don't paint anything.
        if (!m_hasReceivedFirstUpdate)
            return;

        if (m_isWaitingForDidUpdateBackingStoreState) {
            // Wait for a DidUpdateBackingStoreState message that contains the new bits before we paint
            // what's currently in the backing store.
            waitForAndDispatchDidUpdateBackingStoreState();
        }

        // Dispatching DidUpdateBackingStoreState (either beneath sendUpdateBackingStoreState or
        // beneath waitForAndDispatchDidUpdateBackingStoreState) could destroy our backing store or
        // change the compositing mode.
        if (!m_backingStore || isInAcceleratedCompositingMode())
            return;
    } else {
        ASSERT(!m_isWaitingForDidUpdateBackingStoreState);
        if (!m_backingStore) {
            // The view has asked us to paint before the web process has painted anything. There's
            // nothing we can do.
            return;
        }
    }

    m_backingStore->paint(context, rect);
    unpaintedRegion.subtract(IntRect(IntPoint(), m_backingStore->size()));

    discardBackingStoreSoon();
}

void DrawingAreaProxyImpl::setBackingStoreIsDiscardable(bool isBackingStoreDiscardable)
{
    if (m_isBackingStoreDiscardable == isBackingStoreDiscardable)
        return;

    m_isBackingStoreDiscardable = isBackingStoreDiscardable;
    if (m_isBackingStoreDiscardable)
        discardBackingStoreSoon();
    else
        m_discardBackingStoreTimer.stop();
}

void DrawingAreaProxyImpl::update(uint64_t backingStoreStateID, const UpdateInfo& updateInfo)
{
    ASSERT_ARG(backingStoreStateID, backingStoreStateID <= m_currentBackingStoreStateID);
    if (backingStoreStateID < m_currentBackingStoreStateID)
        return;

    // FIXME: Handle the case where the view is hidden.

    incorporateUpdate(updateInfo);
    process().send(Messages::DrawingArea::DidUpdate(), m_webPageProxy.pageID());
}

void DrawingAreaProxyImpl::didUpdateBackingStoreState(uint64_t backingStoreStateID, const UpdateInfo& updateInfo, const LayerTreeContext& layerTreeContext)
{
    AcceleratedDrawingAreaProxy::didUpdateBackingStoreState(backingStoreStateID, updateInfo, layerTreeContext);
    if (isInAcceleratedCompositingMode()) {
        ASSERT(!m_backingStore);
        return;
    }

    // If we have a backing store the right size, reuse it.
    if (m_backingStore && (m_backingStore->size() != updateInfo.viewSize || m_backingStore->deviceScaleFactor() != updateInfo.deviceScaleFactor))
        m_backingStore = nullptr;
    incorporateUpdate(updateInfo);
}

void DrawingAreaProxyImpl::exitAcceleratedCompositingMode(uint64_t backingStoreStateID, const UpdateInfo& updateInfo)
{
    ASSERT_ARG(backingStoreStateID, backingStoreStateID <= m_currentBackingStoreStateID);
    if (backingStoreStateID < m_currentBackingStoreStateID)
        return;

    AcceleratedDrawingAreaProxy::exitAcceleratedCompositingMode();

    incorporateUpdate(updateInfo);
}

void DrawingAreaProxyImpl::incorporateUpdate(const UpdateInfo& updateInfo)
{
    ASSERT(!isInAcceleratedCompositingMode());

    if (updateInfo.updateRectBounds.isEmpty())
        return;

    if (!m_backingStore)
        m_backingStore = std::make_unique<BackingStore>(updateInfo.viewSize, updateInfo.deviceScaleFactor, m_webPageProxy);

    m_backingStore->incorporateUpdate(updateInfo);

    Region damageRegion;
    if (updateInfo.scrollRect.isEmpty()) {
        for (const auto& rect : updateInfo.updateRects)
            damageRegion.unite(rect);
    } else
        damageRegion = IntRect(IntPoint(), m_webPageProxy.viewSize());
    m_webPageProxy.setViewNeedsDisplay(damageRegion);
}

void DrawingAreaProxyImpl::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    m_backingStore = nullptr;
    AcceleratedDrawingAreaProxy::enterAcceleratedCompositingMode(layerTreeContext);
}

void DrawingAreaProxyImpl::discardBackingStoreSoon()
{
    if (!m_backingStore || !m_isBackingStoreDiscardable || m_discardBackingStoreTimer.isActive())
        return;

    // We'll wait this many seconds after the last paint before throwing away our backing store to save memory.
    // FIXME: It would be smarter to make this delay based on how expensive painting is. See <http://webkit.org/b/55733>.
    static const Seconds discardBackingStoreDelay = 2_s;

    m_discardBackingStoreTimer.startOneShot(discardBackingStoreDelay);
}

void DrawingAreaProxyImpl::discardBackingStore()
{
    if (!m_backingStore)
        return;
    m_backingStore = nullptr;
    backingStoreStateDidChange(DoNotRespondImmediately);
}

DrawingAreaProxyImpl::DrawingMonitor::DrawingMonitor(WebPageProxy& webPage)
    : m_timer(RunLoop::main(), this, &DrawingMonitor::stop)
#if PLATFORM(GTK)
    , m_webPage(webPage)
#endif
{
#if USE(GLIB_EVENT_LOOP)
    // Give redraws more priority.
    m_timer.setPriority(GDK_PRIORITY_REDRAW - 10);
#endif
}

DrawingAreaProxyImpl::DrawingMonitor::~DrawingMonitor()
{
    m_callback = nullptr;
    stop();
}

int DrawingAreaProxyImpl::DrawingMonitor::webViewDrawCallback(DrawingAreaProxyImpl::DrawingMonitor* monitor)
{
    monitor->didDraw();
    return FALSE;
}

void DrawingAreaProxyImpl::DrawingMonitor::start(WTF::Function<void (CallbackBase::Error)>&& callback)
{
    m_startTime = MonotonicTime::now();
    m_callback = WTFMove(callback);
#if PLATFORM(GTK)
    g_signal_connect_swapped(m_webPage.viewWidget(), "draw", reinterpret_cast<GCallback>(webViewDrawCallback), this);
    m_timer.startOneShot(1_s);
#else
    m_timer.startOneShot(0_s);
#endif
}

void DrawingAreaProxyImpl::DrawingMonitor::stop()
{
    m_timer.stop();
#if PLATFORM(GTK)
    g_signal_handlers_disconnect_by_func(m_webPage.viewWidget(), reinterpret_cast<gpointer>(webViewDrawCallback), this);
#endif
    m_startTime = MonotonicTime();
    if (m_callback) {
        m_callback(CallbackBase::Error::None);
        m_callback = nullptr;
    }
}

void DrawingAreaProxyImpl::DrawingMonitor::didDraw()
{
    // We wait up to 1 second for draw events. If there are several draw events queued quickly,
    // we want to wait until all of them have been processed, so after receiving a draw, we wait
    // up to 100ms for the next one or stop.
    if (MonotonicTime::now() - m_startTime > 1_s)
        stop();
    else
        m_timer.startOneShot(100_ms);
}

void DrawingAreaProxyImpl::dispatchAfterEnsuringDrawing(WTF::Function<void(CallbackBase::Error)>&& callbackFunction)
{
    if (!m_webPageProxy.isValid()) {
        callbackFunction(CallbackBase::Error::OwnerWasInvalidated);
        return;
    }

    if (!m_drawingMonitor)
        m_drawingMonitor = std::make_unique<DrawingAreaProxyImpl::DrawingMonitor>(m_webPageProxy);
    m_drawingMonitor->start(WTFMove(callbackFunction));
}

} // namespace WebKit
