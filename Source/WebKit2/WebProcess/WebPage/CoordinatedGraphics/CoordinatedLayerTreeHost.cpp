/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Company 100, Inc.
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
#include "CoordinatedLayerTreeHost.h"

#include "CoordinatedGraphicsArgumentCoders.h"
#include "CoordinatedLayerTreeHostProxyMessages.h"
#include "DrawingAreaImpl.h"
#include "GraphicsContext.h"
#include "WebCoordinatedSurface.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsSurface.h>
#include <WebCore/Page.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderLayerBacking.h>
#include <WebCore/RenderLayerCompositor.h>
#include <WebCore/RenderView.h>
#include <WebCore/Settings.h>
#include <WebCore/SurfaceUpdateInfo.h>
#include <WebCore/TextureMapperPlatformLayer.h>
#include <wtf/TemporaryChange.h>

#if ENABLE(CSS_SHADERS)
#include "CustomFilterValidatedProgram.h"
#include "ValidatedCustomFilterOperation.h"
#endif

using namespace WebCore;

namespace WebKit {

PassRefPtr<CoordinatedLayerTreeHost> CoordinatedLayerTreeHost::create(WebPage* webPage)
{
    return adoptRef(new CoordinatedLayerTreeHost(webPage));
}

CoordinatedLayerTreeHost::~CoordinatedLayerTreeHost()
{
#if ENABLE(CSS_SHADERS)
    disconnectCustomFilterPrograms();
#endif
    purgeBackingStores();

    HashSet<WebCore::CoordinatedGraphicsLayer*>::iterator end = m_registeredLayers.end();
    for (HashSet<WebCore::CoordinatedGraphicsLayer*>::iterator it = m_registeredLayers.begin(); it != end; ++it)
        (*it)->setCoordinator(0);
}

CoordinatedLayerTreeHost::CoordinatedLayerTreeHost(WebPage* webPage)
    : LayerTreeHost(webPage)
    , m_notifyAfterScheduledLayerFlush(false)
    , m_isValid(true)
    , m_isPurging(false)
    , m_isFlushingLayerChanges(false)
    , m_waitingForUIProcess(true)
    , m_isSuspended(false)
    , m_shouldSyncFrame(false)
    , m_didInitializeRootCompositingLayer(false)
    , m_layerFlushTimer(this, &CoordinatedLayerTreeHost::layerFlushTimerFired)
    , m_releaseInactiveAtlasesTimer(this, &CoordinatedLayerTreeHost::releaseInactiveAtlasesTimerFired)
    , m_layerFlushSchedulingEnabled(true)
    , m_forceRepaintAsyncCallbackID(0)
    , m_animationsLocked(false)
{
    m_webPage->corePage()->settings()->setScrollingCoordinatorEnabled(true);
    m_webPage->corePage()->settings()->setApplyDeviceScaleFactorInCompositor(true);

    // Create a root layer.
    m_rootLayer = GraphicsLayer::create(this, this);
    CoordinatedGraphicsLayer* coordinatedRootLayer = toCoordinatedGraphicsLayer(m_rootLayer.get());
    coordinatedRootLayer->setRootLayer(true);
#ifndef NDEBUG
    m_rootLayer->setName("CoordinatedLayerTreeHost root layer");
#endif
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setSize(m_webPage->size());
    m_layerTreeContext.coordinatedLayerID = toCoordinatedGraphicsLayer(coordinatedRootLayer)->id();

    m_nonCompositedContentLayer = GraphicsLayer::create(this, this);
#ifndef NDEBUG
    m_nonCompositedContentLayer->setName("CoordinatedLayerTreeHost non-composited content");
#endif
    m_nonCompositedContentLayer->setDrawsContent(true);
    m_nonCompositedContentLayer->setSize(m_webPage->size());

    m_nonCompositedContentLayer->setShowDebugBorder(m_webPage->corePage()->settings()->showDebugBorders());
    m_nonCompositedContentLayer->setShowRepaintCounter(m_webPage->corePage()->settings()->showRepaintCounter());

    m_rootLayer->addChild(m_nonCompositedContentLayer.get());

    CoordinatedSurface::setFactory(createCoordinatedSurface);

    if (m_webPage->hasPageOverlay())
        createPageOverlayLayer();

    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::setLayerFlushSchedulingEnabled(bool layerFlushingEnabled)
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

void CoordinatedLayerTreeHost::scheduleLayerFlush()
{
    if (!m_layerFlushSchedulingEnabled)
        return;

    if (!m_layerFlushTimer.isActive())
        m_layerFlushTimer.startOneShot(0);
}

void CoordinatedLayerTreeHost::cancelPendingLayerFlush()
{
    m_layerFlushTimer.stop();
}

void CoordinatedLayerTreeHost::setShouldNotifyAfterNextScheduledLayerFlush(bool notifyAfterScheduledLayerFlush)
{
    m_notifyAfterScheduledLayerFlush = notifyAfterScheduledLayerFlush;
}

void CoordinatedLayerTreeHost::setRootCompositingLayer(WebCore::GraphicsLayer* graphicsLayer)
{
    m_nonCompositedContentLayer->removeAllChildren();
    m_nonCompositedContentLayer->setContentsOpaque(m_webPage->drawsBackground() && !m_webPage->drawsTransparentBackground());

    // Add the accelerated layer tree hierarchy.
    if (graphicsLayer)
        m_nonCompositedContentLayer->addChild(graphicsLayer);
}

void CoordinatedLayerTreeHost::invalidate()
{
    cancelPendingLayerFlush();

    ASSERT(m_isValid);
    m_rootLayer = nullptr;
    m_isValid = false;
}

void CoordinatedLayerTreeHost::setNonCompositedContentsNeedDisplay()
{
    m_nonCompositedContentLayer->setNeedsDisplay();
    if (m_pageOverlayLayer)
        m_pageOverlayLayer->setNeedsDisplay();

    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::setNonCompositedContentsNeedDisplayInRect(const WebCore::IntRect& rect)
{
    m_nonCompositedContentLayer->setNeedsDisplayInRect(rect);
    if (m_pageOverlayLayer)
        m_pageOverlayLayer->setNeedsDisplayInRect(rect);

    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::scrollNonCompositedContents(const WebCore::IntRect&)
{
    // Do nothing because we scroll using TiledBackingStore.
}

void CoordinatedLayerTreeHost::forceRepaint()
{
    // This is necessary for running layout tests. Since in this case we are not waiting for a UIProcess to reply nicely.
    // Instead we are just triggering forceRepaint. But we still want to have the scripted animation callbacks being executed.
    syncDisplayState();

    // We need to schedule another flush, otherwise the forced paint might cancel a later expected flush.
    // This is aligned with LayerTreeHostCA.
    scheduleLayerFlush();
    flushPendingLayerChanges();
}

bool CoordinatedLayerTreeHost::forceRepaintAsync(uint64_t callbackID)
{
    // We expect the UI process to not require a new repaint until the previous one has finished.
    ASSERT(!m_forceRepaintAsyncCallbackID);
    m_forceRepaintAsyncCallbackID = callbackID;
    scheduleLayerFlush();
    return true;
}

void CoordinatedLayerTreeHost::sizeDidChange(const WebCore::IntSize& newSize)
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

void CoordinatedLayerTreeHost::didInstallPageOverlay()
{
    createPageOverlayLayer();
    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::didUninstallPageOverlay()
{
    destroyPageOverlayLayer();
    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::setPageOverlayNeedsDisplay(const WebCore::IntRect& rect)
{
    ASSERT(m_pageOverlayLayer);
    m_pageOverlayLayer->setNeedsDisplayInRect(rect);
    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::setPageOverlayOpacity(float value)
{
    ASSERT(m_pageOverlayLayer);
    m_pageOverlayLayer->setOpacity(value);
    scheduleLayerFlush();
}

bool CoordinatedLayerTreeHost::flushPendingLayerChanges()
{
    if (m_waitingForUIProcess)
        return false;

    TemporaryChange<bool> protector(m_isFlushingLayerChanges, true);

    createCompositingLayers();

    initializeRootCompositingLayerIfNeeded();

    m_rootLayer->flushCompositingStateForThisLayerOnly();
    m_nonCompositedContentLayer->flushCompositingStateForThisLayerOnly();
    if (m_pageOverlayLayer)
        m_pageOverlayLayer->flushCompositingStateForThisLayerOnly();

    bool didSync = m_webPage->corePage()->mainFrame()->view()->flushCompositingStateIncludingSubframes();

    flushPendingImageBackingChanges();

    deleteCompositingLayers();

    if (m_shouldSyncFrame) {
        didSync = true;

        IntSize contentsSize = roundedIntSize(m_nonCompositedContentLayer->size());
        IntRect coveredRect = toCoordinatedGraphicsLayer(m_nonCompositedContentLayer.get())->coverRect();
        m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::DidRenderFrame(m_visibleContentsRect.location(), contentsSize, coveredRect));
        m_waitingForUIProcess = true;
        m_shouldSyncFrame = false;
    } else
        unlockAnimations();

    if (m_forceRepaintAsyncCallbackID) {
        m_webPage->send(Messages::WebPageProxy::VoidCallback(m_forceRepaintAsyncCallbackID));
        m_forceRepaintAsyncCallbackID = 0;
    }

    return didSync;
}

void CoordinatedLayerTreeHost::createCompositingLayers()
{
    if (m_layersToCreate.isEmpty())
        return;

    // If a layer gets created and deleted in the same cycle, we can simply remove it from m_layersToCreate and m_layersToDelete.
    for (int i = m_layersToCreate.size() - 1; i >= 0; --i) {
        size_t index = m_layersToDelete.find(m_layersToCreate[i]);
        if (index != notFound) {
            m_layersToCreate.remove(i);
            m_layersToDelete.remove(index);
        }
    }

    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::CreateCompositingLayers(m_layersToCreate));
    m_layersToCreate.clear();
    m_shouldSyncFrame = true;
}

void CoordinatedLayerTreeHost::deleteCompositingLayers()
{
    if (m_layersToDelete.isEmpty())
        return;

    if (m_isPurging) {
        m_layersToDelete.clear();
        return;
    }

    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::DeleteCompositingLayers(m_layersToDelete));
    m_layersToDelete.clear();
    m_shouldSyncFrame = true;
}

void CoordinatedLayerTreeHost::initializeRootCompositingLayerIfNeeded()
{
    if (m_didInitializeRootCompositingLayer)
        return;

    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::SetRootCompositingLayer(toCoordinatedGraphicsLayer(m_rootLayer.get())->id()));
    m_didInitializeRootCompositingLayer = true;
    m_shouldSyncFrame = true;
}

void CoordinatedLayerTreeHost::syncLayerState(CoordinatedLayerID id, const CoordinatedLayerInfo& info)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::SetCompositingLayerState(id, info));
}

void CoordinatedLayerTreeHost::syncLayerChildren(CoordinatedLayerID id, const Vector<CoordinatedLayerID>& children)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::SetCompositingLayerChildren(id, children));
}

#if USE(GRAPHICS_SURFACE)
void CoordinatedLayerTreeHost::createCanvas(CoordinatedLayerID id, PlatformLayer* canvasPlatformLayer)
{
    m_shouldSyncFrame = true;
    GraphicsSurfaceToken token = canvasPlatformLayer->graphicsSurfaceToken();
    IntSize canvasSize = canvasPlatformLayer->platformLayerSize();
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::CreateCanvas(id, canvasSize, token));
}

void CoordinatedLayerTreeHost::syncCanvas(CoordinatedLayerID id, PlatformLayer* canvasPlatformLayer)
{
    m_shouldSyncFrame = true;
    uint32_t frontBuffer = canvasPlatformLayer->copyToGraphicsSurface();
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::SyncCanvas(id, frontBuffer));
}

void CoordinatedLayerTreeHost::destroyCanvas(CoordinatedLayerID id)
{
    if (m_isPurging)
        return;

    m_shouldSyncFrame = true;
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::DestroyCanvas(id));
}
#endif

void CoordinatedLayerTreeHost::setLayerRepaintCount(CoordinatedLayerID id, int value)
{
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::SetLayerRepaintCount(id, value));
}

#if ENABLE(CSS_FILTERS)
void CoordinatedLayerTreeHost::syncLayerFilters(CoordinatedLayerID id, const FilterOperations& filters)
{
    m_shouldSyncFrame = true;
#if ENABLE(CSS_SHADERS)
    checkCustomFilterProgramProxies(filters);
#endif
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::SetCompositingLayerFilters(id, filters));
}
#endif

#if ENABLE(CSS_SHADERS)
void CoordinatedLayerTreeHost::checkCustomFilterProgramProxies(const FilterOperations& filters)
{
    // We need to create the WebCustomFilterProgramProxy objects before we get to serialize the
    // custom filters to the other process. That's because WebCustomFilterProgramProxy needs
    // to link back to the coordinator, so that we can send a message to the UI process when
    // the program is not needed anymore.
    // Note that the serialization will only happen at a later time in ArgumentCoder<WebCore::FilterOperations>::encode.
    // At that point the program will only be serialized once. All the other times it will only use the ID of the program.
    for (size_t i = 0; i < filters.size(); ++i) {
        const FilterOperation* operation = filters.at(i);
        if (operation->getOperationType() != FilterOperation::VALIDATED_CUSTOM)
            continue;
        const ValidatedCustomFilterOperation* customOperation = static_cast<const ValidatedCustomFilterOperation*>(operation);
        ASSERT(customOperation->validatedProgram()->isInitialized());
        TextureMapperPlatformCompiledProgram* program = customOperation->validatedProgram()->platformCompiledProgram();

        RefPtr<WebCustomFilterProgramProxy> customFilterProgramProxy;
        if (program->client())
            customFilterProgramProxy = static_cast<WebCustomFilterProgramProxy*>(program->client());
        else {
            customFilterProgramProxy = WebCustomFilterProgramProxy::create();
            program->setClient(customFilterProgramProxy);
        }

        if (!customFilterProgramProxy->client()) {
            customFilterProgramProxy->setClient(this);
            m_customFilterPrograms.add(customFilterProgramProxy.get());
            m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::CreateCustomFilterProgram(customFilterProgramProxy->id(), customOperation->validatedProgram()->validatedProgramInfo()));
        } else {
            // If the client was not disconnected then this coordinator must be the client for it.
            ASSERT(customFilterProgramProxy->client() == this);
        }
    }
}

void CoordinatedLayerTreeHost::removeCustomFilterProgramProxy(WebCustomFilterProgramProxy* customFilterProgramProxy)
{
    // At this time the shader is not needed anymore, so we remove it from our set and
    // send a message to the other process to delete it.
    m_customFilterPrograms.remove(customFilterProgramProxy);
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::RemoveCustomFilterProgram(customFilterProgramProxy->id()));
}

void CoordinatedLayerTreeHost::disconnectCustomFilterPrograms()
{
    // Make sure that WebCore will not call into this coordinator anymore.
    HashSet<WebCustomFilterProgramProxy*>::iterator iter = m_customFilterPrograms.begin();
    for (; iter != m_customFilterPrograms.end(); ++iter)
        (*iter)->setClient(0);
}
#endif

void CoordinatedLayerTreeHost::detachLayer(CoordinatedGraphicsLayer* layer)
{
    m_registeredLayers.remove(layer);
    m_layersToDelete.append(layer->id());
    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::lockAnimations()
{
    m_animationsLocked = true;
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::SetAnimationsLocked(true));
}

void CoordinatedLayerTreeHost::unlockAnimations()
{
    if (!m_animationsLocked)
        return;

    m_animationsLocked = false;
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::SetAnimationsLocked(false));
}

void CoordinatedLayerTreeHost::performScheduledLayerFlush()
{
    if (m_isSuspended || m_waitingForUIProcess)
        return;

    // We lock the animations while performing layout, to avoid flickers caused by animations continuing in the UI process while
    // the web process layout wants to cancel them.
    lockAnimations();
    syncDisplayState();

    // We can unlock the animations before flushing if there are no visible changes, for example if there are content updates
    // in a layer with opacity 0.
    bool canUnlockBeforeFlush = !m_isValid || !toCoordinatedGraphicsLayer(m_rootLayer.get())->hasPendingVisibleChanges();
    if (canUnlockBeforeFlush)
        unlockAnimations();

    if (!m_isValid)
        return;

    if (flushPendingLayerChanges())
        didPerformScheduledLayerFlush();
}

void CoordinatedLayerTreeHost::syncDisplayState()
{
#if ENABLE(REQUEST_ANIMATION_FRAME) && !USE(REQUEST_ANIMATION_FRAME_TIMER) && !USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    // Make sure that any previously registered animation callbacks are being executed before we flush the layers.
    m_webPage->corePage()->mainFrame()->view()->serviceScriptedAnimations(convertSecondsToDOMTimeStamp(currentTime()));
#endif

    m_webPage->layoutIfNeeded();
}

void CoordinatedLayerTreeHost::didPerformScheduledLayerFlush()
{
    if (m_notifyAfterScheduledLayerFlush) {
        static_cast<DrawingAreaImpl*>(m_webPage->drawingArea())->layerHostDidFlushLayers();
        m_notifyAfterScheduledLayerFlush = false;
    }
}

void CoordinatedLayerTreeHost::layerFlushTimerFired(Timer<CoordinatedLayerTreeHost>*)
{
    performScheduledLayerFlush();
}

void CoordinatedLayerTreeHost::createPageOverlayLayer()
{
    ASSERT(!m_pageOverlayLayer);

    m_pageOverlayLayer = GraphicsLayer::create(this, this);
#ifndef NDEBUG
    m_pageOverlayLayer->setName("CoordinatedLayerTreeHost page overlay content");
#endif

    m_pageOverlayLayer->setDrawsContent(true);
    m_pageOverlayLayer->setSize(m_webPage->size());

    m_rootLayer->addChild(m_pageOverlayLayer.get());
}

void CoordinatedLayerTreeHost::destroyPageOverlayLayer()
{
    ASSERT(m_pageOverlayLayer);
    m_pageOverlayLayer->removeFromParent();
    m_pageOverlayLayer = nullptr;
}

PassRefPtr<CoordinatedImageBacking> CoordinatedLayerTreeHost::createImageBackingIfNeeded(Image* image)
{
    CoordinatedImageBackingID imageID = CoordinatedImageBacking::getCoordinatedImageBackingID(image);
    ImageBackingMap::iterator it = m_imageBackings.find(imageID);
    RefPtr<CoordinatedImageBacking> imageBacking;
    if (it == m_imageBackings.end()) {
        imageBacking = CoordinatedImageBacking::create(this, image);
        m_imageBackings.add(imageID, imageBacking);
    } else
        imageBacking = it->value;

    return imageBacking;
}

void CoordinatedLayerTreeHost::createImageBacking(CoordinatedImageBackingID imageID)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::CreateImageBacking(imageID));
}

bool CoordinatedLayerTreeHost::updateImageBacking(CoordinatedImageBackingID imageID, PassRefPtr<CoordinatedSurface> coordinatedSurface)
{
    m_shouldSyncFrame = true;
    WebCoordinatedSurface* webCoordinatedSurface = static_cast<WebCoordinatedSurface*>(coordinatedSurface.get());
    WebCoordinatedSurface::Handle handle;
    if (!webCoordinatedSurface->createHandle(handle))
        return false;
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::UpdateImageBacking(imageID, handle));
    return true;
}

void CoordinatedLayerTreeHost::clearImageBackingContents(CoordinatedImageBackingID imageID)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::ClearImageBackingContents(imageID));
}

void CoordinatedLayerTreeHost::removeImageBacking(CoordinatedImageBackingID imageID)
{
    if (m_isPurging)
        return;

    ASSERT(m_imageBackings.contains(imageID));
    m_shouldSyncFrame = true;
    m_imageBackings.remove(imageID);
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::RemoveImageBacking(imageID));
}

void CoordinatedLayerTreeHost::flushPendingImageBackingChanges()
{
    ImageBackingMap::iterator end = m_imageBackings.end();
    for (ImageBackingMap::iterator iter = m_imageBackings.begin(); iter != end; ++iter)
        iter->value->update();
}

void CoordinatedLayerTreeHost::notifyAnimationStarted(const WebCore::GraphicsLayer*, double /* time */)
{
}

void CoordinatedLayerTreeHost::notifyFlushRequired(const WebCore::GraphicsLayer*)
{
    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::paintContents(const WebCore::GraphicsLayer* graphicsLayer, WebCore::GraphicsContext& graphicsContext, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& clipRect)
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

PassOwnPtr<GraphicsLayer> CoordinatedLayerTreeHost::createGraphicsLayer(GraphicsLayerClient* client)
{
    CoordinatedGraphicsLayer* layer = new CoordinatedGraphicsLayer(client);
    layer->setCoordinator(this);
    m_registeredLayers.add(layer);
    m_layersToCreate.append(layer->id());
    layer->setNeedsVisibleRectAdjustment();
    scheduleLayerFlush();
    return adoptPtr(layer);
}

PassRefPtr<CoordinatedSurface> CoordinatedLayerTreeHost::createCoordinatedSurface(const IntSize& size, CoordinatedSurface::Flags flags)
{
    return WebCoordinatedSurface::create(size, flags);
}

float CoordinatedLayerTreeHost::deviceScaleFactor() const
{
    return m_webPage->deviceScaleFactor();
}

float CoordinatedLayerTreeHost::pageScaleFactor() const
{
    return m_webPage->pageScaleFactor();
}

bool LayerTreeHost::supportsAcceleratedCompositing()
{
    return true;
}

void CoordinatedLayerTreeHost::createTile(CoordinatedLayerID layerID, uint32_t tileID, const WebCore::SurfaceUpdateInfo& updateInfo, const WebCore::IntRect& tileRect)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::CreateTileForLayer(layerID, tileID, tileRect, updateInfo));
}

void CoordinatedLayerTreeHost::updateTile(CoordinatedLayerID layerID, uint32_t tileID, const WebCore::SurfaceUpdateInfo& updateInfo, const WebCore::IntRect& tileRect)
{
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::UpdateTileForLayer(layerID, tileID, tileRect, updateInfo));
}

void CoordinatedLayerTreeHost::removeTile(CoordinatedLayerID layerID, uint32_t tileID)
{
    if (m_isPurging)
        return;
    m_shouldSyncFrame = true;
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::RemoveTileForLayer(layerID, tileID));
}

bool CoordinatedLayerTreeHost::createUpdateAtlas(uint32_t atlasID, PassRefPtr<CoordinatedSurface> coordinatedSurface)
{
    WebCoordinatedSurface* webCoordinatedSurface = static_cast<WebCoordinatedSurface*>(coordinatedSurface.get());
    WebCoordinatedSurface::Handle handle;
    if (!webCoordinatedSurface->createHandle(handle))
        return false;
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::CreateUpdateAtlas(atlasID, handle));
    return true;
}

void CoordinatedLayerTreeHost::removeUpdateAtlas(uint32_t atlasID)
{
    if (m_isPurging)
        return;
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::RemoveUpdateAtlas(atlasID));
}

WebCore::FloatRect CoordinatedLayerTreeHost::visibleContentsRect() const
{
    return m_visibleContentsRect;
}


void CoordinatedLayerTreeHost::setLayerAnimations(CoordinatedLayerID layerID, const GraphicsLayerAnimations& animations)
{
    m_shouldSyncFrame = true;
    GraphicsLayerAnimations activeAnimations = animations.getActiveAnimations();
#if ENABLE(CSS_SHADERS)
    for (size_t i = 0; i < activeAnimations.animations().size(); ++i) {
        const KeyframeValueList& keyframes = animations.animations().at(i).keyframes();
        if (keyframes.property() != AnimatedPropertyWebkitFilter)
            continue;
        for (size_t j = 0; j < keyframes.size(); ++j) {
            const FilterAnimationValue* filterValue = static_cast<const FilterAnimationValue*>(keyframes.at(i));
            checkCustomFilterProgramProxies(*filterValue->value());
        }
    }
#endif
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::SetLayerAnimations(layerID, activeAnimations));
}

void CoordinatedLayerTreeHost::setVisibleContentsRect(const FloatRect& rect, const FloatPoint& trajectoryVector)
{
    // A zero trajectoryVector indicates that tiles all around the viewport are requested.
    toCoordinatedGraphicsLayer(m_nonCompositedContentLayer.get())->setVisibleContentRectTrajectoryVector(trajectoryVector);

    bool contentsRectDidChange = rect != m_visibleContentsRect;
    if (contentsRectDidChange) {
        m_visibleContentsRect = rect;

        HashSet<WebCore::CoordinatedGraphicsLayer*>::iterator end = m_registeredLayers.end();
        for (HashSet<WebCore::CoordinatedGraphicsLayer*>::iterator it = m_registeredLayers.begin(); it != end; ++it) {
            (*it)->setNeedsVisibleRectAdjustment();
        }
    }

    scheduleLayerFlush();
    if (m_webPage->useFixedLayout()) {
        // Round the rect instead of enclosing it to make sure that its size stays
        // the same while panning. This can have nasty effects on layout.
        m_webPage->setFixedVisibleContentRect(roundedIntRect(rect));
    }
}

void CoordinatedLayerTreeHost::deviceOrPageScaleFactorChanged()
{
    m_rootLayer->deviceOrPageScaleFactorChanged();
    m_nonCompositedContentLayer->deviceOrPageScaleFactorChanged();
    if (m_pageOverlayLayer)
        m_pageOverlayLayer->deviceOrPageScaleFactorChanged();
}

GraphicsLayerFactory* CoordinatedLayerTreeHost::graphicsLayerFactory()
{
    return this;
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
void CoordinatedLayerTreeHost::scheduleAnimation()
{
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::RequestAnimationFrame());
}

void CoordinatedLayerTreeHost::animationFrameReady()
{
    scheduleLayerFlush();
}
#endif

void CoordinatedLayerTreeHost::renderNextFrame()
{
    m_waitingForUIProcess = false;
    scheduleLayerFlush();
    for (unsigned i = 0; i < m_updateAtlases.size(); ++i)
        m_updateAtlases[i]->didSwapBuffers();
}

void CoordinatedLayerTreeHost::purgeBackingStores()
{
    TemporaryChange<bool> purgingToggle(m_isPurging, true);

    HashSet<WebCore::CoordinatedGraphicsLayer*>::iterator end = m_registeredLayers.end();
    for (HashSet<WebCore::CoordinatedGraphicsLayer*>::iterator it = m_registeredLayers.begin(); it != end; ++it)
        (*it)->purgeBackingStores();

    m_imageBackings.clear();
    m_updateAtlases.clear();
}

PassOwnPtr<GraphicsContext> CoordinatedLayerTreeHost::beginContentUpdate(const IntSize& size, CoordinatedSurface::Flags flags, uint32_t& atlasID, IntPoint& offset)
{
    OwnPtr<GraphicsContext> graphicsContext;
    for (unsigned i = 0; i < m_updateAtlases.size(); ++i) {
        UpdateAtlas* atlas = m_updateAtlases[i].get();
        if (atlas->supportsAlpha() == (flags & CoordinatedSurface::SupportsAlpha)) {
            // This will return null if there is no available buffer space.
            graphicsContext = atlas->beginPaintingOnAvailableBuffer(atlasID, size, offset);
            if (graphicsContext)
                return graphicsContext.release();
        }
    }

    static const int ScratchBufferDimension = 1024; // Should be a power of two.
    m_updateAtlases.append(adoptPtr(new UpdateAtlas(this, ScratchBufferDimension, flags)));
    scheduleReleaseInactiveAtlases();
    return m_updateAtlases.last()->beginPaintingOnAvailableBuffer(atlasID, size, offset);
}

const double ReleaseInactiveAtlasesTimerInterval = 0.5;

void CoordinatedLayerTreeHost::scheduleReleaseInactiveAtlases()
{
    if (!m_releaseInactiveAtlasesTimer.isActive())
        m_releaseInactiveAtlasesTimer.startRepeating(ReleaseInactiveAtlasesTimerInterval);
}

void CoordinatedLayerTreeHost::releaseInactiveAtlasesTimerFired(Timer<CoordinatedLayerTreeHost>*)
{
    // We always want to keep one atlas for non-composited content.
    OwnPtr<UpdateAtlas> atlasToKeepAnyway;
    bool foundActiveAtlasForNonCompositedContent = false;
    for (int i = m_updateAtlases.size() - 1;  i >= 0; --i) {
        UpdateAtlas* atlas = m_updateAtlases[i].get();
        if (!atlas->isInUse())
            atlas->addTimeInactive(ReleaseInactiveAtlasesTimerInterval);
        bool usableForNonCompositedContent = !atlas->supportsAlpha();
        if (atlas->isInactive()) {
            if (!foundActiveAtlasForNonCompositedContent && !atlasToKeepAnyway && usableForNonCompositedContent)
                atlasToKeepAnyway = m_updateAtlases[i].release();
            m_updateAtlases.remove(i);
        } else if (usableForNonCompositedContent)
            foundActiveAtlasForNonCompositedContent = true;
    }

    if (!foundActiveAtlasForNonCompositedContent && atlasToKeepAnyway)
        m_updateAtlases.append(atlasToKeepAnyway.release());

    if (m_updateAtlases.size() <= 1)
        m_releaseInactiveAtlasesTimer.stop();
}

void CoordinatedLayerTreeHost::setBackgroundColor(const WebCore::Color& color)
{
    m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::SetBackgroundColor(color));
}

} // namespace WebKit
#endif // USE(COORDINATED_GRAPHICS)
