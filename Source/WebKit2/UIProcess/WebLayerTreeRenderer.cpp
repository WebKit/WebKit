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

#if USE(UI_SIDE_COMPOSITING)

#include "WebLayerTreeRenderer.h"

#include "GraphicsLayerTextureMapper.h"
#include "LayerBackingStore.h"
#include "LayerTreeHostProxy.h"
#include "MessageID.h"
#include "ShareableBitmap.h"
#include "TextureMapper.h"
#include "TextureMapperBackingStore.h"
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

void WebLayerTreeRenderer::callOnMainTread(const Function<void()>& function)
{
    if (isMainThread())
        function();
    else
        MainThreadGuardedInvoker<WebLayerTreeRenderer>::call(this, function);
}

static IntPoint boundedScrollPosition(const IntPoint& scrollPosition, const IntRect& visibleContentRect, const FloatSize& contentSize)
{
    IntSize size(contentSize.width(), contentSize.height());
    int scrollPositionX = std::max(scrollPosition.x(), 0);
    scrollPositionX = std::min(scrollPositionX, size.width() - visibleContentRect.width());

    int scrollPositionY = std::max(scrollPosition.y(), 0);
    scrollPositionY = std::min(scrollPositionY, size.height() - visibleContentRect.height());
    return IntPoint(scrollPositionX, scrollPositionY);
}

WebLayerTreeRenderer::WebLayerTreeRenderer(LayerTreeHostProxy* layerTreeHostProxy)
    : m_layerTreeHostProxy(layerTreeHostProxy)
    , m_rootLayerID(InvalidWebLayerID)
    , m_isActive(false)
{
}

WebLayerTreeRenderer::~WebLayerTreeRenderer()
{
}

PassOwnPtr<GraphicsLayer> WebLayerTreeRenderer::createLayer(WebLayerID layerID)
{
    GraphicsLayer* newLayer = new GraphicsLayerTextureMapper(this);
    TextureMapperLayer* layer = toTextureMapperLayer(newLayer);
    layer->setShouldUpdateBackingStoreFromLayer(false);
    return adoptPtr(newLayer);
}

void WebLayerTreeRenderer::paintToCurrentGLContext(const TransformationMatrix& matrix, float opacity, const FloatRect& clipRect, TextureMapper::PaintFlags PaintFlags)
{
    if (!m_textureMapper)
        m_textureMapper = TextureMapper::create(TextureMapper::OpenGLMode);
    ASSERT(m_textureMapper->accelerationMode() == TextureMapper::OpenGLMode);

    // We need to compensate for the rounding error that happens due to m_visibleContentsRect being
    // int and not float. We do that by moving the TransformationMatrix by the delta between the
    // position of m_visibleContentsRect and the position it would have if it wasn't rounded.
 
    TransformationMatrix newMatrix = matrix;
    newMatrix.translate(m_accurateVisibleContentsPosition.x() / m_contentsScale - m_visibleContentsRect.x(), m_accurateVisibleContentsPosition.y() / m_contentsScale - m_visibleContentsRect.y());
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

    if (currentRootLayer->opacity() != opacity || currentRootLayer->transform() != newMatrix) {
        currentRootLayer->setOpacity(opacity);
        currentRootLayer->setTransform(newMatrix);
        currentRootLayer->syncCompositingStateForThisLayerOnly();
    }

    layer->paint();
    m_textureMapper->endClip();
    m_textureMapper->endPainting();
}

void WebLayerTreeRenderer::paintToGraphicsContext(QPainter* painter)
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

void WebLayerTreeRenderer::setContentsSize(const WebCore::FloatSize& contentsSize)
{
    m_contentsSize = contentsSize;
}

void WebLayerTreeRenderer::setVisibleContentsRect(const IntRect& rect, float scale, const WebCore::FloatPoint& accurateVisibleContentsPosition)
{
    m_visibleContentsRect = rect;
    m_contentsScale = scale;
    m_accurateVisibleContentsPosition = accurateVisibleContentsPosition;
}

void WebLayerTreeRenderer::updateViewport()
{
    if (m_layerTreeHostProxy)
        m_layerTreeHostProxy->updateViewport();
}

void WebLayerTreeRenderer::adjustPositionForFixedLayers()
{
    if (m_fixedLayers.isEmpty())
        return;

    IntPoint scrollPosition = boundedScrollPosition(m_visibleContentsRect.location(), m_visibleContentsRect, m_contentsSize);

    LayerMap::iterator end = m_fixedLayers.end();
    for (LayerMap::iterator it = m_fixedLayers.begin(); it != end; ++it)
        toTextureMapperLayer(it->second)->setScrollPositionDeltaIfNeeded(IntPoint(scrollPosition.x() - m_renderedContentsScrollPosition.x(), scrollPosition.y() - m_renderedContentsScrollPosition.y()));
}

void WebLayerTreeRenderer::didChangeScrollPosition(const IntPoint& position)
{
    m_pendingRenderedContentsScrollPosition = boundedScrollPosition(position, m_visibleContentsRect, m_contentsSize);
}

void WebLayerTreeRenderer::setLayerChildren(WebLayerID id, const Vector<WebLayerID>& childIDs)
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
void WebLayerTreeRenderer::setLayerFilters(WebLayerID id, const FilterOperations& filters)
{
    ensureLayer(id);
    LayerMap::iterator it = m_layers.find(id);
    ASSERT(it != m_layers.end());

    GraphicsLayer* layer = it->second;
    layer->setFilters(filters);
}
#endif

void WebLayerTreeRenderer::setLayerState(WebLayerID id, const WebLayerInfo& layerInfo)
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

void WebLayerTreeRenderer::deleteLayer(WebLayerID layerID)
{
    GraphicsLayer* layer = layerByID(layerID);
    if (!layer)
        return;

    layer->removeFromParent();
    m_layers.remove(layerID);
    m_fixedLayers.remove(layerID);
    delete layer;
}


void WebLayerTreeRenderer::ensureLayer(WebLayerID id)
{
    // We have to leak the new layer's pointer and manage it ourselves,
    // because OwnPtr is not copyable.
    if (m_layers.find(id) == m_layers.end())
        m_layers.add(id, createLayer(id).leakPtr());
}

void WebLayerTreeRenderer::setRootLayerID(WebLayerID layerID)
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

PassRefPtr<LayerBackingStore> WebLayerTreeRenderer::getBackingStore(WebLayerID id)
{
    TextureMapperLayer* layer = toTextureMapperLayer(layerByID(id));
    ASSERT(layer);
    RefPtr<LayerBackingStore> backingStore = static_cast<LayerBackingStore*>(layer->backingStore().get());
    if (!backingStore) {
        backingStore = LayerBackingStore::create();
        layer->setBackingStore(backingStore.get());
    }
    ASSERT(backingStore);
    return backingStore;
}

void WebLayerTreeRenderer::createTile(WebLayerID layerID, int tileID, float scale)
{
    getBackingStore(layerID)->createTile(tileID, scale);
}

void WebLayerTreeRenderer::removeTile(WebLayerID layerID, int tileID)
{
    getBackingStore(layerID)->removeTile(tileID);
}

void WebLayerTreeRenderer::updateTile(WebLayerID layerID, int tileID, const TileUpdate& update)
{
    RefPtr<LayerBackingStore> backingStore = getBackingStore(layerID);
    backingStore->updateTile(tileID, update.sourceRect, update.targetRect, update.surface, update.offset);
    m_backingStoresWithPendingBuffers.add(backingStore);
}

void WebLayerTreeRenderer::createImage(int64_t imageID, PassRefPtr<ShareableBitmap> weakBitmap)
{
    RefPtr<ShareableBitmap> bitmap = weakBitmap;
    RefPtr<TextureMapperTiledBackingStore> backingStore = TextureMapperTiledBackingStore::create();
    m_directlyCompositedImages.set(imageID, backingStore);
    backingStore->updateContents(m_textureMapper.get(), bitmap->createImage().get());
}

void WebLayerTreeRenderer::destroyImage(int64_t imageID)
{
    m_directlyCompositedImages.remove(imageID);
}

void WebLayerTreeRenderer::assignImageToLayer(GraphicsLayer* layer, int64_t imageID)
{
    if (!imageID) {
        layer->setContentsToMedia(0);
        return;
    }

    HashMap<int64_t, RefPtr<TextureMapperBackingStore> >::iterator it = m_directlyCompositedImages.find(imageID);
    ASSERT(it != m_directlyCompositedImages.end());
    layer->setContentsToMedia(it->second.get());
}

void WebLayerTreeRenderer::commitTileOperations()
{
    HashSet<RefPtr<LayerBackingStore> >::iterator end = m_backingStoresWithPendingBuffers.end();
    for (HashSet<RefPtr<LayerBackingStore> >::iterator it = m_backingStoresWithPendingBuffers.begin(); it != end; ++it)
        (*it)->commitTileOperations(m_textureMapper.get());

    m_backingStoresWithPendingBuffers.clear();
}

void WebLayerTreeRenderer::flushLayerChanges()
{
    m_renderedContentsScrollPosition = m_pendingRenderedContentsScrollPosition;

    m_rootLayer->syncCompositingState(FloatRect());
    commitTileOperations();

    // The pending tiles state is on its way for the screen, tell the web process to render the next one.
    callOnMainThread(bind(&WebLayerTreeRenderer::renderNextFrame, this));
}

void WebLayerTreeRenderer::renderNextFrame()
{
    if (m_layerTreeHostProxy)
        m_layerTreeHostProxy->renderNextFrame();
}

void WebLayerTreeRenderer::ensureRootLayer()
{
    if (m_rootLayer)
        return;
    if (!m_textureMapper)
        m_textureMapper = TextureMapper::create(TextureMapper::OpenGLMode);

    m_rootLayer = createLayer(InvalidWebLayerID);
    m_rootLayer->setMasksToBounds(false);
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setAnchorPoint(FloatPoint3D(0, 0, 0));

    // The root layer should not have zero size, or it would be optimized out.
    m_rootLayer->setSize(FloatSize(1.0, 1.0));
    toTextureMapperLayer(m_rootLayer.get())->setTextureMapper(m_textureMapper.get());
}

void WebLayerTreeRenderer::syncRemoteContent()
{
    // We enqueue messages and execute them during paint, as they require an active GL context.
    ensureRootLayer();

    for (size_t i = 0; i < m_renderQueue.size(); ++i)
        m_renderQueue[i]();

    m_renderQueue.clear();
}

void WebLayerTreeRenderer::purgeGLResources()
{
    TextureMapperLayer* layer = toTextureMapperLayer(rootLayer());

    if (layer)
        layer->clearBackingStoresRecursive();

    m_directlyCompositedImages.clear();

    m_rootLayer->removeAllChildren();
    m_rootLayer.clear();
    m_rootLayerID = InvalidWebLayerID;
    m_layers.clear();
    m_fixedLayers.clear();
    m_textureMapper.clear();
    m_backingStoresWithPendingBuffers.clear();

    callOnMainThread(bind(&WebLayerTreeRenderer::purgeBackingStores, this));
}

void WebLayerTreeRenderer::purgeBackingStores()
{
    if (m_layerTreeHostProxy)
        m_layerTreeHostProxy->purgeBackingStores();
}

void WebLayerTreeRenderer::detach()
{
    m_layerTreeHostProxy = 0;
}

void WebLayerTreeRenderer::appendUpdate(const Function<void()>& function)
{
    if (!m_isActive)
        return;

    m_renderQueue.append(function);
}

void WebLayerTreeRenderer::setActive(bool active)
{
    if (m_isActive == active)
        return;

    // Have to clear render queue in both cases.
    // If there are some updates in queue during activation then those updates are from previous instance of paint node
    // and cannot be applied to the newly created instance.
    m_renderQueue.clear();
    m_isActive = active;
}

} // namespace WebKit

#endif // USE(UI_SIDE_COMPOSITING)
