/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <WebCore/Region.h>
#include <optional>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(GTK)
#include <gtk/gtk.h>
#endif

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(DrawingAreaProxyCoordinatedGraphics);

Ref<DrawingAreaProxyCoordinatedGraphics> DrawingAreaProxyCoordinatedGraphics::create(WebPageProxy& page, WebProcessProxy& webProcessProxy)
{
    return adoptRef(*new DrawingAreaProxyCoordinatedGraphics(page, webProcessProxy));
}

DrawingAreaProxyCoordinatedGraphics::DrawingAreaProxyCoordinatedGraphics(WebPageProxy& webPageProxy, WebProcessProxy& webProcessProxy)
    : DrawingAreaProxy(webPageProxy, webProcessProxy)
#if !PLATFORM(WPE)
    , m_discardBackingStoreTimer(RunLoop::currentSingleton(), this, &DrawingAreaProxyCoordinatedGraphics::discardBackingStore)
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
void DrawingAreaProxyCoordinatedGraphics::paint(PlatformPaintContextPtr cr, const IntRect& rect, Region& unpaintedRegion)
{
    unpaintedRegion = rect;

    if (isInAcceleratedCompositingMode())
        return;

    if (!m_backingStore && !forceUpdateIfNeeded())
        return;

    m_backingStore->paint(cr, rect);
    unpaintedRegion.subtract(IntRect(IntPoint(), m_backingStore->size()));

    discardBackingStoreSoon();
}

bool DrawingAreaProxyCoordinatedGraphics::forceUpdateIfNeeded()
{
    ASSERT(!isInAcceleratedCompositingMode());

    if (!webProcessProxy().hasConnection())
        return false;

    if (webProcessProxy().state() == WebProcessProxy::State::Launching)
        return false;

    if (m_isWaitingForDidUpdateGeometry)
        return false;

    RefPtr page = this->page();
    if (!page)
        return false;

    if (!page->isViewVisible())
        return false;

    SetForScope inForceUpdate(m_inForceUpdate, true);
    send(Messages::DrawingArea::ForceUpdate());
    webProcessProxy().connection().waitForAndDispatchImmediately<Messages::DrawingAreaProxy::Update>(identifier(), 500_ms);
    return !!m_backingStore;
}

void DrawingAreaProxyCoordinatedGraphics::incorporateUpdate(UpdateInfo&& updateInfo)
{
    ASSERT(!isInAcceleratedCompositingMode());

    if (updateInfo.updateRectBounds.isEmpty())
        return;

    if (!m_backingStore || m_backingStore->size() != updateInfo.viewSize || m_backingStore->deviceScaleFactor() != updateInfo.deviceScaleFactor)
        m_backingStore = makeUnique<BackingStore>(updateInfo.viewSize, updateInfo.deviceScaleFactor);

    if (m_inForceUpdate) {
        m_backingStore->incorporateUpdate(WTFMove(updateInfo));
        return;
    }

    RefPtr page = this->page();
    if (!page)
        return;

    Region damageRegion;
    if (updateInfo.scrollRect.isEmpty()) {
        for (const auto& rect : updateInfo.updateRects)
            damageRegion.unite(rect);
    } else
        damageRegion = IntRect(IntPoint(), page->viewSize());

    m_backingStore->incorporateUpdate(WTFMove(updateInfo));
    page->setViewNeedsDisplay(damageRegion);
}
#endif

#if HAVE(DISPLAY_LINK)
std::optional<WebCore::FramesPerSecond> DrawingAreaProxyCoordinatedGraphics::displayNominalFramesPerSecond()
{
    if (!page())
        return std::nullopt;
    if (auto displayId = page()->displayID())
        return page()->legacyMainFrameProcess().nominalFramesPerSecondForDisplay(displayId.value());
    return std::nullopt;
}
#endif

void DrawingAreaProxyCoordinatedGraphics::sizeDidChange()
{
    if (!page() || !page()->hasRunningProcess())
        return;

    if (m_isWaitingForDidUpdateGeometry)
        return;

    sendUpdateGeometry();
}

void DrawingAreaProxyCoordinatedGraphics::deviceScaleFactorDidChange(CompletionHandler<void()>&& completionHandler)
{
    if (!page()) {
        completionHandler();
        return;
    }
    sendWithAsyncReply(Messages::DrawingArea::SetDeviceScaleFactor(page()->deviceScaleFactor()), WTFMove(completionHandler));
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
void DrawingAreaProxyCoordinatedGraphics::adjustTransientZoom(double scale, FloatPoint originInLayerForPageScale, FloatPoint)
{
    send(Messages::DrawingArea::AdjustTransientZoom(scale, originInLayerForPageScale));
}

void DrawingAreaProxyCoordinatedGraphics::commitTransientZoom(double scale, FloatPoint origin)
{
    sendWithAsyncReply(Messages::DrawingArea::CommitTransientZoom(scale, origin), [] { });
}
#endif

void DrawingAreaProxyCoordinatedGraphics::update(uint64_t, UpdateInfo&& updateInfo)
{
    if (m_isWaitingForDidUpdateGeometry && updateInfo.viewSize != m_lastSentSize) {
        send(Messages::DrawingArea::DisplayDidRefresh());
        return;
    }

    // FIXME: Handle the case where the view is hidden.

#if !PLATFORM(WPE)
    incorporateUpdate(WTFMove(updateInfo));
#endif

    if (!m_isWaitingForDidUpdateGeometry)
        send(Messages::DrawingArea::DisplayDidRefresh());
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

bool DrawingAreaProxyCoordinatedGraphics::alwaysUseCompositing() const
{
    if (!page())
        return false;
    return page()->preferences().acceleratedCompositingEnabled() && page()->preferences().forceCompositingMode();
}

void DrawingAreaProxyCoordinatedGraphics::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    ASSERT(!isInAcceleratedCompositingMode());
#if !PLATFORM(WPE)
    m_backingStore = nullptr;
#endif
    m_layerTreeContext = layerTreeContext;
    if (RefPtr page = this->page())
        page->enterAcceleratedCompositingMode(layerTreeContext);
}

void DrawingAreaProxyCoordinatedGraphics::exitAcceleratedCompositingMode()
{
    ASSERT(isInAcceleratedCompositingMode());

    m_layerTreeContext = { };
    if (RefPtr page = this->page())
        page->exitAcceleratedCompositingMode();
}

void DrawingAreaProxyCoordinatedGraphics::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    ASSERT(isInAcceleratedCompositingMode());

    m_layerTreeContext = layerTreeContext;
    if (RefPtr page = this->page())
        page->updateAcceleratedCompositingMode(layerTreeContext);
}

void DrawingAreaProxyCoordinatedGraphics::dispatchPresentationCallbacksAfterFlushingLayers(IPC::Connection& connection, Vector<IPC::AsyncReplyID>&& callbackIDs)
{
    for (auto& callbackID : callbackIDs) {
        if (auto callback = connection.takeAsyncReplyHandler(callbackID))
            callback(nullptr, nullptr);
    }
}

void DrawingAreaProxyCoordinatedGraphics::sendUpdateGeometry()
{
    ASSERT(!m_isWaitingForDidUpdateGeometry);
    m_lastSentSize = size();
    m_isWaitingForDidUpdateGeometry = true;

    sendWithAsyncReply(Messages::DrawingArea::UpdateGeometry(size()), [weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;
        weakThis->didUpdateGeometry();
    });
}

void DrawingAreaProxyCoordinatedGraphics::didUpdateGeometry()
{
    ASSERT(m_isWaitingForDidUpdateGeometry);

    m_isWaitingForDidUpdateGeometry = false;

    // If the view was resized while we were waiting for a DidUpdateGeometry reply from the web process,
    // we need to resend the new size here.
    if (m_lastSentSize != size())
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
    send(Messages::DrawingArea::DidDiscardBackingStore());
}
#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(DrawingAreaProxyCoordinatedGraphics::DrawingMonitor);

DrawingAreaProxyCoordinatedGraphics::DrawingMonitor::DrawingMonitor(WebPageProxy& webPage)
    : m_timer(RunLoop::mainSingleton(), this, &DrawingMonitor::stop)
{
#if USE(GLIB_EVENT_LOOP)
    m_timer.setPriority(RunLoopSourcePriority::RunLoopDispatcher);
#endif
}

DrawingAreaProxyCoordinatedGraphics::DrawingMonitor::~DrawingMonitor()
{
    if (m_callback)
        m_callback();
    stop();
}

void DrawingAreaProxyCoordinatedGraphics::DrawingMonitor::start(CompletionHandler<void()>&& callback)
{
    m_callback = WTFMove(callback);
    m_timer.startOneShot(0_s);
}

void DrawingAreaProxyCoordinatedGraphics::DrawingMonitor::stop()
{
    m_timer.stop();
    if (m_callback)
        m_callback();
}

void DrawingAreaProxyCoordinatedGraphics::dispatchAfterEnsuringDrawing(CompletionHandler<void()>&& callbackFunction)
{
    RefPtr page = this->page();
    if (!page || !page->hasRunningProcess()) {
        callbackFunction();
        return;
    }

    if (!m_drawingMonitor)
        m_drawingMonitor = makeUnique<DrawingAreaProxyCoordinatedGraphics::DrawingMonitor>(*page);
    m_drawingMonitor->start(WTFMove(callbackFunction));
}

} // namespace WebKit
