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
#include "DrawingAreaCoordinatedGraphics.h"

#include "DrawingAreaProxyMessages.h"
#include "LayerTreeHost.h"
#include "MessageSenderInlines.h"
#include "ShareableBitmap.h"
#include "UpdateInfo.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPageInlines.h"
#include "WebPreferencesKeys.h"
#include "WebProcess.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/Region.h>
#include <WebCore/Settings.h>
#include <wtf/SetForScope.h>

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebKit {
using namespace WebCore;

DrawingAreaCoordinatedGraphics::DrawingAreaCoordinatedGraphics(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(DrawingAreaType::CoordinatedGraphics, parameters.drawingAreaIdentifier, webPage)
    , m_isPaintingSuspended(!(parameters.activityState & ActivityState::IsVisible))
    , m_exitCompositingTimer(RunLoop::main(), this, &DrawingAreaCoordinatedGraphics::exitAcceleratedCompositingMode)
    , m_discardPreviousLayerTreeHostTimer(RunLoop::main(), this, &DrawingAreaCoordinatedGraphics::discardPreviousLayerTreeHost)
    , m_displayTimer(RunLoop::main(), this, &DrawingAreaCoordinatedGraphics::displayTimerFired)
{
#if USE(GLIB_EVENT_LOOP)
    m_discardPreviousLayerTreeHostTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
#if !PLATFORM(WPE)
    m_displayTimer.setPriority(RunLoopSourcePriority::NonAcceleratedDrawingTimer);
#endif
#endif

    updatePreferences(parameters.store);
}

DrawingAreaCoordinatedGraphics::~DrawingAreaCoordinatedGraphics() = default;

void DrawingAreaCoordinatedGraphics::setNeedsDisplay()
{
    if (m_layerTreeHost) {
        ASSERT(m_dirtyRegion.isEmpty());
        return;
    }

    setNeedsDisplayInRect(m_webPage.bounds());
}

void DrawingAreaCoordinatedGraphics::setNeedsDisplayInRect(const IntRect& rect)
{
    if (m_layerTreeHost) {
        ASSERT(m_dirtyRegion.isEmpty());
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
        m_layerTreeHost->setNonCompositedContentsNeedDisplay(rect);
#endif
        return;
    }

    IntRect dirtyRect = rect;
    dirtyRect.intersect(m_webPage.bounds());
    if (dirtyRect.isEmpty())
        return;

    m_dirtyRegion.unite(dirtyRect);
    scheduleDisplay();
}

void DrawingAreaCoordinatedGraphics::scroll(const IntRect& scrollRect, const IntSize& scrollDelta)
{
    if (m_layerTreeHost) {
        ASSERT(m_scrollRect.isEmpty());
        ASSERT(m_scrollOffset.isEmpty());
        ASSERT(m_dirtyRegion.isEmpty());
        m_layerTreeHost->scrollNonCompositedContents(scrollRect);
        return;
    }

    if (scrollRect.isEmpty())
        return;

    if (m_previousLayerTreeHost)
        m_previousLayerTreeHost->scrollNonCompositedContents(scrollRect);

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

void DrawingAreaCoordinatedGraphics::forceRepaint()
{
    if (m_inUpdateGeometry)
        return;

    if (!m_layerTreeHost) {
        m_isWaitingForDidUpdate = false;
        m_dirtyRegion = m_webPage.bounds();
        display();
        return;
    }

    if (m_layerTreeStateIsFrozen)
        return;

    setNeedsDisplay();
    m_webPage.layoutIfNeeded();
    if (!m_layerTreeHost)
        return;

    // FIXME: We need to do the same work as the layerHostDidFlushLayers function here,
    // but clearly it doesn't make sense to call the function with that name.
    // Consider refactoring and renaming it.
    if (m_compositingAccordingToProxyMessages)
        m_layerTreeHost->forceRepaint();
    else {
        // Call setShouldNotifyAfterNextScheduledLayerFlush(false) here to
        // prevent layerHostDidFlushLayers() from being called a second time.
        m_layerTreeHost->setShouldNotifyAfterNextScheduledLayerFlush(false);
#if USE(COORDINATED_GRAPHICS)
        layerHostDidFlushLayers();
#endif
    }
}

void DrawingAreaCoordinatedGraphics::forceRepaintAsync(WebPage& page, CompletionHandler<void()>&& completionHandler)
{
    if (m_layerTreeStateIsFrozen) {
        page.forceRepaintWithoutCallback();
        return completionHandler();
    }

    if (m_layerTreeHost)
        m_layerTreeHost->forceRepaintAsync(WTFMove(completionHandler));
    else
        completionHandler();
}

void DrawingAreaCoordinatedGraphics::setLayerTreeStateIsFrozen(bool isFrozen)
{
    if (m_layerTreeStateIsFrozen == isFrozen)
        return;

    m_layerTreeStateIsFrozen = isFrozen;

    if (m_layerTreeHost)
        m_layerTreeHost->setLayerFlushSchedulingEnabled(!isFrozen);

    if (isFrozen)
        m_exitCompositingTimer.stop();
    else if (m_wantsToExitAcceleratedCompositingMode)
        exitAcceleratedCompositingModeSoon();
    else if (!m_layerTreeHost)
        scheduleDisplay();
}

void DrawingAreaCoordinatedGraphics::updatePreferences(const WebPreferencesStore& store)
{
    Settings& settings = m_webPage.corePage()->settings();
#if PLATFORM(WAYLAND)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland
        && &PlatformDisplay::sharedDisplayForCompositing() == &PlatformDisplay::sharedDisplay()) {
        // We failed to create the shared display for compositing, disable accelerated compositing.
        settings.setAcceleratedCompositingEnabled(false);
    }
#endif
    settings.setForceCompositingMode(store.getBoolValueForKey(WebPreferencesKey::forceCompositingModeKey()));
    // Fixed position elements need to be composited and create stacking contexts
    // in order to be scrolled by the ScrollingCoordinator.
    settings.setAcceleratedCompositingForFixedPositionEnabled(settings.acceleratedCompositingEnabled());

    m_alwaysUseCompositing = settings.acceleratedCompositingEnabled() && settings.forceCompositingMode();

    m_supportsAsyncScrolling = store.getBoolValueForKey(WebPreferencesKey::threadedScrollingEnabledKey());
#if ENABLE(DEVELOPER_MODE)
    if (m_supportsAsyncScrolling) {
        auto* disableAsyncScrolling = getenv("WEBKIT_DISABLE_ASYNC_SCROLLING");
        if (disableAsyncScrolling && strcmp(disableAsyncScrolling, "0"))
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

void DrawingAreaCoordinatedGraphics::mainFrameContentSizeChanged(WebCore::FrameIdentifier, const IntSize& size)
{
    if (m_layerTreeHost)
        m_layerTreeHost->contentsSizeChanged(size);
    else if (m_previousLayerTreeHost)
        m_previousLayerTreeHost->contentsSizeChanged(size);
}

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
void DrawingAreaCoordinatedGraphics::deviceOrPageScaleFactorChanged()
{
    if (m_layerTreeHost)
        m_layerTreeHost->deviceOrPageScaleFactorChanged();
    else if (m_previousLayerTreeHost)
        m_previousLayerTreeHost->deviceOrPageScaleFactorChanged();
}

void DrawingAreaCoordinatedGraphics::didChangeViewportAttributes(ViewportAttributes&& attrs)
{
    if (m_layerTreeHost)
        m_layerTreeHost->didChangeViewportAttributes(WTFMove(attrs));
    else if (m_previousLayerTreeHost)
        m_previousLayerTreeHost->didChangeViewportAttributes(WTFMove(attrs));
}

bool DrawingAreaCoordinatedGraphics::enterAcceleratedCompositingModeIfNeeded()
{
    ASSERT(!m_layerTreeHost);
    if (!m_alwaysUseCompositing)
        return false;

    enterAcceleratedCompositingMode(nullptr);
    return true;
}
#endif

void DrawingAreaCoordinatedGraphics::setDeviceScaleFactor(float deviceScaleFactor)
{
    m_webPage.setDeviceScaleFactor(deviceScaleFactor);
}

bool DrawingAreaCoordinatedGraphics::supportsAsyncScrolling() const
{
    return m_supportsAsyncScrolling;
}

void DrawingAreaCoordinatedGraphics::registerScrollingTree()
{
#if ENABLE(SCROLLING_THREAD)
    if (m_supportsAsyncScrolling)
        WebProcess::singleton().eventDispatcher().addScrollingTreeForPage(m_webPage);
#endif
}

void DrawingAreaCoordinatedGraphics::unregisterScrollingTree()
{
#if ENABLE(SCROLLING_THREAD)
    if (m_supportsAsyncScrolling)
        WebProcess::singleton().eventDispatcher().removeScrollingTreeForPage(m_webPage);
#endif
}

GraphicsLayerFactory* DrawingAreaCoordinatedGraphics::graphicsLayerFactory()
{
    if (!m_layerTreeHost)
        enterAcceleratedCompositingMode(nullptr);
    return m_layerTreeHost ? m_layerTreeHost->graphicsLayerFactory() : nullptr;
}

void DrawingAreaCoordinatedGraphics::setRootCompositingLayer(WebCore::Frame&, GraphicsLayer* graphicsLayer)
{
    if (m_layerTreeHost) {
        if (graphicsLayer) {
            // We're already in accelerated compositing mode, but the root compositing layer changed.
            m_exitCompositingTimer.stop();
            m_wantsToExitAcceleratedCompositingMode = false;

            // If we haven't sent the EnterAcceleratedCompositingMode message, make sure that the
            // layer tree host calls us back after the next layer flush so we can send it then.
            if (!m_compositingAccordingToProxyMessages)
                m_layerTreeHost->setShouldNotifyAfterNextScheduledLayerFlush(true);
        }
        m_layerTreeHost->setRootCompositingLayer(graphicsLayer);

        if (!graphicsLayer && !m_alwaysUseCompositing) {
            // We'll exit accelerated compositing mode on a timer, to avoid re-entering
            // compositing code via display() and layout.
            // If we're leaving compositing mode because of a setSize, it is safe to
            // exit accelerated compositing mode right away.
            if (m_inUpdateGeometry)
                exitAcceleratedCompositingMode();
            else
                exitAcceleratedCompositingModeSoon();
        }
        return;
    }

    if (!graphicsLayer)
        return;

    // We're actually entering accelerated compositing mode.
    enterAcceleratedCompositingMode(graphicsLayer);
}

void DrawingAreaCoordinatedGraphics::triggerRenderingUpdate()
{
    if (m_layerTreeStateIsFrozen)
        return;

    if (m_layerTreeHost)
        m_layerTreeHost->scheduleLayerFlush();
    else
        scheduleDisplay();
}

#if USE(COORDINATED_GRAPHICS) || USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
void DrawingAreaCoordinatedGraphics::layerHostDidFlushLayers()
{
    ASSERT(m_layerTreeHost);
    m_layerTreeHost->forceRepaint();
}
#endif

RefPtr<DisplayRefreshMonitor> DrawingAreaCoordinatedGraphics::createDisplayRefreshMonitor(PlatformDisplayID displayID)
{
    if (!m_layerTreeHost || m_wantsToExitAcceleratedCompositingMode || exitAcceleratedCompositingModePending())
        return nullptr;
    return m_layerTreeHost->createDisplayRefreshMonitor(displayID);
}

void DrawingAreaCoordinatedGraphics::activityStateDidChange(OptionSet<ActivityState> changed, ActivityStateChangeID, CompletionHandler<void()>&& completionHandler)
{
    if (changed & ActivityState::IsVisible) {
        if (m_webPage.isVisible())
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
    else if (m_previousLayerTreeHost)
        m_previousLayerTreeHost->setViewOverlayRootLayer(viewOverlayRootLayer);
}

void DrawingAreaCoordinatedGraphics::updateGeometry(const IntSize& size, CompletionHandler<void()>&& completionHandler)
{
    SetForScope inUpdateGeometry(m_inUpdateGeometry, true);
    m_webPage.setSize(size);
    m_webPage.layoutIfNeeded();

    if (!m_layerTreeHost)
        m_dirtyRegion = IntRect(IntPoint(), size);

    LayerTreeContext previousLayerTreeContext;
    if (m_layerTreeHost) {
        previousLayerTreeContext = m_layerTreeHost->layerTreeContext();
        m_layerTreeHost->sizeDidChange(m_webPage.size());
    } else if (m_previousLayerTreeHost)
        m_previousLayerTreeHost->sizeDidChange(m_webPage.size());

    if (m_layerTreeHost) {
        auto layerTreeContext = m_layerTreeHost->layerTreeContext();
        m_layerTreeHost->forceRepaint();
        if (layerTreeContext != previousLayerTreeContext)
            send(Messages::DrawingAreaProxy::UpdateAcceleratedCompositingMode(0, layerTreeContext));
    } else {
        UpdateInfo updateInfo;
        if (m_isPaintingSuspended) {
            updateInfo.viewSize = m_webPage.size();
            updateInfo.deviceScaleFactor = m_webPage.corePage()->deviceScaleFactor();
        } else
            display(updateInfo);
        if (!m_layerTreeHost)
            send(Messages::DrawingAreaProxy::Update(0, WTFMove(updateInfo)));
    }

    completionHandler();
}

void DrawingAreaCoordinatedGraphics::targetRefreshRateDidChange(unsigned rate)
{
    UNUSED_PARAM(rate);
#if !USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    if (m_layerTreeHost)
        m_layerTreeHost->targetRefreshRateDidChange(rate);
#endif
}

void DrawingAreaCoordinatedGraphics::displayDidRefresh()
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

#if PLATFORM(GTK)
void DrawingAreaCoordinatedGraphics::adjustTransientZoom(double scale, FloatPoint origin)
{
    if (!m_transientZoom) {
        auto* frameView = m_webPage.localMainFrameView();
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

    m_webPage.scalePage(scale / m_webPage.viewScaleFactor(), roundedIntPoint(-unscrolledOrigin));
}

void DrawingAreaCoordinatedGraphics::commitTransientZoom(double scale, FloatPoint origin)
{
    if (m_layerTreeHost)
        m_layerTreeHost->commitTransientZoom(scale, origin);

    FloatPoint unscrolledOrigin(origin);
    unscrolledOrigin.moveBy(-m_transientZoomInitialOrigin);

    m_webPage.scalePage(scale / m_webPage.viewScaleFactor(), roundedIntPoint(-unscrolledOrigin));

    m_transientZoom = false;
}
#endif

void DrawingAreaCoordinatedGraphics::exitAcceleratedCompositingModeSoon()
{
    if (m_layerTreeStateIsFrozen) {
        m_wantsToExitAcceleratedCompositingMode = true;
        return;
    }

    if (exitAcceleratedCompositingModePending())
        return;

    m_exitCompositingTimer.startOneShot(0_s);
}

void DrawingAreaCoordinatedGraphics::discardPreviousLayerTreeHost()
{
    m_discardPreviousLayerTreeHostTimer.stop();
    m_previousLayerTreeHost = nullptr;
}

void DrawingAreaCoordinatedGraphics::suspendPainting()
{
    ASSERT(!m_isPaintingSuspended);

    if (m_layerTreeHost)
        m_layerTreeHost->pauseRendering();
    else
        m_displayTimer.stop();

    m_isPaintingSuspended = true;

    m_webPage.corePage()->suspendScriptedAnimations();
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

    m_webPage.corePage()->resumeScriptedAnimations();
}

void DrawingAreaCoordinatedGraphics::enterAcceleratedCompositingMode(GraphicsLayer* graphicsLayer)
{
#if PLATFORM(GTK)
    if (!m_alwaysUseCompositing) {
        m_webPage.corePage()->settings().setForceCompositingMode(true);
        m_alwaysUseCompositing = true;
    }
#endif
    m_discardPreviousLayerTreeHostTimer.stop();

    m_exitCompositingTimer.stop();
    m_wantsToExitAcceleratedCompositingMode = false;

    auto changeWindowScreen = [&] {
        // In order to ensure that we get a unique DisplayRefreshMonitor per-DrawingArea (necessary because ThreadedDisplayRefreshMonitor
        // is driven by the ThreadedCompositor of the drawing area), give each page a unique DisplayID derived from DrawingArea's unique ID.
        m_webPage.windowScreenDidChange(m_layerTreeHost->displayID(), std::nullopt);
    };

    ASSERT(!m_layerTreeHost);
    if (m_previousLayerTreeHost) {
        m_layerTreeHost = WTFMove(m_previousLayerTreeHost);
        changeWindowScreen();
        m_layerTreeHost->setIsDiscardable(false);
        m_layerTreeHost->resumeRendering();
        if (!m_layerTreeStateIsFrozen)
            m_layerTreeHost->setLayerFlushSchedulingEnabled(true);
    } else {
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
        m_layerTreeHost = makeUnique<LayerTreeHost>(m_webPage);
#elif USE(COORDINATED_GRAPHICS)
        m_layerTreeHost = makeUnique<LayerTreeHost>(m_webPage, std::numeric_limits<uint32_t>::max() - m_identifier.toUInt64());
#else
        m_layerTreeHost = nullptr;
        return;
#endif
        changeWindowScreen();
        if (m_layerTreeStateIsFrozen)
            m_layerTreeHost->setLayerFlushSchedulingEnabled(false);
        if (m_isPaintingSuspended)
            m_layerTreeHost->pauseRendering();
    }

    if (!m_inUpdateGeometry)
        m_layerTreeHost->setShouldNotifyAfterNextScheduledLayerFlush(true);

    m_layerTreeHost->setRootCompositingLayer(graphicsLayer);

    if (m_shouldSendEnterAcceleratedCompositingMode)
        sendEnterAcceleratedCompositingModeIfNeeded();

    // Non-composited content will now be handled exclusively by the layer tree host.
    m_dirtyRegion = WebCore::Region();
    m_scrollRect = IntRect();
    m_scrollOffset = IntSize();
    m_displayTimer.stop();
    m_isWaitingForDidUpdate = false;
}

void DrawingAreaCoordinatedGraphics::sendEnterAcceleratedCompositingModeIfNeeded()
{
    if (m_compositingAccordingToProxyMessages)
        return;

    if (!m_layerTreeHost) {
        m_shouldSendEnterAcceleratedCompositingMode = true;
        return;
    }

    send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(0, m_layerTreeHost->layerTreeContext()));
    m_compositingAccordingToProxyMessages = true;
}

void DrawingAreaCoordinatedGraphics::exitAcceleratedCompositingMode()
{
    if (m_alwaysUseCompositing)
        return;

    ASSERT(!m_layerTreeStateIsFrozen);

    m_exitCompositingTimer.stop();
    m_wantsToExitAcceleratedCompositingMode = false;

    ASSERT(m_layerTreeHost);
    m_previousLayerTreeHost = WTFMove(m_layerTreeHost);
    m_previousLayerTreeHost->setIsDiscardable(true);
    m_previousLayerTreeHost->pauseRendering();
    m_previousLayerTreeHost->setLayerFlushSchedulingEnabled(false);
    m_discardPreviousLayerTreeHostTimer.startOneShot(5_s);

    // Always use the primary display ID (0) when not in accelerated compositing mode.
    m_webPage.windowScreenDidChange(0, std::nullopt);

    m_dirtyRegion = m_webPage.bounds();

    if (m_inUpdateGeometry)
        return;

    UpdateInfo updateInfo;
    if (m_isPaintingSuspended) {
        updateInfo.viewSize = m_webPage.size();
        updateInfo.deviceScaleFactor = m_webPage.corePage()->deviceScaleFactor();
    } else
        display(updateInfo);

    // Send along a complete update of the page so we can paint the contents right after we exit the
    // accelerated compositing mode, eliminiating flicker.
    if (m_compositingAccordingToProxyMessages) {
        send(Messages::DrawingAreaProxy::ExitAcceleratedCompositingMode(0, WTFMove(updateInfo)));
        m_compositingAccordingToProxyMessages = false;
    } else {
        // If we left accelerated compositing mode before we sent an EnterAcceleratedCompositingMode message to the
        // UI process, we still need to let it know about the new contents, so send an Update message.
        send(Messages::DrawingAreaProxy::Update(0, WTFMove(updateInfo)));
    }
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

    UpdateInfo updateInfo;
    display(updateInfo);

    if (updateInfo.updateRectBounds.isEmpty())
        return;

    if (m_layerTreeHost) {
        // The call to update caused layout which turned on accelerated compositing.
        // Don't send an Update message in this case.
        return;
    }

    if (m_compositingAccordingToProxyMessages) {
        send(Messages::DrawingAreaProxy::ExitAcceleratedCompositingMode(0, WTFMove(updateInfo)));
        m_compositingAccordingToProxyMessages = false;
    } else
        send(Messages::DrawingAreaProxy::Update(0, WTFMove(updateInfo)));
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

    m_webPage.updateRendering();
    m_webPage.finalizeRenderingUpdate({ });
    m_webPage.flushPendingEditorStateUpdate();

    // The layout may have put the page into accelerated compositing mode. If the LayerTreeHost is
    // in charge of displaying, we have nothing more to do.
    if (m_layerTreeHost)
        return;

    if (m_dirtyRegion.isEmpty())
        return;

    updateInfo.viewSize = m_webPage.size();
    updateInfo.deviceScaleFactor = m_webPage.corePage()->deviceScaleFactor();

    IntRect bounds = m_dirtyRegion.bounds();
    ASSERT(m_webPage.bounds().contains(bounds));

    IntSize bitmapSize = bounds.size();
    float deviceScaleFactor = m_webPage.corePage()->deviceScaleFactor();
    bitmapSize.scale(deviceScaleFactor);
    auto bitmap = ShareableBitmap::create({ bitmapSize });
    if (!bitmap)
        return;

    if (auto handle = bitmap->createHandle())
        updateInfo.bitmapHandle = WTFMove(*handle);
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
            m_webPage.drawRect(*graphicsContext, rect);
        updateInfo.updateRects.append(rect);
    }

    m_webPage.didUpdateRendering();

    // Layout can trigger more calls to setNeedsDisplay and we don't want to process them
    // until the UI process has painted the update, so we stop the timer here.
    m_displayTimer.stop();
}

void DrawingAreaCoordinatedGraphics::forceUpdate()
{
    if (m_isWaitingForDidUpdate || m_layerTreeHost)
        return;

    m_dirtyRegion = m_webPage.bounds();
    display();
}

void DrawingAreaCoordinatedGraphics::didDiscardBackingStore()
{
    // Ensure the next update will cover the entire view, since the UI process discarded its backing store.
    m_dirtyRegion = m_webPage.bounds();
}

} // namespace WebKit
