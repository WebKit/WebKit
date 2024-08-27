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

#include "WebPage.h"
#include "WebPageInlines.h"
#include <WebCore/DOMWindow.h>
#include <WebCore/Document.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/InspectorController.h>
#include <WebCore/LocalDOMWindow.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/NicosiaBackingStore.h>
#include <WebCore/NicosiaContentLayer.h>
#include <WebCore/NicosiaImageBacking.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformDisplay.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NumberOfCores.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/StringToIntegerConversion.h>

#if USE(CAIRO)
#include <WebCore/NicosiaPaintingEngine.h>
#elif USE(SKIA)
#include <WebCore/ProcessCapabilities.h>
#include <WebCore/SkiaAcceleratedBufferPool.h>
#endif

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebKit {
using namespace WebCore;

#if USE(SKIA)
static unsigned skiaNumberOfCpuPaintingThreads()
{
    static std::optional<unsigned> numberOfCpuPaintingThreads;
    if (!numberOfCpuPaintingThreads.has_value()) {
        numberOfCpuPaintingThreads = std::max(1, std::min(8, WTF::numberOfProcessorCores() / 2));

        if (const char* numThreadsEnv = getenv("WEBKIT_SKIA_CPU_PAINTING_THREADS")) {
            auto newValue = parseInteger<unsigned>(StringView::fromLatin1(numThreadsEnv));
            if (newValue && *newValue <= 8)
                numberOfCpuPaintingThreads = *newValue;
            else
                WTFLogAlways("The number of Skia/CPU painting threads is not between 0 and 8. Using the default value %u\n", numberOfCpuPaintingThreads.value());
        }
    }

    return numberOfCpuPaintingThreads.value();
}
#endif

CompositingCoordinator::CompositingCoordinator(WebPage& page, CompositingCoordinator::Client& client)
    : m_page(page)
    , m_client(client)
#if USE(CAIRO)
    , m_paintingEngine(Nicosia::PaintingEngine::create())
#endif
{
#if USE(SKIA)
    if (ProcessCapabilities::canUseAcceleratedBuffers() && PlatformDisplay::sharedDisplay().skiaGLContext())
        m_skiaAcceleratedBufferPool = makeUnique<SkiaAcceleratedBufferPool>();
    else if (auto numberOfThreads = skiaNumberOfCpuPaintingThreads(); numberOfThreads > 0)
        m_skiaUnacceleratedThreadedRenderingPool = WorkerPool::create("SkiaPaintingThread"_s, numberOfThreads);
#endif

    m_nicosia.scene = Nicosia::Scene::create();
    m_nicosia.sceneIntegration = Nicosia::SceneIntegration::create(*m_nicosia.scene, *this);

    m_rootLayer = GraphicsLayer::create(this, *this);
#ifndef NDEBUG
    m_rootLayer->setName(MAKE_STATIC_STRING_IMPL("CompositingCoordinator root layer"));
#endif
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setSize(m_page.size());
}

CompositingCoordinator::~CompositingCoordinator()
{
    ASSERT(!m_rootLayer);

    for (auto& registeredLayer : m_registeredLayers.values())
        registeredLayer->invalidateCoordinator();
}

void CompositingCoordinator::invalidate()
{
    m_nicosia.sceneIntegration->invalidate();

    m_rootLayer = nullptr;
    purgeBackingStores();

#if USE(SKIA)
    m_skiaAcceleratedBufferPool = nullptr;
    m_skiaUnacceleratedThreadedRenderingPool = nullptr;
#endif
}

void CompositingCoordinator::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    if (m_rootCompositingLayer == graphicsLayer)
        return;

    if (m_rootCompositingLayer)
        m_rootCompositingLayer->removeFromParent();

    m_rootCompositingLayer = graphicsLayer;
    if (m_rootCompositingLayer)
        m_rootLayer->addChildAtIndex(*m_rootCompositingLayer, 0);
}

void CompositingCoordinator::setViewOverlayRootLayer(GraphicsLayer* graphicsLayer)
{
    if (m_overlayCompositingLayer == graphicsLayer)
        return;

    if (m_overlayCompositingLayer)
        m_overlayCompositingLayer->removeFromParent();

    m_overlayCompositingLayer = graphicsLayer;
    if (m_overlayCompositingLayer)
        m_rootLayer->addChild(*m_overlayCompositingLayer);
}

void CompositingCoordinator::sizeDidChange(const IntSize& newSize)
{
    m_rootLayer->setSize(newSize);
}

bool CompositingCoordinator::flushPendingLayerChanges(OptionSet<FinalizeRenderingUpdateFlags> flags)
{
    TraceScope traceScope(BackingStoreFlushStart, BackingStoreFlushEnd);
    SetForScope protector(m_isFlushingLayerChanges, true);

    bool shouldSyncFrame = initializeRootCompositingLayerIfNeeded();

    m_page.updateRendering();
    m_page.flushPendingEditorStateUpdate();

    WTFBeginSignpost(this, FlushRootCompositingLayer);
    m_rootLayer->flushCompositingStateForThisLayerOnly();
    m_client.didFlushRootLayer(m_visibleContentsRect);
    WTFEndSignpost(this, FlushRootCompositingLayer);

    if (m_overlayCompositingLayer)
        m_overlayCompositingLayer->flushCompositingState(FloatRect(FloatPoint(), m_rootLayer->size()));

    m_page.finalizeRenderingUpdate(flags);

    WTFBeginSignpost(this, FinalizeCompositingStateFlush);
    auto& coordinatedLayer = downcast<CoordinatedGraphicsLayer>(*m_rootLayer);
    auto [performLayerSync, platformLayerUpdated] = coordinatedLayer.finalizeCompositingStateFlush();
    shouldSyncFrame |= performLayerSync;
    shouldSyncFrame |= m_forceFrameSync;
    WTFEndSignpost(this, FinalizeCompositingStateFlush);

    if (shouldSyncFrame) {
        WTFBeginSignpost(this, SyncFrame);

        m_nicosia.scene->accessState(
            [this](Nicosia::Scene::State& state)
            {
                bool platformLayerUpdated = false;
                for (auto& compositionLayer : m_nicosia.state.layers) {
                    compositionLayer->flushState(
                        [&platformLayerUpdated]
                        (const Nicosia::CompositionLayer::LayerState& state)
                        {
                            if (state.backingStore)
                                state.backingStore->flushUpdate();

                            if (state.imageBacking)
                                state.imageBacking->flushUpdate();

                            if (state.contentLayer)
                                platformLayerUpdated |= state.contentLayer->flushUpdate();
                        });
                }

                ++state.id;
                state.platformLayerUpdated = platformLayerUpdated;
                state.layers = m_nicosia.state.layers;
                state.rootLayer = m_nicosia.state.rootLayer;
            });

        m_client.commitSceneState(m_nicosia.scene);
        m_forceFrameSync = false;

        WTFEndSignpost(this, SyncFrame);
    }
#if HAVE(DISPLAY_LINK)
    else if (platformLayerUpdated)
        m_client.commitSceneState(nullptr);
#endif

    m_page.didUpdateRendering();

    // Eject any backing stores whose only reference is held in the HashMap cache.
    m_imageBackingStores.removeIf(
        [](auto& it) {
            return it.value->hasOneRef();
        });

    return true;
}

double CompositingCoordinator::timestamp() const
{
    auto* localMainFrame = dynamicDowncast<WebCore::LocalFrame>(m_page.corePage()->mainFrame());
    auto* document = localMainFrame ? localMainFrame->document() : nullptr;
    if (!document)
        return 0;
    return document->domWindow() ? document->domWindow()->nowTimestamp().seconds() : document->monotonicTimestamp();
}

void CompositingCoordinator::syncDisplayState()
{
    if (auto* localMainFrame = dynamicDowncast<WebCore::LocalFrame>(m_page.corePage()->mainFrame()))
        localMainFrame->view()->updateLayoutAndStyleIfNeededRecursive();
}

double CompositingCoordinator::nextAnimationServiceTime() const
{
    // According to the requestAnimationFrame spec, rAF callbacks should not be faster than 60FPS.
    static const double MinimalTimeoutForAnimations = 1. / 60.;
    return std::max<double>(0., MinimalTimeoutForAnimations - timestamp() + m_lastAnimationServiceTime);
}

bool CompositingCoordinator::initializeRootCompositingLayerIfNeeded()
{
    if (m_didInitializeRootCompositingLayer)
        return false;

    auto& rootLayer = downcast<CoordinatedGraphicsLayer>(*m_rootLayer);
    m_nicosia.state.rootLayer = rootLayer.compositionLayer();
    m_didInitializeRootCompositingLayer = true;
    return true;
}

Ref<GraphicsLayer> CompositingCoordinator::createGraphicsLayer(GraphicsLayer::Type layerType, GraphicsLayerClient& client)
{
    auto layer = adoptRef(*new CoordinatedGraphicsLayer(layerType, client));
    layer->setCoordinatorIncludingSubLayersIfNeeded(this);
    return layer;
}

FloatRect CompositingCoordinator::visibleContentsRect() const
{
    return m_visibleContentsRect;
}

void CompositingCoordinator::setVisibleContentsRect(const FloatRect& rect)
{
    bool contentsRectDidChange = rect != m_visibleContentsRect;
    if (contentsRectDidChange) {
        m_visibleContentsRect = rect;

        for (auto& registeredLayer : m_registeredLayers.values())
            registeredLayer->setNeedsVisibleRectAdjustment();
    }

    auto* localMainFrame = dynamicDowncast<WebCore::LocalFrame>(m_page.corePage()->mainFrame());
    auto* view = localMainFrame ? localMainFrame->view() : nullptr;
    if (view->useFixedLayout() && contentsRectDidChange) {
        // Round the rect instead of enclosing it to make sure that its size stays
        // the same while panning. This can have nasty effects on layout.
        view->setFixedVisibleContentRect(roundedIntRect(rect));
    }
}

void CompositingCoordinator::detachLayer(CoordinatedGraphicsLayer* layer)
{
    if (m_isPurging)
        return;

    {
        auto& compositionLayer = layer->compositionLayer();
        m_nicosia.state.layers.remove(compositionLayer);
        compositionLayer->setSceneIntegration(nullptr);
    }
    m_registeredLayers.remove(layer->id());
    notifyFlushRequired(layer);
}

void CompositingCoordinator::attachLayer(CoordinatedGraphicsLayer* layer)
{
    {
        auto& compositionLayer = layer->compositionLayer();
        m_nicosia.state.layers.add(compositionLayer);
        compositionLayer->setSceneIntegration(m_nicosia.sceneIntegration.copyRef());
    }
    m_registeredLayers.add(layer->id(), layer);
    layer->setNeedsVisibleRectAdjustment();
    notifyFlushRequired(layer);
}

void CompositingCoordinator::purgeBackingStores()
{
    SetForScope purgingToggle(m_isPurging, true);

    for (auto& registeredLayer : m_registeredLayers.values())
        registeredLayer->purgeBackingStores();
}

#if USE(CAIRO)
Nicosia::PaintingEngine& CompositingCoordinator::paintingEngine()
{
    return *m_paintingEngine;
}
#endif

RefPtr<Nicosia::ImageBackingStore> CompositingCoordinator::imageBackingStore(uint64_t nativeImageID, Function<RefPtr<Nicosia::Buffer>()> createBuffer)
{
    auto addResult = m_imageBackingStores.ensure(nativeImageID,
        [&] {
            auto store = adoptRef(*new Nicosia::ImageBackingStore);
            store->backingStoreState().buffer = createBuffer();
            return store;
        });
    return addResult.iterator->value.copyRef();
}

void CompositingCoordinator::requestUpdate()
{
    m_client.updateScene();
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
