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

#include "CoordinatedBackingStore.h"
#include <WebCore/NicosiaBuffer.h>
#include <WebCore/TextureMapper.h>
#include <WebCore/TextureMapperBackingStore.h>
#include <WebCore/TextureMapperGL.h>
#include <WebCore/TextureMapperLayer.h>
#include <wtf/Atomics.h>

namespace WebKit {
using namespace WebCore;

void CoordinatedGraphicsScene::dispatchOnMainThread(Function<void()>&& function)
{
    if (RunLoop::isMain()) {
        function();
        return;
    }

    RunLoop::main().dispatch([protectedThis = makeRef(*this), function = WTFMove(function)] {
        function();
    });
}

void CoordinatedGraphicsScene::dispatchOnClientRunLoop(Function<void()>&& function)
{
    if (&m_clientRunLoop == &RunLoop::current()) {
        function();
        return;
    }

    m_clientRunLoop.dispatch([protectedThis = makeRef(*this), function = WTFMove(function)] {
        function();
    });
}

static bool layerShouldHaveBackingStore(TextureMapperLayer* layer)
{
    return layer->drawsContent() && layer->contentsAreVisible() && !layer->size().isEmpty();
}

CoordinatedGraphicsScene::CoordinatedGraphicsScene(CoordinatedGraphicsSceneClient* client)
    : m_client(client)
    , m_isActive(false)
    , m_rootLayerID(InvalidCoordinatedLayerID)
    , m_viewBackgroundColor(Color::white)
    , m_clientRunLoop(RunLoop::current())
{
}

CoordinatedGraphicsScene::~CoordinatedGraphicsScene()
{
}

void CoordinatedGraphicsScene::applyStateChanges(const Vector<CoordinatedGraphicsState>& states)
{
    if (!m_textureMapper) {
        m_textureMapper = TextureMapper::create();
        static_cast<TextureMapperGL*>(m_textureMapper.get())->setEnableEdgeDistanceAntialiasing(true);
    }

    ensureRootLayer();

    for (auto& state : states)
        commitSceneState(state);
}

void CoordinatedGraphicsScene::paintToCurrentGLContext(const TransformationMatrix& matrix, float opacity, const FloatRect& clipRect, const Color& backgroundColor, bool drawsBackground, const FloatPoint& contentPosition, TextureMapper::PaintFlags PaintFlags)
{
    adjustPositionForFixedLayers(contentPosition);
    TextureMapperLayer* currentRootLayer = rootLayer();
    if (!currentRootLayer)
        return;

#if USE(COORDINATED_GRAPHICS_THREADED)
    for (auto& proxy : m_platformLayerProxies.values())
        proxy->swapBuffer();
#endif

    currentRootLayer->setTextureMapper(m_textureMapper.get());
    currentRootLayer->applyAnimationsRecursively();
    m_textureMapper->beginPainting(PaintFlags);
    m_textureMapper->beginClip(TransformationMatrix(), clipRect);

    if (drawsBackground) {
        RGBA32 rgba = makeRGBA32FromFloats(backgroundColor.red(),
            backgroundColor.green(), backgroundColor.blue(),
            backgroundColor.alpha() * opacity);
        m_textureMapper->drawSolidColor(clipRect, TransformationMatrix(), Color(rgba));
    } else
        m_textureMapper->clearColor(m_viewBackgroundColor);

    if (currentRootLayer->opacity() != opacity || currentRootLayer->transform() != matrix) {
        currentRootLayer->setOpacity(opacity);
        currentRootLayer->setTransform(matrix);
    }

    currentRootLayer->paint();
    m_fpsCounter.updateFPSAndDisplay(*m_textureMapper, clipRect.location(), matrix);
    m_textureMapper->endClip();
    m_textureMapper->endPainting();

    if (currentRootLayer->descendantsOrSelfHaveRunningAnimations())
        updateViewport();
}

void CoordinatedGraphicsScene::updateViewport()
{
    if (m_client)
        m_client->updateViewport();
}

void CoordinatedGraphicsScene::adjustPositionForFixedLayers(const FloatPoint& contentPosition)
{
    if (m_fixedLayers.isEmpty())
        return;

    // Fixed layer positions are updated by the web process when we update the visible contents rect / scroll position.
    // If we want those layers to follow accurately the viewport when we move between the web process updates, we have to offset
    // them by the delta between the current position and the position of the viewport used for the last layout.
    FloatSize delta = contentPosition - m_renderedContentsScrollPosition;

    for (auto& fixedLayer : m_fixedLayers.values())
        fixedLayer->setScrollPositionDeltaIfNeeded(delta);
}

void CoordinatedGraphicsScene::syncPlatformLayerIfNeeded(TextureMapperLayer* layer, const CoordinatedGraphicsLayerState& state)
{
#if USE(COORDINATED_GRAPHICS_THREADED)
    if (!state.platformLayerChanged)
        return;

    if (state.platformLayerProxy) {
        m_platformLayerProxies.set(layer, state.platformLayerProxy);
        state.platformLayerProxy->activateOnCompositingThread(this, layer);
    } else
        m_platformLayerProxies.remove(layer);
#else
    UNUSED_PARAM(layer);
    UNUSED_PARAM(state);
#endif
}

#if USE(COORDINATED_GRAPHICS_THREADED)
void CoordinatedGraphicsScene::onNewBufferAvailable()
{
    updateViewport();
}

TextureMapperGL* CoordinatedGraphicsScene::texmapGL()
{
    if (!m_textureMapper)
        return nullptr;

    return static_cast<TextureMapperGL*>(m_textureMapper.get());
}
#endif

void CoordinatedGraphicsScene::setLayerRepaintCountIfNeeded(TextureMapperLayer* layer, const CoordinatedGraphicsLayerState& state)
{
    if (!layer->isShowingRepaintCounter() || !state.repaintCountChanged)
        return;

    layer->setRepaintCount(state.repaintCount);
}

void CoordinatedGraphicsScene::setLayerChildrenIfNeeded(TextureMapperLayer* layer, const CoordinatedGraphicsLayerState& state)
{
    if (!state.childrenChanged)
        return;

    Vector<TextureMapperLayer*> children;
    children.reserveCapacity(state.children.size());
    for (auto& child : state.children)
        children.append(layerByID(child));

    layer->setChildren(children);
}

void CoordinatedGraphicsScene::setLayerFiltersIfNeeded(TextureMapperLayer* layer, const CoordinatedGraphicsLayerState& state)
{
    if (!state.filtersChanged)
        return;

    layer->setFilters(state.filters);
}

void CoordinatedGraphicsScene::setLayerState(CoordinatedLayerID id, const CoordinatedGraphicsLayerState& layerState)
{
    ASSERT(m_rootLayerID != InvalidCoordinatedLayerID);
    TextureMapperLayer* layer = layerByID(id);

    if (layerState.positionChanged)
        layer->setPosition(layerState.pos);

    if (layerState.anchorPointChanged)
        layer->setAnchorPoint(layerState.anchorPoint);

    if (layerState.sizeChanged)
        layer->setSize(layerState.size);

    if (layerState.transformChanged)
        layer->setTransform(layerState.transform);

    if (layerState.childrenTransformChanged)
        layer->setChildrenTransform(layerState.childrenTransform);

    if (layerState.contentsRectChanged)
        layer->setContentsRect(layerState.contentsRect);

    if (layerState.contentsTilingChanged) {
        layer->setContentsTilePhase(layerState.contentsTilePhase);
        layer->setContentsTileSize(layerState.contentsTileSize);
    }

    if (layerState.opacityChanged)
        layer->setOpacity(layerState.opacity);

    if (layerState.solidColorChanged)
        layer->setSolidColor(layerState.solidColor);

    if (layerState.debugVisualsChanged)
        layer->setDebugVisuals(layerState.debugVisuals.showDebugBorders, layerState.debugVisuals.debugBorderColor, layerState.debugVisuals.debugBorderWidth, layerState.debugVisuals.showRepaintCounter);

    if (layerState.replicaChanged)
        layer->setReplicaLayer(getLayerByIDIfExists(layerState.replica));

    if (layerState.maskChanged)
        layer->setMaskLayer(getLayerByIDIfExists(layerState.mask));

    if (layerState.imageChanged)
        assignImageBackingToLayer(layer, layerState.imageID);

    if (layerState.flagsChanged) {
        layer->setContentsOpaque(layerState.contentsOpaque);
        layer->setDrawsContent(layerState.drawsContent);
        layer->setContentsVisible(layerState.contentsVisible);
        layer->setBackfaceVisibility(layerState.backfaceVisible);

        // Never clip the root layer.
        layer->setMasksToBounds(id == m_rootLayerID ? false : layerState.masksToBounds);
        layer->setPreserves3D(layerState.preserves3D);

        bool fixedToViewportChanged = layer->fixedToViewport() != layerState.fixedToViewport;
        layer->setFixedToViewport(layerState.fixedToViewport);
        if (fixedToViewportChanged) {
            if (layerState.fixedToViewport)
                m_fixedLayers.add(id, layer);
            else
                m_fixedLayers.remove(id);
        }

        layer->setIsScrollable(layerState.isScrollable);
    }

    if (layerState.committedScrollOffsetChanged)
        layer->didCommitScrollOffset(layerState.committedScrollOffset);

    prepareContentBackingStore(layer);

    // Apply Operations.
    setLayerChildrenIfNeeded(layer, layerState);
    createTilesIfNeeded(layer, layerState);
    removeTilesIfNeeded(layer, layerState);
    updateTilesIfNeeded(layer, layerState);
    setLayerFiltersIfNeeded(layer, layerState);
    setLayerAnimationsIfNeeded(layer, layerState);
    syncPlatformLayerIfNeeded(layer, layerState);
    setLayerRepaintCountIfNeeded(layer, layerState);
}

TextureMapperLayer* CoordinatedGraphicsScene::getLayerByIDIfExists(CoordinatedLayerID id)
{
    return (id != InvalidCoordinatedLayerID) ? layerByID(id) : 0;
}

void CoordinatedGraphicsScene::createLayers(const Vector<CoordinatedLayerID>& layerIDs)
{
    for (auto& layerID : layerIDs)
        createLayer(layerID);
}

void CoordinatedGraphicsScene::createLayer(CoordinatedLayerID id)
{
    std::unique_ptr<TextureMapperLayer> newLayer = std::make_unique<TextureMapperLayer>();
    newLayer->setID(id);
    newLayer->setScrollClient(this);
    m_layers.add(id, WTFMove(newLayer));
}

void CoordinatedGraphicsScene::deleteLayers(const Vector<CoordinatedLayerID>& layerIDs)
{
    for (auto& layerID : layerIDs)
        deleteLayer(layerID);
}

void CoordinatedGraphicsScene::deleteLayer(CoordinatedLayerID layerID)
{
    std::unique_ptr<TextureMapperLayer> layer = m_layers.take(layerID);
    ASSERT(layer);

    m_backingStores.remove(layer.get());
    m_fixedLayers.remove(layerID);
#if USE(COORDINATED_GRAPHICS_THREADED)
    if (auto platformLayerProxy = m_platformLayerProxies.take(layer.get()))
        platformLayerProxy->invalidate();
#endif
}

void CoordinatedGraphicsScene::setRootLayerID(CoordinatedLayerID layerID)
{
    ASSERT(layerID != InvalidCoordinatedLayerID);
    ASSERT(m_rootLayerID == InvalidCoordinatedLayerID);

    m_rootLayerID = layerID;

    TextureMapperLayer* layer = layerByID(layerID);
    ASSERT(m_rootLayer->children().isEmpty());
    m_rootLayer->addChild(layer);
}

void CoordinatedGraphicsScene::prepareContentBackingStore(TextureMapperLayer* layer)
{
    if (!layerShouldHaveBackingStore(layer)) {
        removeBackingStoreIfNeeded(layer);
        return;
    }

    createBackingStoreIfNeeded(layer);
    resetBackingStoreSizeToLayerSize(layer);
}

void CoordinatedGraphicsScene::createBackingStoreIfNeeded(TextureMapperLayer* layer)
{
    if (m_backingStores.contains(layer))
        return;

    auto backingStore = CoordinatedBackingStore::create();
    m_backingStores.add(layer, backingStore.copyRef());
    layer->setBackingStore(WTFMove(backingStore));
}

void CoordinatedGraphicsScene::removeBackingStoreIfNeeded(TextureMapperLayer* layer)
{
    RefPtr<CoordinatedBackingStore> backingStore = m_backingStores.take(layer);
    if (!backingStore)
        return;

    layer->setBackingStore(nullptr);
}

void CoordinatedGraphicsScene::resetBackingStoreSizeToLayerSize(TextureMapperLayer* layer)
{
    RefPtr<CoordinatedBackingStore> backingStore = m_backingStores.get(layer);
    ASSERT(backingStore);
    backingStore->setSize(layer->size());
    m_backingStoresWithPendingBuffers.add(backingStore);
}

void CoordinatedGraphicsScene::createTilesIfNeeded(TextureMapperLayer* layer, const CoordinatedGraphicsLayerState& state)
{
    if (state.tilesToCreate.isEmpty())
        return;

    RefPtr<CoordinatedBackingStore> backingStore = m_backingStores.get(layer);
    ASSERT(backingStore || !layerShouldHaveBackingStore(layer));
    if (!backingStore)
        return;

    for (auto& tile : state.tilesToCreate)
        backingStore->createTile(tile.tileID, tile.scale);
}

void CoordinatedGraphicsScene::removeTilesIfNeeded(TextureMapperLayer* layer, const CoordinatedGraphicsLayerState& state)
{
    if (state.tilesToRemove.isEmpty())
        return;

    RefPtr<CoordinatedBackingStore> backingStore = m_backingStores.get(layer);
    if (!backingStore)
        return;

    for (auto& tile : state.tilesToRemove)
        backingStore->removeTile(tile);

    m_backingStoresWithPendingBuffers.add(backingStore);
}

void CoordinatedGraphicsScene::updateTilesIfNeeded(TextureMapperLayer* layer, const CoordinatedGraphicsLayerState& state)
{
    if (state.tilesToUpdate.isEmpty())
        return;

    RefPtr<CoordinatedBackingStore> backingStore = m_backingStores.get(layer);
    ASSERT(backingStore || !layerShouldHaveBackingStore(layer));
    if (!backingStore)
        return;

    for (auto& tile : state.tilesToUpdate) {
        const SurfaceUpdateInfo& surfaceUpdateInfo = tile.updateInfo;

        auto surfaceIt = m_surfaces.find(surfaceUpdateInfo.atlasID);
        ASSERT(surfaceIt != m_surfaces.end());

        backingStore->updateTile(tile.tileID, surfaceUpdateInfo.updateRect, tile.tileRect, surfaceIt->value.copyRef(), surfaceUpdateInfo.surfaceOffset);
        m_backingStoresWithPendingBuffers.add(backingStore);
    }
}

void CoordinatedGraphicsScene::syncUpdateAtlases(const CoordinatedGraphicsState& state)
{
    for (auto& atlas : state.updateAtlasesToCreate)
        createUpdateAtlas(atlas.first, atlas.second.copyRef());
}

void CoordinatedGraphicsScene::createUpdateAtlas(uint32_t atlasID, RefPtr<Nicosia::Buffer>&& buffer)
{
    ASSERT(!m_surfaces.contains(atlasID));
    m_surfaces.add(atlasID, WTFMove(buffer));
}

void CoordinatedGraphicsScene::removeUpdateAtlas(uint32_t atlasID)
{
    ASSERT(m_surfaces.contains(atlasID));
    m_surfaces.remove(atlasID);
}

void CoordinatedGraphicsScene::releaseUpdateAtlases(const Vector<uint32_t>& atlasesToRemove)
{
    for (auto& atlas : atlasesToRemove)
        removeUpdateAtlas(atlas);
}

void CoordinatedGraphicsScene::syncImageBackings(const CoordinatedGraphicsState& state)
{
    for (auto& image : state.imagesToRemove)
        removeImageBacking(image);

    for (auto& image : state.imagesToCreate)
        createImageBacking(image);

    for (auto& image : state.imagesToUpdate)
        updateImageBacking(image.first, image.second.copyRef());

    for (auto& image : state.imagesToClear)
        clearImageBackingContents(image);
}

void CoordinatedGraphicsScene::createImageBacking(CoordinatedImageBackingID imageID)
{
    ASSERT(!m_imageBackings.contains(imageID));
    m_imageBackings.add(imageID, CoordinatedBackingStore::create());
}

void CoordinatedGraphicsScene::updateImageBacking(CoordinatedImageBackingID imageID, RefPtr<Nicosia::Buffer>&& buffer)
{
    ASSERT(m_imageBackings.contains(imageID));
    auto it = m_imageBackings.find(imageID);
    RefPtr<CoordinatedBackingStore> backingStore = it->value;

    // CoordinatedImageBacking is realized to CoordinatedBackingStore with only one tile in UI Process.
    backingStore->createTile(1 /* id */, 1 /* scale */);
    IntRect rect(IntPoint::zero(), buffer->size());
    // See CoordinatedGraphicsLayer::shouldDirectlyCompositeImage()
    ASSERT(2000 >= std::max(rect.width(), rect.height()));
    backingStore->setSize(rect.size());
    backingStore->updateTile(1 /* id */, rect, rect, WTFMove(buffer), rect.location());

    m_backingStoresWithPendingBuffers.add(backingStore);
}

void CoordinatedGraphicsScene::clearImageBackingContents(CoordinatedImageBackingID imageID)
{
    ASSERT(m_imageBackings.contains(imageID));
    auto it = m_imageBackings.find(imageID);
    RefPtr<CoordinatedBackingStore> backingStore = it->value;
    backingStore->removeAllTiles();
    m_backingStoresWithPendingBuffers.add(backingStore);
}

void CoordinatedGraphicsScene::removeImageBacking(CoordinatedImageBackingID imageID)
{
    ASSERT(m_imageBackings.contains(imageID));

    // We don't want TextureMapperLayer refers a dangling pointer.
    m_releasedImageBackings.append(m_imageBackings.take(imageID));
}

void CoordinatedGraphicsScene::assignImageBackingToLayer(TextureMapperLayer* layer, CoordinatedImageBackingID imageID)
{
    if (imageID == InvalidCoordinatedImageBackingID) {
        layer->setContentsLayer(0);
        return;
    }

    auto it = m_imageBackings.find(imageID);
    ASSERT(it != m_imageBackings.end());
    layer->setContentsLayer(it->value.get());
}

void CoordinatedGraphicsScene::removeReleasedImageBackingsIfNeeded()
{
    m_releasedImageBackings.clear();
}

void CoordinatedGraphicsScene::commitPendingBackingStoreOperations()
{
    for (auto& backingStore : m_backingStoresWithPendingBuffers)
        backingStore->commitTileOperations(*m_textureMapper);

    m_backingStoresWithPendingBuffers.clear();
}

void CoordinatedGraphicsScene::commitSceneState(const CoordinatedGraphicsState& state)
{
    if (!m_client)
        return;

    m_renderedContentsScrollPosition = state.scrollPosition;

    createLayers(state.layersToCreate);
    deleteLayers(state.layersToRemove);

    if (state.rootCompositingLayer != m_rootLayerID)
        setRootLayerID(state.rootCompositingLayer);

    syncImageBackings(state);
    syncUpdateAtlases(state);

    for (auto& layer : state.layersToUpdate)
        setLayerState(layer.first, layer.second);

    commitPendingBackingStoreOperations();
    removeReleasedImageBackingsIfNeeded();
}

void CoordinatedGraphicsScene::renderNextFrame()
{
    if (!m_client)
        return;
    dispatchOnMainThread([this] {
        if (m_client)
            m_client->renderNextFrame();
    });
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

    m_imageBackings.clear();
    m_releasedImageBackings.clear();
#if USE(COORDINATED_GRAPHICS_THREADED)
    for (auto& proxy : m_platformLayerProxies.values())
        proxy->invalidate();
    m_platformLayerProxies.clear();
#endif
    m_surfaces.clear();

    m_rootLayer = nullptr;
    m_rootLayerID = InvalidCoordinatedLayerID;
    m_layers.clear();
    m_fixedLayers.clear();
    m_textureMapper = nullptr;
    m_backingStores.clear();
    m_backingStoresWithPendingBuffers.clear();
}

void CoordinatedGraphicsScene::commitScrollOffset(uint32_t layerID, const IntSize& offset)
{
    if (!m_client)
        return;
    dispatchOnMainThread([this, layerID, offset] {
        if (m_client)
            m_client->commitScrollOffset(layerID, offset);
    });
}

void CoordinatedGraphicsScene::setLayerAnimationsIfNeeded(TextureMapperLayer* layer, const CoordinatedGraphicsLayerState& state)
{
    if (!state.animationsChanged)
        return;

    layer->setAnimations(state.animations);
}

void CoordinatedGraphicsScene::detach()
{
    ASSERT(RunLoop::isMain());
    m_isActive = false;
    m_client = nullptr;
}

void CoordinatedGraphicsScene::setActive(bool active)
{
    if (!m_client || m_isActive == active)
        return;

    m_isActive = active;
    if (m_isActive)
        renderNextFrame();
}

TextureMapperLayer* CoordinatedGraphicsScene::findScrollableContentsLayerAt(const FloatPoint& point)
{
    return rootLayer() ? rootLayer()->findScrollableContentsLayerAt(point) : 0;
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
