/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#import "WebPage.h"
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/MainFrame.h>
#import <WebCore/Settings.h>

using namespace WebCore;

namespace WebKit {

RemoteLayerTreeDrawingArea::RemoteLayerTreeDrawingArea(WebPage* webPage, const WebPageCreationParameters&)
    : DrawingArea(DrawingAreaTypeRemoteLayerTree, webPage)
    , m_remoteLayerTreeContext(std::make_unique<RemoteLayerTreeContext>(webPage))
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
    m_rootLayer = rootLayer ? static_cast<GraphicsLayerCARemote*>(rootLayer)->platformCALayer() : nullptr;

    m_remoteLayerTreeContext->setRootLayer(rootLayer);
}

void RemoteLayerTreeDrawingArea::scheduleCompositingLayerFlush()
{
    m_remoteLayerTreeContext->scheduleLayerFlush();
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

void RemoteLayerTreeDrawingArea::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& graphicsContext, GraphicsLayerPaintingPhase, const IntRect& clipRect)
{
    for (const auto& overlayAndLayer : m_pageOverlayLayers) {
        if (overlayAndLayer.value.get() == graphicsLayer) {
            m_webPage->drawPageOverlay(overlayAndLayer.key, graphicsContext, clipRect);
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
    m_remoteLayerTreeContext->setIsFlushingSuspended(isFrozen);
}

} // namespace WebKit
