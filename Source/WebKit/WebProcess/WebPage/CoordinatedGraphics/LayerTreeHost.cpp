/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Company 100, Inc.
 * Copyright (C) 2014-2019 Igalia S.L.
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
#include "LayerTreeHost.h"

#if USE(COORDINATED_GRAPHICS)

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

static const PlatformDisplayID primaryDisplayID = 0;
#if PLATFORM(GTK)
static const PlatformDisplayID compositingDisplayID = 1;
#else
static const PlatformDisplayID compositingDisplayID = primaryDisplayID;
#endif

LayerTreeHost::LayerTreeHost(WebPage& webPage)
    : m_webPage(webPage)
    , m_coordinator(webPage.corePage(), *this)
    , m_compositorClient(*this)
    , m_surface(AcceleratedSurface::create(webPage, *this))
    , m_viewportController(webPage.size())
    , m_layerFlushTimer(RunLoop::main(), this, &LayerTreeHost::layerFlushTimerFired)
{
#if USE(GLIB_EVENT_LOOP)
    m_layerFlushTimer.setPriority(RunLoopSourcePriority::LayerFlushTimer);
    m_layerFlushTimer.setName("[WebKit] LayerTreeHost");
#endif
    m_coordinator.createRootLayer(m_webPage.size());
    scheduleLayerFlush();

    if (FrameView* frameView = m_webPage.mainFrameView()) {
        auto contentsSize = frameView->contentsSize();
        if (!contentsSize.isEmpty())
            m_viewportController.didChangeContentsSize(contentsSize);
    }

    IntSize scaledSize(m_webPage.size());
    scaledSize.scale(m_webPage.deviceScaleFactor());
    float scaleFactor = m_webPage.deviceScaleFactor() * m_viewportController.pageScaleFactor();

    if (m_surface) {
        TextureMapper::PaintFlags paintFlags = 0;

        if (m_surface->shouldPaintMirrored())
            paintFlags |= TextureMapper::PaintingMirrored;

        m_compositor = ThreadedCompositor::create(m_compositorClient, m_compositorClient, compositingDisplayID, scaledSize, scaleFactor, ThreadedCompositor::ShouldDoFrameSync::Yes, paintFlags);
        m_layerTreeContext.contextID = m_surface->surfaceID();
    } else
        m_compositor = ThreadedCompositor::create(m_compositorClient, m_compositorClient, compositingDisplayID, scaledSize, scaleFactor);

    m_webPage.windowScreenDidChange(compositingDisplayID);

    didChangeViewport();
}

LayerTreeHost::~LayerTreeHost()
{
    ASSERT(!m_isValid);
}

void LayerTreeHost::setLayerFlushSchedulingEnabled(bool layerFlushingEnabled)
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

void LayerTreeHost::setShouldNotifyAfterNextScheduledLayerFlush(bool notifyAfterScheduledLayerFlush)
{
    m_notifyAfterScheduledLayerFlush = notifyAfterScheduledLayerFlush;
}

void LayerTreeHost::scheduleLayerFlush()
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

void LayerTreeHost::cancelPendingLayerFlush()
{
    m_layerFlushTimer.stop();
}

void LayerTreeHost::layerFlushTimerFired()
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

void LayerTreeHost::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    m_coordinator.setRootCompositingLayer(graphicsLayer);
}

void LayerTreeHost::setViewOverlayRootLayer(GraphicsLayer* viewOverlayRootLayer)
{
    m_viewOverlayRootLayer = viewOverlayRootLayer;
    m_coordinator.setViewOverlayRootLayer(viewOverlayRootLayer);
}

void LayerTreeHost::invalidate()
{
    ASSERT(m_isValid);
    m_isValid = false;

    cancelPendingLayerFlush();

    m_coordinator.invalidate();
    m_compositor->invalidate();
    m_surface = nullptr;
}

void LayerTreeHost::scrollNonCompositedContents(const IntRect& rect)
{
    auto* frameView = m_webPage.mainFrameView();
    if (!frameView || !frameView->delegatesScrolling())
        return;

    m_viewportController.didScroll(rect.location());
    if (m_isDiscardable)
        m_discardableSyncActions.add(DiscardableSyncActions::UpdateViewport);
    else
        didChangeViewport();
}

void LayerTreeHost::forceRepaint()
{
    // This is necessary for running layout tests. Since in this case we are not waiting for a UIProcess to reply nicely.
    // Instead we are just triggering forceRepaint. But we still want to have the scripted animation callbacks being executed.
    m_coordinator.syncDisplayState();

    // We need to schedule another flush, otherwise the forced paint might cancel a later expected flush.
    scheduleLayerFlush();

    if (!m_isWaitingForRenderer)
        m_coordinator.flushPendingLayerChanges();
    m_compositor->forceRepaint();
}

bool LayerTreeHost::forceRepaintAsync(CallbackID callbackID)
{
    scheduleLayerFlush();

    // We want a clean repaint, meaning that if we're currently waiting for the renderer
    // to finish an update, we'll have to schedule another flush when it's done.
    ASSERT(!m_forceRepaintAsync.callbackID);
    m_forceRepaintAsync.callbackID = OptionalCallbackID(callbackID);
    m_forceRepaintAsync.needsFreshFlush = m_scheduledWhileWaitingForRenderer;
    return true;
}

void LayerTreeHost::sizeDidChange(const IntSize& size)
{
    if (m_isDiscardable) {
        m_discardableSyncActions.add(DiscardableSyncActions::UpdateSize);
        m_viewportController.didChangeViewportSize(size);
        return;
    }

    if (m_surface && m_surface->hostResize(size))
        m_layerTreeContext.contextID = m_surface->surfaceID();

    m_coordinator.sizeDidChange(size);
    scheduleLayerFlush();

    m_viewportController.didChangeViewportSize(size);
    IntSize scaledSize(size);
    scaledSize.scale(m_webPage.deviceScaleFactor());
    m_compositor->setViewportSize(scaledSize, m_webPage.deviceScaleFactor() * m_viewportController.pageScaleFactor());
    didChangeViewport();
}

void LayerTreeHost::pauseRendering()
{
    m_isSuspended = true;
}

void LayerTreeHost::resumeRendering()
{
    m_isSuspended = false;
    scheduleLayerFlush();
}

GraphicsLayerFactory* LayerTreeHost::graphicsLayerFactory()
{
    return &m_coordinator;
}

void LayerTreeHost::contentsSizeChanged(const IntSize& newSize)
{
    m_viewportController.didChangeContentsSize(newSize);
    if (m_isDiscardable)
        m_discardableSyncActions.add(DiscardableSyncActions::UpdateViewport);
    else
        didChangeViewport();
}

void LayerTreeHost::didChangeViewportAttributes(ViewportAttributes&& attr)
{
    m_viewportController.didChangeViewportAttributes(WTFMove(attr));
    if (m_isDiscardable)
        m_discardableSyncActions.add(DiscardableSyncActions::UpdateViewport);
    else
        didChangeViewport();
}

void LayerTreeHost::didChangeViewport()
{
    FloatRect visibleRect(m_viewportController.visibleContentsRect());
    if (visibleRect.isEmpty())
        return;

    // When using non overlay scrollbars, the contents size doesn't include the scrollbars, but we need to include them
    // in the visible area used by the compositor to ensure that the scrollbar layers are also updated.
    // See https://bugs.webkit.org/show_bug.cgi?id=160450.
    FrameView* view = m_webPage.corePage()->mainFrame().view();
    Scrollbar* scrollbar = view->verticalScrollbar();
    if (scrollbar && !scrollbar->isOverlayScrollbar())
        visibleRect.expand(scrollbar->width(), 0);
    scrollbar = view->horizontalScrollbar();
    if (scrollbar && !scrollbar->isOverlayScrollbar())
        visibleRect.expand(0, scrollbar->height());

    m_coordinator.setVisibleContentsRect(visibleRect);
    scheduleLayerFlush();

    float pageScale = m_viewportController.pageScaleFactor();
    IntPoint scrollPosition = roundedIntPoint(visibleRect.location());
    if (m_lastScrollPosition != scrollPosition) {
        m_lastScrollPosition = scrollPosition;
        m_compositor->setScrollPosition(m_lastScrollPosition, m_webPage.deviceScaleFactor() * pageScale);

        if (!view->useFixedLayout())
            view->notifyScrollPositionChanged(m_lastScrollPosition);
    }

    if (m_lastPageScaleFactor != pageScale) {
        m_lastPageScaleFactor = pageScale;
        m_webPage.scalePage(pageScale, m_lastScrollPosition);
    }
}

void LayerTreeHost::setIsDiscardable(bool discardable)
{
    m_isDiscardable = discardable;
    if (m_isDiscardable) {
        m_discardableSyncActions = OptionSet<DiscardableSyncActions>();
        m_webPage.windowScreenDidChange(primaryDisplayID);
        return;
    }
    m_webPage.windowScreenDidChange(compositingDisplayID);

    if (m_discardableSyncActions.isEmpty())
        return;

    if (m_discardableSyncActions.contains(DiscardableSyncActions::UpdateSize)) {
        // Size changes already sets the scale factor and updates the viewport.
        sizeDidChange(m_webPage.size());
        return;
    }

    if (m_discardableSyncActions.contains(DiscardableSyncActions::UpdateScale))
        deviceOrPageScaleFactorChanged();

    if (m_discardableSyncActions.contains(DiscardableSyncActions::UpdateViewport))
        didChangeViewport();
}

#if PLATFORM(GTK) && PLATFORM(X11) && !USE(REDIRECTED_XCOMPOSITE_WINDOW)
void LayerTreeHost::setNativeSurfaceHandleForCompositing(uint64_t handle)
{
    m_layerTreeContext.contextID = handle;
    m_compositor->setNativeSurfaceHandleForCompositing(handle);
    scheduleLayerFlush();
}
#endif

void LayerTreeHost::deviceOrPageScaleFactorChanged()
{
    if (m_isDiscardable) {
        m_discardableSyncActions.add(DiscardableSyncActions::UpdateScale);
        return;
    }

    if (m_surface && m_surface->hostResize(m_webPage.size()))
        m_layerTreeContext.contextID = m_surface->surfaceID();

    m_coordinator.deviceOrPageScaleFactorChanged();
    m_webPage.corePage()->pageOverlayController().didChangeDeviceScaleFactor();
    m_compositor->setScaleFactor(m_webPage.deviceScaleFactor() * m_viewportController.pageScaleFactor());
}

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
RefPtr<DisplayRefreshMonitor> LayerTreeHost::createDisplayRefreshMonitor(PlatformDisplayID displayID)
{
    return m_compositor->displayRefreshMonitor(displayID);
}
#endif

void LayerTreeHost::didFlushRootLayer(const FloatRect& visibleContentRect)
{
    // Because our view-relative overlay root layer is not attached to the FrameView's GraphicsLayer tree, we need to flush it manually.
    if (m_viewOverlayRootLayer)
        m_viewOverlayRootLayer->flushCompositingState(visibleContentRect);
}

void LayerTreeHost::commitSceneState(const CoordinatedGraphicsState& state)
{
    m_isWaitingForRenderer = true;
    m_compositor->updateSceneState(state);
}

void LayerTreeHost::frameComplete()
{
    m_compositor->frameComplete();
}

uint64_t LayerTreeHost::nativeSurfaceHandleForCompositing()
{
    if (!m_surface)
        return m_layerTreeContext.contextID;

    m_surface->initialize();
    return m_surface->window();
}

void LayerTreeHost::didDestroyGLContext()
{
    if (m_surface)
        m_surface->finalize();
}

void LayerTreeHost::willRenderFrame()
{
    if (m_surface)
        m_surface->willRenderFrame();
}

void LayerTreeHost::didRenderFrame()
{
    if (m_surface)
        m_surface->didRenderFrame();
}

void LayerTreeHost::requestDisplayRefreshMonitorUpdate()
{
    // Flush layers to cause a repaint. If m_isWaitingForRenderer was true at this point, the layer
    // flush won't do anything, but that means there's a painting ongoing that will send the
    // display refresh notification when it's done.
    m_coordinator.forceFrameSync();
    scheduleLayerFlush();
}

void LayerTreeHost::handleDisplayRefreshMonitorUpdate(bool hasBeenRescheduled)
{
    // Call renderNextFrame. If hasBeenRescheduled is true, the layer flush will force a repaint
    // that will cause the display refresh notification to come.
    renderNextFrame(hasBeenRescheduled);
    m_compositor->handleDisplayRefreshMonitorUpdate();
}

void LayerTreeHost::renderNextFrame(bool forceRepaint)
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

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
