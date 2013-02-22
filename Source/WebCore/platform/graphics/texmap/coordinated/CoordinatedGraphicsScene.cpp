/*
    Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2012 Company 100, Inc.

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

#if USE(COORDINATED_GRAPHICS)

#include "CoordinatedGraphicsScene.h"

#include "CoordinatedBackingStore.h"
#include "GraphicsLayerTextureMapper.h"
#include "TextureMapper.h"
#include "TextureMapperBackingStore.h"
#include "TextureMapperGL.h"
#include "TextureMapperLayer.h"
#include <OpenGLShims.h>
#include <wtf/Atomics.h>
#include <wtf/MainThread.h>

#if ENABLE(CSS_SHADERS)
#include "CoordinatedCustomFilterOperation.h"
#include "CoordinatedCustomFilterProgram.h"
#include "CustomFilterProgram.h"
#include "CustomFilterProgramInfo.h"
#endif

namespace WebCore {

void CoordinatedGraphicsScene::dispatchOnMainThread(const Function<void()>& function)
{
    if (isMainThread())
        function();
    else
        callOnMainThread(function);
}

static bool layerShouldHaveBackingStore(GraphicsLayer* layer)
{
    return layer->drawsContent() && layer->contentsAreVisible() && !layer->size().isEmpty();
}

CoordinatedGraphicsScene::CoordinatedGraphicsScene(CoordinatedGraphicsSceneClient* client)
    : m_client(client)
    , m_isActive(false)
    , m_rootLayerID(InvalidCoordinatedLayerID)
    , m_animationsLocked(false)
#if ENABLE(REQUEST_ANIMATION_FRAME)
    , m_animationFrameRequested(false)
#endif
    , m_backgroundColor(Color::white)
    , m_setDrawsBackground(false)
{
    ASSERT(isMainThread());
}

CoordinatedGraphicsScene::~CoordinatedGraphicsScene()
{
}

void CoordinatedGraphicsScene::paintToCurrentGLContext(const TransformationMatrix& matrix, float opacity, const FloatRect& clipRect, TextureMapper::PaintFlags PaintFlags)
{
    if (!m_textureMapper) {
        m_textureMapper = TextureMapper::create(TextureMapper::OpenGLMode);
        static_cast<TextureMapperGL*>(m_textureMapper.get())->setEnableEdgeDistanceAntialiasing(true);
    }

    ASSERT(m_textureMapper->accelerationMode() == TextureMapper::OpenGLMode);
    syncRemoteContent();

    adjustPositionForFixedLayers();
    GraphicsLayer* currentRootLayer = rootLayer();
    if (!currentRootLayer)
        return;

    TextureMapperLayer* layer = toTextureMapperLayer(currentRootLayer);

    if (!layer)
        return;

    layer->setTextureMapper(m_textureMapper.get());
    if (!m_animationsLocked)
        layer->applyAnimationsRecursively();
    m_textureMapper->beginPainting(PaintFlags);
    m_textureMapper->beginClip(TransformationMatrix(), clipRect);

    if (m_setDrawsBackground) {
        RGBA32 rgba = makeRGBA32FromFloats(m_backgroundColor.red(),
            m_backgroundColor.green(), m_backgroundColor.blue(),
            m_backgroundColor.alpha() * opacity);
        m_textureMapper->drawSolidColor(clipRect, TransformationMatrix(), Color(rgba));
    }

    if (currentRootLayer->opacity() != opacity || currentRootLayer->transform() != matrix) {
        currentRootLayer->setOpacity(opacity);
        currentRootLayer->setTransform(matrix);
        currentRootLayer->flushCompositingStateForThisLayerOnly();
    }

    layer->paint();
    m_fpsCounter.updateFPSAndDisplay(m_textureMapper.get(), clipRect.location(), matrix);
    m_textureMapper->endClip();
    m_textureMapper->endPainting();

    if (layer->descendantsOrSelfHaveRunningAnimations())
        dispatchOnMainThread(bind(&CoordinatedGraphicsScene::updateViewport, this));

#if ENABLE(REQUEST_ANIMATION_FRAME)
    if (m_animationFrameRequested) {
        m_animationFrameRequested = false;
        dispatchOnMainThread(bind(&CoordinatedGraphicsScene::animationFrameReady, this));
    }
#endif
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
void CoordinatedGraphicsScene::animationFrameReady()
{
    ASSERT(isMainThread());
    if (m_client)
        m_client->animationFrameReady();
}

void CoordinatedGraphicsScene::requestAnimationFrame()
{
    m_animationFrameRequested = true;
}
#endif

void CoordinatedGraphicsScene::paintToGraphicsContext(PlatformGraphicsContext* platformContext)
{
    if (!m_textureMapper)
        m_textureMapper = TextureMapper::create();
    ASSERT(m_textureMapper->accelerationMode() == TextureMapper::SoftwareMode);
    syncRemoteContent();
    TextureMapperLayer* layer = toTextureMapperLayer(rootLayer());

    if (!layer)
        return;

    GraphicsContext graphicsContext(platformContext);
    m_textureMapper->setGraphicsContext(&graphicsContext);
    m_textureMapper->beginPainting();

    IntRect clipRect = graphicsContext.clipBounds();
    if (m_setDrawsBackground)
        m_textureMapper->drawSolidColor(clipRect, TransformationMatrix(), m_backgroundColor);

    layer->paint();
    m_fpsCounter.updateFPSAndDisplay(m_textureMapper.get(), clipRect.location());
    m_textureMapper->endPainting();
    m_textureMapper->setGraphicsContext(0);
}

void CoordinatedGraphicsScene::setScrollPosition(const FloatPoint& scrollPosition)
{
    m_scrollPosition = scrollPosition;
}

void CoordinatedGraphicsScene::updateViewport()
{
    ASSERT(isMainThread());
    if (m_client)
        m_client->updateViewport();
}

void CoordinatedGraphicsScene::adjustPositionForFixedLayers()
{
    if (m_fixedLayers.isEmpty())
        return;

    // Fixed layer positions are updated by the web process when we update the visible contents rect / scroll position.
    // If we want those layers to follow accurately the viewport when we move between the web process updates, we have to offset
    // them by the delta between the current position and the position of the viewport used for the last layout.
    FloatSize delta = m_scrollPosition - m_renderedContentsScrollPosition;

    LayerRawPtrMap::iterator end = m_fixedLayers.end();
    for (LayerRawPtrMap::iterator it = m_fixedLayers.begin(); it != end; ++it)
        toTextureMapperLayer(it->value)->setScrollPositionDeltaIfNeeded(delta);
}

#if USE(GRAPHICS_SURFACE)
void CoordinatedGraphicsScene::createCanvas(CoordinatedLayerID id, const IntSize&, PassRefPtr<GraphicsSurface> surface)
{
    ASSERT(m_textureMapper);
    GraphicsLayer* layer = layerByID(id);
    ASSERT(!m_surfaceBackingStores.contains(id));

    RefPtr<TextureMapperSurfaceBackingStore> canvasBackingStore(TextureMapperSurfaceBackingStore::create());
    m_surfaceBackingStores.set(id, canvasBackingStore);

    canvasBackingStore->setGraphicsSurface(surface);
    layer->setContentsToMedia(canvasBackingStore.get());
}

void CoordinatedGraphicsScene::syncCanvas(CoordinatedLayerID id, uint32_t frontBuffer)
{
    ASSERT(m_textureMapper);
    ASSERT(m_surfaceBackingStores.contains(id));

    SurfaceBackingStoreMap::iterator it = m_surfaceBackingStores.find(id);
    RefPtr<TextureMapperSurfaceBackingStore> canvasBackingStore = it->value;

    canvasBackingStore->swapBuffersIfNeeded(frontBuffer);
}

void CoordinatedGraphicsScene::destroyCanvas(CoordinatedLayerID id)
{
    ASSERT(m_textureMapper);
    GraphicsLayer* layer = layerByID(id);
    ASSERT(m_surfaceBackingStores.contains(id));

    m_surfaceBackingStores.remove(id);
    layer->setContentsToMedia(0);
}
#endif

void CoordinatedGraphicsScene::setLayerRepaintCount(CoordinatedLayerID id, int value)
{
    GraphicsLayer* layer = layerByID(id);
    toGraphicsLayerTextureMapper(layer)->setRepaintCount(value);
}

void CoordinatedGraphicsScene::setLayerChildren(CoordinatedLayerID id, const Vector<CoordinatedLayerID>& childIDs)
{
    GraphicsLayer* layer = layerByID(id);
    Vector<GraphicsLayer*> children;

    for (size_t i = 0; i < childIDs.size(); ++i) {
        CoordinatedLayerID childID = childIDs[i];
        GraphicsLayer* child = layerByID(childID);
        children.append(child);
    }
    layer->setChildren(children);
}

#if ENABLE(CSS_FILTERS)
void CoordinatedGraphicsScene::setLayerFilters(CoordinatedLayerID id, const FilterOperations& filters)
{
    GraphicsLayer* layer = layerByID(id);

#if ENABLE(CSS_SHADERS)
    injectCachedCustomFilterPrograms(filters);
#endif
    layer->setFilters(filters);
}
#endif

#if ENABLE(CSS_SHADERS)
void CoordinatedGraphicsScene::injectCachedCustomFilterPrograms(const FilterOperations& filters) const
{
    for (size_t i = 0; i < filters.size(); ++i) {
        FilterOperation* operation = filters.operations().at(i).get();
        if (operation->getOperationType() != FilterOperation::CUSTOM)
            continue;

        CoordinatedCustomFilterOperation* customOperation = static_cast<CoordinatedCustomFilterOperation*>(operation);
        ASSERT(!customOperation->program());
        CustomFilterProgramMap::const_iterator iter = m_customFilterPrograms.find(customOperation->programID());
        ASSERT(iter != m_customFilterPrograms.end());
        customOperation->setProgram(iter->value.get());
    }
}

void CoordinatedGraphicsScene::createCustomFilterProgram(int id, const CustomFilterProgramInfo& programInfo)
{
    ASSERT(!m_customFilterPrograms.contains(id));
    m_customFilterPrograms.set(id, CoordinatedCustomFilterProgram::create(programInfo.vertexShaderString(), programInfo.fragmentShaderString(), programInfo.programType(), programInfo.mixSettings(), programInfo.meshType()));
}

void CoordinatedGraphicsScene::removeCustomFilterProgram(int id)
{
    CustomFilterProgramMap::iterator iter = m_customFilterPrograms.find(id);
    ASSERT(iter != m_customFilterPrograms.end());
    if (m_textureMapper)
        m_textureMapper->removeCachedCustomFilterProgram(iter->value.get());
    m_customFilterPrograms.remove(iter);
}
#endif // ENABLE(CSS_SHADERS)

void CoordinatedGraphicsScene::setLayerState(CoordinatedLayerID id, const CoordinatedLayerInfo& layerInfo)
{
    ASSERT(m_rootLayerID != InvalidCoordinatedLayerID);
    GraphicsLayer* layer = layerByID(id);

    layer->setReplicatedByLayer(getLayerByIDIfExists(layerInfo.replica));
    layer->setMaskLayer(getLayerByIDIfExists(layerInfo.mask));

    layer->setAnchorPoint(layerInfo.anchorPoint);
    layer->setPosition(layerInfo.pos);
    layer->setSize(layerInfo.size);

    layer->setTransform(layerInfo.transform);
    layer->setChildrenTransform(layerInfo.childrenTransform);
    layer->setBackfaceVisibility(layerInfo.backfaceVisible);
    layer->setContentsOpaque(layerInfo.contentsOpaque);
    layer->setContentsRect(layerInfo.contentsRect);
    layer->setContentsToSolidColor(layerInfo.solidColor);
    layer->setDrawsContent(layerInfo.drawsContent);
    layer->setContentsVisible(layerInfo.contentsVisible);
    toGraphicsLayerTextureMapper(layer)->setFixedToViewport(layerInfo.fixedToViewport);
    layer->setShowDebugBorder(layerInfo.showDebugBorders);
    layer->setDebugBorder(layerInfo.debugBorderColor, layerInfo.debugBorderWidth);
    layer->setShowRepaintCounter(layerInfo.showRepaintCounter);

    if (layerInfo.fixedToViewport)
        m_fixedLayers.add(id, layer);
    else
        m_fixedLayers.remove(id);

    assignImageBackingToLayer(id, layer, layerInfo.imageID);
    prepareContentBackingStore(layer);

    // Never make the root layer clip.
    layer->setMasksToBounds(layerInfo.isRootLayer ? false : layerInfo.masksToBounds);
    layer->setOpacity(layerInfo.opacity);
    layer->setPreserves3D(layerInfo.preserves3D);
}

GraphicsLayer* CoordinatedGraphicsScene::getLayerByIDIfExists(CoordinatedLayerID id)
{
    return (id != InvalidCoordinatedLayerID) ? layerByID(id) : 0;
}

void CoordinatedGraphicsScene::createLayers(const Vector<CoordinatedLayerID>& ids)
{
    for (size_t index = 0; index < ids.size(); ++index)
        createLayer(ids[index]);
}

void CoordinatedGraphicsScene::createLayer(CoordinatedLayerID id)
{
    OwnPtr<GraphicsLayer> newLayer = GraphicsLayer::create(0 /* factory */, this);
    toGraphicsLayerTextureMapper(newLayer.get())->setHasOwnBackingStore(false);
    m_layers.add(id, newLayer.release());
}

void CoordinatedGraphicsScene::deleteLayers(const Vector<CoordinatedLayerID>& layerIDs)
{
    for (size_t index = 0; index < layerIDs.size(); ++index)
        deleteLayer(layerIDs[index]);
}

void CoordinatedGraphicsScene::deleteLayer(CoordinatedLayerID layerID)
{
    OwnPtr<GraphicsLayer> layer = m_layers.take(layerID);
    ASSERT(layer);

    m_backingStores.remove(layer.get());
    m_fixedLayers.remove(layerID);
#if USE(GRAPHICS_SURFACE)
    m_surfaceBackingStores.remove(layerID);
#endif
}

void CoordinatedGraphicsScene::setRootLayerID(CoordinatedLayerID layerID)
{
    ASSERT(layerID != InvalidCoordinatedLayerID);
    ASSERT(m_rootLayerID == InvalidCoordinatedLayerID);

    m_rootLayerID = layerID;

    GraphicsLayer* layer = layerByID(layerID);
    ASSERT(m_rootLayer->children().isEmpty());
    m_rootLayer->addChild(layer);
}

void CoordinatedGraphicsScene::prepareContentBackingStore(GraphicsLayer* graphicsLayer)
{
    if (!layerShouldHaveBackingStore(graphicsLayer)) {
        removeBackingStoreIfNeeded(graphicsLayer);
        return;
    }

    createBackingStoreIfNeeded(graphicsLayer);
    resetBackingStoreSizeToLayerSize(graphicsLayer);
}

void CoordinatedGraphicsScene::createBackingStoreIfNeeded(GraphicsLayer* graphicsLayer)
{
    if (m_backingStores.contains(graphicsLayer))
        return;

    RefPtr<CoordinatedBackingStore> backingStore(CoordinatedBackingStore::create());
    m_backingStores.add(graphicsLayer, backingStore);
    toGraphicsLayerTextureMapper(graphicsLayer)->setBackingStore(backingStore);
}

void CoordinatedGraphicsScene::removeBackingStoreIfNeeded(GraphicsLayer* graphicsLayer)
{
    RefPtr<CoordinatedBackingStore> backingStore = m_backingStores.take(graphicsLayer);
    if (!backingStore)
        return;

    toGraphicsLayerTextureMapper(graphicsLayer)->setBackingStore(0);
}

void CoordinatedGraphicsScene::resetBackingStoreSizeToLayerSize(GraphicsLayer* graphicsLayer)
{
    RefPtr<CoordinatedBackingStore> backingStore = m_backingStores.get(graphicsLayer);
    ASSERT(backingStore);
    backingStore->setSize(graphicsLayer->size());
    m_backingStoresWithPendingBuffers.add(backingStore);
}

void CoordinatedGraphicsScene::createTile(CoordinatedLayerID layerID, uint32_t tileID, float scale)
{
    GraphicsLayer* layer = layerByID(layerID);
    RefPtr<CoordinatedBackingStore> backingStore = m_backingStores.get(layer);
    ASSERT(backingStore);
    backingStore->createTile(tileID, scale);
}

void CoordinatedGraphicsScene::removeTile(CoordinatedLayerID layerID, uint32_t tileID)
{
    GraphicsLayer* layer = layerByID(layerID);
    RefPtr<CoordinatedBackingStore> backingStore = m_backingStores.get(layer);
    if (!backingStore)
        return;

    backingStore->removeTile(tileID);
    m_backingStoresWithPendingBuffers.add(backingStore);
}

void CoordinatedGraphicsScene::updateTile(CoordinatedLayerID layerID, uint32_t tileID, const TileUpdate& update)
{
    GraphicsLayer* layer = layerByID(layerID);
    RefPtr<CoordinatedBackingStore> backingStore = m_backingStores.get(layer);
    ASSERT(backingStore);

    SurfaceMap::iterator it = m_surfaces.find(update.atlasID);
    ASSERT(it != m_surfaces.end());

    backingStore->updateTile(tileID, update.sourceRect, update.tileRect, it->value, update.offset);
    m_backingStoresWithPendingBuffers.add(backingStore);
}

void CoordinatedGraphicsScene::createUpdateAtlas(uint32_t atlasID, PassRefPtr<CoordinatedSurface> surface)
{
    ASSERT(!m_surfaces.contains(atlasID));
    m_surfaces.add(atlasID, surface);
}

void CoordinatedGraphicsScene::removeUpdateAtlas(uint32_t atlasID)
{
    ASSERT(m_surfaces.contains(atlasID));
    m_surfaces.remove(atlasID);
}

void CoordinatedGraphicsScene::createImageBacking(CoordinatedImageBackingID imageID)
{
    ASSERT(!m_imageBackings.contains(imageID));
    RefPtr<CoordinatedBackingStore> backingStore(CoordinatedBackingStore::create());
    m_imageBackings.add(imageID, backingStore.release());
}

void CoordinatedGraphicsScene::updateImageBacking(CoordinatedImageBackingID imageID, PassRefPtr<CoordinatedSurface> surface)
{
    ASSERT(m_imageBackings.contains(imageID));
    ImageBackingMap::iterator it = m_imageBackings.find(imageID);
    RefPtr<CoordinatedBackingStore> backingStore = it->value;

    // CoordinatedImageBacking is realized to CoordinatedBackingStore with only one tile in UI Process.
    backingStore->createTile(1 /* id */, 1 /* scale */);
    IntRect rect(IntPoint::zero(), surface->size());
    // See CoordinatedGraphicsLayer::shouldDirectlyCompositeImage()
    ASSERT(2000 >= std::max(rect.width(), rect.height()));
    backingStore->setSize(rect.size());
    backingStore->updateTile(1 /* id */, rect, rect, surface, rect.location());

    m_backingStoresWithPendingBuffers.add(backingStore);
}

void CoordinatedGraphicsScene::clearImageBackingContents(CoordinatedImageBackingID imageID)
{
    ASSERT(m_imageBackings.contains(imageID));
    ImageBackingMap::iterator it = m_imageBackings.find(imageID);
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

void CoordinatedGraphicsScene::assignImageBackingToLayer(CoordinatedLayerID id, GraphicsLayer* layer, CoordinatedImageBackingID imageID)
{
#if USE(GRAPHICS_SURFACE)
    if (m_surfaceBackingStores.contains(id))
        return;
#endif

    if (imageID == InvalidCoordinatedImageBackingID) {
        layer->setContentsToMedia(0);
        return;
    }
    ImageBackingMap::iterator it = m_imageBackings.find(imageID);
    ASSERT(it != m_imageBackings.end());
    layer->setContentsToMedia(it->value.get());
}

void CoordinatedGraphicsScene::removeReleasedImageBackingsIfNeeded()
{
    m_releasedImageBackings.clear();
}

void CoordinatedGraphicsScene::commitPendingBackingStoreOperations()
{
    HashSet<RefPtr<CoordinatedBackingStore> >::iterator end = m_backingStoresWithPendingBuffers.end();
    for (HashSet<RefPtr<CoordinatedBackingStore> >::iterator it = m_backingStoresWithPendingBuffers.begin(); it != end; ++it)
        (*it)->commitTileOperations(m_textureMapper.get());

    m_backingStoresWithPendingBuffers.clear();
}

void CoordinatedGraphicsScene::flushLayerChanges(const FloatPoint& scrollPosition)
{
    m_renderedContentsScrollPosition = scrollPosition;

    // Since the frame has now been rendered, we can safely unlock the animations until the next layout.
    setAnimationsLocked(false);

    m_rootLayer->flushCompositingState(FloatRect());
    commitPendingBackingStoreOperations();
    removeReleasedImageBackingsIfNeeded();

    // The pending tiles state is on its way for the screen, tell the web process to render the next one.
    dispatchOnMainThread(bind(&CoordinatedGraphicsScene::renderNextFrame, this));
}

void CoordinatedGraphicsScene::renderNextFrame()
{
    if (m_client)
        m_client->renderNextFrame();
}

void CoordinatedGraphicsScene::ensureRootLayer()
{
    if (m_rootLayer)
        return;

    m_rootLayer = GraphicsLayer::create(0 /* factory */, this);
    m_rootLayer->setMasksToBounds(false);
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setAnchorPoint(FloatPoint3D(0, 0, 0));

    // The root layer should not have zero size, or it would be optimized out.
    m_rootLayer->setSize(FloatSize(1.0, 1.0));

    ASSERT(m_textureMapper);
    toTextureMapperLayer(m_rootLayer.get())->setTextureMapper(m_textureMapper.get());
}

void CoordinatedGraphicsScene::syncRemoteContent()
{
    // We enqueue messages and execute them during paint, as they require an active GL context.
    ensureRootLayer();

    Vector<Function<void()> > renderQueue;
    bool calledOnMainThread = WTF::isMainThread();
    if (!calledOnMainThread)
        m_renderQueueMutex.lock();
    renderQueue.swap(m_renderQueue);
    if (!calledOnMainThread)
        m_renderQueueMutex.unlock();

    for (size_t i = 0; i < renderQueue.size(); ++i)
        renderQueue[i]();
}

void CoordinatedGraphicsScene::purgeGLResources()
{
    m_imageBackings.clear();
    m_releasedImageBackings.clear();
#if USE(GRAPHICS_SURFACE)
    m_surfaceBackingStores.clear();
#endif
    m_surfaces.clear();

    m_rootLayer.clear();
    m_rootLayerID = InvalidCoordinatedLayerID;
    m_layers.clear();
    m_fixedLayers.clear();
    m_textureMapper.clear();
    m_backingStores.clear();
    m_backingStoresWithPendingBuffers.clear();

    setActive(false);
    dispatchOnMainThread(bind(&CoordinatedGraphicsScene::purgeBackingStores, this));
}

void CoordinatedGraphicsScene::purgeBackingStores()
{
    if (m_client)
        m_client->purgeBackingStores();
}

void CoordinatedGraphicsScene::setLayerAnimations(CoordinatedLayerID id, const GraphicsLayerAnimations& animations)
{
    GraphicsLayerTextureMapper* layer = toGraphicsLayerTextureMapper(layerByID(id));
#if ENABLE(CSS_SHADERS)
    for (size_t i = 0; i < animations.animations().size(); ++i) {
        const KeyframeValueList& keyframes = animations.animations().at(i).keyframes();
        if (keyframes.property() != AnimatedPropertyWebkitFilter)
            continue;
        for (size_t j = 0; j < keyframes.size(); ++j) {
            const FilterAnimationValue* filterValue = static_cast<const FilterAnimationValue*>(keyframes.at(i));
            injectCachedCustomFilterPrograms(*filterValue->value());
        }
    }
#endif
    layer->setAnimations(animations);
}

void CoordinatedGraphicsScene::setAnimationsLocked(bool locked)
{
    m_animationsLocked = locked;
}

void CoordinatedGraphicsScene::detach()
{
    ASSERT(isMainThread());
    m_client = 0;
}

void CoordinatedGraphicsScene::appendUpdate(const Function<void()>& function)
{
    if (!m_isActive)
        return;

    ASSERT(isMainThread());
    MutexLocker locker(m_renderQueueMutex);
    m_renderQueue.append(function);
}

void CoordinatedGraphicsScene::setActive(bool active)
{
    if (m_isActive == active)
        return;

    // Have to clear render queue in both cases.
    // If there are some updates in queue during activation then those updates are from previous instance of paint node
    // and cannot be applied to the newly created instance.
    m_renderQueue.clear();
    m_isActive = active;
    if (m_isActive)
        dispatchOnMainThread(bind(&CoordinatedGraphicsScene::renderNextFrame, this));
}

void CoordinatedGraphicsScene::setBackgroundColor(const Color& color)
{
    m_backgroundColor = color;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
