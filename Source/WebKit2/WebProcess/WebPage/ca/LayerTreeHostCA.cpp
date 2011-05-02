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
#include "LayerTreeHostCA.h"

#include "DrawingAreaImpl.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsLayerCA.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/Settings.h>

using namespace WebCore;

namespace WebKit {

LayerTreeHostCA::LayerTreeHostCA(WebPage* webPage)
    : LayerTreeHost(webPage)
    , m_isValid(true)
    , m_notifyAfterScheduledLayerFlush(false)
{
}

void LayerTreeHostCA::initialize()
{
    // Create a root layer.
    m_rootLayer = GraphicsLayer::create(this);
#ifndef NDEBUG
    m_rootLayer->setName("LayerTreeHost root layer");
#endif
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setSize(m_webPage->size());
    static_cast<GraphicsLayerCA*>(m_rootLayer.get())->platformCALayer()->setGeometryFlipped(true);

    m_nonCompositedContentLayer = GraphicsLayer::create(this);
    static_cast<GraphicsLayerCA*>(m_nonCompositedContentLayer.get())->setAllowTiledLayer(false);
#ifndef NDEBUG
    m_nonCompositedContentLayer->setName("LayerTreeHost non-composited content");
#endif
    m_nonCompositedContentLayer->setDrawsContent(true);
    m_nonCompositedContentLayer->setContentsOpaque(m_webPage->drawsBackground() && !m_webPage->drawsTransparentBackground());
    m_nonCompositedContentLayer->setSize(m_webPage->size());
    if (m_webPage->corePage()->settings()->acceleratedDrawingEnabled())
        m_nonCompositedContentLayer->setAcceleratesDrawing(true);

    m_rootLayer->addChild(m_nonCompositedContentLayer.get());

    if (m_webPage->hasPageOverlay())
        createPageOverlayLayer();

    platformInitialize(m_layerTreeContext);

    scheduleLayerFlush();
}

LayerTreeHostCA::~LayerTreeHostCA()
{
    ASSERT(!m_isValid);
    ASSERT(!m_rootLayer);
}

const LayerTreeContext& LayerTreeHostCA::layerTreeContext()
{
    return m_layerTreeContext;
}

void LayerTreeHostCA::setShouldNotifyAfterNextScheduledLayerFlush(bool notifyAfterScheduledLayerFlush)
{
    m_notifyAfterScheduledLayerFlush = notifyAfterScheduledLayerFlush;
}

void LayerTreeHostCA::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    m_nonCompositedContentLayer->removeAllChildren();

    // Add the accelerated layer tree hierarchy.
    if (graphicsLayer)
        m_nonCompositedContentLayer->addChild(graphicsLayer);
}

void LayerTreeHostCA::invalidate()
{
    ASSERT(m_isValid);
    m_rootLayer = nullptr;
    m_isValid = false;
}

void LayerTreeHostCA::setNonCompositedContentsNeedDisplay(const IntRect& rect)
{
    m_nonCompositedContentLayer->setNeedsDisplayInRect(rect);
    if (m_pageOverlayLayer)
        m_pageOverlayLayer->setNeedsDisplayInRect(rect);

    scheduleLayerFlush();
}

void LayerTreeHostCA::scrollNonCompositedContents(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    setNonCompositedContentsNeedDisplay(scrollRect);
}

void LayerTreeHostCA::sizeDidChange(const IntSize& newSize)
{
    m_rootLayer->setSize(newSize);

    // If the newSize exposes new areas of the non-composited content a setNeedsDisplay is needed
    // for those newly exposed areas.
    FloatSize oldSize = m_nonCompositedContentLayer->size();
    m_nonCompositedContentLayer->setSize(newSize);

    if (newSize.width() > oldSize.width()) {
        float height = std::min(static_cast<float>(newSize.height()), oldSize.height());
        m_nonCompositedContentLayer->setNeedsDisplayInRect(FloatRect(oldSize.width(), 0, newSize.width() - oldSize.width(), height));
    }

    if (newSize.height() > oldSize.height())
        m_nonCompositedContentLayer->setNeedsDisplayInRect(FloatRect(0, oldSize.height(), newSize.width(), newSize.height() - oldSize.height()));

    if (m_pageOverlayLayer)
        m_pageOverlayLayer->setSize(newSize);

    scheduleLayerFlush();
    flushPendingLayerChanges();
}

void LayerTreeHostCA::forceRepaint()
{
    scheduleLayerFlush();
    flushPendingLayerChanges();
}    

void LayerTreeHostCA::didInstallPageOverlay()
{
    createPageOverlayLayer();
    scheduleLayerFlush();
}

void LayerTreeHostCA::didUninstallPageOverlay()
{
    destroyPageOverlayLayer();
    scheduleLayerFlush();
}

void LayerTreeHostCA::setPageOverlayNeedsDisplay(const IntRect& rect)
{
    ASSERT(m_pageOverlayLayer);
    m_pageOverlayLayer->setNeedsDisplayInRect(rect);
    scheduleLayerFlush();
}

void LayerTreeHostCA::notifyAnimationStarted(const WebCore::GraphicsLayer*, double time)
{
}

void LayerTreeHostCA::notifySyncRequired(const WebCore::GraphicsLayer*)
{
}

void LayerTreeHostCA::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& graphicsContext, GraphicsLayerPaintingPhase, const IntRect& clipRect)
{
    if (graphicsLayer == m_nonCompositedContentLayer) {
        m_webPage->drawRect(graphicsContext, clipRect);
        return;
    }

    if (graphicsLayer == m_pageOverlayLayer) {
        m_webPage->drawPageOverlay(graphicsContext, clipRect);
        return;
    }
}

bool LayerTreeHostCA::showDebugBorders() const
{
    return m_webPage->corePage()->settings()->showDebugBorders();
}

bool LayerTreeHostCA::showRepaintCounter() const
{
    return m_webPage->corePage()->settings()->showRepaintCounter();
}

void LayerTreeHostCA::performScheduledLayerFlush()
{
    {
        RefPtr<LayerTreeHostCA> protect(this);
        m_webPage->layoutIfNeeded();

        if (!m_isValid)
            return;
    }

    if (!flushPendingLayerChanges())
        return;

    didPerformScheduledLayerFlush();
}

void LayerTreeHostCA::didPerformScheduledLayerFlush()
{
    if (m_notifyAfterScheduledLayerFlush) {
        // Let the drawing area know that we've done a flush of the layer changes.
        static_cast<DrawingAreaImpl*>(m_webPage->drawingArea())->layerHostDidFlushLayers();
        m_notifyAfterScheduledLayerFlush = false;
    }
}

bool LayerTreeHostCA::flushPendingLayerChanges()
{
    m_rootLayer->syncCompositingStateForThisLayerOnly();
    m_nonCompositedContentLayer->syncCompositingStateForThisLayerOnly();
    if (m_pageOverlayLayer)
        m_pageOverlayLayer->syncCompositingStateForThisLayerOnly();

    return m_webPage->corePage()->mainFrame()->view()->syncCompositingStateIncludingSubframes();
}

void LayerTreeHostCA::createPageOverlayLayer()
{
    ASSERT(!m_pageOverlayLayer);

    m_pageOverlayLayer = GraphicsLayer::create(this);
#ifndef NDEBUG
    m_pageOverlayLayer->setName("LayerTreeHost page overlay content");
#endif

    m_pageOverlayLayer->setDrawsContent(true);
    m_pageOverlayLayer->setSize(m_webPage->size());

    m_rootLayer->addChild(m_pageOverlayLayer.get());
}

void LayerTreeHostCA::destroyPageOverlayLayer()
{
    ASSERT(m_pageOverlayLayer);
    m_pageOverlayLayer->removeFromParent();
    m_pageOverlayLayer = nullptr;
}

} // namespace WebKit
