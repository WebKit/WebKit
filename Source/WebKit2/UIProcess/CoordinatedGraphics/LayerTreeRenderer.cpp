/*
    Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)

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
#include "GraphicsLayerTextureMapper.h"
#include "LayerTreeCoordinatorProxy.h"
#include "MessageID.h"
#include "ShareableBitmap.h"
#include "TextureMapper.h"
#include "TextureMapperBackingStore.h"
#include "TextureMapperGL.h"
#include "TextureMapperLayer.h"
#include "UpdateInfo.h"
#include <OpenGLShims.h>
#include <wtf/Atomics.h>
#include <wtf/MainThread.h>

namespace WebKit {

using namespace WebCore;

template<class T> class MainThreadGuardedInvoker {
public:
    static void call(PassRefPtr<T> objectToGuard, const Function<void()>& function)
    {
        MainThreadGuardedInvoker<T>* invoker = new MainThreadGuardedInvoker<T>(objectToGuard, function);
        callOnMainThread(invoke, invoker);
    }

private:
    MainThreadGuardedInvoker(PassRefPtr<T> object, const Function<void()>& newFunction)
        : objectToGuard(object)
        , function(newFunction)
    {
    }

    RefPtr<T> objectToGuard;
    Function<void()> function;
    static void invoke(void* data)
    {
        MainThreadGuardedInvoker<T>* invoker = static_cast<MainThreadGuardedInvoker<T>*>(data);
        invoker->function();
        delete invoker;
    }
};

void LayerTreeRenderer::callOnMainTread(const Function<void()>& function)
{
    if (isMainThread())
        function();
    else
        MainThreadGuardedInvoker<LayerTreeRenderer>::call(this, function);
}

static FloatPoint boundedScrollPosition(const FloatPoint& scrollPosition, const FloatRect& visibleContentRect, const FloatSize& contentSize)
{
    float scrollPositionX = std::max(scrollPosition.x(), 0.0f);
    scrollPositionX = std::min(scrollPositionX, contentSize.width() - visibleContentRect.width());

    float scrollPositionY = std::max(scrollPosition.y(), 0.0f);
    scrollPositionY = std::min(scrollPositionY, contentSize.height() - visibleContentRect.height());
    return FloatPoint(scrollPositionX, scrollPositionY);
}

LayerTreeRenderer::LayerTreeRenderer(LayerTreeCoordinatorProxy* layerTreeCoordinatorProxy)
    : m_layerTreeCoordinatorProxy(layerTreeCoordinatorProxy)
    , m_rootLayerID(InvalidWebLayerID)
    , m_isActive(false)
{
}

LayerTreeRenderer::~LayerTreeRenderer()
{
}

PassOwnPtr<GraphicsLayer> LayerTreeRenderer::createLayer(WebLayerID layerID)
{
    GraphicsLayer* newLayer = new GraphicsLayerTextureMapper(this);
    TextureMapperLayer* layer = toTextureMapperLayer(newLayer);
    layer->setShouldUpdateBackingStoreFromLayer(false);
    return adoptPtr(newLayer);
}

void LayerTreeRenderer::paintToCurrentGLContext(const TransformationMatrix& matrix, float opacity, const FloatRect& clipRect, TextureMapper::PaintFlags PaintFlags)
{
    if (!m_textureMapper)
        m_textureMapper = TextureMapper::create(TextureMapper::OpenGLMode);
    ASSERT(m_textureMapper->accelerationMode() == TextureMapper::OpenGLMode);

    adjustPositionForFixedLayers();
    GraphicsLayer* currentRootLayer = rootLayer();
    if (!currentRootLayer)
        return;

    TextureMapperLayer* layer = toTextureMapperLayer(currentRootLayer);

    if (!layer)
        return;

    layer->setTextureMapper(m_textureMapper.get());
    m_textureMapper->beginPainting(PaintFlags);
    m_textureMapper->beginClip(TransformationMatrix(), clipRect);

    if (currentRootLayer->opacity() != opacity || currentRootLayer->transform() != matrix) {
        currentRootLayer->setOpacity(opacity);
        currentRootLayer->setTransform(matrix);
        currentRootLayer->syncCompositingStateForThisLayerOnly();
    }

    layer->paint();
    m_textureMapper->endClip();
    m_textureMapper->endPainting();
}

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
    if (m_layerTreeCoordinatorProxy)
        m_layerTreeCoordinatorProxy->updateViewport();
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

    LayerMap::iterator end = m_fixedLayers.end();
    for (LayerMap::iterator it = m_fixedLayers.begin(); it != end; ++it)
        toTextureMapperLayer(it->second)->setScrollPositionDeltaIfNeeded(delta);
}

void LayerTreeRenderer::didChangeScrollPosition(const IntPoint& position)
{
    m_pendingRenderedContentsScrollPosition = position;
}

void LayerTreeRenderer::syncCanvas(WebLayerID id, const WebCore::IntSize& canvasSize, uint64_t graphicsSurfaceToken, uint32_t frontBuffer)
{
    if (canvasSize.isEmpty() || !m_textureMapper)
        return;

#if USE(GRAPHICS_SURFACE)
    ensureLayer(id);
    GraphicsLayer* layer = layerByID(id);

    RefPtr<TextureMapperSurfaceBackingStore> canvasBackingStore;
    SurfaceBackingStoreMap::iterator it = m_surfaceBackingStores.find(id);
    if (it == m_surfaceBackingStores.end()) {
        canvasBackingStore = TextureMapperSurfaceBackingStore::create();
        m_surfaceBackingStores.set(id, canvasBackingStore);
    } else
        canvasBackingStore = it->second;

    canvasBackingStore->setGraphicsSurface(graphicsSurfaceToken, canvasSize, frontBuffer);
    layer->setContentsToMedia(canvasBackingStore.get());
#endif
}

void LayerTreeRenderer::setLayerChildren(WebLayerID id, const Vector<WebLayerID>& childIDs)
{
    ensureLayer(id);
    LayerMap::iterator it = m_layers.find(id);
    GraphicsLayer* layer = it->second;
    Vector<GraphicsLayer*> children;

    for (size_t i = 0; i < childIDs.size(); ++i) {
        WebLayerID childID = childIDs[i];
        GraphicsLayer* child = layerByID(childID);
        if (!child) {
            child = createLayer(childID).leakPtr();
            m_layers.add(childID, child);
        }
        children.append(child);
    }
    layer->setChildren(children);
}

#if ENABLE(CSS_FILTERS)
void LayerTreeRenderer::setLayerFilters(WebLayerID id, const FilterOperations& filters)
{
    ensureLayer(id);
    LayerMap::iterator it = m_layers.find(id);
    ASSERT(it != m_layers.end());

    GraphicsLayer* layer = it->second;
    layer->setFilters(filters);
}
#endif

void LayerTreeRenderer::setLayerState(WebLayerID id, const WebLayerInfo& layerInfo)
{
    ensureLayer(id);
    LayerMap::iterator it = m_layers.find(id);
    ASSERT(it != m_layers.end());

    GraphicsLayer* layer = it->second;

    layer->setReplicatedByLayer(layerByID(layerInfo.replica));
    layer->setMaskLayer(layerByID(layerInfo.mask));

    layer->setPosition(layerInfo.pos);
    layer->setSize(layerInfo.size);
    layer->setTransform(layerInfo.transform);
    layer->setAnchorPoint(layerInfo.anchorPoint);
    layer->setChildrenTransform(layerInfo.childrenTransform);
    layer->setBackfaceVisibility(layerInfo.backfaceVisible);
    layer->setContentsOpaque(layerInfo.contentsOpaque);
    layer->setContentsRect(layerInfo.contentsRect);
    layer->setDrawsContent(layerInfo.drawsContent);
    layer->setContentsVisible(layerInfo.contentsVisible);
    toGraphicsLayerTextureMapper(layer)->setFixedToViewport(layerInfo.fixedToViewport);

    if (layerInfo.fixedToViewport)
        m_fixedLayers.add(id, layer);
    else
        m_fixedLayers.remove(id);

    assignImageToLayer(layer, layerInfo.imageBackingStoreID);

    // Never make the root layer clip.
    layer->setMasksToBounds(layerInfo.isRootLayer ? false : layerInfo.masksToBounds);
    layer->setOpacity(layerInfo.opacity);
    layer->setPreserves3D(layerInfo.preserves3D);
    if (layerInfo.isRootLayer && m_rootLayerID != id)
        setRootLayerID(id);
}

void LayerTreeRenderer::deleteLayer(WebLayerID layerID)
{
    GraphicsLayer* layer = layerByID(layerID);
    if (!layer)
        return;

    layer->removeFromParent();
    m_layers.remove(layerID);
    m_fixedLayers.remove(layerID);
#if USE(GRAPHICS_SURFACE)
    m_surfaceBackingStores.remove(layerID);
#endif
    delete layer;
}


void LayerTreeRenderer::ensureLayer(WebLayerID id)
{
    // We have to leak the new layer's pointer and manage it ourselves,
    // because OwnPtr is not copyable.
    if (m_layers.find(id) == m_layers.end())
        m_layers.add(id, createLayer(id).leakPtr());
}

void LayerTreeRenderer::setRootLayerID(WebLayerID layerID)
{
    if (layerID == m_rootLayerID)
        return;

    m_rootLayerID = layerID;

    m_rootLayer->removeAllChildren();

    if (!layerID)
        return;

    GraphicsLayer* layer = layerByID(layerID);
    if (!layer)
        return;

    m_rootLayer->addChild(layer);
}

PassRefPtr<CoordinatedBackingStore> LayerTreeRenderer::getBackingStore(WebLayerID id)
{
    TextureMapperLayer* layer = toTextureMapperLayer(layerByID(id));
    ASSERT(layer);
    RefPtr<CoordinatedBackingStore> backingStore = static_cast<CoordinatedBackingStore*>(layer->backingStore().get());
    if (!backingStore) {
        backingStore = CoordinatedBackingStore::create();
        layer->setBackingStore(backingStore.get());
    }
    ASSERT(backingStore);
    return backingStore;
}

void LayerTreeRenderer::createTile(WebLayerID layerID, int tileID, float scale)
{
    getBackingStore(layerID)->createTile(tileID, scale);
}

void LayerTreeRenderer::removeTile(WebLayerID layerID, int tileID)
{
    getBackingStore(layerID)->removeTile(tileID);
}

void LayerTreeRenderer::updateTile(WebLayerID layerID, int tileID, const TileUpdate& update)
{
    RefPtr<CoordinatedBackingStore> backingStore = getBackingStore(layerID);
    backingStore->updateTile(tileID, update.sourceRect, update.targetRect, update.surface, update.offset);
    m_backingStoresWithPendingBuffers.add(backingStore);
}

void LayerTreeRenderer::createImage(int64_t imageID, PassRefPtr<ShareableBitmap> weakBitmap)
{
    RefPtr<ShareableBitmap> bitmap = weakBitmap;
    RefPtr<TextureMapperTiledBackingStore> backingStore = TextureMapperTiledBackingStore::create();
    m_directlyCompositedImages.set(imageID, backingStore);
    backingStore->updateContents(m_textureMapper.get(), bitmap->createImage().get());
}

void LayerTreeRenderer::destroyImage(int64_t imageID)
{
    m_directlyCompositedImages.remove(imageID);
}

void LayerTreeRenderer::assignImageToLayer(GraphicsLayer* layer, int64_t imageID)
{
    if (!imageID) {
        layer->setContentsToMedia(0);
        return;
    }

    HashMap<int64_t, RefPtr<TextureMapperBackingStore> >::iterator it = m_directlyCompositedImages.find(imageID);
    ASSERT(it != m_directlyCompositedImages.end());
    layer->setContentsToMedia(it->second.get());
}

void LayerTreeRenderer::commitTileOperations()
{
    HashSet<RefPtr<CoordinatedBackingStore> >::iterator end = m_backingStoresWithPendingBuffers.end();
    for (HashSet<RefPtr<CoordinatedBackingStore> >::iterator it = m_backingStoresWithPendingBuffers.begin(); it != end; ++it)
        (*it)->commitTileOperations(m_textureMapper.get());

    m_backingStoresWithPendingBuffers.clear();
}

void LayerTreeRenderer::flushLayerChanges()
{
    m_renderedContentsScrollPosition = m_pendingRenderedContentsScrollPosition;

    m_rootLayer->syncCompositingState(FloatRect());
    commitTileOperations();

    // The pending tiles state is on its way for the screen, tell the web process to render the next one.
    callOnMainThread(bind(&LayerTreeRenderer::renderNextFrame, this));
}

void LayerTreeRenderer::renderNextFrame()
{
    if (m_layerTreeCoordinatorProxy)
        m_layerTreeCoordinatorProxy->renderNextFrame();
}

void LayerTreeRenderer::ensureRootLayer()
{
    if (m_rootLayer)
        return;
    if (!m_textureMapper) {
        m_textureMapper = TextureMapper::create(TextureMapper::OpenGLMode);
        static_cast<TextureMapperGL*>(m_textureMapper.get())->setEnableEdgeDistanceAntialiasing(true);
    }

    m_rootLayer = createLayer(InvalidWebLayerID);
    m_rootLayer->setMasksToBounds(false);
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setAnchorPoint(FloatPoint3D(0, 0, 0));

    // The root layer should not have zero size, or it would be optimized out.
    m_rootLayer->setSize(FloatSize(1.0, 1.0));
    toTextureMapperLayer(m_rootLayer.get())->setTextureMapper(m_textureMapper.get());
}

void LayerTreeRenderer::syncRemoteContent()
{
    // We enqueue messages and execute them during paint, as they require an active GL context.
    ensureRootLayer();

    for (size_t i = 0; i < m_renderQueue.size(); ++i)
        m_renderQueue[i]();

    m_renderQueue.clear();
}

void LayerTreeRenderer::purgeGLResources()
{
    TextureMapperLayer* layer = toTextureMapperLayer(rootLayer());

    if (layer)
        layer->clearBackingStoresRecursive();

    m_directlyCompositedImages.clear();
#if USE(GRAPHICS_SURFACE)
    m_surfaceBackingStores.clear();
#endif

    m_rootLayer->removeAllChildren();
    m_rootLayer.clear();
    m_rootLayerID = InvalidWebLayerID;
    m_layers.clear();
    m_fixedLayers.clear();
    m_textureMapper.clear();
    m_backingStoresWithPendingBuffers.clear();

    setActive(false);

    callOnMainThread(bind(&LayerTreeRenderer::purgeBackingStores, this));
}

void LayerTreeRenderer::setAnimatedOpacity(uint32_t id, float opacity)
{
    GraphicsLayer* layer = layerByID(id);
    ASSERT(layer);

    layer->setOpacity(opacity);
}

void LayerTreeRenderer::setAnimatedTransform(uint32_t id, const WebCore::TransformationMatrix& transform)
{
    GraphicsLayer* layer = layerByID(id);
    ASSERT(layer);

    layer->setTransform(transform);
}

void LayerTreeRenderer::purgeBackingStores()
{
    if (m_layerTreeCoordinatorProxy)
        m_layerTreeCoordinatorProxy->purgeBackingStores();
}

void LayerTreeRenderer::detach()
{
    m_layerTreeCoordinatorProxy = 0;
}

void LayerTreeRenderer::appendUpdate(const Function<void()>& function)
{
    if (!m_isActive)
        return;

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
        renderNextFrame();
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
