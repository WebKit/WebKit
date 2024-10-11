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
#include "WebProcess.h"
#include <WebCore/AsyncScrollingCoordinator.h>
#include <WebCore/Chrome.h>
#include <WebCore/Damage.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/NativeImage.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/RenderLayerBacking.h>
#include <WebCore/RenderView.h>
#include <WebCore/ThreadedScrollingTree.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>

#if USE(CAIRO)
#include <WebCore/NicosiaPaintingEngine.h>
#elif USE(SKIA)
#include <WebCore/ProcessCapabilities.h>
#include <WebCore/SkiaThreadedPaintingPool.h>
#endif

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(LayerTreeHost);

#if HAVE(DISPLAY_LINK)
LayerTreeHost::LayerTreeHost(WebPage& webPage)
#else
LayerTreeHost::LayerTreeHost(WebPage& webPage, WebCore::PlatformDisplayID displayID)
#endif
    : m_webPage(webPage)
    , m_viewportController(webPage.size())
    , m_layerFlushTimer(RunLoop::main(), this, &LayerTreeHost::layerFlushTimerFired)
#if !HAVE(DISPLAY_LINK)
    , m_displayID(displayID)
#endif
#if USE(CAIRO)
    , m_paintingEngine(Nicosia::PaintingEngine::create())
#endif
{
#if USE(SKIA)
    if (ProcessCapabilities::canUseAcceleratedBuffers() && PlatformDisplay::sharedDisplay().skiaGLContext())
        m_skiaAcceleratedBitmapTexturePool = makeUnique<BitmapTexturePool>();
    else
        m_skiaThreadedPaintingPool = SkiaThreadedPaintingPool::create();
#endif

    m_nicosia.scene = Nicosia::Scene::create();
    m_nicosia.sceneIntegration = Nicosia::SceneIntegration::create(*m_nicosia.scene, *this);

    m_rootLayer = GraphicsLayer::create(this, *this);
#ifndef NDEBUG
    m_rootLayer->setName(MAKE_STATIC_STRING_IMPL("LayerTreeHost root layer"));
#endif
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setSize(m_webPage.size());

#if USE(GLIB_EVENT_LOOP)
    m_layerFlushTimer.setPriority(RunLoopSourcePriority::LayerFlushTimer);
    m_layerFlushTimer.setName("[WebKit] LayerTreeHost"_s);
#endif
    scheduleLayerFlush();

    if (auto* frameView = m_webPage.localMainFrameView()) {
        auto contentsSize = frameView->contentsSize();
        if (!contentsSize.isEmpty())
            m_viewportController.didChangeContentsSize(contentsSize);
    }

#if HAVE(DISPLAY_LINK)
    m_compositor = ThreadedCompositor::create(*this, m_webPage.deviceScaleFactor() * m_viewportController.pageScaleFactor());
#else
    m_compositor = ThreadedCompositor::create(*this, *this, m_webPage.deviceScaleFactor() * m_viewportController.pageScaleFactor(), displayID);
#endif
    m_layerTreeContext.contextID = m_compositor->surfaceID();

    didChangeViewport();
}

LayerTreeHost::~LayerTreeHost()
{
    cancelPendingLayerFlush();

    m_nicosia.sceneIntegration->invalidate();

    m_rootLayer = nullptr;
    {
        SetForScope purgingToggle(m_isPurgingBackingStores, true);
        for (auto& registeredLayer : m_registeredLayers.values()) {
            registeredLayer->purgeBackingStores();
            registeredLayer->invalidateCoordinator();
        }
    }

#if USE(SKIA)
    m_skiaAcceleratedBitmapTexturePool = nullptr;
    m_skiaThreadedPaintingPool = nullptr;
#endif

    m_compositor->invalidate();
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

void LayerTreeHost::scheduleLayerFlush()
{
    WTFEmitSignpost(this, ScheduleLayerFlush, "isWaitingForRenderer %i", m_isWaitingForRenderer);

    if (!m_layerFlushSchedulingEnabled)
        return;

    if (m_webPage.size().isEmpty())
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

void LayerTreeHost::flushLayers()
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    TraceScope traceScope(FlushPendingLayerChangesStart, FlushPendingLayerChangesEnd);
#endif
    SetForScope isFlushingLayerChanges(m_isFlushingLayerChanges, true);

    bool shouldSyncFrame = false;
    if (!m_didInitializeRootCompositingLayer) {
        auto& rootLayer = downcast<CoordinatedGraphicsLayer>(*m_rootLayer);
        m_nicosia.state.rootLayer = rootLayer.compositionLayer();
        m_didInitializeRootCompositingLayer = true;
        shouldSyncFrame = true;
    }

    Ref page { m_webPage };
    page->updateRendering();
    page->flushPendingEditorStateUpdate();

    WTFBeginSignpost(this, FlushRootCompositingLayer);
    m_rootLayer->flushCompositingStateForThisLayerOnly();
    if (m_overlayCompositingLayer)
        m_overlayCompositingLayer->flushCompositingState(m_visibleContentsRect);
    WTFEndSignpost(this, FlushRootCompositingLayer);

    OptionSet<FinalizeRenderingUpdateFlags> flags;
#if PLATFORM(GTK)
    if (!m_transientZoom)
        flags.add(FinalizeRenderingUpdateFlags::ApplyScrollingTreeLayerPositions);
#else
    flags.add(FinalizeRenderingUpdateFlags::ApplyScrollingTreeLayerPositions);
#endif
    page->finalizeRenderingUpdate(flags);

    WTFBeginSignpost(this, FinalizeCompositingStateFlush);
    auto& coordinatedLayer = downcast<CoordinatedGraphicsLayer>(*m_rootLayer);
    auto [performLayerSync, platformLayerUpdated] = coordinatedLayer.finalizeCompositingStateFlush();
    shouldSyncFrame |= performLayerSync;
    shouldSyncFrame |= m_forceFrameSync;
    WTFEndSignpost(this, FinalizeCompositingStateFlush);

    if (shouldSyncFrame) {
        WTFBeginSignpost(this, SyncFrame);

        m_nicosia.scene->accessState([this](Nicosia::Scene::State& state) {
            for (auto& compositionLayer : m_nicosia.state.layers) {
                compositionLayer->flushState([] (const Nicosia::CompositionLayer::LayerState& state) {
                    if (state.backingStore)
                        state.backingStore->flushUpdate();
                });
            }

            ++state.id;
            state.layers = m_nicosia.state.layers;
            state.rootLayer = m_nicosia.state.rootLayer;
        });

        commitSceneState(m_nicosia.scene);
        m_forceFrameSync = false;

        WTFEndSignpost(this, SyncFrame);
    }
#if HAVE(DISPLAY_LINK)
    else if (platformLayerUpdated)
        commitSceneState(nullptr);
#endif

    page->didUpdateRendering();

    // Eject any backing stores whose only reference is held in the HashMap cache.
    m_imageBackingStores.removeIf(
        [](auto& it) {
            return it.value->hasOneRef();
        });
}

void LayerTreeHost::layerFlushTimerFired()
{
    WTFBeginSignpost(this, LayerFlushTimerFired, "isWaitingForRenderer %i", m_isWaitingForRenderer);

    if (m_isSuspended) {
        WTFEndSignpost(this, LayerFlushTimerFired);
        return;
    }

    if (m_isWaitingForRenderer) {
        WTFEndSignpost(this, LayerFlushTimerFired);
        return;
    }

    // If a force-repaint callback was registered, we should force a 'frame sync' that
    // will guarantee us a call to renderNextFrame() once the update is complete.
    if (m_forceRepaintAsync.callback)
        m_forceFrameSync = true;

    flushLayers();

#if PLATFORM(GTK)
    // If we have an active transient zoom, we want the zoom to win over any changes
    // that WebCore makes to the relevant layers, so re-apply our changes after flushing.
    if (m_transientZoom)
        applyTransientZoomToLayers(m_transientZoomScale, m_transientZoomOrigin);
#endif

    WTFEndSignpost(this, LayerFlushTimerFired);
}

void LayerTreeHost::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    if (m_rootCompositingLayer == graphicsLayer)
        return;

    if (m_rootCompositingLayer)
        m_rootCompositingLayer->removeFromParent();

    m_rootCompositingLayer = graphicsLayer;
    if (m_rootCompositingLayer)
        m_rootLayer->addChildAtIndex(*m_rootCompositingLayer, 0);
}

void LayerTreeHost::setViewOverlayRootLayer(GraphicsLayer* graphicsLayer)
{
    if (m_overlayCompositingLayer == graphicsLayer)
        return;

    if (m_overlayCompositingLayer)
        m_overlayCompositingLayer->removeFromParent();

    m_overlayCompositingLayer = graphicsLayer;
    if (m_overlayCompositingLayer)
        m_rootLayer->addChild(*m_overlayCompositingLayer);
}

void LayerTreeHost::scrollNonCompositedContents(const IntRect& rect)
{
    m_scrolledSinceLastFrame = true;

    auto* frameView = m_webPage.localMainFrameView();
    if (!frameView || !frameView->delegatesScrolling())
        return;

    m_viewportController.didScroll(rect.location());
    didChangeViewport();
}

void LayerTreeHost::forceRepaint()
{
    // This is necessary for running layout tests. Since in this case we are not waiting for a UIProcess to reply nicely.
    // Instead we are just triggering forceRepaint. But we still want to have the scripted animation callbacks being executed.
    if (auto* frameView = m_webPage.localMainFrameView())
        frameView->updateLayoutAndStyleIfNeededRecursive();

    // We need to schedule another flush, otherwise the forced paint might cancel a later expected flush.
    m_forceFrameSync = true;
    scheduleLayerFlush();

    if (!m_isWaitingForRenderer)
        flushLayers();

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
    m_rootLayer->setSize(size);
    scheduleLayerFlush();

    m_viewportController.didChangeViewportSize(size);
    m_compositor->setViewportSize(size, m_webPage.deviceScaleFactor() * m_viewportController.pageScaleFactor());
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
    return this;
}

void LayerTreeHost::contentsSizeChanged(const IntSize& newSize)
{
    m_viewportController.didChangeContentsSize(newSize);
    didChangeViewport();
}

void LayerTreeHost::didChangeViewportAttributes(ViewportAttributes&& attr)
{
    m_viewportController.didChangeViewportAttributes(WTFMove(attr));
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
    auto* localMainFrame = dynamicDowncast<WebCore::LocalFrame>(m_webPage.corePage()->mainFrame());
    auto* view = localMainFrame ? localMainFrame->view() : nullptr;
    if (!view)
        return;

    auto* scrollbar = view->verticalScrollbar();
    if (scrollbar && !scrollbar->isOverlayScrollbar())
        visibleRect.expand(scrollbar->width(), 0);
    scrollbar = view->horizontalScrollbar();
    if (scrollbar && !scrollbar->isOverlayScrollbar())
        visibleRect.expand(0, scrollbar->height());

    if (visibleRect != m_visibleContentsRect) {
        m_visibleContentsRect = visibleRect;
        for (auto& registeredLayer : m_registeredLayers.values())
            registeredLayer->setNeedsVisibleRectAdjustment();
        if (view->useFixedLayout()) {
            // Round the rect instead of enclosing it to make sure that its size stay
            // the same while panning. This can have nasty effects on layout.
            view->setFixedVisibleContentRect(roundedIntRect(visibleRect));
        }
    }

    scheduleLayerFlush();

    float pageScale = m_viewportController.pageScaleFactor();
    IntPoint scrollPosition = roundedIntPoint(visibleRect.location());
    if (m_lastScrollPosition != scrollPosition) {
        m_scrolledSinceLastFrame = true;
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

void LayerTreeHost::deviceOrPageScaleFactorChanged()
{
    m_webPage.corePage()->pageOverlayController().didChangeDeviceScaleFactor();
    m_compositor->setViewportSize(m_webPage.size(), m_webPage.deviceScaleFactor() * m_viewportController.pageScaleFactor());
    didChangeViewport();
}

void LayerTreeHost::backgroundColorDidChange()
{
    m_compositor->backgroundColorDidChange();
}

void LayerTreeHost::detachLayer(CoordinatedGraphicsLayer* layer)
{
    if (m_isPurgingBackingStores)
        return;

    {
        auto& compositionLayer = layer->compositionLayer();
        m_nicosia.state.layers.remove(compositionLayer);
        compositionLayer->setSceneIntegration(nullptr);
    }
    m_registeredLayers.remove(layer->id());
}

void LayerTreeHost::attachLayer(CoordinatedGraphicsLayer* layer)
{
    {
        auto& compositionLayer = layer->compositionLayer();
        m_nicosia.state.layers.add(compositionLayer);
        compositionLayer->setSceneIntegration(m_nicosia.sceneIntegration.copyRef());
    }
    m_registeredLayers.add(layer->id(), layer);
    layer->setNeedsVisibleRectAdjustment();
}

#if USE(CAIRO)
Nicosia::PaintingEngine& LayerTreeHost::paintingEngine()
{
    return *m_paintingEngine;
}
#endif

Ref<CoordinatedImageBackingStore> LayerTreeHost::imageBackingStore(Ref<NativeImage>&& nativeImage)
{
    auto nativeImageID = CoordinatedImageBackingStore::uniqueIDForNativeImage(nativeImage.get());
    auto addResult = m_imageBackingStores.ensure(nativeImageID, [&] {
        return CoordinatedImageBackingStore::create(WTFMove(nativeImage));
    });
    return addResult.iterator->value;
}

Ref<GraphicsLayer> LayerTreeHost::createGraphicsLayer(GraphicsLayer::Type layerType, GraphicsLayerClient& client)
{
    auto layer = adoptRef(*new CoordinatedGraphicsLayer(layerType, client));
    layer->setCoordinatorIncludingSubLayersIfNeeded(this);
    return layer;
}

#if !HAVE(DISPLAY_LINK)
RefPtr<DisplayRefreshMonitor> LayerTreeHost::createDisplayRefreshMonitor(PlatformDisplayID displayID)
{
    ASSERT(m_displayID == displayID);
    return Ref { m_compositor->displayRefreshMonitor() };
}

void LayerTreeHost::requestDisplayRefreshMonitorUpdate()
{
    // Flush layers to cause a repaint. If m_isWaitingForRenderer was true at this point, the layer
    // flush won't do anything, but that means there's a painting ongoing that will send the
    // display refresh notification when it's done.
    m_forceFrameSync = true;
    scheduleLayerFlush();
}

void LayerTreeHost::handleDisplayRefreshMonitorUpdate(bool hasBeenRescheduled)
{
    // Call renderNextFrame. If hasBeenRescheduled is true, the layer flush will force a repaint
    // that will cause the display refresh notification to come.
    renderNextFrame(hasBeenRescheduled);
}
#endif

void LayerTreeHost::willRenderFrame()
{
    if (auto* drawingArea = m_webPage.drawingArea())
        drawingArea->willStartRenderingUpdateDisplay();
}

void LayerTreeHost::didRenderFrame()
{
    if (auto* drawingArea = m_webPage.drawingArea())
        drawingArea->didCompleteRenderingUpdateDisplay();
}

#if HAVE(DISPLAY_LINK)
void LayerTreeHost::didComposite(uint32_t compositionResponseID)
{
    if (!m_isWaitingForRenderer || (m_isWaitingForRenderer && m_compositionRequestID == compositionResponseID))
        renderNextFrame(false);
}
#endif

void LayerTreeHost::commitSceneState(const RefPtr<Nicosia::Scene>& state)
{
    m_isWaitingForRenderer = true;
    m_compositionRequestID = m_compositor->requestComposition(state);
    WTFEmitSignpost(this, CommitSceneState, "compositionRequestID %i", m_compositionRequestID);
}

void LayerTreeHost::requestUpdate()
{
    m_compositor->updateScene();
}

void LayerTreeHost::renderNextFrame(bool forceRepaint)
{
    WTFBeginSignpost(this, RenderNextFrame);

    m_isWaitingForRenderer = false;
    bool scheduledWhileWaitingForRenderer = std::exchange(m_scheduledWhileWaitingForRenderer, false);

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
            m_forceFrameSync = true;
        layerFlushTimerFired();
    }

    WTFEndSignpost(this, RenderNextFrame);
}

#if PLATFORM(GTK)
FloatPoint LayerTreeHost::constrainTransientZoomOrigin(double scale, FloatPoint origin) const
{
    auto* frameView = m_webPage.localMainFrameView();
    if (!frameView)
        return origin;

    FloatRect visibleContentRect = frameView->visibleContentRectIncludingScrollbars();

    FloatPoint constrainedOrigin = visibleContentRect.location();
    constrainedOrigin.moveBy(-origin);

    IntSize scaledTotalContentsSize = frameView->totalContentsSize();
    scaledTotalContentsSize.scale(scale * m_webPage.viewScaleFactor() / m_webPage.totalScaleFactor());

    // Scaling may have exposed the overhang area, so we need to constrain the final
    // layer position exactly like scrolling will once it's committed, to ensure that
    // scrolling doesn't make the view jump.
    constrainedOrigin = ScrollableArea::constrainScrollPositionForOverhang(roundedIntRect(visibleContentRect),
        scaledTotalContentsSize, roundedIntPoint(constrainedOrigin), frameView->scrollOrigin(),
        frameView->headerHeight(), frameView->footerHeight());
    constrainedOrigin.moveBy(-visibleContentRect.location());
    constrainedOrigin = -constrainedOrigin;

    return constrainedOrigin;
}

CoordinatedGraphicsLayer* LayerTreeHost::layerForTransientZoom() const
{
    auto* frameView = m_webPage.localMainFrameView();
    if (!frameView)
        return nullptr;

    RenderLayerBacking* renderViewBacking = frameView->renderView()->layer()->backing();

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

#if PLATFORM(WPE) && USE(GBM) && ENABLE(WPE_PLATFORM)
void LayerTreeHost::preferredBufferFormatsDidChange()
{
    m_compositor->preferredBufferFormatsDidChange();
}
#endif

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
