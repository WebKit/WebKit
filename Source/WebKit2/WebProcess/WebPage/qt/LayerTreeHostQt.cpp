/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
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

#if USE(ACCELERATED_COMPOSITING)

#include "LayerTreeHostQt.h"

#include "DrawingAreaImpl.h"
#include "GraphicsContext.h"
#include "LayerTreeHostProxyMessages.h"
#include "MessageID.h"
#include "WebGraphicsLayer.h"
#include "WebPage.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/Page.h>
#include <WebCore/Settings.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<LayerTreeHostQt> LayerTreeHostQt::create(WebPage* webPage)
{
    return adoptRef(new LayerTreeHostQt(webPage));
}

LayerTreeHostQt::~LayerTreeHostQt()
{
}

LayerTreeHostQt::LayerTreeHostQt(WebPage* webPage)
    : LayerTreeHost(webPage)
    , m_notifyAfterScheduledLayerFlush(false)
    , m_isValid(true)
#if USE(TILED_BACKING_STORE)
    , m_waitingForUIProcess(false)
    , m_isSuspended(false)
#endif
    , m_shouldSyncFrame(false)
    , m_shouldSyncRootLayer(true)
    , m_layerFlushTimer(this, &LayerTreeHostQt::layerFlushTimerFired)
    , m_layerFlushSchedulingEnabled(true)
{
    // Create a root layer.
    m_rootLayer = GraphicsLayer::create(this);
    WebGraphicsLayer* webRootLayer = toWebGraphicsLayer(m_rootLayer.get());
    webRootLayer->setRootLayer(true);
#ifndef NDEBUG
    m_rootLayer->setName("LayerTreeHostQt root layer");
#endif
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setSize(m_webPage->size());
    m_layerTreeContext.webLayerID = toWebGraphicsLayer(webRootLayer)->id();

    m_nonCompositedContentLayer = GraphicsLayer::create(this);
#if USE(TILED_BACKING_STORE)
    toWebGraphicsLayer(m_rootLayer.get())->setLayerTreeTileClient(this);
#endif
#ifndef NDEBUG
    m_nonCompositedContentLayer->setName("LayerTreeHostQt non-composited content");
#endif
    m_nonCompositedContentLayer->setDrawsContent(true);
    m_nonCompositedContentLayer->setContentsOpaque(m_webPage->drawsBackground() && !m_webPage->drawsTransparentBackground());
    m_nonCompositedContentLayer->setSize(m_webPage->size());

    m_rootLayer->addChild(m_nonCompositedContentLayer.get());

    if (m_webPage->hasPageOverlay())
        createPageOverlayLayer();

    scheduleLayerFlush();
}

void LayerTreeHostQt::setLayerFlushSchedulingEnabled(bool layerFlushingEnabled)
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

void LayerTreeHostQt::scheduleLayerFlush()
{
    if (!m_layerFlushSchedulingEnabled)
        return;

    if (!m_layerFlushTimer.isActive())
        m_layerFlushTimer.startOneShot(0);
}

void LayerTreeHostQt::cancelPendingLayerFlush()
{
    m_layerFlushTimer.stop();
}

void LayerTreeHostQt::setShouldNotifyAfterNextScheduledLayerFlush(bool notifyAfterScheduledLayerFlush)
{
    m_notifyAfterScheduledLayerFlush = notifyAfterScheduledLayerFlush;
}

void LayerTreeHostQt::setRootCompositingLayer(WebCore::GraphicsLayer* graphicsLayer)
{
    m_nonCompositedContentLayer->removeAllChildren();

    // Add the accelerated layer tree hierarchy.
    if (graphicsLayer)
        m_nonCompositedContentLayer->addChild(graphicsLayer);
}

void LayerTreeHostQt::invalidate()
{
    cancelPendingLayerFlush();

    ASSERT(m_isValid);
    m_rootLayer = nullptr;
    m_isValid = false;
}

void LayerTreeHostQt::setNonCompositedContentsNeedDisplay(const WebCore::IntRect& rect)
{
    m_nonCompositedContentLayer->setNeedsDisplayInRect(rect);
    if (m_pageOverlayLayer)
        m_pageOverlayLayer->setNeedsDisplayInRect(rect);

    scheduleLayerFlush();
}

void LayerTreeHostQt::scrollNonCompositedContents(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset)
{
    setNonCompositedContentsNeedDisplay(scrollRect);
}

void LayerTreeHostQt::forceRepaint()
{
    scheduleLayerFlush();
}

void LayerTreeHostQt::sizeDidChange(const WebCore::IntSize& newSize)
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
}

void LayerTreeHostQt::didInstallPageOverlay()
{
    createPageOverlayLayer();
    scheduleLayerFlush();
}

void LayerTreeHostQt::didUninstallPageOverlay()
{
    destroyPageOverlayLayer();
    scheduleLayerFlush();
}

void LayerTreeHostQt::setPageOverlayNeedsDisplay(const WebCore::IntRect& rect)
{
    ASSERT(m_pageOverlayLayer);
    m_pageOverlayLayer->setNeedsDisplayInRect(rect);
    scheduleLayerFlush();
}

bool LayerTreeHostQt::flushPendingLayerChanges()
{
    bool didSync = m_webPage->corePage()->mainFrame()->view()->syncCompositingStateIncludingSubframes();
    m_nonCompositedContentLayer->syncCompositingStateForThisLayerOnly();
    if (m_pageOverlayLayer)
        m_pageOverlayLayer->syncCompositingStateForThisLayerOnly();

    m_rootLayer->syncCompositingStateForThisLayerOnly();
    return didSync;
}

void LayerTreeHostQt::didSyncCompositingStateForLayer(const WebLayerInfo& info)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeHostProxy::SyncCompositingLayerState(info));
}

void LayerTreeHostQt::didDeleteLayer(WebLayerID id)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeHostProxy::DeleteCompositingLayer(id));
}

void LayerTreeHostQt::performScheduledLayerFlush()
{
    m_webPage->layoutIfNeeded();

    if (!m_isValid)
        return;

#if USE(TILED_BACKING_STORE)
    if (m_isSuspended || m_waitingForUIProcess)
        return;
#endif

    m_shouldSyncFrame = false;
    flushPendingLayerChanges();
    if (!m_shouldSyncFrame)
        return;

    if (m_shouldSyncRootLayer) {
        m_webPage->send(Messages::LayerTreeHostProxy::SetRootCompositingLayer(toWebGraphicsLayer(m_rootLayer.get())->id()));
        m_shouldSyncRootLayer = false;
    }

    m_webPage->send(Messages::LayerTreeHostProxy::DidRenderFrame());

    if (!m_notifyAfterScheduledLayerFlush)
        return;

    // Let the drawing area know that we've done a flush of the layer changes.
    static_cast<DrawingAreaImpl*>(m_webPage->drawingArea())->layerHostDidFlushLayers();
    m_notifyAfterScheduledLayerFlush = false;
}

void LayerTreeHostQt::layerFlushTimerFired(Timer<LayerTreeHostQt>*)
{
    performScheduledLayerFlush();
}

void LayerTreeHostQt::createPageOverlayLayer()
{
    ASSERT(!m_pageOverlayLayer);

    m_pageOverlayLayer = GraphicsLayer::create(this);
#ifndef NDEBUG
    m_pageOverlayLayer->setName("LayerTreeHostQt page overlay content");
#endif

    m_pageOverlayLayer->setDrawsContent(true);
    m_pageOverlayLayer->setSize(m_webPage->size());

    m_rootLayer->addChild(m_pageOverlayLayer.get());
}

void LayerTreeHostQt::destroyPageOverlayLayer()
{
    ASSERT(m_pageOverlayLayer);
    m_pageOverlayLayer->removeFromParent();
    m_pageOverlayLayer = nullptr;
}

int64_t LayerTreeHostQt::adoptImageBackingStore(Image* image)
{
    if (!image)
        return InvalidWebLayerID;
    QPixmap* pixmap = image->nativeImageForCurrentFrame();

    if (!pixmap)
        return InvalidWebLayerID;

    int64_t key = pixmap->cacheKey();
    HashMap<int64_t, int>::iterator it = m_directlyCompositedImageRefCounts.find(key);

    if (it != m_directlyCompositedImageRefCounts.end()) {
        ++(it->second);
        return key;
    }

    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(image->size(), image->currentFrameHasAlpha() ? ShareableBitmap::SupportsAlpha : 0);
    {
        OwnPtr<WebCore::GraphicsContext> graphicsContext = bitmap->createGraphicsContext();
        graphicsContext->drawImage(image, ColorSpaceDeviceRGB, IntPoint::zero());
    }

    ShareableBitmap::Handle handle;
    bitmap->createHandle(handle);
    m_webPage->send(Messages::LayerTreeHostProxy::CreateDirectlyCompositedImage(key, handle));
    m_directlyCompositedImageRefCounts.add(key, 1);
    return key;
}

void LayerTreeHostQt::releaseImageBackingStore(int64_t key)
{
    if (!key)
        return;
    HashMap<int64_t, int>::iterator it = m_directlyCompositedImageRefCounts.find(key);
    if (it == m_directlyCompositedImageRefCounts.end())
        return;

    it->second--;

    if (it->second)
        return;

    m_directlyCompositedImageRefCounts.remove(it);
    m_webPage->send(Messages::LayerTreeHostProxy::DestroyDirectlyCompositedImage(key));
}


void LayerTreeHostQt::notifyAnimationStarted(const WebCore::GraphicsLayer*, double time)
{
}

void LayerTreeHostQt::notifySyncRequired(const WebCore::GraphicsLayer*)
{
}

void LayerTreeHostQt::paintContents(const WebCore::GraphicsLayer* graphicsLayer, WebCore::GraphicsContext& graphicsContext, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& clipRect)
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

bool LayerTreeHostQt::showDebugBorders() const
{
    return m_webPage->corePage()->settings()->showDebugBorders();
}

bool LayerTreeHostQt::showRepaintCounter() const
{
    return m_webPage->corePage()->settings()->showRepaintCounter();
}

bool LayerTreeHost::supportsAcceleratedCompositing()
{
    return true;
}

#if USE(TILED_BACKING_STORE)
void LayerTreeHostQt::createTile(WebLayerID layerID, int tileID, const UpdateInfo& updateInfo)
{
    m_webPage->send(Messages::LayerTreeHostProxy::CreateTileForLayer(layerID, tileID, updateInfo));
}

void LayerTreeHostQt::updateTile(WebLayerID layerID, int tileID, const UpdateInfo& updateInfo)
{
    m_webPage->send(Messages::LayerTreeHostProxy::UpdateTileForLayer(layerID, tileID, updateInfo));
}
void LayerTreeHostQt::removeTile(WebLayerID layerID, int tileID)
{
    m_webPage->send(Messages::LayerTreeHostProxy::RemoveTileForLayer(layerID, tileID));
}

void LayerTreeHostQt::setVisibleContentRectForLayer(int layerID, const WebCore::IntRect& rect)
{
    WebGraphicsLayer* layer = WebGraphicsLayer::layerByID(layerID);
    if (!layer)
        return;
    FloatRect visibleRect(rect);
    layer->setVisibleContentRect(rect);
}

void LayerTreeHostQt::setVisibleContentRectAndScale(const IntRect& rect, float scale)
{
    WebGraphicsLayer* layer = toWebGraphicsLayer(m_rootLayer.get());
    if (!layer)
        return;
    layer->setContentsScale(scale);
    toWebGraphicsLayer(m_nonCompositedContentLayer.get())->setVisibleContentRect(rect);
}

void LayerTreeHostQt::setVisibleContentRectTrajectoryVector(const FloatPoint& trajectoryVector)
{
    toWebGraphicsLayer(m_nonCompositedContentLayer.get())->setVisibleContentRectTrajectoryVector(trajectoryVector);
}

void LayerTreeHostQt::renderNextFrame()
{
    m_waitingForUIProcess = false;
    scheduleLayerFlush();
}

bool LayerTreeHostQt::layerTreeTileUpdatesAllowed() const
{
    return !m_isSuspended && !m_waitingForUIProcess;
}

#endif

} // namespace WebKit
#else
#include "LayerTreeHost.h"

using namespace WebCore;

namespace WebKit {

bool LayerTreeHost::supportsAcceleratedCompositing()
{
    return false;
}

} // namespace WebKit
#endif
