/*
    Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2012 Company 100, Inc.
    Copyright (C) 2017 Sony Interactive Entertainment Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "CoordinatedGraphicsScene.h"

#if USE(COORDINATED_GRAPHICS)

#include <WebCore/CoordinatedBackingStore.h>
#include <WebCore/NicosiaBackingStoreTextureMapperImpl.h>
#include <WebCore/NicosiaBuffer.h>
#include <WebCore/NicosiaCompositionLayerTextureMapperImpl.h>
#include <WebCore/NicosiaContentLayerTextureMapperImpl.h>
#include <WebCore/NicosiaImageBackingTextureMapperImpl.h>
#include <WebCore/NicosiaScene.h>
#include <WebCore/TextureMapper.h>
#include <WebCore/TextureMapperBackingStore.h>
#include <WebCore/TextureMapperGL.h>
#include <WebCore/TextureMapperLayer.h>
#include <wtf/Atomics.h>

namespace WebKit {
using namespace WebCore;

static bool layerShouldHaveBackingStore(TextureMapperLayer* layer)
{
    return layer->drawsContent() && layer->contentsAreVisible() && !layer->size().isEmpty();
}

CoordinatedGraphicsScene::CoordinatedGraphicsScene(CoordinatedGraphicsSceneClient* client)
    : m_client(client)
{
}

CoordinatedGraphicsScene::~CoordinatedGraphicsScene() = default;

void CoordinatedGraphicsScene::applyStateChanges(const Vector<CoordinatedGraphicsState>& states)
{
    if (!m_textureMapper) {
        m_textureMapper = TextureMapper::create();
        static_cast<TextureMapperGL*>(m_textureMapper.get())->setEnableEdgeDistanceAntialiasing(true);
    }

    ensureRootLayer();

    for (auto& state : states)
        commitSceneState(state.nicosia);
}

void CoordinatedGraphicsScene::paintToCurrentGLContext(const TransformationMatrix& matrix, const FloatRect& clipRect, TextureMapper::PaintFlags PaintFlags)
{
    updateSceneState();

    TextureMapperLayer* currentRootLayer = rootLayer();
    if (!currentRootLayer)
        return;

    currentRootLayer->setTextureMapper(m_textureMapper.get());
    bool sceneHasRunningAnimations = currentRootLayer->applyAnimationsRecursively(MonotonicTime::now());
    m_textureMapper->beginPainting(PaintFlags);
    m_textureMapper->beginClip(TransformationMatrix(), clipRect);

    if (currentRootLayer->transform() != matrix)
        currentRootLayer->setTransform(matrix);

    currentRootLayer->paint();
    m_fpsCounter.updateFPSAndDisplay(*m_textureMapper, clipRect.location(), matrix);
    m_textureMapper->endClip();
    m_textureMapper->endPainting();

    if (sceneHasRunningAnimations)
        updateViewport();
}

void CoordinatedGraphicsScene::updateViewport()
{
    if (m_client)
        m_client->updateViewport();
}

#if USE(COORDINATED_GRAPHICS_THREADED)
void CoordinatedGraphicsScene::onNewBufferAvailable()
{
    updateViewport();
}
#endif

Nicosia::CompositionLayerTextureMapperImpl& compositionLayerImpl(Nicosia::CompositionLayer& compositionLayer)
{
    return downcast<Nicosia::CompositionLayerTextureMapperImpl>(compositionLayer.impl());
}

Nicosia::ContentLayerTextureMapperImpl& contentLayerImpl(Nicosia::ContentLayer& contentLayer)
{
    return downcast<Nicosia::ContentLayerTextureMapperImpl>(contentLayer.impl());
}

Nicosia::BackingStoreTextureMapperImpl& backingStoreImpl(Nicosia::BackingStore& backingStore)
{
    return downcast<Nicosia::BackingStoreTextureMapperImpl>(backingStore.impl());
}

Nicosia::ImageBackingTextureMapperImpl& imageBackingImpl(Nicosia::ImageBacking& imageBacking)
{
    return downcast<Nicosia::ImageBackingTextureMapperImpl>(imageBacking.impl());
}

TextureMapperLayer& texmapLayer(Nicosia::CompositionLayer& compositionLayer)
{
    auto& compositionState = compositionLayerImpl(compositionLayer).compositionState();
    if (!compositionState.layer) {
        compositionState.layer = std::make_unique<TextureMapperLayer>();
        compositionState.layer->setID(compositionLayer.id());
    }
    return *compositionState.layer;
}

void updateBackingStore(TextureMapperLayer& layer,
    Nicosia::BackingStoreTextureMapperImpl::CompositionState& compositionState,
    const Nicosia::BackingStoreTextureMapperImpl::TileUpdate& update)
{
    if (!layerShouldHaveBackingStore(&layer)) {
        layer.setBackingStore(nullptr);
        compositionState.backingStore = nullptr;
        return;
    }

    if (!compositionState.backingStore)
        compositionState.backingStore = CoordinatedBackingStore::create();
    auto& backingStore = *compositionState.backingStore;

    layer.setBackingStore(&backingStore);
    backingStore.setSize(layer.size());

    for (auto& tile : update.tilesToCreate)
        backingStore.createTile(tile.tileID, tile.scale);
    for (auto& tile : update.tilesToRemove)
        backingStore.removeTile(tile.tileID);
    for (auto& tile : update.tilesToUpdate) {
        backingStore.updateTile(tile.tileID, tile.updateInfo.updateRect,
            tile.tileRect, tile.updateInfo.buffer.copyRef(), { 0, 0 });
    }
}

void updateImageBacking(TextureMapperLayer& layer,
    Nicosia::ImageBackingTextureMapperImpl::CompositionState& compositionState,
    Nicosia::ImageBackingTextureMapperImpl::Update& update)
{
    if (!update.isVisible) {
        layer.setBackingStore(nullptr);
        return;
    }

    if (!compositionState.backingStore)
        compositionState.backingStore = CoordinatedBackingStore::create();
    auto& backingStore = *compositionState.backingStore;
    layer.setContentsLayer(&backingStore);

    if (!update.buffer)
        return;

    backingStore.createTile(1, 1.0);
    WebCore::IntRect rect { { }, update.buffer->size() };
    ASSERT(2000 >= std::max(rect.width(), rect.height()));
    backingStore.setSize(rect.size());
    backingStore.updateTile(1, rect, rect, WTFMove(update.buffer), rect.location());
}

void removeLayer(Nicosia::CompositionLayer& layer)
{
    layer.accessCommitted(
        [](const Nicosia::CompositionLayer::LayerState& committed)
        {
            if (committed.backingStore) {
                auto& compositionState = backingStoreImpl(*committed.backingStore).compositionState();
                compositionState.backingStore = nullptr;
            }

            if (committed.contentLayer)
                contentLayerImpl(*committed.contentLayer).proxy().invalidate();

            if (committed.imageBacking) {
                auto& compositionState = imageBackingImpl(*committed.imageBacking).compositionState();
                compositionState.backingStore = nullptr;
            }
        });

    auto& compositionState = compositionLayerImpl(layer).compositionState();
    compositionState.layer = nullptr;
}

void CoordinatedGraphicsScene::commitSceneState(const CoordinatedGraphicsState::NicosiaState& state)
{
    if (!m_client)
        return;

    m_nicosia.scene = state.scene;
}

void CoordinatedGraphicsScene::updateSceneState()
{
    if (!m_nicosia.scene)
        return;

    // Store layer and impl references along with the corresponding update
    // for each type of possible layer backing.
    struct {
        struct BackingStore {
            std::reference_wrapper<TextureMapperLayer> layer;
            std::reference_wrapper<Nicosia::BackingStoreTextureMapperImpl> backingStore;
            Nicosia::BackingStoreTextureMapperImpl::TileUpdate update;
        };
        Vector<BackingStore> backingStore;

        struct ContentLayer {
            std::reference_wrapper<TextureMapperLayer> layer;
            std::reference_wrapper<TextureMapperPlatformLayerProxy> proxy;
            bool needsActivation { false };
        };
        Vector<ContentLayer> contentLayer;

        struct ImageBacking {
            std::reference_wrapper<TextureMapperLayer> layer;
            std::reference_wrapper<Nicosia::ImageBackingTextureMapperImpl> imageBacking;
            Nicosia::ImageBackingTextureMapperImpl::Update update;
        };
        Vector<ImageBacking> imageBacking;
    } layersByBacking;

    // Access the scene state and perform state update for each layer.
    m_nicosia.scene->accessState(
        [this, &layersByBacking](Nicosia::Scene::State& state)
        {
            // FIXME: try to minimize the amount of work in case the Scene::State object
            // didn't change (i.e. no layer flush was done), but don't forget to properly
            // gather and update proxy objects for content layers.

            // Handle the root layer, adding it to the TextureMapperLayer tree
            // on the first update. No such change is expected later.
            {
                auto& rootLayer = texmapLayer(*state.rootLayer);
                if (rootLayer.id() != m_rootLayerID) {
                    m_rootLayerID = rootLayer.id();
                    RELEASE_ASSERT(m_rootLayer->children().isEmpty());
                    m_rootLayer->addChild(&rootLayer);
                }
            }

            // Gather all the to-be-removed layers so that composition-side state
            // can be properly purged after the current state's set of layers is adopted.
            HashSet<RefPtr<Nicosia::CompositionLayer>> removedLayers;
            for (auto& layer : m_nicosia.state.layers) {
                if (!state.layers.contains(layer))
                    removedLayers.add(layer);
            }

            m_nicosia.state = state;

            for (auto& layer : removedLayers)
                removeLayer(*layer);
            removedLayers = { };

            // Iterate the current state's set of layers, updating state values according to
            // the incoming state changes. Layer backings are stored so that the updates
            // (possibly time-consuming) can be done outside of this scene update.
            for (auto& compositionLayer : m_nicosia.state.layers) {
                auto& layer = texmapLayer(*compositionLayer);
                compositionLayer->commitState(
                    [this, &layer, &compositionLayer, &layersByBacking]
                    (const Nicosia::CompositionLayer::LayerState& layerState)
                    {
                        if (layerState.delta.positionChanged)
                            layer.setPosition(layerState.position);
                        if (layerState.delta.anchorPointChanged)
                            layer.setAnchorPoint(layerState.anchorPoint);
                        if (layerState.delta.sizeChanged)
                            layer.setSize(layerState.size);

                        if (layerState.delta.transformChanged)
                            layer.setTransform(layerState.transform);
                        if (layerState.delta.childrenTransformChanged)
                            layer.setChildrenTransform(layerState.childrenTransform);

                        if (layerState.delta.contentsRectChanged)
                            layer.setContentsRect(layerState.contentsRect);
                        if (layerState.delta.contentsTilingChanged) {
                            layer.setContentsTilePhase(layerState.contentsTilePhase);
                            layer.setContentsTileSize(layerState.contentsTileSize);
                        }

                        if (layerState.delta.opacityChanged)
                            layer.setOpacity(layerState.opacity);
                        if (layerState.delta.solidColorChanged)
                            layer.setSolidColor(layerState.solidColor);

                        if (layerState.delta.filtersChanged)
                            layer.setFilters(layerState.filters);
                        if (layerState.delta.animationsChanged)
                            layer.setAnimations(layerState.animations);

                        if (layerState.delta.childrenChanged) {
                            layer.setChildren(WTF::map(layerState.children,
                                [](auto& child) { return &texmapLayer(*child); }));
                        }

                        if (layerState.delta.maskChanged)
                            layer.setMaskLayer(layerState.mask ? &texmapLayer(*layerState.mask) : nullptr);
                        if (layerState.delta.replicaChanged)
                            layer.setReplicaLayer(layerState.replica ? &texmapLayer(*layerState.replica) : nullptr);

                        if (layerState.delta.flagsChanged) {
                            layer.setContentsOpaque(layerState.flags.contentsOpaque);
                            layer.setDrawsContent(layerState.flags.drawsContent);
                            layer.setContentsVisible(layerState.flags.contentsVisible);
                            layer.setBackfaceVisibility(layerState.flags.backfaceVisible);
                            layer.setMasksToBounds(layerState.flags.masksToBounds);
                            layer.setPreserves3D(layerState.flags.preserves3D);
                        }

                        if (layerState.delta.repaintCounterChanged)
                            layer.setRepaintCounter(layerState.repaintCounter.visible, layerState.repaintCounter.count);

                        if (layerState.delta.debugBorderChanged)
                            layer.setDebugVisuals(layerState.debugBorder.visible, layerState.debugBorder.color, layerState.debugBorder.width);

                        if (layerState.backingStore) {
                            auto& impl = backingStoreImpl(*layerState.backingStore);
                            layersByBacking.backingStore.append(
                                { std::ref(layer), std::ref(impl), impl.takeUpdate() });
                        } else
                            layer.setBackingStore(nullptr);

                        if (layerState.contentLayer) {
                            auto& impl = contentLayerImpl(*layerState.contentLayer);
                            layersByBacking.contentLayer.append(
                                { std::ref(layer), std::ref(impl.proxy()), layerState.delta.contentLayerChanged });
                        } else if (layerState.imageBacking) {
                            auto& impl = imageBackingImpl(*layerState.imageBacking);
                            layersByBacking.imageBacking.append(
                                { std::ref(layer), std::ref(impl), impl.takeUpdate() });
                        } else
                            layer.setContentsLayer(nullptr);
                    });
            }
        });

    // Iterate through each backing type of layers and gather backing store
    // or proxy objects that need an update.
    // FIXME: HashSet<std::reference_wrapper<>> would be ideal, but doesn't work (yet).
    HashSet<Ref<WebCore::CoordinatedBackingStore>> backingStoresWithPendingBuffers;
    HashSet<Ref<WebCore::TextureMapperPlatformLayerProxy>> proxiesForSwapping;

    {
        for (auto& entry : layersByBacking.backingStore) {
            auto& compositionState = entry.backingStore.get().compositionState();
            updateBackingStore(entry.layer.get(), compositionState, entry.update);

            if (compositionState.backingStore)
                backingStoresWithPendingBuffers.add(makeRef(*compositionState.backingStore));
        }

        layersByBacking.backingStore = { };
    }

    {
        for (auto& entry : layersByBacking.contentLayer) {
            auto& proxy = entry.proxy.get();
            if (entry.needsActivation)
                proxy.activateOnCompositingThread(this, &entry.layer.get());
            proxiesForSwapping.add(makeRef(proxy));
        }

        layersByBacking.contentLayer = { };
    }

    {
        for (auto& entry : layersByBacking.imageBacking) {
            auto& compositionState = entry.imageBacking.get().compositionState();
            updateImageBacking(entry.layer.get(), compositionState, entry.update);

            if (compositionState.backingStore)
                backingStoresWithPendingBuffers.add(makeRef(*compositionState.backingStore));
        }

        layersByBacking.imageBacking = { };
    }

    for (auto& backingStore : backingStoresWithPendingBuffers)
        backingStore->commitTileOperations(*m_textureMapper);

    for (auto& proxy : proxiesForSwapping)
        proxy->swapBuffer();
}

void CoordinatedGraphicsScene::ensureRootLayer()
{
    if (m_rootLayer)
        return;

    m_rootLayer = std::make_unique<TextureMapperLayer>();
    m_rootLayer->setMasksToBounds(false);
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setAnchorPoint(FloatPoint3D(0, 0, 0));

    // The root layer should not have zero size, or it would be optimized out.
    m_rootLayer->setSize(FloatSize(1.0, 1.0));

    ASSERT(m_textureMapper);
    m_rootLayer->setTextureMapper(m_textureMapper.get());
}

void CoordinatedGraphicsScene::purgeGLResources()
{
    ASSERT(!m_client);

    if (m_nicosia.scene) {
        m_nicosia.scene->accessState(
            [this](const Nicosia::Scene::State& state)
            {
                ASSERT(state.id == m_nicosia.state.id);
                for (auto& layer : m_nicosia.state.layers)
                    removeLayer(*layer);
                m_nicosia.state.layers = { };
                m_nicosia.state.rootLayer = nullptr;
            });
        m_nicosia.scene = nullptr;
    }

    m_rootLayer = nullptr;
    m_rootLayerID = 0;
    m_textureMapper = nullptr;
}

void CoordinatedGraphicsScene::detach()
{
    ASSERT(RunLoop::isMain());
    m_isActive = false;
    m_client = nullptr;
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
