/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2019, 2026 Igalia S.L.
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
#include "DrawingAreaCoordinatedGraphicsGLib.h"

#include "DrawingAreaProxyMessages.h"
#include "LayerTreeHost.h"
#include "MessageSenderInlines.h"
#include "NonCompositedFrameRenderer.h"
#include "WebDisplayRefreshMonitor.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPageInlines.h"
#include "WebPreferencesKeys.h"
#include "WebProcess.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/Region.h>
#include <WebCore/Settings.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/WindowEventLoop.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>

namespace WebKit {
using namespace WebCore;

DrawingAreaCoordinatedGraphics::DrawingAreaCoordinatedGraphics(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(parameters.drawingAreaIdentifier, webPage)
    , m_isPaintingSuspended(!(parameters.activityState & ActivityState::IsVisible))
{
}

DrawingAreaCoordinatedGraphics::~DrawingAreaCoordinatedGraphics() = default;

void DrawingAreaCoordinatedGraphics::setNeedsDisplay()
{
    if (!m_renderer)
        return;

    m_renderer->setNeedsDisplay();
}

void DrawingAreaCoordinatedGraphics::setNeedsDisplayInRect(const IntRect& rect)
{
    if (!m_renderer)
        return;

    m_renderer->setNeedsDisplayInRect(rect);
}

void DrawingAreaCoordinatedGraphics::scroll(const IntRect&, const IntSize&)
{
    setNeedsDisplay();
}

void DrawingAreaCoordinatedGraphics::updateRenderingWithForcedRepaint()
{
    if (!m_renderer)
        return;

    m_renderer->updateRenderingWithForcedRepaint();
}

void DrawingAreaCoordinatedGraphics::updateRenderingWithForcedRepaintAsync(WebPage&, CompletionHandler<void()>&& completionHandler)
{
    if (!m_renderer) {
        completionHandler();
        return;
    }

    m_renderer->updateRenderingWithForcedRepaintAsync(WTF::move(completionHandler));
}

void DrawingAreaCoordinatedGraphics::setLayerTreeStateIsFrozen(bool isFrozen)
{
    if (m_layerTreeStateIsFrozen == isFrozen)
        return;

    m_layerTreeStateIsFrozen = isFrozen;
    if (m_renderer)
        m_renderer->setLayerTreeStateIsFrozen(isFrozen);
}

void DrawingAreaCoordinatedGraphics::updatePreferences(const WebPreferencesStore& store)
{
    Ref page = *m_webPage->corePage();
    Settings& settings = page->settings();
#if PLATFORM(GTK)
    if (settings.hardwareAccelerationEnabled())
        WebProcess::singleton().initializePlatformDisplayIfNeeded();
#endif
    settings.setForceCompositingMode(store.getBoolValueForKey(WebPreferencesKey::forceCompositingModeKey()));
    // Fixed position elements need to be composited and create stacking contexts
    // in order to be scrolled by the ScrollingCoordinator.
    settings.setAcceleratedCompositingForFixedPositionEnabled(settings.acceleratedCompositingEnabled());

    m_supportsAsyncScrolling = settings.acceleratedCompositingEnabled() && store.getBoolValueForKey(WebPreferencesKey::threadedScrollingEnabledKey());
#if ENABLE(DEVELOPER_MODE)
    if (m_supportsAsyncScrolling) {
        auto disableAsyncScrolling = StringView::fromLatin1(getenv("WEBKIT_DISABLE_ASYNC_SCROLLING"));
        if (!disableAsyncScrolling.isEmpty() && disableAsyncScrolling == "0"_s)
            m_supportsAsyncScrolling = false;
    }
#endif

    // If async scrolling is disabled, we have to force-disable async frame and overflow scrolling
    // to keep the non-async scrolling on those elements working.
    if (!m_supportsAsyncScrolling) {
        settings.setAsyncFrameScrollingEnabled(false);
        settings.setAsyncOverflowScrollingEnabled(false);
    }
}

bool DrawingAreaCoordinatedGraphics::enterAcceleratedCompositingModeIfNeeded()
{
    Ref webPage = m_webPage;
    if (webPage->corePage()->settings().acceleratedCompositingEnabled())
        m_renderer = LayerTreeHost::create(webPage.get());
    else
        m_renderer = NonCompositedFrameRenderer::create(webPage.get());

    if (m_renderer) {
        if (m_layerTreeStateIsFrozen)
            m_renderer->setLayerTreeStateIsFrozen(true);
        if (m_isPaintingSuspended)
            m_renderer->suspend();
    }

    return true;
}

void DrawingAreaCoordinatedGraphics::backgroundColorDidChange()
{
    if (!m_renderer)
        return;

    m_renderer->backgroundColorDidChange();
}

void DrawingAreaCoordinatedGraphics::setDeviceScaleFactor(float deviceScaleFactor, CompletionHandler<void()>&& completionHandler)
{
    Ref webPage = m_webPage;
    webPage->setDeviceScaleFactor(deviceScaleFactor);
    if (m_renderer && !webPage->size().isEmpty())
        m_renderer->sizeDidChange();
    completionHandler();
}

bool DrawingAreaCoordinatedGraphics::supportsAsyncScrolling() const
{
    return m_supportsAsyncScrolling;
}

void DrawingAreaCoordinatedGraphics::registerScrollingTree()
{
#if ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
    if (m_supportsAsyncScrolling)
        WebProcess::singleton().eventDispatcher().addScrollingTreeForPage(m_webPage);
#endif // ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
}

void DrawingAreaCoordinatedGraphics::unregisterScrollingTree()
{
#if ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
    if (m_supportsAsyncScrolling)
        WebProcess::singleton().eventDispatcher().removeScrollingTreeForPage(m_webPage);
#endif // ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
}

GraphicsLayerFactory* DrawingAreaCoordinatedGraphics::graphicsLayerFactory()
{
    if (!m_renderer)
        return nullptr;

    return m_renderer->graphicsLayerFactory();
}

void DrawingAreaCoordinatedGraphics::setRootCompositingLayer(WebCore::Frame&, GraphicsLayer* graphicsLayer)
{
    if (!m_renderer)
        return;

    m_renderer->setRootCompositingLayer(graphicsLayer);
}

void DrawingAreaCoordinatedGraphics::attachViewOverlayGraphicsLayer(WebCore::FrameIdentifier, GraphicsLayer* viewOverlayRootLayer)
{
    if (!m_renderer)
        return;

    m_renderer->setViewOverlayRootLayer(viewOverlayRootLayer);
}

void DrawingAreaCoordinatedGraphics::triggerRenderingUpdate()
{
    if (!m_renderer)
        return;

    m_renderer->scheduleRenderingUpdate();
}

RefPtr<DisplayRefreshMonitor> DrawingAreaCoordinatedGraphics::createDisplayRefreshMonitor(PlatformDisplayID displayID)
{
    return WebDisplayRefreshMonitor::create(displayID);
}

void DrawingAreaCoordinatedGraphics::activityStateDidChange(OptionSet<ActivityState> changed, ActivityStateChangeID, CompletionHandler<void()>&& completionHandler)
{
    if (changed & ActivityState::IsVisible) {
        if (m_webPage->isVisible())
            resumePainting();
        else
            suspendPainting();
    }
    completionHandler();
}

void DrawingAreaCoordinatedGraphics::updateGeometry(const IntSize& size, CompletionHandler<void()>&& completionHandler)
{
    SetForScope inUpdateGeometry(m_inUpdateGeometry, true);
    Ref webPage = m_webPage;
    webPage->setSize(size);
    webPage->layoutIfNeeded();

    if (m_renderer)
        m_renderer->sizeDidChange();

    completionHandler();
}

void DrawingAreaCoordinatedGraphics::dispatchAfterEnsuringDrawing(IPC::AsyncReplyID callbackID)
{
    m_pendingAfterDrawCallbackIDs.append(callbackID);

    if (!m_renderer || !m_renderer->ensureDrawing()) {
        // We can't ensure drawing, so process pending callbacks.
        dispatchPendingCallbacksAfterEnsuringDrawing();
    }
}

void DrawingAreaCoordinatedGraphics::dispatchPendingCallbacksAfterEnsuringDrawing()
{
    if (m_pendingAfterDrawCallbackIDs.isEmpty())
        return;

    send(Messages::DrawingAreaProxy::DispatchPresentationCallbacksAfterFlushingLayers(m_pendingAfterDrawCallbackIDs));
    m_pendingAfterDrawCallbackIDs.clear();
}

#if PLATFORM(GTK)
void DrawingAreaCoordinatedGraphics::adjustTransientZoom(double scale, FloatPoint origin)
{
    if (!m_transientZoom) {
        RefPtr frameView = protect(m_webPage)->localMainFrameView();
        if (!frameView)
            return;

        FloatRect unobscuredContentRect = frameView->unobscuredContentRectIncludingScrollbars();

        m_transientZoom = true;
        m_transientZoomInitialOrigin = unobscuredContentRect.location();
    }

    if (m_renderer) {
        FloatPoint unscrolledOrigin(origin);
        unscrolledOrigin.moveBy(-m_transientZoomInitialOrigin);
        m_renderer->adjustTransientZoom(scale, origin, unscrolledOrigin);
    }
}

void DrawingAreaCoordinatedGraphics::commitTransientZoom(double scale, FloatPoint origin, CompletionHandler<void()>&& completionHandler)
{
    if (m_renderer) {
        FloatPoint unscrolledOrigin(origin);
        unscrolledOrigin.moveBy(-m_transientZoomInitialOrigin);
        m_renderer->commitTransientZoom(scale, origin, unscrolledOrigin);
    }

    m_transientZoom = false;
    completionHandler();
}
#endif

void DrawingAreaCoordinatedGraphics::suspendPainting()
{
    ASSERT(!m_isPaintingSuspended);

    if (m_renderer)
        m_renderer->suspend();

    m_isPaintingSuspended = true;
}

void DrawingAreaCoordinatedGraphics::resumePainting()
{
    if (!m_isPaintingSuspended) {
        // FIXME: We can get a call to resumePainting when painting is not suspended.
        // This happens when sending a synchronous message to create a new page. See <rdar://problem/8976531>.
        return;
    }

    m_isPaintingSuspended = false;

    if (m_renderer)
        m_renderer->resume();
}

void DrawingAreaCoordinatedGraphics::sendEnterAcceleratedCompositingModeIfNeeded()
{
    if (m_compositingAccordingToProxyMessages)
        return;

    RELEASE_ASSERT(m_renderer);
    LayerTreeContext layerTreeContext;
    layerTreeContext.contextID = m_renderer->surfaceID();

    send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(0, layerTreeContext));
    m_compositingAccordingToProxyMessages = true;
}

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM) && (USE(GBM) || OS(ANDROID))
void DrawingAreaCoordinatedGraphics::preferredBufferFormatsDidChange()
{
    if (!m_renderer)
        return;

    m_renderer->preferredBufferFormatsDidChange();
}
#endif

#if ENABLE(DAMAGE_TRACKING)
void DrawingAreaCoordinatedGraphics::resetDamageHistoryForTesting()
{
    if (!m_renderer)
        return;

    m_renderer->resetDamageHistoryForTesting();
}

void DrawingAreaCoordinatedGraphics::foreachRegionInDamageHistoryForTesting(Function<void(const Region&)>&& callback) const
{
    if (!m_renderer)
        return;

    m_renderer->foreachRegionInDamageHistoryForTesting(WTF::move(callback));
}
#endif

void DrawingAreaCoordinatedGraphics::fillGLInformation(RenderProcessInfo&& info, CompletionHandler<void(RenderProcessInfo&&)>&& completionHandler)
{
    if (!m_renderer) {
        completionHandler(WTF::move(info));
        return;
    }

    m_renderer->fillGLInformation(WTF::move(info), WTF::move(completionHandler));
}

bool DrawingAreaCoordinatedGraphics::shouldUseTiledBackingForFrameView(const WebCore::LocalFrameView& frameView) const
{
    return frameView.frame().isMainFrame() || m_webPage->corePage()->settings().asyncFrameScrollingEnabled();
}

} // namespace WebKit
