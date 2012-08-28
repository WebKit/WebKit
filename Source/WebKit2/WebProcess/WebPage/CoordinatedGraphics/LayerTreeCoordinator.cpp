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

#if USE(COORDINATED_GRAPHICS)
#include "LayerTreeCoordinator.h"

#include "CoordinatedGraphicsLayer.h"
#include "DrawingAreaImpl.h"
#include "GraphicsContext.h"
#include "LayerTreeCoordinatorProxyMessages.h"
#include "MessageID.h"
#include "SurfaceUpdateInfo.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
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

PassRefPtr<LayerTreeCoordinator> LayerTreeCoordinator::create(WebPage* webPage)
{
    return adoptRef(new LayerTreeCoordinator(webPage));
}

LayerTreeCoordinator::~LayerTreeCoordinator()
{
    // Prevent setCoordinatedGraphicsLayerClient(0) -> detachLayer() from modifying the set while we iterate it.
    HashSet<WebCore::CoordinatedGraphicsLayer*> registeredLayers;
    registeredLayers.swap(m_registeredLayers);

    HashSet<WebCore::CoordinatedGraphicsLayer*>::iterator end = registeredLayers.end();
    for (HashSet<WebCore::CoordinatedGraphicsLayer*>::iterator it = registeredLayers.begin(); it != end; ++it)
        (*it)->setCoordinatedGraphicsLayerClient(0);
}

LayerTreeCoordinator::LayerTreeCoordinator(WebPage* webPage)
    : LayerTreeHost(webPage)
    , m_notifyAfterScheduledLayerFlush(false)
    , m_isValid(true)
    , m_waitingForUIProcess(true)
    , m_isSuspended(false)
    , m_contentsScale(1)
    , m_shouldSendScrollPositionUpdate(true)
    , m_shouldSyncFrame(false)
    , m_shouldSyncRootLayer(true)
    , m_layerFlushTimer(this, &LayerTreeCoordinator::layerFlushTimerFired)
    , m_layerFlushSchedulingEnabled(true)
    , m_forceRepaintAsyncCallbackID(0)
{
    // Create a root layer.
    m_rootLayer = GraphicsLayer::create(this);
    CoordinatedGraphicsLayer* webRootLayer = toCoordinatedGraphicsLayer(m_rootLayer.get());
    webRootLayer->setRootLayer(true);
#ifndef NDEBUG
    m_rootLayer->setName("LayerTreeCoordinator root layer");
#endif
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setSize(m_webPage->size());
    m_layerTreeContext.webLayerID = toCoordinatedGraphicsLayer(webRootLayer)->id();

    m_nonCompositedContentLayer = GraphicsLayer::create(this);
    toCoordinatedGraphicsLayer(m_rootLayer.get())->setCoordinatedGraphicsLayerClient(this);
#ifndef NDEBUG
    m_nonCompositedContentLayer->setName("LayerTreeCoordinator non-composited content");
#endif
    m_nonCompositedContentLayer->setDrawsContent(true);
    m_nonCompositedContentLayer->setSize(m_webPage->size());

    m_rootLayer->addChild(m_nonCompositedContentLayer.get());

    if (m_webPage->hasPageOverlay())
        createPageOverlayLayer();

    scheduleLayerFlush();
}

void LayerTreeCoordinator::setLayerFlushSchedulingEnabled(bool layerFlushingEnabled)
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

void LayerTreeCoordinator::scheduleLayerFlush()
{
    if (!m_layerFlushSchedulingEnabled)
        return;

    if (!m_layerFlushTimer.isActive())
        m_layerFlushTimer.startOneShot(0);
}

void LayerTreeCoordinator::cancelPendingLayerFlush()
{
    m_layerFlushTimer.stop();
}

void LayerTreeCoordinator::setShouldNotifyAfterNextScheduledLayerFlush(bool notifyAfterScheduledLayerFlush)
{
    m_notifyAfterScheduledLayerFlush = notifyAfterScheduledLayerFlush;
}

void LayerTreeCoordinator::setRootCompositingLayer(WebCore::GraphicsLayer* graphicsLayer)
{
    m_nonCompositedContentLayer->removeAllChildren();
    m_nonCompositedContentLayer->setContentsOpaque(m_webPage->drawsBackground() && !m_webPage->drawsTransparentBackground());

    // Add the accelerated layer tree hierarchy.
    if (graphicsLayer)
        m_nonCompositedContentLayer->addChild(graphicsLayer);
}

void LayerTreeCoordinator::invalidate()
{
    cancelPendingLayerFlush();

    ASSERT(m_isValid);
    m_rootLayer = nullptr;
    m_isValid = false;
}

void LayerTreeCoordinator::setNonCompositedContentsNeedDisplay(const WebCore::IntRect& rect)
{
    m_nonCompositedContentLayer->setNeedsDisplayInRect(rect);
    if (m_pageOverlayLayer)
        m_pageOverlayLayer->setNeedsDisplayInRect(rect);

    scheduleLayerFlush();
}

void LayerTreeCoordinator::scrollNonCompositedContents(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset)
{
    setNonCompositedContentsNeedDisplay(scrollRect);
}

void LayerTreeCoordinator::forceRepaint()
{
    // This is necessary for running layout tests. Since in this case we are not waiting for a UIProcess to reply nicely.
    // Instead we are just triggering forceRepaint. But we still want to have the scripted animation callbacks being executed.
    syncDisplayState();

    // We need to schedule another flush, otherwise the forced paint might cancel a later expected flush.
    // This is aligned with LayerTreeHostCA.
    scheduleLayerFlush();
    flushPendingLayerChanges();
}

bool LayerTreeCoordinator::forceRepaintAsync(uint64_t callbackID)
{
    // We expect the UI process to not require a new repaint until the previous one has finished.
    ASSERT(!m_forceRepaintAsyncCallbackID);
    m_forceRepaintAsyncCallbackID = callbackID;
    scheduleLayerFlush();
    return true;
}

void LayerTreeCoordinator::sizeDidChange(const WebCore::IntSize& newSize)
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

void LayerTreeCoordinator::didInstallPageOverlay()
{
    createPageOverlayLayer();
    scheduleLayerFlush();
}

void LayerTreeCoordinator::didUninstallPageOverlay()
{
    destroyPageOverlayLayer();
    scheduleLayerFlush();
}

void LayerTreeCoordinator::setPageOverlayNeedsDisplay(const WebCore::IntRect& rect)
{
    ASSERT(m_pageOverlayLayer);
    m_pageOverlayLayer->setNeedsDisplayInRect(rect);
    scheduleLayerFlush();
}

void LayerTreeCoordinator::setPageOverlayOpacity(float value)
{
    ASSERT(m_pageOverlayLayer);
    m_pageOverlayLayer->setOpacity(value);
    scheduleLayerFlush();
}

bool LayerTreeCoordinator::flushPendingLayerChanges()
{
    if (m_waitingForUIProcess)
        return false;

    m_shouldSyncFrame = false;
    bool didSync = m_webPage->corePage()->mainFrame()->view()->syncCompositingStateIncludingSubframes();
    m_nonCompositedContentLayer->syncCompositingStateForThisLayerOnly();
    if (m_pageOverlayLayer)
        m_pageOverlayLayer->syncCompositingStateForThisLayerOnly();

    m_rootLayer->syncCompositingStateForThisLayerOnly();

    if (m_shouldSyncRootLayer) {
        m_webPage->send(Messages::LayerTreeCoordinatorProxy::SetRootCompositingLayer(toCoordinatedGraphicsLayer(m_rootLayer.get())->id()));
        m_shouldSyncRootLayer = false;
    }

    if (m_shouldSyncFrame) {
        didSync = true;
        m_webPage->send(Messages::LayerTreeCoordinatorProxy::DidRenderFrame());
        m_waitingForUIProcess = true;
        m_shouldSyncFrame = false;
    }

    if (m_forceRepaintAsyncCallbackID) {
        m_webPage->send(Messages::WebPageProxy::VoidCallback(m_forceRepaintAsyncCallbackID));
        m_forceRepaintAsyncCallbackID = 0;
    }

    return didSync;
}

void LayerTreeCoordinator::syncLayerState(WebLayerID id, const WebLayerInfo& info)
{
    if (m_shouldSendScrollPositionUpdate) {
        m_webPage->send(Messages::LayerTreeCoordinatorProxy::DidChangeScrollPosition(m_visibleContentsRect.location()));
        m_shouldSendScrollPositionUpdate = false;
    }

    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeCoordinatorProxy::SetCompositingLayerState(id, info));
}

void LayerTreeCoordinator::syncLayerChildren(WebLayerID id, const Vector<WebLayerID>& children)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeCoordinatorProxy::SetCompositingLayerChildren(id, children));
}

void LayerTreeCoordinator::syncCanvas(WebLayerID id, const IntSize& canvasSize, uint64_t graphicsSurfaceToken, uint32_t frontBuffer)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeCoordinatorProxy::SyncCanvas(id, canvasSize, graphicsSurfaceToken, frontBuffer));
}

#if ENABLE(CSS_FILTERS)
void LayerTreeCoordinator::syncLayerFilters(WebLayerID id, const FilterOperations& filters)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeCoordinatorProxy::SetCompositingLayerFilters(id, filters));
}
#endif

void LayerTreeCoordinator::attachLayer(CoordinatedGraphicsLayer* layer)
{
    ASSERT(!m_registeredLayers.contains(layer));
    m_registeredLayers.add(layer);

    layer->setContentsScale(m_contentsScale);
    layer->adjustVisibleRect();
}

void LayerTreeCoordinator::detachLayer(CoordinatedGraphicsLayer* layer)
{
    m_registeredLayers.remove(layer);
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeCoordinatorProxy::DeleteCompositingLayer(layer->id()));
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

    if (!renderLayer->renderer()->isOutOfFlowPositioned() || renderLayer->renderer()->style()->position() != FixedPosition)
        return;

    if (!renderLayer->renderer()->container()->isRenderView())
        return;

    if (!renderLayer->isStackingContext())
        return;

    CoordinatedGraphicsLayer* graphicsLayer = toCoordinatedGraphicsLayer(backing->graphicsLayer());
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

void LayerTreeCoordinator::syncFixedLayers()
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

void LayerTreeCoordinator::performScheduledLayerFlush()
{
    if (m_isSuspended || m_waitingForUIProcess)
        return;

    syncDisplayState();

    if (!m_isValid)
        return;

    if (flushPendingLayerChanges())
        didPerformScheduledLayerFlush();
}

void LayerTreeCoordinator::syncDisplayState()
{
#if ENABLE(REQUEST_ANIMATION_FRAME) && !USE(REQUEST_ANIMATION_FRAME_TIMER) && !USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    // Make sure that any previously registered animation callbacks are being executed before we flush the layers.
    m_webPage->corePage()->mainFrame()->view()->serviceScriptedAnimations(convertSecondsToDOMTimeStamp(currentTime()));
#endif

    m_webPage->layoutIfNeeded();
}

void LayerTreeCoordinator::didPerformScheduledLayerFlush()
{
    if (m_notifyAfterScheduledLayerFlush) {
        static_cast<DrawingAreaImpl*>(m_webPage->drawingArea())->layerHostDidFlushLayers();
        m_notifyAfterScheduledLayerFlush = false;
    }
}

void LayerTreeCoordinator::layerFlushTimerFired(Timer<LayerTreeCoordinator>*)
{
    performScheduledLayerFlush();
}

void LayerTreeCoordinator::createPageOverlayLayer()
{
    ASSERT(!m_pageOverlayLayer);

    m_pageOverlayLayer = GraphicsLayer::create(this);
#ifndef NDEBUG
    m_pageOverlayLayer->setName("LayerTreeCoordinator page overlay content");
#endif

    m_pageOverlayLayer->setDrawsContent(true);
    m_pageOverlayLayer->setSize(m_webPage->size());

    m_rootLayer->addChild(m_pageOverlayLayer.get());
}

void LayerTreeCoordinator::destroyPageOverlayLayer()
{
    ASSERT(m_pageOverlayLayer);
    m_pageOverlayLayer->removeFromParent();
    m_pageOverlayLayer = nullptr;
}

int64_t LayerTreeCoordinator::adoptImageBackingStore(Image* image)
{
    if (!image)
        return InvalidWebLayerID;

    int64_t key = 0;

#if PLATFORM(QT)
    QImage* nativeImage = image->nativeImageForCurrentFrame();

    if (!nativeImage)
        return InvalidWebLayerID;

    key = nativeImage->cacheKey();
#endif

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
    m_webPage->send(Messages::LayerTreeCoordinatorProxy::CreateDirectlyCompositedImage(key, handle));
    m_directlyCompositedImageRefCounts.add(key, 1);
    return key;
}

void LayerTreeCoordinator::releaseImageBackingStore(int64_t key)
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
    m_webPage->send(Messages::LayerTreeCoordinatorProxy::DestroyDirectlyCompositedImage(key));
}


void LayerTreeCoordinator::notifyAnimationStarted(const WebCore::GraphicsLayer*, double time)
{
}

void LayerTreeCoordinator::notifySyncRequired(const WebCore::GraphicsLayer*)
{
}

void LayerTreeCoordinator::paintContents(const WebCore::GraphicsLayer* graphicsLayer, WebCore::GraphicsContext& graphicsContext, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& clipRect)
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

bool LayerTreeCoordinator::showDebugBorders(const WebCore::GraphicsLayer*) const
{
    return m_webPage->corePage()->settings()->showDebugBorders();
}

bool LayerTreeCoordinator::showRepaintCounter(const WebCore::GraphicsLayer*) const
{
    return m_webPage->corePage()->settings()->showRepaintCounter();
}

bool LayerTreeHost::supportsAcceleratedCompositing()
{
    return true;
}

void LayerTreeCoordinator::createTile(WebLayerID layerID, int tileID, const SurfaceUpdateInfo& updateInfo, const WebCore::IntRect& targetRect)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeCoordinatorProxy::CreateTileForLayer(layerID, tileID, targetRect, updateInfo));
}

void LayerTreeCoordinator::updateTile(WebLayerID layerID, int tileID, const SurfaceUpdateInfo& updateInfo, const WebCore::IntRect& targetRect)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeCoordinatorProxy::UpdateTileForLayer(layerID, tileID, targetRect, updateInfo));
}

void LayerTreeCoordinator::removeTile(WebLayerID layerID, int tileID)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeCoordinatorProxy::RemoveTileForLayer(layerID, tileID));
}

WebCore::IntRect LayerTreeCoordinator::visibleContentsRect() const
{
    return m_visibleContentsRect;
}


void LayerTreeCoordinator::setLayerAnimatedOpacity(WebLayerID id, float opacity)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeCoordinatorProxy::SetLayerAnimatedOpacity(id, opacity));
}

void LayerTreeCoordinator::setLayerAnimatedTransform(WebLayerID id, const TransformationMatrix& transform)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::LayerTreeCoordinatorProxy::SetLayerAnimatedTransform(id, transform));
}

void LayerTreeCoordinator::setVisibleContentsRect(const IntRect& rect, float scale, const FloatPoint& trajectoryVector)
{
    bool contentsRectDidChange = rect != m_visibleContentsRect;
    bool contentsScaleDidChange = scale != m_contentsScale;

    // A zero trajectoryVector indicates that tiles all around the viewport are requested.
    toCoordinatedGraphicsLayer(m_nonCompositedContentLayer.get())->setVisibleContentRectTrajectoryVector(trajectoryVector);

    if (contentsRectDidChange || contentsScaleDidChange) {
        m_visibleContentsRect = rect;
        m_contentsScale = scale;

        HashSet<WebCore::CoordinatedGraphicsLayer*>::iterator end = m_registeredLayers.end();
        for (HashSet<WebCore::CoordinatedGraphicsLayer*>::iterator it = m_registeredLayers.begin(); it != end; ++it) {
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

void LayerTreeCoordinator::scheduleAnimation()
{
    scheduleLayerFlush();
}

void LayerTreeCoordinator::renderNextFrame()
{
    m_waitingForUIProcess = false;
    scheduleLayerFlush();
    for (int i = 0; i < m_updateAtlases.size(); ++i)
        m_updateAtlases[i]->didSwapBuffers();
}

bool LayerTreeCoordinator::layerTreeTileUpdatesAllowed() const
{
    return !m_isSuspended && !m_waitingForUIProcess;
}

void LayerTreeCoordinator::purgeBackingStores()
{
    HashSet<WebCore::CoordinatedGraphicsLayer*>::iterator end = m_registeredLayers.end();
    for (HashSet<WebCore::CoordinatedGraphicsLayer*>::iterator it = m_registeredLayers.begin(); it != end; ++it)
        (*it)->purgeBackingStores();

    ASSERT(!m_directlyCompositedImageRefCounts.size());
    m_updateAtlases.clear();
}

PassOwnPtr<WebCore::GraphicsContext> LayerTreeCoordinator::beginContentUpdate(const WebCore::IntSize& size, ShareableBitmap::Flags flags, ShareableSurface::Handle& handle, WebCore::IntPoint& offset)
{
    OwnPtr<WebCore::GraphicsContext> graphicsContext;
    for (int i = 0; i < m_updateAtlases.size(); ++i) {
        UpdateAtlas* atlas = m_updateAtlases[i].get();
        if (atlas->flags() == flags) {
            // This will return null if there is no available buffer space.
            graphicsContext = atlas->beginPaintingOnAvailableBuffer(handle, size, offset);
            if (graphicsContext)
                return graphicsContext.release();
        }
    }

    static const int ScratchBufferDimension = 1024; // Should be a power of two.
    m_updateAtlases.append(adoptPtr(new UpdateAtlas(ScratchBufferDimension, flags)));
    return m_updateAtlases.last()->beginPaintingOnAvailableBuffer(handle, size, offset);
}

} // namespace WebKit
#endif // USE(COORDINATED_GRAPHICS)
