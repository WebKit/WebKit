/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteLayerTreeDrawingArea.h"

#import "DrawingAreaProxyMessages.h"
#import "GraphicsLayerCARemote.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteLayerTreeDrawingAreaProxyMessages.h"
#import "RemoteScrollingCoordinator.h"
#import "RemoteScrollingCoordinatorTransaction.h"
#import "WebPage.h"
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/MainFrame.h>
#import <WebCore/Settings.h>
#import <WebCore/TiledBacking.h>

using namespace WebCore;

namespace WebKit {

RemoteLayerTreeDrawingArea::RemoteLayerTreeDrawingArea(WebPage* webPage, const WebPageCreationParameters&)
    : DrawingArea(DrawingAreaTypeRemoteLayerTree, webPage)
    , m_remoteLayerTreeContext(std::make_unique<RemoteLayerTreeContext>(webPage))
    , m_exposedRect(FloatRect::infiniteRect())
    , m_scrolledExposedRect(FloatRect::infiniteRect())
    , m_layerFlushTimer(this, &RemoteLayerTreeDrawingArea::layerFlushTimerFired)
    , m_isFlushingSuspended(false)
    , m_hasDeferredFlush(false)
{
    webPage->corePage()->settings().setForceCompositingMode(true);
#if PLATFORM(IOS)
    webPage->corePage()->settings().setDelegatesPageScaling(true);
#endif
}

RemoteLayerTreeDrawingArea::~RemoteLayerTreeDrawingArea()
{
}

void RemoteLayerTreeDrawingArea::setNeedsDisplay()
{
}

void RemoteLayerTreeDrawingArea::setNeedsDisplayInRect(const IntRect&)
{
}

void RemoteLayerTreeDrawingArea::scroll(const IntRect& scrollRect, const IntSize& scrollDelta)
{
}

GraphicsLayerFactory* RemoteLayerTreeDrawingArea::graphicsLayerFactory()
{
    return m_remoteLayerTreeContext.get();
}

void RemoteLayerTreeDrawingArea::setRootCompositingLayer(GraphicsLayer* rootLayer)
{
    m_rootLayer = rootLayer ? toGraphicsLayerCARemote(rootLayer)->platformCALayer() : nullptr;
}

void RemoteLayerTreeDrawingArea::updateGeometry(const IntSize& viewSize, const IntSize& layerPosition)
{
    m_viewSize = viewSize;
    m_webPage->setSize(viewSize);

    for (const auto& overlayAndLayer : m_pageOverlayLayers) {
        GraphicsLayer* layer = overlayAndLayer.value.get();
        if (layer->drawsContent())
            layer->setSize(viewSize);
    }

    scheduleCompositingLayerFlush();

    m_webPage->send(Messages::DrawingAreaProxy::DidUpdateGeometry());
}

bool RemoteLayerTreeDrawingArea::shouldUseTiledBackingForFrameView(const FrameView* frameView)
{
    return frameView && frameView->frame().isMainFrame();
}

void RemoteLayerTreeDrawingArea::updatePreferences(const WebPreferencesStore&)
{
    Settings& settings = m_webPage->corePage()->settings();

    // Fixed position elements need to be composited and create stacking contexts
    // in order to be scrolled by the ScrollingCoordinator.
    settings.setAcceleratedCompositingForFixedPositionEnabled(true);
    settings.setFixedPositionCreatesStackingContext(true);

    for (const auto& overlayAndLayer : m_pageOverlayLayers) {
        overlayAndLayer.value->setAcceleratesDrawing(settings.acceleratedDrawingEnabled());
        overlayAndLayer.value->setShowDebugBorder(settings.showDebugBorders());
        overlayAndLayer.value->setShowRepaintCounter(settings.showRepaintCounter());
    }
}

void RemoteLayerTreeDrawingArea::didInstallPageOverlay(PageOverlay* pageOverlay)
{
    std::unique_ptr<GraphicsLayerCARemote> layer(static_cast<GraphicsLayerCARemote*>(GraphicsLayer::create(graphicsLayerFactory(), this).release()));
#ifndef NDEBUG
    layer->setName("page overlay content");
#endif

    layer->setAcceleratesDrawing(m_webPage->corePage()->settings().acceleratedDrawingEnabled());
    layer->setShowDebugBorder(m_webPage->corePage()->settings().showDebugBorders());
    layer->setShowRepaintCounter(m_webPage->corePage()->settings().showRepaintCounter());

    m_rootLayer->appendSublayer(layer->platformCALayer());
    m_remoteLayerTreeContext->outOfTreeLayerWasAdded(layer.get());

    m_pageOverlayLayers.add(pageOverlay, std::move(layer));
    scheduleCompositingLayerFlush();
}

void RemoteLayerTreeDrawingArea::didUninstallPageOverlay(PageOverlay* pageOverlay)
{
    std::unique_ptr<GraphicsLayerCARemote> layer = m_pageOverlayLayers.take(pageOverlay);
    ASSERT(layer);

    m_remoteLayerTreeContext->outOfTreeLayerWillBeRemoved(layer.get());
    layer->platformCALayer()->removeFromSuperlayer();

    scheduleCompositingLayerFlush();
}

void RemoteLayerTreeDrawingArea::setPageOverlayNeedsDisplay(PageOverlay* pageOverlay, const IntRect& rect)
{
    GraphicsLayerCARemote* layer = m_pageOverlayLayers.get(pageOverlay);

    if (!layer)
        return;

    if (!layer->drawsContent()) {
        layer->setDrawsContent(true);
        layer->setSize(m_viewSize);
    }

    layer->setNeedsDisplayInRect(rect);
    scheduleCompositingLayerFlush();
}

void RemoteLayerTreeDrawingArea::setPageOverlayOpacity(PageOverlay* pageOverlay, float opacity)
{
    GraphicsLayerCARemote* layer = m_pageOverlayLayers.get(pageOverlay);
    
    if (!layer)
        return;
    
    layer->setOpacity(opacity);
    scheduleCompositingLayerFlush();
}

void RemoteLayerTreeDrawingArea::clearPageOverlay(PageOverlay* pageOverlay)
{
    GraphicsLayer* layer = m_pageOverlayLayers.get(pageOverlay);

    if (!layer)
        return;

    layer->setDrawsContent(false);
    layer->setSize(IntSize());
    scheduleCompositingLayerFlush();
}

void RemoteLayerTreeDrawingArea::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& graphicsContext, GraphicsLayerPaintingPhase, const FloatRect& clipRect)
{
    for (const auto& overlayAndLayer : m_pageOverlayLayers) {
        if (overlayAndLayer.value.get() == graphicsLayer) {
            m_webPage->drawPageOverlay(overlayAndLayer.key, graphicsContext, enclosingIntRect(clipRect));
            break;
        }
    }
}

float RemoteLayerTreeDrawingArea::deviceScaleFactor() const
{
    return m_webPage->corePage()->deviceScaleFactor();
}

#if PLATFORM(IOS)
void RemoteLayerTreeDrawingArea::setDeviceScaleFactor(float deviceScaleFactor)
{
    m_webPage->setDeviceScaleFactor(deviceScaleFactor);
}
#endif

void RemoteLayerTreeDrawingArea::setLayerTreeStateIsFrozen(bool isFrozen)
{
    if (m_isFlushingSuspended == isFrozen)
        return;

    m_isFlushingSuspended = isFrozen;

    if (!m_isFlushingSuspended && m_hasDeferredFlush) {
        m_hasDeferredFlush = false;
        scheduleCompositingLayerFlush();
    }
}

void RemoteLayerTreeDrawingArea::forceRepaint()
{
    if (m_isFlushingSuspended)
        return;

    for (Frame* frame = &m_webPage->corePage()->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        FrameView* frameView = frame->view();
        if (!frameView || !frameView->tiledBacking())
            continue;

        frameView->tiledBacking()->forceRepaint();
    }

    flushLayers();
}

void RemoteLayerTreeDrawingArea::setExposedRect(const FloatRect& exposedRect)
{
    m_exposedRect = exposedRect;
    updateScrolledExposedRect();
}

#if PLATFORM(IOS)
void RemoteLayerTreeDrawingArea::setExposedContentRect(const FloatRect& exposedContentRect)
{
    FrameView* frameView = m_webPage->corePage()->mainFrame().view();
    if (!frameView)
        return;

    frameView->setExposedContentRect(enclosingIntRect(exposedContentRect));
    scheduleCompositingLayerFlush();
}
#endif

void RemoteLayerTreeDrawingArea::updateScrolledExposedRect()
{
    FrameView* frameView = m_webPage->corePage()->mainFrame().view();
    if (!frameView)
        return;

    m_scrolledExposedRect = m_exposedRect;

#if !PLATFORM(IOS)
    if (!m_exposedRect.isInfinite()) {
        IntPoint scrollPositionWithOrigin = frameView->scrollPosition() + toIntSize(frameView->scrollOrigin());
        m_scrolledExposedRect.moveBy(scrollPositionWithOrigin);
    }
#endif

    frameView->setExposedRect(m_scrolledExposedRect);

    for (const auto& layer : m_pageOverlayLayers.values()) {
        if (TiledBacking* tiledBacking = layer->tiledBacking())
            tiledBacking->setExposedRect(m_scrolledExposedRect);
    }
    
    frameView->adjustTiledBackingCoverage();
}

void RemoteLayerTreeDrawingArea::setCustomFixedPositionRect(const FloatRect& fixedPositionRect)
{
#if PLATFORM(IOS)
    FrameView* frameView = m_webPage->corePage()->mainFrame().view();
    if (!frameView)
        return;

    frameView->setCustomFixedPositionLayoutRect(enclosingIntRect(fixedPositionRect));
#endif
}

TiledBacking* RemoteLayerTreeDrawingArea::mainFrameTiledBacking() const
{
    FrameView* frameView = m_webPage->corePage()->mainFrame().view();
    return frameView ? frameView->tiledBacking() : 0;
}

void RemoteLayerTreeDrawingArea::scheduleCompositingLayerFlush()
{
    if (m_layerFlushTimer.isActive())
        return;

    m_layerFlushTimer.startOneShot(0);
}

void RemoteLayerTreeDrawingArea::layerFlushTimerFired(WebCore::Timer<RemoteLayerTreeDrawingArea>*)
{
    flushLayers();
}

void RemoteLayerTreeDrawingArea::flushLayers()
{
    if (!m_rootLayer)
        return;

    if (m_isFlushingSuspended) {
        m_hasDeferredFlush = true;
        return;
    }

    m_webPage->layoutIfNeeded();
    m_webPage->corePage()->mainFrame().view()->flushCompositingStateIncludingSubframes();

    m_remoteLayerTreeContext->flushOutOfTreeLayers();

    ASSERT(m_rootLayer);

    // FIXME: minize these transactions if nothing changed.
    RemoteLayerTreeTransaction layerTransaction;
    m_remoteLayerTreeContext->buildTransaction(layerTransaction, *m_rootLayer);
    m_webPage->willCommitLayerTree(layerTransaction);

    RemoteScrollingCoordinatorTransaction scrollingTransaction;
#if ENABLE(ASYNC_SCROLLING)
    if (m_webPage->scrollingCoordinator())
        toRemoteScrollingCoordinator(m_webPage->scrollingCoordinator())->buildTransaction(scrollingTransaction);
#endif

    m_webPage->send(Messages::RemoteLayerTreeDrawingAreaProxy::CommitLayerTree(layerTransaction, scrollingTransaction));
}

} // namespace WebKit
