/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

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
#include "WebGraphicsLayer.h"

#include "BackingStore.h"
#include "FloatQuad.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "LayerTreeHostProxyMessages.h"
#include "Page.h"
#include "TiledBackingStoreRemoteTile.h"
#include "WebPage.h"
#include <wtf/CurrentTime.h>
#include <wtf/HashMap.h>
#include <wtf/text/CString.h>

using namespace WebKit;

namespace WebCore {

static HashMap<WebLayerID, WebGraphicsLayer*>& layerByIDMap()
{
    static HashMap<WebLayerID, WebGraphicsLayer*> globalMap;
    return globalMap;
}

WebGraphicsLayer* WebGraphicsLayer::layerByID(WebKit::WebLayerID id)
{
    HashMap<WebLayerID, WebGraphicsLayer*>& table = layerByIDMap();
    HashMap<WebLayerID, WebGraphicsLayer*>::iterator it = table.find(id);
    if (it == table.end())
        return 0;
    return it->second;
}

static WebLayerID toWebLayerID(GraphicsLayer* layer)
{
    return layer ? toWebGraphicsLayer(layer)->id() : 0;
}

void WebGraphicsLayer::didChangeLayerState()
{
    m_shouldSyncLayerState = true;
    if (client())
        client()->notifySyncRequired(this);
}

void WebGraphicsLayer::didChangeChildren()
{
    m_shouldSyncChildren = true;
    if (client())
        client()->notifySyncRequired(this);
}

#if ENABLE(CSS_FILTERS)
void WebGraphicsLayer::didChangeFilters()
{
    m_shouldSyncFilters = true;
    if (client())
        client()->notifySyncRequired(this);
}
#endif

void WebGraphicsLayer::setShouldUpdateVisibleRect()
{
    if (!transform().isAffine())
        return;

    m_shouldUpdateVisibleRect = true;
    for (size_t i = 0; i < children().size(); ++i)
        toWebGraphicsLayer(children()[i])->setShouldUpdateVisibleRect();
    if (replicaLayer())
        toWebGraphicsLayer(replicaLayer())->setShouldUpdateVisibleRect();
}

void WebGraphicsLayer::didChangeGeometry()
{
    didChangeLayerState();
    setShouldUpdateVisibleRect();
}

WebGraphicsLayer::WebGraphicsLayer(GraphicsLayerClient* client)
    : GraphicsLayer(client)
    , m_maskTarget(0)
    , m_inUpdateMode(false)
    , m_shouldUpdateVisibleRect(true)
    , m_shouldSyncLayerState(true)
    , m_shouldSyncChildren(true)
    , m_fixedToViewport(false)
    , m_webGraphicsLayerClient(0)
    , m_contentsScale(1)
{
    static WebLayerID nextLayerID = 1;
    m_id = nextLayerID++;
    layerByIDMap().add(id(), this);
}

WebGraphicsLayer::~WebGraphicsLayer()
{
    layerByIDMap().remove(id());

    if (m_webGraphicsLayerClient) {
        purgeBackingStores();
        m_webGraphicsLayerClient->detachLayer(this);
    }
    willBeDestroyed();
}

void WebGraphicsLayer::willBeDestroyed()
{
    GraphicsLayer::willBeDestroyed();
}

bool WebGraphicsLayer::setChildren(const Vector<GraphicsLayer*>& children)
{
    bool ok = GraphicsLayer::setChildren(children);
    if (!ok)
        return false;
    for (int i = 0; i < children.size(); ++i) {
        WebGraphicsLayer* child = toWebGraphicsLayer(children[i]);
        child->setWebGraphicsLayerClient(m_webGraphicsLayerClient);
        child->didChangeLayerState();
    }
    didChangeChildren();
    return true;
}

void WebGraphicsLayer::addChild(GraphicsLayer* layer)
{
    GraphicsLayer::addChild(layer);
    toWebGraphicsLayer(layer)->setWebGraphicsLayerClient(m_webGraphicsLayerClient);
    toWebGraphicsLayer(layer)->didChangeLayerState();
    didChangeChildren();
}

void WebGraphicsLayer::addChildAtIndex(GraphicsLayer* layer, int index)
{
    GraphicsLayer::addChildAtIndex(layer, index);
    toWebGraphicsLayer(layer)->setWebGraphicsLayerClient(m_webGraphicsLayerClient);
    toWebGraphicsLayer(layer)->didChangeLayerState();
    didChangeChildren();
}

void WebGraphicsLayer::addChildAbove(GraphicsLayer* layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildAbove(layer, sibling);
    toWebGraphicsLayer(layer)->setWebGraphicsLayerClient(m_webGraphicsLayerClient);
    toWebGraphicsLayer(layer)->didChangeLayerState();
    didChangeChildren();
}

void WebGraphicsLayer::addChildBelow(GraphicsLayer* layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(layer, sibling);
    toWebGraphicsLayer(layer)->setWebGraphicsLayerClient(m_webGraphicsLayerClient);
    toWebGraphicsLayer(layer)->didChangeLayerState();
    didChangeChildren();
}

bool WebGraphicsLayer::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    bool ok = GraphicsLayer::replaceChild(oldChild, newChild);
    if (!ok)
        return false;
    didChangeChildren();
    toWebGraphicsLayer(oldChild)->didChangeLayerState();
    toWebGraphicsLayer(newChild)->setWebGraphicsLayerClient(m_webGraphicsLayerClient);
    toWebGraphicsLayer(newChild)->didChangeLayerState();
    return true;
}

void WebGraphicsLayer::removeFromParent()
{
    if (WebGraphicsLayer* parentLayer = toWebGraphicsLayer(parent()))
        parentLayer->didChangeChildren();
    GraphicsLayer::removeFromParent();

    didChangeLayerState();
}

void WebGraphicsLayer::setPosition(const FloatPoint& p)
{
    if (position() == p)
        return;

    GraphicsLayer::setPosition(p);
    didChangeGeometry();
}

void WebGraphicsLayer::setAnchorPoint(const FloatPoint3D& p)
{
    if (anchorPoint() == p)
        return;

    GraphicsLayer::setAnchorPoint(p);
    didChangeGeometry();
}

void WebGraphicsLayer::setSize(const FloatSize& size)
{
    if (this->size() == size)
        return;

    GraphicsLayer::setSize(size);
    setNeedsDisplay();
    if (maskLayer())
        maskLayer()->setSize(size);
    didChangeGeometry();
}

void WebGraphicsLayer::setTransform(const TransformationMatrix& t)
{
    if (transform() == t)
        return;

    GraphicsLayer::setTransform(t);
    didChangeGeometry();
}

void WebGraphicsLayer::setChildrenTransform(const TransformationMatrix& t)
{
    if (childrenTransform() == t)
        return;

    GraphicsLayer::setChildrenTransform(t);
    didChangeGeometry();
}

void WebGraphicsLayer::setPreserves3D(bool b)
{
    if (preserves3D() == b)
        return;

    GraphicsLayer::setPreserves3D(b);
    didChangeGeometry();
}

void WebGraphicsLayer::setMasksToBounds(bool b)
{
    if (masksToBounds() == b)
        return;
    GraphicsLayer::setMasksToBounds(b);
    didChangeGeometry();
}

void WebGraphicsLayer::setDrawsContent(bool b)
{
    if (drawsContent() == b)
        return;
    GraphicsLayer::setDrawsContent(b);

    didChangeLayerState();
}

void WebGraphicsLayer::setContentsOpaque(bool b)
{
    if (contentsOpaque() == b)
        return;
    if (m_mainBackingStore)
        m_mainBackingStore->setSupportsAlpha(!b);
    GraphicsLayer::setContentsOpaque(b);
    didChangeLayerState();
}

void WebGraphicsLayer::setBackfaceVisibility(bool b)
{
    if (backfaceVisibility() == b)
        return;

    GraphicsLayer::setBackfaceVisibility(b);
    didChangeLayerState();
}

void WebGraphicsLayer::setOpacity(float opacity)
{
    if (this->opacity() == opacity)
        return;

    GraphicsLayer::setOpacity(opacity);
    didChangeLayerState();
}

void WebGraphicsLayer::setContentsRect(const IntRect& r)
{
    if (contentsRect() == r)
        return;

    GraphicsLayer::setContentsRect(r);
    didChangeLayerState();
}

void WebGraphicsLayer::setContentsNeedsDisplay()
{
    RefPtr<Image> image = m_image;
    setContentsToImage(0);
    setContentsToImage(image.get());
}

#if ENABLE(CSS_FILTERS)
bool WebGraphicsLayer::setFilters(const FilterOperations& newFilters)
{
    if (filters() == newFilters)
        return true;
    didChangeFilters();
    return GraphicsLayer::setFilters(newFilters);
}
#endif


void WebGraphicsLayer::setContentsToImage(Image* image)
{
    if (image == m_image)
        return;
    int64_t newID = 0;
    if (m_webGraphicsLayerClient) {
        // We adopt first, in case this is the same frame - that way we avoid destroying and recreating the image.
        newID = m_webGraphicsLayerClient->adoptImageBackingStore(image);
        m_webGraphicsLayerClient->releaseImageBackingStore(m_layerInfo.imageBackingStoreID);
        didChangeLayerState();
        if (m_layerInfo.imageBackingStoreID && newID == m_layerInfo.imageBackingStoreID)
            return;
    } else {
        // If m_webGraphicsLayerClient is not set yet there should be no backing store ID.
        ASSERT(!m_layerInfo.imageBackingStoreID);
        didChangeLayerState();
    }

    m_layerInfo.imageBackingStoreID = newID;
    m_image = image;
    GraphicsLayer::setContentsToImage(image);
}

void WebGraphicsLayer::setMaskLayer(GraphicsLayer* layer)
{
    if (layer == maskLayer())
        return;

    GraphicsLayer::setMaskLayer(layer);

    if (!layer)
        return;

    layer->setSize(size());
    WebGraphicsLayer* webGraphicsLayer = toWebGraphicsLayer(layer);
    webGraphicsLayer->setWebGraphicsLayerClient(m_webGraphicsLayerClient);
    webGraphicsLayer->setMaskTarget(this);
    webGraphicsLayer->didChangeLayerState();
    didChangeLayerState();

}

void WebGraphicsLayer::setReplicatedByLayer(GraphicsLayer* layer)
{
    if (layer == replicaLayer())
        return;

    if (layer)
        toWebGraphicsLayer(layer)->setWebGraphicsLayerClient(m_webGraphicsLayerClient);

    GraphicsLayer::setReplicatedByLayer(layer);
    didChangeLayerState();
}

void WebGraphicsLayer::setNeedsDisplay()
{
    setNeedsDisplayInRect(IntRect(IntPoint::zero(), IntSize(size().width(), size().height())));
}

void WebGraphicsLayer::setNeedsDisplayInRect(const FloatRect& rect)
{
    if (m_mainBackingStore)
        m_mainBackingStore->invalidate(IntRect(rect));
    didChangeLayerState();
}

WebLayerID WebGraphicsLayer::id() const
{
    return m_id;
}

void WebGraphicsLayer::syncCompositingState(const FloatRect& rect)
{
    if (WebGraphicsLayer* mask = toWebGraphicsLayer(maskLayer()))
        mask->syncCompositingStateForThisLayerOnly();

    if (WebGraphicsLayer* replica = toWebGraphicsLayer(replicaLayer()))
        replica->syncCompositingStateForThisLayerOnly();

    m_webGraphicsLayerClient->syncFixedLayers();

    syncCompositingStateForThisLayerOnly();

    for (size_t i = 0; i < children().size(); ++i)
        children()[i]->syncCompositingState(rect);
}

WebGraphicsLayer* toWebGraphicsLayer(GraphicsLayer* layer)
{
    return static_cast<WebGraphicsLayer*>(layer);
}

void WebGraphicsLayer::syncChildren()
{
    if (!m_shouldSyncChildren)
        return;
    m_shouldSyncChildren = false;
    Vector<WebLayerID> childIDs;
    for (size_t i = 0; i < children().size(); ++i)
        childIDs.append(toWebLayerID(children()[i]));

    m_webGraphicsLayerClient->syncLayerChildren(m_id, childIDs);
}

#if ENABLE(CSS_FILTERS)
void WebGraphicsLayer::syncFilters()
{
    if (!m_shouldSyncFilters)
        return;
    m_shouldSyncFilters = false;
    m_webGraphicsLayerClient->syncLayerFilters(m_id, filters());
}
#endif

void WebGraphicsLayer::syncLayerState()
 {
    if (!m_shouldSyncLayerState)
        return;
    m_shouldSyncLayerState = false;
    m_layerInfo.fixedToViewport = fixedToViewport();

    m_layerInfo.anchorPoint = anchorPoint();
    m_layerInfo.backfaceVisible = backfaceVisibility();
    m_layerInfo.childrenTransform = childrenTransform();
    m_layerInfo.contentsOpaque = contentsOpaque();
    m_layerInfo.contentsRect = contentsRect();
    m_layerInfo.drawsContent = drawsContent();
    m_layerInfo.mask = toWebLayerID(maskLayer());
    m_layerInfo.masksToBounds = masksToBounds();
    m_layerInfo.opacity = opacity();
    m_layerInfo.parent = toWebLayerID(parent());
    m_layerInfo.pos = position();
    m_layerInfo.preserves3D = preserves3D();
    m_layerInfo.replica = toWebLayerID(replicaLayer());
    m_layerInfo.size = size();
    m_layerInfo.transform = transform();
    m_webGraphicsLayerClient->syncLayerState(m_id, m_layerInfo);
}

void WebGraphicsLayer::ensureImageBackingStore()
{
    if (!m_image)
        return;
    if (!m_layerInfo.imageBackingStoreID)
        m_layerInfo.imageBackingStoreID = m_webGraphicsLayerClient->adoptImageBackingStore(m_image.get());
}
void WebGraphicsLayer::syncCompositingStateForThisLayerOnly()
{
    // The remote image might have been released by purgeBackingStores.
    ensureImageBackingStore();
    computeTransformedVisibleRect();
    syncChildren();
    syncLayerState();
#if ENABLE(CSS_FILTERS)
    syncFilters();
#endif
    updateContentBuffers();
}

void WebGraphicsLayer::tiledBackingStorePaintBegin()
{
}

void WebGraphicsLayer::setRootLayer(bool isRoot)
{
    m_layerInfo.isRootLayer = isRoot;
    didChangeLayerState();
}

void WebGraphicsLayer::setVisibleContentRectTrajectoryVector(const FloatPoint& trajectoryVector)
{
    if (m_mainBackingStore)
        m_mainBackingStore->coverWithTilesIfNeeded(trajectoryVector);
}

void WebGraphicsLayer::setContentsScale(float scale)
{
    m_contentsScale = scale;
    adjustContentsScale();
}

float WebGraphicsLayer::effectiveContentsScale()
{
    return shouldUseTiledBackingStore() ? m_contentsScale : 1;
}

void WebGraphicsLayer::adjustContentsScale()
{
    if (!drawsContent())
        return;

    if (!m_mainBackingStore || m_mainBackingStore->contentsScale() == effectiveContentsScale())
        return;

    // Between creating the new backing store and painting the content,
    // we do not want to drop the previous one as that might result in
    // briefly seeing flickering as the old tiles may be dropped before
    // something replaces them.
    m_previousBackingStore = m_mainBackingStore.release();

    // No reason to save the previous backing store for non-visible areas.
    m_previousBackingStore->removeAllNonVisibleTiles();

    createBackingStore();
}

void WebGraphicsLayer::createBackingStore()
{
    m_mainBackingStore = adoptPtr(new TiledBackingStore(this, TiledBackingStoreRemoteTileBackend::create(this)));
    m_mainBackingStore->setSupportsAlpha(!contentsOpaque());
    m_mainBackingStore->setContentsScale(effectiveContentsScale());
}

void WebGraphicsLayer::tiledBackingStorePaint(GraphicsContext* context, const IntRect& rect)
{
    if (rect.isEmpty())
        return;
    paintGraphicsLayerContents(*context, rect);
}

void WebGraphicsLayer::tiledBackingStorePaintEnd(const Vector<IntRect>& updatedRects)
{
}

bool WebGraphicsLayer::tiledBackingStoreUpdatesAllowed() const
{
    if (!m_inUpdateMode)
        return false;
    return m_webGraphicsLayerClient->layerTreeTileUpdatesAllowed();
}

IntRect WebGraphicsLayer::tiledBackingStoreContentsRect()
{
    return IntRect(0, 0, size().width(), size().height());
}

bool WebGraphicsLayer::shouldUseTiledBackingStore()
{
    return !selfOrAncestorHaveNonAffineTransforms();
}

IntRect WebGraphicsLayer::tiledBackingStoreVisibleRect()
{
    if (!shouldUseTiledBackingStore())
        return tiledBackingStoreContentsRect();

    // Non-invertible layers are not visible.
    if (!m_layerTransform.combined().isInvertible())
        return IntRect();

    // Return a projection of the visible rect (surface coordinates) onto the layer's plane (layer coordinates).
    // The resulting quad might be squewed and the visible rect is the bounding box of this quad,
    // so it might spread further than the real visible area (and then even more amplified by the cover rect multiplier).
    return enclosingIntRect(m_layerTransform.combined().inverse().clampedBoundsOfProjectedQuad(FloatQuad(FloatRect(m_webGraphicsLayerClient->visibleContentsRect()))));
}

Color WebGraphicsLayer::tiledBackingStoreBackgroundColor() const
{
    return contentsOpaque() ? Color::white : Color::transparent;
}

PassOwnPtr<WebCore::GraphicsContext> WebGraphicsLayer::beginContentUpdate(const WebCore::IntSize& size, ShareableSurface::Handle& handle, WebCore::IntPoint& offset)
{
    return m_webGraphicsLayerClient->beginContentUpdate(size, contentsOpaque() ? 0 : ShareableBitmap::SupportsAlpha, handle, offset);
}

void WebGraphicsLayer::createTile(int tileID, const SurfaceUpdateInfo& updateInfo, const IntRect& targetRect)
{
    m_webGraphicsLayerClient->createTile(id(), tileID, updateInfo, targetRect);
}

void WebGraphicsLayer::updateTile(int tileID, const SurfaceUpdateInfo& updateInfo, const IntRect& targetRect)
{
    m_webGraphicsLayerClient->updateTile(id(), tileID, updateInfo, targetRect);
}

void WebGraphicsLayer::removeTile(int tileID)
{
    m_webGraphicsLayerClient->removeTile(id(), tileID);
}

void WebGraphicsLayer::updateContentBuffers()
{
    if (!drawsContent()) {
        m_mainBackingStore.clear();
        m_previousBackingStore.clear();
        return;
    }

    m_inUpdateMode = true;
    // This is the only place we (re)create the main tiled backing store, once we
    // have a remote client and we are ready to send our data to the UI process.
    if (!m_mainBackingStore)
        createBackingStore();
    m_mainBackingStore->updateTileBuffers();
    m_inUpdateMode = false;

    // The previous backing store is kept around to avoid flickering between
    // removing the existing tiles and painting the new ones. The first time
    // the visibleRect is full painted we remove the previous backing store.
    if (m_mainBackingStore->visibleAreaIsCovered())
        m_previousBackingStore.clear();
}

void WebGraphicsLayer::purgeBackingStores()
{
    m_mainBackingStore.clear();
    m_previousBackingStore.clear();

    if (m_layerInfo.imageBackingStoreID) {
        m_webGraphicsLayerClient->releaseImageBackingStore(m_layerInfo.imageBackingStoreID);
        m_layerInfo.imageBackingStoreID = 0;
    }

    didChangeLayerState();
    didChangeChildren();
}

void WebGraphicsLayer::setWebGraphicsLayerClient(WebKit::WebGraphicsLayerClient* client)
{
    if (m_webGraphicsLayerClient == client)
        return;

    if (WebGraphicsLayer* replica = toWebGraphicsLayer(replicaLayer()))
        replica->setWebGraphicsLayerClient(client);
    if (WebGraphicsLayer* mask = toWebGraphicsLayer(maskLayer()))
        mask->setWebGraphicsLayerClient(client);
    for (size_t i = 0; i < children().size(); ++i) {
        WebGraphicsLayer* layer = toWebGraphicsLayer(this->children()[i]);
        layer->setWebGraphicsLayerClient(client);
    }

    // We have to release resources on the UI process here if the remote client has changed or is removed.
    if (m_webGraphicsLayerClient) {
        purgeBackingStores();
        m_webGraphicsLayerClient->detachLayer(this);
    }
    m_webGraphicsLayerClient = client;
    if (client)
        client->attachLayer(this);
}

void WebGraphicsLayer::adjustVisibleRect()
{
    if (m_mainBackingStore)
        m_mainBackingStore->coverWithTilesIfNeeded();
}

void WebGraphicsLayer::computeTransformedVisibleRect()
{
    if (!m_shouldUpdateVisibleRect)
        return;
    m_shouldUpdateVisibleRect = false;
    m_layerTransform.setLocalTransform(transform());
    m_layerTransform.setPosition(position());
    m_layerTransform.setAnchorPoint(anchorPoint());
    m_layerTransform.setSize(size());
    m_layerTransform.setFlattening(!preserves3D());
    m_layerTransform.setChildrenTransform(childrenTransform());
    m_layerTransform.combineTransforms(parent() ? toWebGraphicsLayer(parent())->m_layerTransform.combinedForChildren() : TransformationMatrix());

    // The combined transform will be used in tiledBackingStoreVisibleRect.
    adjustVisibleRect();
    adjustContentsScale();
}

static PassOwnPtr<GraphicsLayer> createWebGraphicsLayer(GraphicsLayerClient* client)
{
    return adoptPtr(new WebGraphicsLayer(client));
}

void WebGraphicsLayer::initFactory()
{
    GraphicsLayer::setGraphicsLayerFactory(createWebGraphicsLayer);
}

bool WebGraphicsLayer::selfOrAncestorHaveNonAffineTransforms()
{
    if (!m_layerTransform.combined().isAffine())
        return true;

    return false;
}

}
#endif
