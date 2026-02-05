/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2019 Igalia S.L.
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
#include "UpdateInfo.h"
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
#include <wtf/SetForScope.h>

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebKit {
using namespace WebCore;

DrawingAreaCoordinatedGraphics::DrawingAreaCoordinatedGraphics(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(parameters.drawingAreaIdentifier, webPage)
    , m_isPaintingSuspended(!(parameters.activityState & ActivityState::IsVisible))
    , m_displayTimer(RunLoop::mainSingleton(), "DrawingAreaCoordinatedGraphics::DisplayTimer"_s, this, &DrawingAreaCoordinatedGraphics::displayTimerFired)
{
#if !PLATFORM(WPE)
    m_displayTimer.setPriority(RunLoopSourcePriority::NonAcceleratedDrawingTimer);
#endif
}

DrawingAreaCoordinatedGraphics::~DrawingAreaCoordinatedGraphics() = default;

void DrawingAreaCoordinatedGraphics::setNeedsDisplay()
{
    if (m_layerTreeHost) {
        ASSERT(m_dirtyRegion.isEmpty());
        return;
    }

    setNeedsDisplayInRect(m_webPage->bounds());
}

void DrawingAreaCoordinatedGraphics::setNeedsDisplayInRect(const IntRect& rect)
{
    if (m_layerTreeHost) {
        ASSERT(m_dirtyRegion.isEmpty());
        return;
    }

    IntRect dirtyRect = rect;
    dirtyRect.intersect(m_webPage->bounds());
    if (dirtyRect.isEmpty())
        return;

    if (m_nonCompositedFrameRenderer) {
        ASSERT(m_dirtyRegion.isEmpty());
        m_nonCompositedFrameRenderer->setNeedsDisplayInRect(dirtyRect);
        scheduleDisplay();
        return;
    }

    m_dirtyRegion.unite(dirtyRect);
    scheduleDisplay();
}

void DrawingAreaCoordinatedGraphics::scroll(const IntRect& scrollRect, const IntSize& scrollDelta)
{
    if (m_layerTreeHost) {
        ASSERT(m_scrollRect.isEmpty());
        ASSERT(m_scrollOffset.isEmpty());
        ASSERT(m_dirtyRegion.isEmpty());
        return;
    }

    if (m_nonCompositedFrameRenderer) {
        m_nonCompositedFrameRenderer->setNeedsDisplayInRect(m_webPage->bounds());
        scheduleDisplay();
        return;
    }

    if (scrollRect.isEmpty())
        return;

    if (!m_scrollRect.isEmpty() && scrollRect != m_scrollRect) {
        unsigned scrollArea = scrollRect.width() * scrollRect.height();
        unsigned currentScrollArea = m_scrollRect.width() * m_scrollRect.height();

        if (currentScrollArea >= scrollArea) {
            // The rect being scrolled is at least as large as the rect we'd like to scroll.
            // Go ahead and just invalidate the scroll rect.
            setNeedsDisplayInRect(scrollRect);
            return;
        }

        // Just repaint the entire current scroll rect, we'll scroll the new rect instead.
        setNeedsDisplayInRect(m_scrollRect);
        m_scrollRect = IntRect();
        m_scrollOffset = IntSize();
    }

    // Get the part of the dirty region that is in the scroll rect.
    WebCore::Region dirtyRegionInScrollRect = intersect(scrollRect, m_dirtyRegion);
    if (!dirtyRegionInScrollRect.isEmpty()) {
        // There are parts of the dirty region that are inside the scroll rect.
        // We need to subtract them from the region, move them and re-add them.
        m_dirtyRegion.subtract(scrollRect);

        // Move the dirty parts.
        WebCore::Region movedDirtyRegionInScrollRect = intersect(translate(dirtyRegionInScrollRect, scrollDelta), scrollRect);

        // And add them back.
        m_dirtyRegion.unite(movedDirtyRegionInScrollRect);
    }

    // Compute the scroll repaint region.
    WebCore::Region scrollRepaintRegion = subtract(scrollRect, translate(scrollRect, scrollDelta));

    m_dirtyRegion.unite(scrollRepaintRegion);
    scheduleDisplay();

    m_scrollRect = scrollRect;
    m_scrollOffset += scrollDelta;
}

void DrawingAreaCoordinatedGraphics::updateRenderingWithForcedRepaint()
{
    if (!m_layerTreeHost) {
        if (m_nonCompositedFrameRenderer) {
            m_nonCompositedFrameRenderer->setNeedsDisplayInRect(m_webPage->bounds());
            display();
            return;
        }
        m_isWaitingForDidUpdate = false;
        m_dirtyRegion = m_webPage->bounds();
        display();
        return;
    }

    if (!m_layerTreeStateIsFrozen)
        m_layerTreeHost->updateRenderingWithForcedRepaint();
}

void DrawingAreaCoordinatedGraphics::updateRenderingWithForcedRepaintAsync(WebPage& page, CompletionHandler<void()>&& completionHandler)
{
    if (!m_layerTreeHost) {
        updateRenderingWithForcedRepaint();
        return completionHandler();
    }

    if (m_layerTreeStateIsFrozen)
        return completionHandler();

    m_layerTreeHost->updateRenderingWithForcedRepaintAsync(WTF::move(completionHandler));
}

void DrawingAreaCoordinatedGraphics::setLayerTreeStateIsFrozen(bool isFrozen)
{
    if (m_layerTreeStateIsFrozen == isFrozen)
        return;

    m_layerTreeStateIsFrozen = isFrozen;

    if (m_layerTreeHost)
        m_layerTreeHost->setLayerTreeStateIsFrozen(isFrozen);
    else if (!isFrozen)
        scheduleDisplay();
}

void DrawingAreaCoordinatedGraphics::updatePreferences(const WebPreferencesStore& store)
{
    Settings& settings = m_webPage->corePage()->settings();
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
    if (m_webPage->corePage()->settings().acceleratedCompositingEnabled()) {
        m_layerTreeHost = makeUnique<LayerTreeHost>(m_webPage);
        if (m_isPaintingSuspended)
            m_layerTreeHost->pauseRendering();
    } else
        m_nonCompositedFrameRenderer = NonCompositedFrameRenderer::create(m_webPage);
    return true;
}

void DrawingAreaCoordinatedGraphics::backgroundColorDidChange()
{
    if (m_layerTreeHost)
        m_layerTreeHost->backgroundColorDidChange();
}

void DrawingAreaCoordinatedGraphics::setDeviceScaleFactor(float deviceScaleFactor, CompletionHandler<void()>&& completionHandler)
{
    Ref webPage = m_webPage.get();
    webPage->setDeviceScaleFactor(deviceScaleFactor);
    if (m_layerTreeHost && !webPage->size().isEmpty())
        m_layerTreeHost->sizeDidChange();
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
    RELEASE_ASSERT(m_layerTreeHost);
    return m_layerTreeHost->graphicsLayerFactory();
}

void DrawingAreaCoordinatedGraphics::setRootCompositingLayer(WebCore::Frame&, GraphicsLayer* graphicsLayer)
{
    if (m_layerTreeHost)
        m_layerTreeHost->setRootCompositingLayer(graphicsLayer);
}

void DrawingAreaCoordinatedGraphics::triggerRenderingUpdate()
{
    if (m_layerTreeStateIsFrozen)
        return;

    if (m_layerTreeHost)
        m_layerTreeHost->scheduleRenderingUpdate();
    else
        scheduleDisplay();
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

void DrawingAreaCoordinatedGraphics::attachViewOverlayGraphicsLayer(WebCore::FrameIdentifier, GraphicsLayer* viewOverlayRootLayer)
{
    if (m_layerTreeHost)
        m_layerTreeHost->setViewOverlayRootLayer(viewOverlayRootLayer);
}

void DrawingAreaCoordinatedGraphics::updateGeometry(const IntSize& size, CompletionHandler<void()>&& completionHandler)
{
    SetForScope inUpdateGeometry(m_inUpdateGeometry, true);
    Ref webPage = m_webPage.get();
    webPage->setSize(size);
    webPage->layoutIfNeeded();

    if (m_layerTreeHost)
        m_layerTreeHost->sizeDidChange();
    else if (m_nonCompositedFrameRenderer) {
        m_nonCompositedFrameRenderer->setNeedsDisplayInRect({ { }, size });
        m_nonCompositedFrameRenderer->display();
    } else {
        m_dirtyRegion = IntRect(IntPoint(), size);
        UpdateInfo updateInfo;
        if (m_isPaintingSuspended) {
            updateInfo.viewSize = webPage->size();
            updateInfo.deviceScaleFactor = webPage->corePage()->deviceScaleFactor();
        } else
            display(updateInfo);
        if (!m_layerTreeHost)
            send(Messages::DrawingAreaProxy::Update(0, WTF::move(updateInfo)));
    }

    completionHandler();
}

void DrawingAreaCoordinatedGraphics::displayDidRefresh(MonotonicTime)
{
    // We might get didUpdate messages from the UI process even after we've
    // entered accelerated compositing mode. Ignore them.
    if (m_layerTreeHost)
        return;

    m_isWaitingForDidUpdate = false;

    if (!m_scheduledWhileWaitingForDidUpdate)
        return;
    m_scheduledWhileWaitingForDidUpdate = false;

    // Display if needed. We call displayTimerFired here since it will throttle updates to 60fps.
    displayTimerFired();
}

void DrawingAreaCoordinatedGraphics::dispatchAfterEnsuringDrawing(IPC::AsyncReplyID callbackID)
{
    m_pendingAfterDrawCallbackIDs.append(callbackID);
    if (m_layerTreeHost) {
        if (!m_layerTreeStateIsFrozen) {
            m_layerTreeHost->ensureDrawing();
            return;
        }
    } else {
        if (!m_isPaintingSuspended) {
            if (m_nonCompositedFrameRenderer)
                m_nonCompositedFrameRenderer->setNeedsDisplayInRect(m_webPage->bounds());
            else
                m_dirtyRegion = m_webPage->bounds();
            scheduleDisplay();
            return;
        }
    }

    // We can't ensure drawing, so process pending callbacks.
    dispatchPendingCallbacksAfterEnsuringDrawing();
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
        RefPtr frameView = m_webPage->localMainFrameView();
        if (!frameView)
            return;

        FloatRect unobscuredContentRect = frameView->unobscuredContentRectIncludingScrollbars();

        m_transientZoom = true;
        m_transientZoomInitialOrigin = unobscuredContentRect.location();
    }

    if (m_layerTreeHost) {
        m_layerTreeHost->adjustTransientZoom(scale, origin);
        return;
    }

    // We can't do transient zoom for non-AC mode, so just zoom in place instead.

    FloatPoint unscrolledOrigin(origin);
    unscrolledOrigin.moveBy(-m_transientZoomInitialOrigin);

    Ref webPage = m_webPage.get();
    webPage->scalePage(scale / webPage->viewScaleFactor(), roundedIntPoint(-unscrolledOrigin));
}

void DrawingAreaCoordinatedGraphics::commitTransientZoom(double scale, FloatPoint origin, CompletionHandler<void()>&& completionHandler)
{
    if (m_layerTreeHost)
        m_layerTreeHost->commitTransientZoom(scale, origin);

    FloatPoint unscrolledOrigin(origin);
    unscrolledOrigin.moveBy(-m_transientZoomInitialOrigin);

    Ref webPage = m_webPage.get();
    webPage->scalePage(scale / webPage->viewScaleFactor(), roundedIntPoint(-unscrolledOrigin));

    m_transientZoom = false;
    completionHandler();
}
#endif

void DrawingAreaCoordinatedGraphics::suspendPainting()
{
    ASSERT(!m_isPaintingSuspended);

    if (m_layerTreeHost)
        m_layerTreeHost->pauseRendering();
    else
        m_displayTimer.stop();

    m_isPaintingSuspended = true;

    m_webPage->corePage()->suspendScriptedAnimations();
}

void DrawingAreaCoordinatedGraphics::resumePainting()
{
    if (!m_isPaintingSuspended) {
        // FIXME: We can get a call to resumePainting when painting is not suspended.
        // This happens when sending a synchronous message to create a new page. See <rdar://problem/8976531>.
        return;
    }

    if (m_layerTreeHost)
        m_layerTreeHost->resumeRendering();

    m_isPaintingSuspended = false;

    // FIXME: We shouldn't always repaint everything here.
    setNeedsDisplay();

    m_webPage->corePage()->resumeScriptedAnimations();
}

void DrawingAreaCoordinatedGraphics::sendEnterAcceleratedCompositingModeIfNeeded()
{
    if (m_compositingAccordingToProxyMessages)
        return;

    RELEASE_ASSERT(m_layerTreeHost || m_nonCompositedFrameRenderer);
    LayerTreeContext layerTreeContext;
    layerTreeContext.contextID = m_layerTreeHost ? m_layerTreeHost->layerTreeContext().contextID : m_nonCompositedFrameRenderer->surfaceID();

    send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(0, layerTreeContext));
    m_compositingAccordingToProxyMessages = true;
}

void DrawingAreaCoordinatedGraphics::scheduleDisplay()
{
    ASSERT(!m_layerTreeHost);

    if (m_isWaitingForDidUpdate) {
        m_scheduledWhileWaitingForDidUpdate = true;
        return;
    }

    if (m_isPaintingSuspended)
        return;

    if (m_displayTimer.isActive())
        return;

    m_displayTimer.startOneShot(0_s);
}

void DrawingAreaCoordinatedGraphics::displayTimerFired()
{
    display();
}

void DrawingAreaCoordinatedGraphics::display()
{
    ASSERT(!m_layerTreeHost);
    ASSERT(!m_isWaitingForDidUpdate);
    ASSERT(!m_inUpdateGeometry);

    if (m_layerTreeStateIsFrozen)
        return;

    if (m_isPaintingSuspended)
        return;

    if (m_nonCompositedFrameRenderer) {
        m_nonCompositedFrameRenderer->display();
        dispatchPendingCallbacksAfterEnsuringDrawing();
        return;
    }

    UpdateInfo updateInfo;
    display(updateInfo);

    if (updateInfo.updateRectBounds.isEmpty())
        return;

    if (m_layerTreeHost) {
        // The call to update caused layout which turned on accelerated compositing.
        // Don't send an Update message in this case.
        return;
    }

    dispatchPendingCallbacksAfterEnsuringDrawing();

    if (m_compositingAccordingToProxyMessages) {
        send(Messages::DrawingAreaProxy::ExitAcceleratedCompositingMode(0, WTF::move(updateInfo)));
        m_compositingAccordingToProxyMessages = false;
    } else
        send(Messages::DrawingAreaProxy::Update(0, WTF::move(updateInfo)));
    m_isWaitingForDidUpdate = true;
    m_scheduledWhileWaitingForDidUpdate = false;
}

static bool shouldPaintBoundsRect(const IntRect& bounds, const Vector<IntRect, 1>& rects)
{
    const size_t rectThreshold = 10;
    const double wastedSpaceThreshold = 0.75;

    if (rects.size() <= 1 || rects.size() > rectThreshold)
        return true;

    // Attempt to guess whether or not we should use the region bounds rect or the individual rects.
    // We do this by computing the percentage of "wasted space" in the bounds. If that wasted space
    // is too large, then we will do individual rect painting instead.
    unsigned boundsArea = bounds.width() * bounds.height();
    unsigned rectsArea = 0;
    for (size_t i = 0; i < rects.size(); ++i)
        rectsArea += rects[i].width() * rects[i].height();

    double wastedSpace = 1 - (static_cast<double>(rectsArea) / boundsArea);

    return wastedSpace <= wastedSpaceThreshold;
}

void DrawingAreaCoordinatedGraphics::display(UpdateInfo& updateInfo)
{
    ASSERT(!m_isPaintingSuspended || m_inUpdateGeometry);
    ASSERT(!m_layerTreeHost);

    Ref webPage = m_webPage.get();
    webPage->updateRendering();
    webPage->finalizeRenderingUpdate({ });
    webPage->flushPendingEditorStateUpdate();

    // The layout may have put the page into accelerated compositing mode. If the LayerTreeHost is
    // in charge of displaying, we have nothing more to do.
    if (m_layerTreeHost)
        return;

    if (m_dirtyRegion.isEmpty())
        return;

    updateInfo.viewSize = webPage->size();
    updateInfo.deviceScaleFactor = webPage->corePage()->deviceScaleFactor();

    IntRect bounds = m_dirtyRegion.bounds();
    ASSERT(webPage->bounds().contains(bounds));

    IntSize bitmapSize = bounds.size();
    float deviceScaleFactor = webPage->corePage()->deviceScaleFactor();
    bitmapSize.scale(deviceScaleFactor);
    auto bitmap = ShareableBitmap::create({ bitmapSize });
    if (!bitmap)
        return;

    if (auto handle = bitmap->createHandle())
        updateInfo.bitmapHandle = WTF::move(*handle);
    else
        return;

    auto rects = m_dirtyRegion.rects();
    if (shouldPaintBoundsRect(bounds, rects)) {
        rects.clear();
        rects.append(bounds);
    }

    updateInfo.scrollRect = m_scrollRect;
    updateInfo.scrollOffset = m_scrollOffset;

    m_dirtyRegion = WebCore::Region();
    m_scrollRect = IntRect();
    m_scrollOffset = IntSize();

    auto graphicsContext = bitmap->createGraphicsContext();
    if (graphicsContext) {
        graphicsContext->applyDeviceScaleFactor(deviceScaleFactor);
        graphicsContext->translate(-bounds.x(), -bounds.y());
    }

    updateInfo.updateRectBounds = bounds;

    for (const auto& rect : rects) {
        if (graphicsContext)
            webPage->drawRect(*graphicsContext, rect);
        updateInfo.updateRects.append(rect);
    }

    webPage->didUpdateRendering();

    // Layout can trigger more calls to setNeedsDisplay and we don't want to process them
    // until the UI process has painted the update, so we stop the timer here.
    m_displayTimer.stop();
}

void DrawingAreaCoordinatedGraphics::forceUpdate()
{
    if (m_isWaitingForDidUpdate || m_layerTreeHost)
        return;

    if (m_nonCompositedFrameRenderer)
        m_nonCompositedFrameRenderer->setNeedsDisplayInRect(m_webPage->bounds());
    else
        m_dirtyRegion = m_webPage->bounds();

    display();
}

void DrawingAreaCoordinatedGraphics::didDiscardBackingStore()
{
    // Ensure the next update will cover the entire view, since the UI process discarded its backing store.
    m_dirtyRegion = m_webPage->bounds();
}

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM) && (USE(GBM) || OS(ANDROID))
void DrawingAreaCoordinatedGraphics::preferredBufferFormatsDidChange()
{
    if (m_layerTreeHost)
        m_layerTreeHost->preferredBufferFormatsDidChange();
}
#endif

#if ENABLE(DAMAGE_TRACKING)
void DrawingAreaCoordinatedGraphics::resetDamageHistoryForTesting()
{
    if (m_layerTreeHost)
        m_layerTreeHost->resetDamageHistoryForTesting();
    else if (m_nonCompositedFrameRenderer)
        m_nonCompositedFrameRenderer->resetDamageHistoryForTesting();
}

void DrawingAreaCoordinatedGraphics::foreachRegionInDamageHistoryForTesting(Function<void(const Region&)>&& callback) const
{
    if (m_layerTreeHost)
        m_layerTreeHost->foreachRegionInDamageHistoryForTesting(WTF::move(callback));
    else if (m_nonCompositedFrameRenderer)
        m_nonCompositedFrameRenderer->foreachRegionInDamageHistoryForTesting(WTF::move(callback));
}
#endif

void DrawingAreaCoordinatedGraphics::fillGLInformation(RenderProcessInfo&& info, CompletionHandler<void(RenderProcessInfo&&)>&& completionHandler)
{
    if (m_layerTreeHost)
        m_layerTreeHost->fillGLInformation(WTF::move(info), WTF::move(completionHandler));
    else
        completionHandler(WTF::move(info));
}

bool DrawingAreaCoordinatedGraphics::shouldUseTiledBackingForFrameView(const WebCore::LocalFrameView& frameView) const
{
    return frameView.frame().isMainFrame() || m_webPage->corePage()->settings().asyncFrameScrollingEnabled();
}

} // namespace WebKit
