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

#include "LayerTreeHostQt.h"

#include "DrawingAreaImpl.h"
#include "GraphicsContext.h"
#include "LayerTreeHostProxyMessages.h"
#include "MessageID.h"
#include "SurfaceUpdateInfo.h"
#include "WebCoreArgumentCoders.h"
#include "WebGraphicsLayer.h"
#include "WebPage.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/Page.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderLayerBacking.h>
#include <WebCore/RenderLayerCompositor.h>
#include <WebCore/RenderView.h>
#include <WebCore/Settings.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<LayerTreeHostQt> LayerTreeHostQt::create(WebPage* webPage)
{
    return adoptRef(new LayerTreeHostQt(webPage));
}

LayerTreeHostQt::~LayerTreeHostQt()
{
    // Prevent setWebGraphicsLayerClient(0) -> detachLayer() from modifying the set while we iterate it.
    HashSet<WebCore::WebGraphicsLayer*> registeredLayers;
    registeredLayers.swap(m_registeredLayers);

    HashSet<WebCore::WebGraphicsLayer*>::iterator end = registeredLayers.end();
    for (HashSet<WebCore::WebGraphicsLayer*>::iterator it = registeredLayers.begin(); it != end; ++it)
        (*it)->setWebGraphicsLayerClient(0);
}

LayerTreeHostQt::LayerTreeHostQt(WebPage* webPage)
    : LayerTreeHost(webPage)
    , m_notifyAfterScheduledLayerFlush(false)
    , m_isValid(true)
    , m_waitingForUIProcess(false)
    , m_isSuspended(false)
    , m_contentsScale(1)
    , m_shouldSendScrollPositionUpdate(true)
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
    toWebGraphicsLayer(m_rootLayer.get())->setWebGraphicsLayerClient(this);
#ifndef NDEBUG
    m_nonCompositedContentLayer->setName("LayerTreeHostQt non-composited content");
#endif
    m_nonCompositedContentLayer->setDrawsContent(true);
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
    m_nonCompositedContentLayer->setContentsOpaque(m_webPage->drawsBackground() && !m_webPage->drawsTransparentBackground());

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
    if (m_rootLayer->size() == newSize)
        return;

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

void LayerTreeHostQt::setPageOverlayOpacity(float value)
{
    ASSERT(m_pageOverlayLayer);
    m_pageOverlayLayer->setOpacity(value);
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

void LayerTreeHostQt::syncLayerState(WebLayerID id, const WebLayerInfo& info)
{
    if (m_shouldSendScrollPositionUpdate) {
        m_webPage->send(Messages::LayerTreeHostProxy::DidChangeScrollPosition(m_visibleContentsRect.location()));
        m_shouldSendScrollPositionUpdate = false;
    }
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeHostProxy::SetCompositingLayerState(id, info));
}

void LayerTreeHostQt::syncLayerChildren(WebLayerID id, const Vector<WebLayerID>& children)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeHostProxy::SetCompositingLayerChildren(id, children));
}

#if ENABLE(CSS_FILTERS)
void LayerTreeHostQt::syncLayerFilters(WebLayerID id, const FilterOperations& filters)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeHostProxy::SetCompositingLayerFilters(id, filters));
}
#endif

void LayerTreeHostQt::attachLayer(WebGraphicsLayer* layer)
{
    ASSERT(!m_registeredLayers.contains(layer));
    m_registeredLayers.add(layer);

    layer->setContentsScale(m_contentsScale);
    layer->adjustVisibleRect();
}

void LayerTreeHostQt::detachLayer(WebGraphicsLayer* layer)
{
    m_registeredLayers.remove(layer);
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeHostProxy::DeleteCompositingLayer(layer->id()));
}

static void updateOffsetFromViewportForSelf(RenderLayer* renderLayer)
{
    // These conditions must match the conditions in RenderLayerCompositor::requiresCompositingForPosition.
    RenderLayerBacking* backing = renderLayer->backing();
    if (!backing)
        return;

    RenderStyle* style = renderLayer->renderer()->style();
    if (!style)
        return;

    if (!renderLayer->renderer()->isPositioned() || renderLayer->renderer()->style()->position() != FixedPosition)
        return;

    if (!renderLayer->renderer()->container()->isRenderView())
        return;

    if (!renderLayer->isStackingContext())
        return;

    WebGraphicsLayer* graphicsLayer = toWebGraphicsLayer(backing->graphicsLayer());
    graphicsLayer->setFixedToViewport(true);
}

static void updateOffsetFromViewportForLayer(RenderLayer* renderLayer)
{
    updateOffsetFromViewportForSelf(renderLayer);

    if (renderLayer->firstChild())
        updateOffsetFromViewportForLayer(renderLayer->firstChild());
    if (renderLayer->nextSibling())
        updateOffsetFromViewportForLayer(renderLayer->nextSibling());
}

void LayerTreeHostQt::syncFixedLayers()
{
    if (!m_webPage->corePage()->settings() || !m_webPage->corePage()->settings()->acceleratedCompositingForFixedPositionEnabled())
        return;

    if (!m_webPage->mainFrame()->view()->hasFixedObjects())
        return;

    RenderLayer* rootRenderLayer = m_webPage->mainFrame()->contentRenderer()->compositor()->rootRenderLayer();
    ASSERT(rootRenderLayer);
    if (rootRenderLayer->firstChild())
        updateOffsetFromViewportForLayer(rootRenderLayer->firstChild());
}

void LayerTreeHostQt::performScheduledLayerFlush()
{
    if (m_isSuspended || m_waitingForUIProcess)
        return;

    m_webPage->layoutIfNeeded();

    if (!m_isValid)
        return;

    m_shouldSyncFrame = false;
    flushPendingLayerChanges();
    if (!m_shouldSyncFrame)
        return;

    if (m_shouldSyncRootLayer) {
        m_webPage->send(Messages::LayerTreeHostProxy::SetRootCompositingLayer(toWebGraphicsLayer(m_rootLayer.get())->id()));
        m_shouldSyncRootLayer = false;
    }

    m_webPage->send(Messages::LayerTreeHostProxy::DidRenderFrame());
    m_waitingForUIProcess = true;

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

    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(image->size(), (image->currentFrameHasAlpha() ? ShareableBitmap::SupportsAlpha : 0));
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
        // Overlays contain transparent contents and won't clear the context as part of their rendering, so we do it here.
        graphicsContext.clearRect(clipRect);
        m_webPage->drawPageOverlay(graphicsContext, clipRect);
        return;
    }
}

bool LayerTreeHostQt::showDebugBorders(const WebCore::GraphicsLayer*) const
{
    return m_webPage->corePage()->settings()->showDebugBorders();
}

bool LayerTreeHostQt::showRepaintCounter(const WebCore::GraphicsLayer*) const
{
    return m_webPage->corePage()->settings()->showRepaintCounter();
}

bool LayerTreeHost::supportsAcceleratedCompositing()
{
    return true;
}

void LayerTreeHostQt::createTile(WebLayerID layerID, int tileID, const SurfaceUpdateInfo& updateInfo, const WebCore::IntRect& targetRect)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeHostProxy::CreateTileForLayer(layerID, tileID, targetRect, updateInfo));
}

void LayerTreeHostQt::updateTile(WebLayerID layerID, int tileID, const SurfaceUpdateInfo& updateInfo, const WebCore::IntRect& targetRect)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeHostProxy::UpdateTileForLayer(layerID, tileID, targetRect, updateInfo));
}

void LayerTreeHostQt::removeTile(WebLayerID layerID, int tileID)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeHostProxy::RemoveTileForLayer(layerID, tileID));
}

WebCore::IntRect LayerTreeHostQt::visibleContentsRect() const
{
    return m_visibleContentsRect;
}

void LayerTreeHostQt::setVisibleContentsRect(const IntRect& rect, float scale, const FloatPoint& trajectoryVector)
{
    bool contentsRectDidChange = rect != m_visibleContentsRect;
    bool contentsScaleDidChange = scale != m_contentsScale;

    if (trajectoryVector != FloatPoint::zero())
        toWebGraphicsLayer(m_nonCompositedContentLayer.get())->setVisibleContentRectTrajectoryVector(trajectoryVector);

    if (contentsRectDidChange || contentsScaleDidChange) {
        m_visibleContentsRect = rect;
        m_contentsScale = scale;

        HashSet<WebCore::WebGraphicsLayer*>::iterator end = m_registeredLayers.end();
        for (HashSet<WebCore::WebGraphicsLayer*>::iterator it = m_registeredLayers.begin(); it != end; ++it) {
            if (contentsScaleDidChange)
                (*it)->setContentsScale(scale);
            if (contentsRectDidChange)
                (*it)->adjustVisibleRect();
        }
    }

    scheduleLayerFlush();
    if (m_webPage->useFixedLayout())
        m_webPage->setFixedVisibleContentRect(rect);
    if (contentsRectDidChange)
        m_shouldSendScrollPositionUpdate = true;
}

void LayerTreeHostQt::renderNextFrame()
{
    m_waitingForUIProcess = false;
    scheduleLayerFlush();
    for (int i = 0; i < m_updateAtlases.size(); ++i)
        m_updateAtlases[i].didSwapBuffers();
}

bool LayerTreeHostQt::layerTreeTileUpdatesAllowed() const
{
    return !m_isSuspended && !m_waitingForUIProcess;
}

void LayerTreeHostQt::purgeBackingStores()
{
    HashSet<WebCore::WebGraphicsLayer*>::iterator end = m_registeredLayers.end();
    for (HashSet<WebCore::WebGraphicsLayer*>::iterator it = m_registeredLayers.begin(); it != end; ++it)
        (*it)->purgeBackingStores();

    ASSERT(!m_directlyCompositedImageRefCounts.size());
    m_updateAtlases.clear();
}

UpdateAtlas& LayerTreeHostQt::getAtlas(ShareableBitmap::Flags flags)
{
    for (int i = 0; i < m_updateAtlases.size(); ++i) {
        if (m_updateAtlases[i].flags() == flags)
            return m_updateAtlases[i];
    }
    static const int ScratchBufferDimension = 2000;
    m_updateAtlases.append(UpdateAtlas(ScratchBufferDimension, flags));
    return m_updateAtlases.last();
}

PassOwnPtr<WebCore::GraphicsContext> LayerTreeHostQt::beginContentUpdate(const WebCore::IntSize& size, ShareableBitmap::Flags flags, ShareableSurface::Handle& handle, WebCore::IntPoint& offset)
{
    UpdateAtlas& atlas = getAtlas(flags);
    if (!atlas.surface()->createHandle(handle))
        return PassOwnPtr<WebCore::GraphicsContext>();

    // This will return null if there is no available buffer.
    OwnPtr<WebCore::GraphicsContext> graphicsContext = atlas.beginPaintingOnAvailableBuffer(size, offset);
    if (!graphicsContext)
        return PassOwnPtr<WebCore::GraphicsContext>();

    return graphicsContext.release();
}

} // namespace WebKit
