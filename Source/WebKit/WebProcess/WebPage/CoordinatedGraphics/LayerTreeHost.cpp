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
#include <WebCore/Chrome.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/RenderLayerBacking.h>
#include <WebCore/RenderView.h>

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebKit {
using namespace WebCore;

LayerTreeHost::LayerTreeHost(WebPage& webPage)
    : m_webPage(webPage)
    , m_compositorClient(*this)
    , m_surface(AcceleratedSurface::create(webPage, *this))
    , m_viewportController(webPage.size())
    , m_layerFlushTimer(RunLoop::main(), this, &LayerTreeHost::layerFlushTimerFired)
    , m_coordinator(webPage, *this)
    , m_displayID(std::numeric_limits<uint32_t>::max() - m_webPage.identifier().toUInt64())
{
#if USE(GLIB_EVENT_LOOP)
    m_layerFlushTimer.setPriority(RunLoopSourcePriority::LayerFlushTimer);
    m_layerFlushTimer.setName("[WebKit] LayerTreeHost");
#endif
    scheduleLayerFlush();

    if (FrameView* frameView = m_webPage.mainFrameView()) {
        auto contentsSize = frameView->contentsSize();
        if (!contentsSize.isEmpty())
            m_viewportController.didChangeContentsSize(contentsSize);
    }

    IntSize scaledSize(m_webPage.size());
    scaledSize.scale(m_webPage.deviceScaleFactor());
    float scaleFactor = m_webPage.deviceScaleFactor() * m_viewportController.pageScaleFactor();

    TextureMapper::PaintFlags paintFlags = 0;
    if (m_surface->shouldPaintMirrored())
        paintFlags |= TextureMapper::PaintingMirrored;

    m_compositor = ThreadedCompositor::create(m_compositorClient, m_compositorClient, m_displayID, scaledSize, scaleFactor, paintFlags);
    m_layerTreeContext.contextID = m_surface->surfaceID();

    didChangeViewport();
}

LayerTreeHost::~LayerTreeHost()
{
    cancelPendingLayerFlush();

    m_coordinator.invalidate();
    m_compositor->invalidate();
    m_surface = nullptr;
}

void LayerTreeHost::setLayerFlushSchedulingEnabled(bool layerFlushingEnabled)
{
    if (m_layerFlushSchedulingEnabled == layerFlushingEnabled)
        return;

    m_layerFlushSchedulingEnabled = layerFlushingEnabled;

    if (m_layerFlushSchedulingEnabled) {
        m_compositor->resume();
        scheduleLayerFlush();
        return;
    }

    cancelPendingLayerFlush();
    m_compositor->suspend();
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

    if (!m_coordinator.rootCompositingLayer())
        return;

    // If a force-repaint callback was registered, we should force a 'frame sync' that
    // will guarantee us a call to renderNextFrame() once the update is complete.
    if (m_forceRepaintAsync.callback)
        m_coordinator.forceFrameSync();

    OptionSet<FinalizeRenderingUpdateFlags> flags;
#if PLATFORM(GTK)
    if (!m_transientZoom)
        flags.add(FinalizeRenderingUpdateFlags::ApplyScrollingTreeLayerPositions);
#else
    flags.add(FinalizeRenderingUpdateFlags::ApplyScrollingTreeLayerPositions);
#endif

    bool didSync = m_coordinator.flushPendingLayerChanges(flags);

#if PLATFORM(GTK)
    // If we have an active transient zoom, we want the zoom to win over any changes
    // that WebCore makes to the relevant layers, so re-apply our changes after flushing.
    if (m_transientZoom)
        applyTransientZoomToLayers(m_transientZoomScale, m_transientZoomOrigin);
#endif

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

    if (!m_isWaitingForRenderer) {
        OptionSet<FinalizeRenderingUpdateFlags> flags;
#if PLATFORM(GTK)
        if (!m_transientZoom)
            flags.add(FinalizeRenderingUpdateFlags::ApplyScrollingTreeLayerPositions);
#else
        flags.add(FinalizeRenderingUpdateFlags::ApplyScrollingTreeLayerPositions);
#endif

        m_coordinator.flushPendingLayerChanges(flags);
    }

    m_compositor->forceRepaint();
}

void LayerTreeHost::forceRepaintAsync(CompletionHandler<void()>&& callback)
{
    scheduleLayerFlush();

    // We want a clean repaint, meaning that if we're currently waiting for the renderer
    // to finish an update, we'll have to schedule another flush when it's done.
    ASSERT(!m_forceRepaintAsync.callback);
    m_forceRepaintAsync.callback = WTFMove(callback);
    m_forceRepaintAsync.needsFreshFlush = m_scheduledWhileWaitingForRenderer;
}

void LayerTreeHost::sizeDidChange(const IntSize& size)
{
    if (m_isDiscardable) {
        m_discardableSyncActions.add(DiscardableSyncActions::UpdateSize);
        m_viewportController.didChangeViewportSize(size);
        return;
    }

    if (m_surface->hostResize(size))
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
    m_compositor->suspend();
}

void LayerTreeHost::resumeRendering()
{
    m_isSuspended = false;
    m_compositor->resume();
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
        return;
    }

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

void LayerTreeHost::deviceOrPageScaleFactorChanged()
{
    if (m_isDiscardable) {
        m_discardableSyncActions.add(DiscardableSyncActions::UpdateScale);
        return;
    }

    if (m_surface->hostResize(m_webPage.size()))
        m_layerTreeContext.contextID = m_surface->surfaceID();

    m_coordinator.deviceOrPageScaleFactorChanged();
    m_webPage.corePage()->pageOverlayController().didChangeDeviceScaleFactor();
    m_compositor->setScaleFactor(m_webPage.deviceScaleFactor() * m_viewportController.pageScaleFactor());
}

RefPtr<DisplayRefreshMonitor> LayerTreeHost::createDisplayRefreshMonitor(PlatformDisplayID displayID)
{
    return m_compositor->displayRefreshMonitor(displayID);
}

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

void LayerTreeHost::updateScene()
{
    m_compositor->updateScene();
}

void LayerTreeHost::frameComplete()
{
    m_compositor->frameComplete();
}

uint64_t LayerTreeHost::nativeSurfaceHandleForCompositing()
{
    m_surface->initialize();
    return m_surface->window();
}

void LayerTreeHost::didDestroyGLContext()
{
    m_surface->finalize();
}

void LayerTreeHost::willRenderFrame()
{
    m_surface->willRenderFrame();
}

void LayerTreeHost::didRenderFrame()
{
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
}

void LayerTreeHost::renderNextFrame(bool forceRepaint)
{
    m_isWaitingForRenderer = false;
    bool scheduledWhileWaitingForRenderer = std::exchange(m_scheduledWhileWaitingForRenderer, false);
    m_coordinator.renderNextFrame();

    if (m_forceRepaintAsync.callback) {
        // If the asynchronous force-repaint needs a separate fresh flush, it was due to
        // the force-repaint request being registered while CoordinatedLayerTreeHost was
        // waiting for the renderer to finish an update.
        ASSERT(!m_forceRepaintAsync.needsFreshFlush || scheduledWhileWaitingForRenderer);

        // Execute the callback if another layer flush and the subsequent state update
        // aren't needed. If they are, the callback will be executed when this function
        // is called after the next update.
        if (!m_forceRepaintAsync.needsFreshFlush)
            m_forceRepaintAsync.callback();
        m_forceRepaintAsync.needsFreshFlush = false;
    }

    if (scheduledWhileWaitingForRenderer || m_layerFlushTimer.isActive() || forceRepaint) {
        m_layerFlushTimer.stop();
        if (forceRepaint)
            m_coordinator.forceFrameSync();
        layerFlushTimerFired();
    }
}

#if PLATFORM(GTK)
FloatPoint LayerTreeHost::constrainTransientZoomOrigin(double scale, FloatPoint origin) const
{
    FrameView& frameView = *m_webPage.mainFrameView();
    FloatRect visibleContentRect = frameView.visibleContentRectIncludingScrollbars();

    FloatPoint constrainedOrigin = visibleContentRect.location();
    constrainedOrigin.moveBy(-origin);

    IntSize scaledTotalContentsSize = frameView.totalContentsSize();
    scaledTotalContentsSize.scale(scale * m_webPage.viewScaleFactor() / m_webPage.totalScaleFactor());

    // Scaling may have exposed the overhang area, so we need to constrain the final
    // layer position exactly like scrolling will once it's committed, to ensure that
    // scrolling doesn't make the view jump.
    constrainedOrigin = ScrollableArea::constrainScrollPositionForOverhang(roundedIntRect(visibleContentRect),
        scaledTotalContentsSize, roundedIntPoint(constrainedOrigin), frameView.scrollOrigin(),
        frameView.headerHeight(), frameView.footerHeight());
    constrainedOrigin.moveBy(-visibleContentRect.location());
    constrainedOrigin = -constrainedOrigin;

    return constrainedOrigin;
}

CoordinatedGraphicsLayer* LayerTreeHost::layerForTransientZoom() const
{
    RenderLayerBacking* renderViewBacking = m_webPage.mainFrameView()->renderView()->layer()->backing();

    if (GraphicsLayer* contentsContainmentLayer = renderViewBacking->contentsContainmentLayer())
        return &downcast<CoordinatedGraphicsLayer>(*contentsContainmentLayer);

    return &downcast<CoordinatedGraphicsLayer>(*renderViewBacking->graphicsLayer());
}

void LayerTreeHost::applyTransientZoomToLayers(double scale, FloatPoint origin)
{
    // FIXME: Scrollbars should stay in-place and change height while zooming.
    FloatPoint constrainedOrigin = constrainTransientZoomOrigin(scale, origin);
    auto* zoomLayer = layerForTransientZoom();

    TransformationMatrix transform;
    transform.translate(constrainedOrigin.x(), constrainedOrigin.y());
    transform.scale(scale);

    zoomLayer->setTransform(transform);
    zoomLayer->setAnchorPoint(FloatPoint3D());
    zoomLayer->setPosition(FloatPoint());
}

void LayerTreeHost::adjustTransientZoom(double scale, FloatPoint origin)
{
    m_transientZoom = true;
    m_transientZoomScale = scale;
    m_transientZoomOrigin = origin;

    applyTransientZoomToLayers(m_transientZoomScale, m_transientZoomOrigin);
}

void LayerTreeHost::commitTransientZoom(double scale, FloatPoint origin)
{
    if (m_transientZoomScale == scale) {
        // If the page scale is already the target scale, setPageScaleFactor() will short-circuit
        // and not apply the transform, so we can't depend on it to do so.
        TransformationMatrix finalTransform;
        finalTransform.scale(scale);

        layerForTransientZoom()->setTransform(finalTransform);
    }

    m_transientZoom = false;
    m_transientZoomScale = 1;
    m_transientZoomOrigin = FloatPoint();
}
#endif

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
