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
#include <WebCore/InspectorController.h>
#include <WebCore/Page.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderLayerBacking.h>
#include <WebCore/RenderLayerCompositor.h>
#include <WebCore/RenderView.h>
#include <WebCore/Settings.h>
#include <WebCore/SurfaceUpdateInfo.h>
#include <WebCore/TextureMapperPlatformLayer.h>
#include <wtf/CurrentTime.h>
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

    LayerMap::iterator end = m_registeredLayers.end();
    for (LayerMap::iterator it = m_registeredLayers.begin(); it != end; ++it)
        it->value->setCoordinator(0);
}

CoordinatedLayerTreeHost::CoordinatedLayerTreeHost(WebPage* webPage)
    : LayerTreeHost(webPage)
    , m_rootCompositingLayer(0)
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
#if ENABLE(REQUEST_ANIMATION_FRAME)
    , m_lastAnimationServiceTime(0)
#endif
{
    m_webPage->corePage()->settings()->setApplyDeviceScaleFactorInCompositor(true);

    // Create a root layer.
    m_rootLayer = GraphicsLayer::create(this, this);
#ifndef NDEBUG
    m_rootLayer->setName("CoordinatedLayerTreeHost root layer");
#endif
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setSize(m_webPage->size());
    m_layerTreeContext.coordinatedLayerID = toCoordinatedGraphicsLayer(m_rootLayer.get())->id();

    CoordinatedSurface::setFactory(createCoordinatedSurface);

    // This is a temporary way to enable this only in the GL case, until TextureMapperImageBuffer is removed.
    // See https://bugs.webkit.org/show_bug.cgi?id=114869
    CoordinatedGraphicsLayer::setShouldSupportContentsTiling(true);

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

    if (!m_layerFlushTimer.isActive() || m_layerFlushTimer.nextFireInterval() > 0)
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
    if (m_rootCompositingLayer)
        m_rootCompositingLayer->removeFromParent();

    m_rootCompositingLayer = graphicsLayer;
    if (m_rootCompositingLayer)
        m_rootLayer->addChildAtIndex(m_rootCompositingLayer, 0);
}

void CoordinatedLayerTreeHost::invalidate()
{
    cancelPendingLayerFlush();

    ASSERT(m_isValid);
    m_rootLayer = nullptr;
    m_isValid = false;
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
    m_rootLayer->setSize(newSize);
    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::didInstallPageOverlay(PageOverlay* pageOverlay)
{
    ASSERT(!m_pageOverlay);
    m_pageOverlay = pageOverlay;

    createPageOverlayLayer();
    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::didUninstallPageOverlay(PageOverlay*)
{
    m_pageOverlay = 0;

    destroyPageOverlayLayer();
    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::setPageOverlayNeedsDisplay(PageOverlay*, const WebCore::IntRect& rect)
{
    ASSERT(m_pageOverlayLayer);
    m_pageOverlayLayer->setNeedsDisplayInRect(rect);
    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::setPageOverlayOpacity(PageOverlay*, float value)
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

    initializeRootCompositingLayerIfNeeded();

    m_rootLayer->flushCompositingStateForThisLayerOnly();
    if (m_pageOverlayLayer)
        m_pageOverlayLayer->flushCompositingStateForThisLayerOnly();

    bool didSync = m_webPage->corePage()->mainFrame()->view()->flushCompositingStateIncludingSubframes();

    toCoordinatedGraphicsLayer(m_rootLayer.get())->updateContentBuffersIncludingSubLayers();
    toCoordinatedGraphicsLayer(m_rootLayer.get())->syncPendingStateChangesIncludingSubLayers();

    flushPendingImageBackingChanges();

    if (m_shouldSyncFrame) {
        didSync = true;

        if (m_rootCompositingLayer) {
            m_state.contentsSize = roundedIntSize(m_rootCompositingLayer->size());
            if (CoordinatedGraphicsLayer* contentsLayer = mainContentsLayer())
                m_state.coveredRect = contentsLayer->coverRect();
        }

        m_state.scrollPosition = m_visibleContentsRect.location();

        m_webPage->send(Messages::CoordinatedLayerTreeHostProxy::CommitCoordinatedGraphicsState(m_state));

        clearPendingStateChanges();
        m_waitingForUIProcess = true;
        m_shouldSyncFrame = false;
    }

    if (m_forceRepaintAsyncCallbackID) {
        m_webPage->send(Messages::WebPageProxy::VoidCallback(m_forceRepaintAsyncCallbackID));
        m_forceRepaintAsyncCallbackID = 0;
    }

    return didSync;
}

void CoordinatedLayerTreeHost::clearPendingStateChanges()
{
    m_state.layersToCreate.clear();
    m_state.layersToUpdate.clear();
    m_state.layersToRemove.clear();

    m_state.imagesToCreate.clear();
    m_state.imagesToRemove.clear();
    m_state.imagesToUpdate.clear();
    m_state.imagesToClear.clear();

    m_state.updateAtlasesToCreate.clear();
    m_state.updateAtlasesToRemove.clear();

#if ENABLE(CSS_SHADERS)
    m_state.customFiltersToCreate.clear();
    m_state.customFiltersToRemove.clear();
#endif
}

void CoordinatedLayerTreeHost::initializeRootCompositingLayerIfNeeded()
{
    if (m_didInitializeRootCompositingLayer)
        return;

    m_state.rootCompositingLayer = toCoordinatedGraphicsLayer(m_rootLayer.get())->id();
    m_didInitializeRootCompositingLayer = true;
    m_shouldSyncFrame = true;
}

void CoordinatedLayerTreeHost::syncLayerState(CoordinatedLayerID id, CoordinatedGraphicsLayerState& state)
{
    m_shouldSyncFrame = true;

#if ENABLE(CSS_SHADERS)
    prepareCustomFilterProxiesIfNeeded(state);
#endif

    m_state.layersToUpdate.append(std::make_pair(id, state));
}

#if ENABLE(CSS_SHADERS)
void CoordinatedLayerTreeHost::prepareCustomFilterProxiesIfNeeded(CoordinatedGraphicsLayerState& state)
{
    if (state.animationsChanged) {
        GraphicsLayerAnimations& activeAnimations = state.animations;
        for (size_t i = 0; i < activeAnimations.animations().size(); ++i) {
            const KeyframeValueList& keyframes = activeAnimations.animations().at(i).keyframes();
            if (keyframes.property() != AnimatedPropertyWebkitFilter)
                continue;
            for (size_t j = 0; j < keyframes.size(); ++j) {
                const FilterAnimationValue& filterValue = static_cast<const FilterAnimationValue&>(keyframes.at(j));
                checkCustomFilterProgramProxies(filterValue.value());
            }
        }
    }

    if (state.filtersChanged)
        checkCustomFilterProgramProxies(state.filters);
}

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
            m_state.customFiltersToCreate.append(std::make_pair(customFilterProgramProxy->id(), customOperation->validatedProgram()->validatedProgramInfo()));
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
    m_state.customFiltersToRemove.append(customFilterProgramProxy->id());
}

void CoordinatedLayerTreeHost::disconnectCustomFilterPrograms()
{
    // Make sure that WebCore will not call into this coordinator anymore.
    HashSet<WebCustomFilterProgramProxy*>::iterator iter = m_customFilterPrograms.begin();
    for (; iter != m_customFilterPrograms.end(); ++iter)
        (*iter)->setClient(0);
}
#endif // ENABLE(CSS_SHADERS)

void CoordinatedLayerTreeHost::detachLayer(CoordinatedGraphicsLayer* layer)
{
    m_registeredLayers.remove(layer->id());

    size_t index = m_state.layersToCreate.find(layer->id());
    if (index != notFound) {
        m_state.layersToCreate.remove(index);
        return;
    }

    m_state.layersToRemove.append(layer->id());
    scheduleLayerFlush();
}

void CoordinatedLayerTreeHost::performScheduledLayerFlush()
{
    if (m_isSuspended || m_waitingForUIProcess)
        return;

    syncDisplayState();

    if (!m_isValid)
        return;

    if (flushPendingLayerChanges())
        didPerformScheduledLayerFlush();
}

void CoordinatedLayerTreeHost::syncDisplayState()
{
#if ENABLE(INSPECTOR)
    m_webPage->corePage()->inspectorController()->didBeginFrame();
#endif

#if ENABLE(REQUEST_ANIMATION_FRAME) && !USE(REQUEST_ANIMATION_FRAME_TIMER) && !USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    // Make sure that any previously registered animation callbacks are being executed before we flush the layers.
    m_lastAnimationServiceTime = WTF::monotonicallyIncreasingTime();
    m_webPage->corePage()->mainFrame()->view()->serviceScriptedAnimations(m_lastAnimationServiceTime);
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
    m_state.imagesToCreate.append(imageID);
}

void CoordinatedLayerTreeHost::updateImageBacking(CoordinatedImageBackingID imageID, PassRefPtr<CoordinatedSurface> coordinatedSurface)
{
    m_shouldSyncFrame = true;
    m_state.imagesToUpdate.append(std::make_pair(imageID, coordinatedSurface));
}

void CoordinatedLayerTreeHost::clearImageBackingContents(CoordinatedImageBackingID imageID)
{
    m_shouldSyncFrame = true;
    m_state.imagesToClear.append(imageID);
}

void CoordinatedLayerTreeHost::removeImageBacking(CoordinatedImageBackingID imageID)
{
    if (m_isPurging)
        return;

    ASSERT(m_imageBackings.contains(imageID));
    m_imageBackings.remove(imageID);

    m_state.imagesToRemove.append(imageID);
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
    if (graphicsLayer == m_pageOverlayLayer) {
        // Overlays contain transparent contents and won't clear the context as part of their rendering, so we do it here.
        graphicsContext.clearRect(clipRect);
        m_webPage->drawPageOverlay(m_pageOverlay.get(), graphicsContext, clipRect);
        return;
    }
}

PassOwnPtr<GraphicsLayer> CoordinatedLayerTreeHost::createGraphicsLayer(GraphicsLayerClient* client)
{
    CoordinatedGraphicsLayer* layer = new CoordinatedGraphicsLayer(client);
    layer->setCoordinator(this);
    m_registeredLayers.add(layer->id(), layer);
    m_state.layersToCreate.append(layer->id());
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

void CoordinatedLayerTreeHost::createUpdateAtlas(uint32_t atlasID, PassRefPtr<CoordinatedSurface> coordinatedSurface)
{
    m_state.updateAtlasesToCreate.append(std::make_pair(atlasID, coordinatedSurface));
}

void CoordinatedLayerTreeHost::removeUpdateAtlas(uint32_t atlasID)
{
    if (m_isPurging)
        return;
    m_state.updateAtlasesToRemove.append(atlasID);
}

WebCore::FloatRect CoordinatedLayerTreeHost::visibleContentsRect() const
{
    return m_visibleContentsRect;
}

CoordinatedGraphicsLayer* CoordinatedLayerTreeHost::mainContentsLayer()
{
    if (!m_rootCompositingLayer)
        return 0;

    return toCoordinatedGraphicsLayer(m_rootCompositingLayer)->findFirstDescendantWithContentsRecursively();
}

void CoordinatedLayerTreeHost::setVisibleContentsRect(const FloatRect& rect, const FloatPoint& trajectoryVector)
{
    // A zero trajectoryVector indicates that tiles all around the viewport are requested.
    if (CoordinatedGraphicsLayer* contentsLayer = mainContentsLayer())
        contentsLayer->setVisibleContentRectTrajectoryVector(trajectoryVector);

    bool contentsRectDidChange = rect != m_visibleContentsRect;
    if (contentsRectDidChange) {
        m_visibleContentsRect = rect;

        LayerMap::iterator end = m_registeredLayers.end();
        for (LayerMap::iterator it = m_registeredLayers.begin(); it != end; ++it) {
            it->value->setNeedsVisibleRectAdjustment();
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
    if (m_pageOverlayLayer)
        m_pageOverlayLayer->deviceOrPageScaleFactorChanged();
}

void CoordinatedLayerTreeHost::pageBackgroundTransparencyChanged()
{
}

GraphicsLayerFactory* CoordinatedLayerTreeHost::graphicsLayerFactory()
{
    return this;
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
void CoordinatedLayerTreeHost::scheduleAnimation()
{
    if (m_waitingForUIProcess)
        return;

    if (m_layerFlushTimer.isActive())
        return;

    // According to the requestAnimationFrame spec, rAF callbacks should not be faster than 60FPS.
    static const double MinimalTimeoutForAnimations = 1. / 60.;
    m_layerFlushTimer.startOneShot(std::max<double>(0., MinimalTimeoutForAnimations - WTF::monotonicallyIncreasingTime() + m_lastAnimationServiceTime));
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

    LayerMap::iterator end = m_registeredLayers.end();
    for (LayerMap::iterator it = m_registeredLayers.begin(); it != end; ++it)
        it->value->purgeBackingStores();

    m_imageBackings.clear();
    m_updateAtlases.clear();
}

bool CoordinatedLayerTreeHost::paintToSurface(const IntSize& size, CoordinatedSurface::Flags flags, uint32_t& atlasID, IntPoint& offset, CoordinatedSurface::Client* client)
{
    for (unsigned i = 0; i < m_updateAtlases.size(); ++i) {
        UpdateAtlas* atlas = m_updateAtlases[i].get();
        if (atlas->supportsAlpha() == (flags & CoordinatedSurface::SupportsAlpha)) {
            // This will false if there is no available buffer space.
            if (atlas->paintOnAvailableBuffer(size, atlasID, offset, client))
                return true;
        }
    }

    static const int ScratchBufferDimension = 1024; // Should be a power of two.
    m_updateAtlases.append(adoptPtr(new UpdateAtlas(this, ScratchBufferDimension, flags)));
    scheduleReleaseInactiveAtlases();
    return m_updateAtlases.last()->paintOnAvailableBuffer(size, atlasID, offset, client);
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

void CoordinatedLayerTreeHost::commitScrollOffset(uint32_t layerID, const WebCore::IntSize& offset)
{
    LayerMap::iterator i = m_registeredLayers.find(layerID);
    if (i == m_registeredLayers.end())
        return;

    i->value->commitScrollOffset(offset);
}

} // namespace WebKit
#endif // USE(COORDINATED_GRAPHICS)
