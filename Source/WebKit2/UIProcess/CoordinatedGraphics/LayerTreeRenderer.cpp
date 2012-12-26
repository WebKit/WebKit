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

#include "LayerTreeRenderer.h"

#include "CoordinatedBackingStore.h"
#include "CoordinatedLayerTreeHostProxy.h"
#include "GraphicsLayerTextureMapper.h"
#include "MessageID.h"
#include "TextureMapper.h"
#include "TextureMapperBackingStore.h"
#include "TextureMapperGL.h"
#include "TextureMapperLayer.h"
#include "UpdateInfo.h"
#include <OpenGLShims.h>
#include <wtf/Atomics.h>
#include <wtf/MainThread.h>

#if ENABLE(CSS_SHADERS)
#include "CustomFilterProgram.h"
#include "CustomFilterProgramInfo.h"
#include "WebCustomFilterOperation.h"
#include "WebCustomFilterProgram.h"
#endif

namespace WebKit {

using namespace WebCore;

void LayerTreeRenderer::dispatchOnMainThread(const Function<void()>& function)
{
    if (isMainThread())
        function();
    else
        callOnMainThread(function);
}

static FloatPoint boundedScrollPosition(const FloatPoint& scrollPosition, const FloatRect& visibleContentRect, const FloatSize& contentSize)
{
    float scrollPositionX = std::max(scrollPosition.x(), 0.0f);
    scrollPositionX = std::min(scrollPositionX, contentSize.width() - visibleContentRect.width());

    float scrollPositionY = std::max(scrollPosition.y(), 0.0f);
    scrollPositionY = std::min(scrollPositionY, contentSize.height() - visibleContentRect.height());
    return FloatPoint(scrollPositionX, scrollPositionY);
}

static bool layerShouldHaveBackingStore(GraphicsLayer* layer)
{
    return layer->drawsContent() && layer->contentsAreVisible() && !layer->size().isEmpty();
}

LayerTreeRenderer::LayerTreeRenderer(CoordinatedLayerTreeHostProxy* coordinatedLayerTreeHostProxy)
    : m_coordinatedLayerTreeHostProxy(coordinatedLayerTreeHostProxy)
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

LayerTreeRenderer::~LayerTreeRenderer()
{
}

void LayerTreeRenderer::paintToCurrentGLContext(const TransformationMatrix& matrix, float opacity, const FloatRect& clipRect, TextureMapper::PaintFlags PaintFlags)
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
    m_textureMapper->endClip();
    m_textureMapper->endPainting();

    if (layer->descendantsOrSelfHaveRunningAnimations())
        dispatchOnMainThread(bind(&LayerTreeRenderer::updateViewport, this));

#if ENABLE(REQUEST_ANIMATION_FRAME)
    if (m_animationFrameRequested) {
        m_animationFrameRequested = false;
        dispatchOnMainThread(bind(&LayerTreeRenderer::animationFrameReady, this));
    }
#endif
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
void LayerTreeRenderer::animationFrameReady()
{
    ASSERT(isMainThread());
    if (m_coordinatedLayerTreeHostProxy)
        m_coordinatedLayerTreeHostProxy->animationFrameReady();
}

void LayerTreeRenderer::requestAnimationFrame()
{
    m_animationFrameRequested = true;
}
#endif

void LayerTreeRenderer::paintToGraphicsContext(BackingStore::PlatformGraphicsContext painter)
{
    if (!m_textureMapper)
        m_textureMapper = TextureMapper::create();
    ASSERT(m_textureMapper->accelerationMode() == TextureMapper::SoftwareMode);
    syncRemoteContent();
    TextureMapperLayer* layer = toTextureMapperLayer(rootLayer());

    if (!layer)
        return;

    GraphicsContext graphicsContext(painter);
    m_textureMapper->setGraphicsContext(&graphicsContext);
    m_textureMapper->beginPainting();

    if (m_setDrawsBackground)
        m_textureMapper->drawSolidColor(graphicsContext.clipBounds(), TransformationMatrix(), m_backgroundColor);

    layer->paint();
    m_textureMapper->endPainting();
    m_textureMapper->setGraphicsContext(0);
}

void LayerTreeRenderer::setContentsSize(const WebCore::FloatSize& contentsSize)
{
    m_contentsSize = contentsSize;
}

void LayerTreeRenderer::setVisibleContentsRect(const FloatRect& rect)
{
    m_visibleContentsRect = rect;
}

void LayerTreeRenderer::updateViewport()
{
    ASSERT(isMainThread());
    if (m_coordinatedLayerTreeHostProxy)
        m_coordinatedLayerTreeHostProxy->updateViewport();
}

void LayerTreeRenderer::adjustPositionForFixedLayers()
{
    if (m_fixedLayers.isEmpty())
        return;

    // Fixed layer positions are updated by the web process when we update the visible contents rect / scroll position.
    // If we want those layers to follow accurately the viewport when we move between the web process updates, we have to offset
    // them by the delta between the current position and the position of the viewport used for the last layout.
    FloatPoint scrollPosition = boundedScrollPosition(m_visibleContentsRect.location(), m_visibleContentsRect, m_contentsSize);
    FloatPoint renderedScrollPosition = boundedScrollPosition(m_renderedContentsScrollPosition, m_visibleContentsRect, m_contentsSize);
    FloatSize delta = scrollPosition - renderedScrollPosition;

    LayerRawPtrMap::iterator end = m_fixedLayers.end();
    for (LayerRawPtrMap::iterator it = m_fixedLayers.begin(); it != end; ++it)
        toTextureMapperLayer(it->value)->setScrollPositionDeltaIfNeeded(delta);
}

void LayerTreeRenderer::didChangeScrollPosition(const FloatPoint& position)
{
    m_pendingRenderedContentsScrollPosition = position;
}

#if USE(GRAPHICS_SURFACE)
void LayerTreeRenderer::createCanvas(CoordinatedLayerID id, const WebCore::IntSize&, PassRefPtr<GraphicsSurface> surface)
{
    ASSERT(m_textureMapper);
    GraphicsLayer* layer = layerByID(id);
    ASSERT(!m_surfaceBackingStores.contains(id));

    RefPtr<TextureMapperSurfaceBackingStore> canvasBackingStore(TextureMapperSurfaceBackingStore::create());
    m_surfaceBackingStores.set(id, canvasBackingStore);

    canvasBackingStore->setGraphicsSurface(surface);
    layer->setContentsToMedia(canvasBackingStore.get());
}

void LayerTreeRenderer::syncCanvas(CoordinatedLayerID id, uint32_t frontBuffer)
{
    ASSERT(m_textureMapper);
    ASSERT(m_surfaceBackingStores.contains(id));

    SurfaceBackingStoreMap::iterator it = m_surfaceBackingStores.find(id);
    RefPtr<TextureMapperSurfaceBackingStore> canvasBackingStore = it->value;

    canvasBackingStore->swapBuffersIfNeeded(frontBuffer);
}

void LayerTreeRenderer::destroyCanvas(CoordinatedLayerID id)
{
    ASSERT(m_textureMapper);
    GraphicsLayer* layer = layerByID(id);
    ASSERT(m_surfaceBackingStores.contains(id));

    m_surfaceBackingStores.remove(id);
    layer->setContentsToMedia(0);
}
#endif

void LayerTreeRenderer::setLayerChildren(CoordinatedLayerID id, const Vector<CoordinatedLayerID>& childIDs)
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
void LayerTreeRenderer::setLayerFilters(CoordinatedLayerID id, const FilterOperations& filters)
{
    GraphicsLayer* layer = layerByID(id);

#if ENABLE(CSS_SHADERS)
    injectCachedCustomFilterPrograms(filters);
#endif
    layer->setFilters(filters);
}
#endif

#if ENABLE(CSS_SHADERS)
void LayerTreeRenderer::injectCachedCustomFilterPrograms(const FilterOperations& filters) const
{
    for (size_t i = 0; i < filters.size(); ++i) {
        FilterOperation* operation = filters.operations().at(i).get();
        if (operation->getOperationType() != FilterOperation::CUSTOM)
            continue;

        WebCustomFilterOperation* customOperation = static_cast<WebCustomFilterOperation*>(operation);
        ASSERT(!customOperation->program());
        CustomFilterProgramMap::const_iterator iter = m_customFilterPrograms.find(customOperation->programID());
        ASSERT(iter != m_customFilterPrograms.end());
        customOperation->setProgram(iter->value.get());
    }
}

void LayerTreeRenderer::createCustomFilterProgram(int id, const WebCore::CustomFilterProgramInfo& programInfo)
{
    ASSERT(!m_customFilterPrograms.contains(id));
    m_customFilterPrograms.set(id, WebCustomFilterProgram::create(programInfo.vertexShaderString(), programInfo.fragmentShaderString(), programInfo.programType(), programInfo.mixSettings(), programInfo.meshType()));
}

void LayerTreeRenderer::removeCustomFilterProgram(int id)
{
    CustomFilterProgramMap::iterator iter = m_customFilterPrograms.find(id);
    ASSERT(iter != m_customFilterPrograms.end());
    if (m_textureMapper)
        m_textureMapper->removeCachedCustomFilterProgram(iter->value.get());
    m_customFilterPrograms.remove(iter);
}
#endif // ENABLE(CSS_SHADERS)

void LayerTreeRenderer::setLayerState(CoordinatedLayerID id, const CoordinatedLayerInfo& layerInfo)
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

    if (layerInfo.fixedToViewport)
        m_fixedLayers.add(id, layer);
    else
        m_fixedLayers.remove(id);

    assignImageBackingToLayer(layer, layerInfo.imageID);
    prepareContentBackingStore(layer);

    // Never make the root layer clip.
    layer->setMasksToBounds(layerInfo.isRootLayer ? false : layerInfo.masksToBounds);
    layer->setOpacity(layerInfo.opacity);
    layer->setPreserves3D(layerInfo.preserves3D);
}

GraphicsLayer* LayerTreeRenderer::getLayerByIDIfExists(CoordinatedLayerID id)
{
    return (id != InvalidCoordinatedLayerID) ? layerByID(id) : 0;
}

void LayerTreeRenderer::createLayer(CoordinatedLayerID id)
{
    OwnPtr<WebCore::GraphicsLayer> newLayer = GraphicsLayer::create(0 /* factory */, this);
    toGraphicsLayerTextureMapper(newLayer.get())->setHasOwnBackingStore(false);
    m_layers.add(id, newLayer.release());
}

void LayerTreeRenderer::deleteLayer(CoordinatedLayerID layerID)
{
    OwnPtr<GraphicsLayer> layer = m_layers.take(layerID);
    ASSERT(layer);

    layer->removeFromParent();
    m_pendingSyncBackingStores.remove(toTextureMapperLayer(layer.get()));
    m_fixedLayers.remove(layerID);
#if USE(GRAPHICS_SURFACE)
    m_surfaceBackingStores.remove(layerID);
#endif
}

void LayerTreeRenderer::setRootLayerID(CoordinatedLayerID layerID)
{
    ASSERT(layerID != InvalidCoordinatedLayerID);
    ASSERT(m_rootLayerID == InvalidCoordinatedLayerID);

    m_rootLayerID = layerID;
    m_rootLayer->removeAllChildren();

    GraphicsLayer* layer = layerByID(layerID);
    m_rootLayer->addChild(layer);
}

CoordinatedBackingStore* LayerTreeRenderer::getBackingStore(GraphicsLayer* graphicsLayer)
{
    TextureMapperLayer* layer = toTextureMapperLayer(graphicsLayer);
    ASSERT(layer);
    CoordinatedBackingStore* backingStore = static_cast<CoordinatedBackingStore*>(layer->backingStore().get());

    BackingStoreMap::iterator it = m_pendingSyncBackingStores.find(layer);
    if (it != m_pendingSyncBackingStores.end())
        backingStore = it->value.get();
    return backingStore;
}

void LayerTreeRenderer::prepareContentBackingStore(GraphicsLayer* graphicsLayer)
{
    if (!layerShouldHaveBackingStore(graphicsLayer)) {
        removeBackingStoreIfNeeded(graphicsLayer);
        return;
    }

    createBackingStoreIfNeeded(graphicsLayer);
}

void LayerTreeRenderer::createBackingStoreIfNeeded(GraphicsLayer* graphicsLayer)
{
    TextureMapperLayer* layer = toTextureMapperLayer(graphicsLayer);
    ASSERT(layer);

    // Make sure the layer does not already have a backing store (committed or pending).
    BackingStoreMap::iterator it = m_pendingSyncBackingStores.find(layer);
    if (it != m_pendingSyncBackingStores.end()) {
        if (!it->value) {
            // There is a pending removal, cancel it.
            m_pendingSyncBackingStores.remove(it);
        }
        // There is already a pending addition.
        return;
    }
    if (layer->backingStore())
        return; // The layer already has a backing store (and no pending removal).

    RefPtr<CoordinatedBackingStore> backingStore(CoordinatedBackingStore::create());

    backingStore->setSize(graphicsLayer->size());
    ASSERT(!m_pendingSyncBackingStores.contains(layer));
    m_pendingSyncBackingStores.add(layer, backingStore);
}

void LayerTreeRenderer::removeBackingStoreIfNeeded(GraphicsLayer* graphicsLayer)
{
    TextureMapperLayer* layer = toTextureMapperLayer(graphicsLayer);
    ASSERT(layer);

    // Check if the layout already has a backing store (committed or pending).
    BackingStoreMap::iterator it = m_pendingSyncBackingStores.find(layer);
    if (it != m_pendingSyncBackingStores.end()) {
        if (it->value) {
            // There is a pending addition, cancel it.
            m_pendingSyncBackingStores.remove(it);
        }
        // There is already a pending removal.
        return;
    }

    if (!layer->backingStore())
        return; // The layer has no backing store (and no pending addition).

    ASSERT(!m_pendingSyncBackingStores.contains(layer));
    m_pendingSyncBackingStores.add(layer, 0);
}

void LayerTreeRenderer::resetBackingStoreSizeToLayerSize(GraphicsLayer* graphicsLayer)
{
    CoordinatedBackingStore* backingStore = getBackingStore(graphicsLayer);
    ASSERT(backingStore);
    backingStore->setSize(graphicsLayer->size());
}

void LayerTreeRenderer::createTile(CoordinatedLayerID layerID, uint32_t tileID, float scale)
{
    GraphicsLayer* layer = layerByID(layerID);
    CoordinatedBackingStore* backingStore = getBackingStore(layer);
    ASSERT(backingStore);
    backingStore->createTile(tileID, scale);
    resetBackingStoreSizeToLayerSize(layer);
}

void LayerTreeRenderer::removeTile(CoordinatedLayerID layerID, uint32_t tileID)
{
    GraphicsLayer* layer = layerByID(layerID);
    CoordinatedBackingStore* backingStore = getBackingStore(layer);
    if (!backingStore)
        return;

    backingStore->removeTile(tileID);
    resetBackingStoreSizeToLayerSize(layer);
    m_backingStoresWithPendingBuffers.add(backingStore);
}

void LayerTreeRenderer::updateTile(CoordinatedLayerID layerID, uint32_t tileID, const TileUpdate& update)
{
    GraphicsLayer* layer = layerByID(layerID);
    RefPtr<CoordinatedBackingStore> backingStore = getBackingStore(layer);
    ASSERT(backingStore);
    backingStore->updateTile(tileID, update.sourceRect, update.tileRect, update.surface, update.offset);
    resetBackingStoreSizeToLayerSize(layer);
    m_backingStoresWithPendingBuffers.add(backingStore);
}

void LayerTreeRenderer::createImageBacking(CoordinatedImageBackingID imageID)
{
    ASSERT(!m_imageBackings.contains(imageID));
    RefPtr<CoordinatedBackingStore> backingStore(CoordinatedBackingStore::create());
    m_imageBackings.add(imageID, backingStore.release());
}

void LayerTreeRenderer::updateImageBacking(CoordinatedImageBackingID imageID, PassRefPtr<CoordinatedSurface> surface)
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

void LayerTreeRenderer::clearImageBackingContents(CoordinatedImageBackingID imageID)
{
    ASSERT(m_imageBackings.contains(imageID));
    ImageBackingMap::iterator it = m_imageBackings.find(imageID);
    RefPtr<CoordinatedBackingStore> backingStore = it->value;
    backingStore->removeAllTiles();
    m_backingStoresWithPendingBuffers.add(backingStore);
}

void LayerTreeRenderer::removeImageBacking(CoordinatedImageBackingID imageID)
{
    ASSERT(m_imageBackings.contains(imageID));

    // We don't want TextureMapperLayer refers a dangling pointer.
    m_releasedImageBackings.append(m_imageBackings.take(imageID));
}

void LayerTreeRenderer::assignImageBackingToLayer(GraphicsLayer* layer, CoordinatedImageBackingID imageID)
{
    if (imageID == InvalidCoordinatedImageBackingID) {
        layer->setContentsToMedia(0);
        return;
    }
    ImageBackingMap::iterator it = m_imageBackings.find(imageID);
    ASSERT(it != m_imageBackings.end());
    layer->setContentsToMedia(it->value.get());
}

void LayerTreeRenderer::removeReleasedImageBackingsIfNeeded()
{
    m_releasedImageBackings.clear();
}

void LayerTreeRenderer::commitPendingBackingStoreOperations()
{
    HashSet<RefPtr<CoordinatedBackingStore> >::iterator end = m_backingStoresWithPendingBuffers.end();
    for (HashSet<RefPtr<CoordinatedBackingStore> >::iterator it = m_backingStoresWithPendingBuffers.begin(); it != end; ++it)
        (*it)->commitTileOperations(m_textureMapper.get());

    m_backingStoresWithPendingBuffers.clear();

    {
        BackingStoreMap::iterator end = m_pendingSyncBackingStores.end();
        BackingStoreMap::iterator it = m_pendingSyncBackingStores.begin();
        for (;it != end; ++it)
            it->key->setBackingStore(it->value);

        m_pendingSyncBackingStores.clear();
    }
}

void LayerTreeRenderer::flushLayerChanges()
{
    m_renderedContentsScrollPosition = m_pendingRenderedContentsScrollPosition;

    // Since the frame has now been rendered, we can safely unlock the animations until the next layout.
    setAnimationsLocked(false);

    m_rootLayer->flushCompositingState(FloatRect());
    commitPendingBackingStoreOperations();
    removeReleasedImageBackingsIfNeeded();

    // The pending tiles state is on its way for the screen, tell the web process to render the next one.
    dispatchOnMainThread(bind(&LayerTreeRenderer::renderNextFrame, this));
}

void LayerTreeRenderer::renderNextFrame()
{
    if (m_coordinatedLayerTreeHostProxy)
        m_coordinatedLayerTreeHostProxy->renderNextFrame();
}

void LayerTreeRenderer::ensureRootLayer()
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

void LayerTreeRenderer::syncRemoteContent()
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

void LayerTreeRenderer::purgeGLResources()
{
    TextureMapperLayer* layer = toTextureMapperLayer(rootLayer());

    if (layer)
        layer->clearBackingStoresRecursive();

    m_imageBackings.clear();
#if USE(GRAPHICS_SURFACE)
    m_surfaceBackingStores.clear();
#endif

    m_rootLayer->removeAllChildren();
    m_rootLayer.clear();
    m_rootLayerID = InvalidCoordinatedLayerID;
    m_layers.clear();
    m_fixedLayers.clear();
    m_textureMapper.clear();
    m_pendingSyncBackingStores.clear();
    m_backingStoresWithPendingBuffers.clear();

    setActive(false);
    dispatchOnMainThread(bind(&LayerTreeRenderer::purgeBackingStores, this));
}

void LayerTreeRenderer::purgeBackingStores()
{
    if (m_coordinatedLayerTreeHostProxy)
        m_coordinatedLayerTreeHostProxy->purgeBackingStores();
}

void LayerTreeRenderer::setLayerAnimations(CoordinatedLayerID id, const GraphicsLayerAnimations& animations)
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

void LayerTreeRenderer::setAnimationsLocked(bool locked)
{
    m_animationsLocked = locked;
}

void LayerTreeRenderer::detach()
{
    ASSERT(isMainThread());
    m_coordinatedLayerTreeHostProxy = 0;
}

void LayerTreeRenderer::appendUpdate(const Function<void()>& function)
{
    if (!m_isActive)
        return;

    ASSERT(isMainThread());
    MutexLocker locker(m_renderQueueMutex);
    m_renderQueue.append(function);
}

void LayerTreeRenderer::setActive(bool active)
{
    if (m_isActive == active)
        return;

    // Have to clear render queue in both cases.
    // If there are some updates in queue during activation then those updates are from previous instance of paint node
    // and cannot be applied to the newly created instance.
    m_renderQueue.clear();
    m_isActive = active;
    if (m_isActive)
        dispatchOnMainThread(bind(&LayerTreeRenderer::renderNextFrame, this));
}

void LayerTreeRenderer::setBackgroundColor(const WebCore::Color& color)
{
    m_backgroundColor = color;
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
