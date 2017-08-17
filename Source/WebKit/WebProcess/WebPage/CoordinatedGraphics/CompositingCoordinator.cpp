/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2013 Company 100, Inc. All rights reserved.
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
#include "CompositingCoordinator.h"

#if USE(COORDINATED_GRAPHICS)

#include <WebCore/DOMWindow.h>
#include <WebCore/Document.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/InspectorController.h>
#include <WebCore/MainFrame.h>
#include <WebCore/Page.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/SetForScope.h>

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

using namespace WebCore;

namespace WebKit {

CompositingCoordinator::CompositingCoordinator(Page* page, CompositingCoordinator::Client& client)
    : m_page(page)
    , m_client(client)
    , m_releaseInactiveAtlasesTimer(RunLoop::main(), this, &CompositingCoordinator::releaseInactiveAtlasesTimerFired)
{
#if USE(GLIB_EVENT_LOOP)
    m_releaseInactiveAtlasesTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
#endif
}

CompositingCoordinator::~CompositingCoordinator()
{
    m_isDestructing = true;

    purgeBackingStores();

    for (auto& registeredLayer : m_registeredLayers.values())
        registeredLayer->setCoordinator(nullptr);
}

void CompositingCoordinator::invalidate()
{
    m_rootLayer = nullptr;
    purgeBackingStores();
}

void CompositingCoordinator::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    if (m_rootCompositingLayer == graphicsLayer)
        return;

    if (m_rootCompositingLayer)
        m_rootCompositingLayer->removeFromParent();

    m_rootCompositingLayer = graphicsLayer;
    if (m_rootCompositingLayer)
        m_rootLayer->addChildAtIndex(m_rootCompositingLayer, 0);
}

void CompositingCoordinator::setViewOverlayRootLayer(GraphicsLayer* graphicsLayer)
{
    if (m_overlayCompositingLayer == graphicsLayer)
        return;

    if (m_overlayCompositingLayer)
        m_overlayCompositingLayer->removeFromParent();

    m_overlayCompositingLayer = graphicsLayer;
    if (m_overlayCompositingLayer)
        m_rootLayer->addChild(m_overlayCompositingLayer);
}

void CompositingCoordinator::sizeDidChange(const IntSize& newSize)
{
    m_rootLayer->setSize(newSize);
    notifyFlushRequired(m_rootLayer.get());
}

bool CompositingCoordinator::flushPendingLayerChanges()
{
    SetForScope<bool> protector(m_isFlushingLayerChanges, true);

    initializeRootCompositingLayerIfNeeded();

    m_rootLayer->flushCompositingStateForThisLayerOnly();
    m_client.didFlushRootLayer(m_visibleContentsRect);

    if (m_overlayCompositingLayer)
        m_overlayCompositingLayer->flushCompositingState(FloatRect(FloatPoint(), m_rootLayer->size()));

    bool didSync = m_page->mainFrame().view()->flushCompositingStateIncludingSubframes();

    auto& coordinatedLayer = downcast<CoordinatedGraphicsLayer>(*m_rootLayer);
    coordinatedLayer.updateContentBuffersIncludingSubLayers();
    coordinatedLayer.syncPendingStateChangesIncludingSubLayers();

    flushPendingImageBackingChanges();

    if (m_shouldSyncFrame) {
        didSync = true;

        if (m_rootCompositingLayer) {
            m_state.contentsSize = roundedIntSize(m_rootCompositingLayer->size());
            if (CoordinatedGraphicsLayer* contentsLayer = mainContentsLayer())
                m_state.coveredRect = contentsLayer->coverRect();
        }
        m_state.scrollPosition = m_visibleContentsRect.location();

        m_client.commitSceneState(m_state);

        if (!m_atlasesToRemove.isEmpty())
            m_client.releaseUpdateAtlases(m_atlasesToRemove);
        m_atlasesToRemove.clear();

        clearPendingStateChanges();
        m_shouldSyncFrame = false;
    }

    return didSync;
}

double CompositingCoordinator::timestamp() const
{
    auto* document = m_page->mainFrame().document();
    if (!document)
        return 0;
    return document->domWindow() ? document->domWindow()->nowTimestamp() : document->monotonicTimestamp();
}

void CompositingCoordinator::syncDisplayState()
{
    m_page->mainFrame().view()->updateLayoutAndStyleIfNeededRecursive();
}

double CompositingCoordinator::nextAnimationServiceTime() const
{
    // According to the requestAnimationFrame spec, rAF callbacks should not be faster than 60FPS.
    static const double MinimalTimeoutForAnimations = 1. / 60.;
    return std::max<double>(0., MinimalTimeoutForAnimations - timestamp() + m_lastAnimationServiceTime);
}

void CompositingCoordinator::clearPendingStateChanges()
{
    m_state.layersToCreate.clear();
    m_state.layersToUpdate.clear();
    m_state.layersToRemove.clear();

    m_state.imagesToCreate.clear();
    m_state.imagesToRemove.clear();
    m_state.imagesToUpdate.clear();
    m_state.imagesToClear.clear();

    m_state.updateAtlasesToCreate.clear();
}

void CompositingCoordinator::initializeRootCompositingLayerIfNeeded()
{
    if (m_didInitializeRootCompositingLayer)
        return;

    m_state.rootCompositingLayer = downcast<CoordinatedGraphicsLayer>(*m_rootLayer).id();
    m_didInitializeRootCompositingLayer = true;
    m_shouldSyncFrame = true;
}

void CompositingCoordinator::createRootLayer(const IntSize& size)
{
    ASSERT(!m_rootLayer);
    // Create a root layer.
    m_rootLayer = GraphicsLayer::create(this, *this);
#ifndef NDEBUG
    m_rootLayer->setName("CompositingCoordinator root layer");
#endif
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setSize(size);
}

void CompositingCoordinator::syncLayerState(CoordinatedLayerID id, CoordinatedGraphicsLayerState& state)
{
    m_shouldSyncFrame = true;
    m_state.layersToUpdate.append(std::make_pair(id, state));
}

Ref<CoordinatedImageBacking> CompositingCoordinator::createImageBackingIfNeeded(Image& image)
{
    CoordinatedImageBackingID imageID = CoordinatedImageBacking::getCoordinatedImageBackingID(&image);
    auto addResult = m_imageBackings.ensure(imageID, [this, &image] {
        return CoordinatedImageBacking::create(*this, image);
    });
    return *addResult.iterator->value;
}

void CompositingCoordinator::createImageBacking(CoordinatedImageBackingID imageID)
{
    m_state.imagesToCreate.append(imageID);
}

void CompositingCoordinator::updateImageBacking(CoordinatedImageBackingID imageID, RefPtr<CoordinatedSurface>&& coordinatedSurface)
{
    m_shouldSyncFrame = true;
    m_state.imagesToUpdate.append(std::make_pair(imageID, WTFMove(coordinatedSurface)));
}

void CompositingCoordinator::clearImageBackingContents(CoordinatedImageBackingID imageID)
{
    m_shouldSyncFrame = true;
    m_state.imagesToClear.append(imageID);
}

void CompositingCoordinator::removeImageBacking(CoordinatedImageBackingID imageID)
{
    if (m_isPurging)
        return;

    ASSERT(m_imageBackings.contains(imageID));
    m_imageBackings.remove(imageID);

    m_state.imagesToRemove.append(imageID);

    size_t imageIDPosition = m_state.imagesToClear.find(imageID);
    if (imageIDPosition != notFound)
        m_state.imagesToClear.remove(imageIDPosition);
}

void CompositingCoordinator::flushPendingImageBackingChanges()
{
    for (auto& imageBacking : m_imageBackings.values())
        imageBacking->update();
}

void CompositingCoordinator::notifyAnimationStarted(const GraphicsLayer*, const String&, double /* time */)
{
}

void CompositingCoordinator::notifyFlushRequired(const GraphicsLayer*)
{
    if (!m_isDestructing && !isFlushingLayerChanges())
        m_client.notifyFlushRequired();
}

void CompositingCoordinator::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& graphicsContext, GraphicsLayerPaintingPhase, const FloatRect& clipRect, GraphicsLayerPaintBehavior)
{
    m_client.paintLayerContents(graphicsLayer, graphicsContext, enclosingIntRect(clipRect));
}

std::unique_ptr<GraphicsLayer> CompositingCoordinator::createGraphicsLayer(GraphicsLayer::Type layerType, GraphicsLayerClient& client)
{
    CoordinatedGraphicsLayer* layer = new CoordinatedGraphicsLayer(layerType, client);
    layer->setCoordinator(this);
    m_registeredLayers.add(layer->id(), layer);
    m_state.layersToCreate.append(layer->id());
    layer->setNeedsVisibleRectAdjustment();
    notifyFlushRequired(layer);
    return std::unique_ptr<GraphicsLayer>(layer);
}

float CompositingCoordinator::deviceScaleFactor() const
{
    return m_page->deviceScaleFactor();
}

float CompositingCoordinator::pageScaleFactor() const
{
    return m_page->pageScaleFactor();
}

void CompositingCoordinator::createUpdateAtlas(uint32_t atlasID, RefPtr<CoordinatedSurface>&& coordinatedSurface)
{
    m_state.updateAtlasesToCreate.append(std::make_pair(atlasID, WTFMove(coordinatedSurface)));
}

void CompositingCoordinator::removeUpdateAtlas(uint32_t atlasID)
{
    if (m_isPurging)
        return;
    m_atlasesToRemove.append(atlasID);
}

FloatRect CompositingCoordinator::visibleContentsRect() const
{
    return m_visibleContentsRect;
}

CoordinatedGraphicsLayer* CompositingCoordinator::mainContentsLayer()
{
    if (!is<CoordinatedGraphicsLayer>(m_rootCompositingLayer))
        return nullptr;

    return downcast<CoordinatedGraphicsLayer>(*m_rootCompositingLayer).findFirstDescendantWithContentsRecursively();
}

void CompositingCoordinator::setVisibleContentsRect(const FloatRect& rect, const FloatPoint& trajectoryVector)
{
    // A zero trajectoryVector indicates that tiles all around the viewport are requested.
    if (CoordinatedGraphicsLayer* contentsLayer = mainContentsLayer())
        contentsLayer->setVisibleContentRectTrajectoryVector(trajectoryVector);

    bool contentsRectDidChange = rect != m_visibleContentsRect;
    if (contentsRectDidChange) {
        m_visibleContentsRect = rect;

        for (auto& registeredLayer : m_registeredLayers.values())
            registeredLayer->setNeedsVisibleRectAdjustment();
    }

    FrameView* view = m_page->mainFrame().view();
    if (view->useFixedLayout() && contentsRectDidChange) {
        // Round the rect instead of enclosing it to make sure that its size stays
        // the same while panning. This can have nasty effects on layout.
        view->setFixedVisibleContentRect(roundedIntRect(rect));
    }
}

void CompositingCoordinator::deviceOrPageScaleFactorChanged()
{
    m_rootLayer->deviceOrPageScaleFactorChanged();
}

void CompositingCoordinator::detachLayer(CoordinatedGraphicsLayer* layer)
{
    if (m_isPurging)
        return;

    m_registeredLayers.remove(layer->id());

    size_t index = m_state.layersToCreate.find(layer->id());
    if (index != notFound) {
        m_state.layersToCreate.remove(index);
        return;
    }

    m_state.layersToRemove.append(layer->id());
    notifyFlushRequired(layer);
}

void CompositingCoordinator::commitScrollOffset(uint32_t layerID, const WebCore::IntSize& offset)
{
    if (auto* layer = m_registeredLayers.get(layerID))
        layer->commitScrollOffset(offset);
}

void CompositingCoordinator::renderNextFrame()
{
    for (auto& atlas : m_updateAtlases)
        atlas->didSwapBuffers();
}

void CompositingCoordinator::purgeBackingStores()
{
    SetForScope<bool> purgingToggle(m_isPurging, true);

    for (auto& registeredLayer : m_registeredLayers.values())
        registeredLayer->purgeBackingStores();

    m_imageBackings.clear();
    m_updateAtlases.clear();
}

bool CompositingCoordinator::paintToSurface(const IntSize& size, CoordinatedSurface::Flags flags, uint32_t& atlasID, IntPoint& offset, CoordinatedSurface::Client& client)
{
    for (auto& updateAtlas : m_updateAtlases) {
        UpdateAtlas* atlas = updateAtlas.get();
        if (atlas->supportsAlpha() == (flags & CoordinatedSurface::SupportsAlpha)) {
            // This will be false if there is no available buffer space.
            if (atlas->paintOnAvailableBuffer(size, atlasID, offset, client))
                return true;
        }
    }

    static const int ScratchBufferDimension = 1024; // Should be a power of two.
    m_updateAtlases.append(std::make_unique<UpdateAtlas>(*this, ScratchBufferDimension, flags));
    scheduleReleaseInactiveAtlases();
    return m_updateAtlases.last()->paintOnAvailableBuffer(size, atlasID, offset, client);
}

const Seconds releaseInactiveAtlasesTimerInterval { 500_ms };

void CompositingCoordinator::scheduleReleaseInactiveAtlases()
{
    if (!m_releaseInactiveAtlasesTimer.isActive())
        m_releaseInactiveAtlasesTimer.startRepeating(releaseInactiveAtlasesTimerInterval);
}

void CompositingCoordinator::releaseInactiveAtlasesTimerFired()
{
    releaseAtlases(MemoryPressureHandler::singleton().isUnderMemoryPressure() ? ReleaseUnused : ReleaseInactive);
}

void CompositingCoordinator::releaseAtlases(ReleaseAtlasPolicy policy)
{
    // We always want to keep one atlas for root contents layer.
    std::unique_ptr<UpdateAtlas> atlasToKeepAnyway;
    bool foundActiveAtlasForRootContentsLayer = false;
    for (int i = m_updateAtlases.size() - 1;  i >= 0; --i) {
        UpdateAtlas* atlas = m_updateAtlases[i].get();
        bool inUse = atlas->isInUse();
        if (!inUse)
            atlas->addTimeInactive(releaseInactiveAtlasesTimerInterval.value());
        bool usableForRootContentsLayer = !atlas->supportsAlpha();
        if (atlas->isInactive() || (!inUse && policy == ReleaseUnused)) {
            if (!foundActiveAtlasForRootContentsLayer && !atlasToKeepAnyway && usableForRootContentsLayer)
                atlasToKeepAnyway = WTFMove(m_updateAtlases[i]);
            m_updateAtlases.remove(i);
        } else if (usableForRootContentsLayer)
            foundActiveAtlasForRootContentsLayer = true;
    }

    if (!foundActiveAtlasForRootContentsLayer && atlasToKeepAnyway)
        m_updateAtlases.append(atlasToKeepAnyway.release());

    m_updateAtlases.shrinkToFit();

    if (m_updateAtlases.size() <= 1)
        m_releaseInactiveAtlasesTimer.stop();

    if (!m_atlasesToRemove.isEmpty())
        m_client.releaseUpdateAtlases(m_atlasesToRemove);
    m_atlasesToRemove.clear();
}

void CompositingCoordinator::clearUpdateAtlases()
{
    if (m_isPurging)
        return;

    m_releaseInactiveAtlasesTimer.stop();
    m_updateAtlases.clear();

    if (!m_atlasesToRemove.isEmpty())
        m_client.releaseUpdateAtlases(m_atlasesToRemove);
    m_atlasesToRemove.clear();
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
