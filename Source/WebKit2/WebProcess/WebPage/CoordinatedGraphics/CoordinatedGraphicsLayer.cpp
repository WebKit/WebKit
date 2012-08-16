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

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedGraphicsLayer.h"

#include "BackingStore.h"
#include "CoordinatedTile.h"
#include "FloatQuad.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "LayerTreeCoordinatorProxyMessages.h"
#include "Page.h"
#include "TextureMapperPlatformLayer.h"
#include "WebPage.h"
#include <wtf/CurrentTime.h>
#include <wtf/HashMap.h>
#include <wtf/text/CString.h>

using namespace WebKit;

namespace WebCore {

static HashMap<WebLayerID, CoordinatedGraphicsLayer*>& layerByIDMap()
{
    static HashMap<WebLayerID, CoordinatedGraphicsLayer*> globalMap;
    return globalMap;
}

CoordinatedGraphicsLayer* CoordinatedGraphicsLayer::layerByID(WebKit::WebLayerID id)
{
    HashMap<WebLayerID, CoordinatedGraphicsLayer*>& table = layerByIDMap();
    HashMap<WebLayerID, CoordinatedGraphicsLayer*>::iterator it = table.find(id);
    if (it == table.end())
        return 0;
    return it->second;
}

static WebLayerID toWebLayerID(GraphicsLayer* layer)
{
    return layer ? toCoordinatedGraphicsLayer(layer)->id() : 0;
}

void CoordinatedGraphicsLayer::didChangeLayerState()
{
    m_shouldSyncLayerState = true;
    if (client())
        client()->notifySyncRequired(this);
}

void CoordinatedGraphicsLayer::didChangeAnimatedProperties()
{
    m_shouldSyncAnimatedProperties = true;
    if (client())
        client()->notifySyncRequired(this);
}

void CoordinatedGraphicsLayer::didChangeChildren()
{
    m_shouldSyncChildren = true;
    if (client())
        client()->notifySyncRequired(this);
}

#if ENABLE(CSS_FILTERS)
void CoordinatedGraphicsLayer::didChangeFilters()
{
    m_shouldSyncFilters = true;
    if (client())
        client()->notifySyncRequired(this);
}
#endif

void CoordinatedGraphicsLayer::setShouldUpdateVisibleRect()
{
    if (!transform().isAffine())
        return;

    m_shouldUpdateVisibleRect = true;
    for (size_t i = 0; i < children().size(); ++i)
        toCoordinatedGraphicsLayer(children()[i])->setShouldUpdateVisibleRect();
    if (replicaLayer())
        toCoordinatedGraphicsLayer(replicaLayer())->setShouldUpdateVisibleRect();
}

void CoordinatedGraphicsLayer::didChangeGeometry()
{
    didChangeLayerState();
    setShouldUpdateVisibleRect();
}

CoordinatedGraphicsLayer::CoordinatedGraphicsLayer(GraphicsLayerClient* client)
    : GraphicsLayer(client)
    , m_maskTarget(0)
    , m_inUpdateMode(false)
    , m_shouldUpdateVisibleRect(true)
    , m_shouldSyncLayerState(true)
    , m_shouldSyncChildren(true)
    , m_shouldSyncFilters(true)
    , m_shouldSyncAnimatedProperties(true)
    , m_fixedToViewport(false)
    , m_canvasNeedsDisplay(false)
    , m_CoordinatedGraphicsLayerClient(0)
    , m_contentsScale(1)
    , m_canvasPlatformLayer(0)
    , m_animationStartedTimer(this, &CoordinatedGraphicsLayer::animationStartedTimerFired)
{
    static WebLayerID nextLayerID = 1;
    m_id = nextLayerID++;
    layerByIDMap().add(id(), this);
}

CoordinatedGraphicsLayer::~CoordinatedGraphicsLayer()
{
    layerByIDMap().remove(id());

    if (m_CoordinatedGraphicsLayerClient) {
        purgeBackingStores();
        m_CoordinatedGraphicsLayerClient->detachLayer(this);
    }
    willBeDestroyed();
}

void CoordinatedGraphicsLayer::willBeDestroyed()
{
    GraphicsLayer::willBeDestroyed();
}

bool CoordinatedGraphicsLayer::setChildren(const Vector<GraphicsLayer*>& children)
{
    bool ok = GraphicsLayer::setChildren(children);
    if (!ok)
        return false;
    for (int i = 0; i < children.size(); ++i) {
        CoordinatedGraphicsLayer* child = toCoordinatedGraphicsLayer(children[i]);
        child->setCoordinatedGraphicsLayerClient(m_CoordinatedGraphicsLayerClient);
        child->didChangeLayerState();
    }
    didChangeChildren();
    return true;
}

void CoordinatedGraphicsLayer::addChild(GraphicsLayer* layer)
{
    GraphicsLayer::addChild(layer);
    toCoordinatedGraphicsLayer(layer)->setCoordinatedGraphicsLayerClient(m_CoordinatedGraphicsLayerClient);
    toCoordinatedGraphicsLayer(layer)->didChangeLayerState();
    didChangeChildren();
}

void CoordinatedGraphicsLayer::addChildAtIndex(GraphicsLayer* layer, int index)
{
    GraphicsLayer::addChildAtIndex(layer, index);
    toCoordinatedGraphicsLayer(layer)->setCoordinatedGraphicsLayerClient(m_CoordinatedGraphicsLayerClient);
    toCoordinatedGraphicsLayer(layer)->didChangeLayerState();
    didChangeChildren();
}

void CoordinatedGraphicsLayer::addChildAbove(GraphicsLayer* layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildAbove(layer, sibling);
    toCoordinatedGraphicsLayer(layer)->setCoordinatedGraphicsLayerClient(m_CoordinatedGraphicsLayerClient);
    toCoordinatedGraphicsLayer(layer)->didChangeLayerState();
    didChangeChildren();
}

void CoordinatedGraphicsLayer::addChildBelow(GraphicsLayer* layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(layer, sibling);
    toCoordinatedGraphicsLayer(layer)->setCoordinatedGraphicsLayerClient(m_CoordinatedGraphicsLayerClient);
    toCoordinatedGraphicsLayer(layer)->didChangeLayerState();
    didChangeChildren();
}

bool CoordinatedGraphicsLayer::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    bool ok = GraphicsLayer::replaceChild(oldChild, newChild);
    if (!ok)
        return false;
    didChangeChildren();
    toCoordinatedGraphicsLayer(oldChild)->didChangeLayerState();
    toCoordinatedGraphicsLayer(newChild)->setCoordinatedGraphicsLayerClient(m_CoordinatedGraphicsLayerClient);
    toCoordinatedGraphicsLayer(newChild)->didChangeLayerState();
    return true;
}

void CoordinatedGraphicsLayer::removeFromParent()
{
    if (CoordinatedGraphicsLayer* parentLayer = toCoordinatedGraphicsLayer(parent()))
        parentLayer->didChangeChildren();
    GraphicsLayer::removeFromParent();

    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setPosition(const FloatPoint& p)
{
    if (position() == p)
        return;

    GraphicsLayer::setPosition(p);
    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setAnchorPoint(const FloatPoint3D& p)
{
    if (anchorPoint() == p)
        return;

    GraphicsLayer::setAnchorPoint(p);
    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setSize(const FloatSize& size)
{
    if (this->size() == size)
        return;

    GraphicsLayer::setSize(size);
    setNeedsDisplay();
    if (maskLayer())
        maskLayer()->setSize(size);
    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setTransform(const TransformationMatrix& t)
{
    if (transform() == t)
        return;

    GraphicsLayer::setTransform(t);
    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setChildrenTransform(const TransformationMatrix& t)
{
    if (childrenTransform() == t)
        return;

    GraphicsLayer::setChildrenTransform(t);
    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setPreserves3D(bool b)
{
    if (preserves3D() == b)
        return;

    GraphicsLayer::setPreserves3D(b);
    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setMasksToBounds(bool b)
{
    if (masksToBounds() == b)
        return;
    GraphicsLayer::setMasksToBounds(b);
    didChangeGeometry();
}

void CoordinatedGraphicsLayer::setDrawsContent(bool b)
{
    if (drawsContent() == b)
        return;
    GraphicsLayer::setDrawsContent(b);

    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setContentsVisible(bool b)
{
    if (contentsAreVisible() == b)
        return;
    GraphicsLayer::setContentsVisible(b);

    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setContentsOpaque(bool b)
{
    if (contentsOpaque() == b)
        return;
    if (m_mainBackingStore)
        m_mainBackingStore->setSupportsAlpha(!b);
    GraphicsLayer::setContentsOpaque(b);
    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setBackfaceVisibility(bool b)
{
    if (backfaceVisibility() == b)
        return;

    GraphicsLayer::setBackfaceVisibility(b);
    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setOpacity(float opacity)
{
    if (this->opacity() == opacity)
        return;

    GraphicsLayer::setOpacity(opacity);
    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setContentsRect(const IntRect& r)
{
    if (contentsRect() == r)
        return;

    GraphicsLayer::setContentsRect(r);
    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setContentsNeedsDisplay()
{
    RefPtr<Image> image = m_image;
    setContentsToImage(0);
    setContentsToImage(image.get());
    m_canvasNeedsDisplay = true;
    if (client())
        client()->notifySyncRequired(this);
}

void CoordinatedGraphicsLayer::setContentsToCanvas(PlatformLayer* platformLayer)
{
    m_canvasPlatformLayer = platformLayer;
    m_canvasNeedsDisplay = true;
    if (client())
        client()->notifySyncRequired(this);
}

#if ENABLE(CSS_FILTERS)
bool CoordinatedGraphicsLayer::setFilters(const FilterOperations& newFilters)
{
    if (filters() == newFilters)
        return true;
    didChangeFilters();
    return GraphicsLayer::setFilters(newFilters);
}
#endif


void CoordinatedGraphicsLayer::setContentsToImage(Image* image)
{
    if (image == m_image)
        return;
    int64_t newID = 0;
    if (m_CoordinatedGraphicsLayerClient) {
        // We adopt first, in case this is the same frame - that way we avoid destroying and recreating the image.
        newID = m_CoordinatedGraphicsLayerClient->adoptImageBackingStore(image);
        m_CoordinatedGraphicsLayerClient->releaseImageBackingStore(m_layerInfo.imageBackingStoreID);
        didChangeLayerState();
        if (m_layerInfo.imageBackingStoreID && newID == m_layerInfo.imageBackingStoreID)
            return;
    } else {
        // If m_CoordinatedGraphicsLayerClient is not set yet there should be no backing store ID.
        ASSERT(!m_layerInfo.imageBackingStoreID);
        didChangeLayerState();
    }

    m_layerInfo.imageBackingStoreID = newID;
    m_image = image;
    GraphicsLayer::setContentsToImage(image);
}

void CoordinatedGraphicsLayer::setMaskLayer(GraphicsLayer* layer)
{
    if (layer == maskLayer())
        return;

    GraphicsLayer::setMaskLayer(layer);

    if (!layer)
        return;

    layer->setSize(size());
    CoordinatedGraphicsLayer* CoordinatedGraphicsLayer = toCoordinatedGraphicsLayer(layer);
    CoordinatedGraphicsLayer->setCoordinatedGraphicsLayerClient(m_CoordinatedGraphicsLayerClient);
    CoordinatedGraphicsLayer->setMaskTarget(this);
    CoordinatedGraphicsLayer->didChangeLayerState();
    didChangeLayerState();

}

void CoordinatedGraphicsLayer::setReplicatedByLayer(GraphicsLayer* layer)
{
    if (layer == replicaLayer())
        return;

    if (layer)
        toCoordinatedGraphicsLayer(layer)->setCoordinatedGraphicsLayerClient(m_CoordinatedGraphicsLayerClient);

    GraphicsLayer::setReplicatedByLayer(layer);
    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setNeedsDisplay()
{
    setNeedsDisplayInRect(IntRect(IntPoint::zero(), IntSize(size().width(), size().height())));
}

void CoordinatedGraphicsLayer::setNeedsDisplayInRect(const FloatRect& rect)
{
    if (m_mainBackingStore)
        m_mainBackingStore->invalidate(IntRect(rect));
    didChangeLayerState();
}

WebLayerID CoordinatedGraphicsLayer::id() const
{
    return m_id;
}

void CoordinatedGraphicsLayer::syncCompositingState(const FloatRect& rect)
{
    if (CoordinatedGraphicsLayer* mask = toCoordinatedGraphicsLayer(maskLayer()))
        mask->syncCompositingStateForThisLayerOnly();

    if (CoordinatedGraphicsLayer* replica = toCoordinatedGraphicsLayer(replicaLayer()))
        replica->syncCompositingStateForThisLayerOnly();

    m_CoordinatedGraphicsLayerClient->syncFixedLayers();

    syncCompositingStateForThisLayerOnly();

    for (size_t i = 0; i < children().size(); ++i)
        children()[i]->syncCompositingState(rect);
}

CoordinatedGraphicsLayer* toCoordinatedGraphicsLayer(GraphicsLayer* layer)
{
    return static_cast<CoordinatedGraphicsLayer*>(layer);
}

void CoordinatedGraphicsLayer::syncChildren()
{
    if (!m_shouldSyncChildren)
        return;
    m_shouldSyncChildren = false;
    Vector<WebLayerID> childIDs;
    for (size_t i = 0; i < children().size(); ++i)
        childIDs.append(toWebLayerID(children()[i]));

    m_CoordinatedGraphicsLayerClient->syncLayerChildren(m_id, childIDs);
}

#if ENABLE(CSS_FILTERS)
void CoordinatedGraphicsLayer::syncFilters()
{
    if (!m_shouldSyncFilters)
        return;
    m_shouldSyncFilters = false;
    m_CoordinatedGraphicsLayerClient->syncLayerFilters(m_id, filters());
}
#endif

void CoordinatedGraphicsLayer::syncLayerState()
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
    m_layerInfo.contentsVisible = contentsAreVisible();
    m_layerInfo.mask = toWebLayerID(maskLayer());
    m_layerInfo.masksToBounds = masksToBounds();
    m_layerInfo.opacity = opacity();
    m_layerInfo.parent = toWebLayerID(parent());
    m_layerInfo.pos = position();
    m_layerInfo.preserves3D = preserves3D();
    m_layerInfo.replica = toWebLayerID(replicaLayer());
    m_layerInfo.size = size();
    m_layerInfo.transform = transform();
    if (!m_animations.hasActiveAnimationsOfType(AnimatedPropertyWebkitTransform))
        m_effectiveTransform = transform();
    if (!m_animations.hasActiveAnimationsOfType(AnimatedPropertyOpacity))
        m_effectiveOpacity = opacity();
    m_CoordinatedGraphicsLayerClient->syncLayerState(m_id, m_layerInfo);
}

void CoordinatedGraphicsLayer::syncAnimatedProperties()
{
    m_animations.apply(this);
    if (!m_shouldSyncAnimatedProperties)
        return;

    m_shouldSyncAnimatedProperties = true;
    if (m_effectiveOpacity != opacity())
        m_CoordinatedGraphicsLayerClient->setLayerAnimatedOpacity(id(), m_effectiveOpacity);
    if (m_effectiveTransform != transform())
        m_CoordinatedGraphicsLayerClient->setLayerAnimatedTransform(id(), m_effectiveTransform);
}


void CoordinatedGraphicsLayer::syncCanvas()
{
    if (!m_canvasNeedsDisplay)
        return;

    if (!m_canvasPlatformLayer)
        return;

#if USE(GRAPHICS_SURFACE)
    uint32_t frontBuffer = m_canvasPlatformLayer->copyToGraphicsSurface();
    uint64_t token = m_canvasPlatformLayer->graphicsSurfaceToken();

    m_CoordinatedGraphicsLayerClient->syncCanvas(m_id, IntSize(size().width(), size().height()), token, frontBuffer);
#endif
    m_canvasNeedsDisplay = false;
}

void CoordinatedGraphicsLayer::ensureImageBackingStore()
{
    if (!m_image)
        return;
    if (!m_layerInfo.imageBackingStoreID)
        m_layerInfo.imageBackingStoreID = m_CoordinatedGraphicsLayerClient->adoptImageBackingStore(m_image.get());
}

void CoordinatedGraphicsLayer::syncCompositingStateForThisLayerOnly()
{
    // The remote image might have been released by purgeBackingStores.
    ensureImageBackingStore();
    syncLayerState();
    syncAnimatedProperties();
    computeTransformedVisibleRect();
    syncChildren();
#if ENABLE(CSS_FILTERS)
    syncFilters();
#endif
    updateContentBuffers();
    syncCanvas();
}

void CoordinatedGraphicsLayer::tiledBackingStorePaintBegin()
{
}

void CoordinatedGraphicsLayer::setRootLayer(bool isRoot)
{
    m_layerInfo.isRootLayer = isRoot;
    didChangeLayerState();
}

void CoordinatedGraphicsLayer::setVisibleContentRectTrajectoryVector(const FloatPoint& trajectoryVector)
{
    if (m_mainBackingStore)
        m_mainBackingStore->coverWithTilesIfNeeded(trajectoryVector);
}

void CoordinatedGraphicsLayer::setContentsScale(float scale)
{
    m_contentsScale = scale;
    adjustContentsScale();
}

float CoordinatedGraphicsLayer::effectiveContentsScale()
{
    return shouldUseTiledBackingStore() ? m_contentsScale : 1;
}

void CoordinatedGraphicsLayer::adjustContentsScale()
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

void CoordinatedGraphicsLayer::createBackingStore()
{
    m_mainBackingStore = adoptPtr(new TiledBackingStore(this, CoordinatedTileBackend::create(this)));
    m_mainBackingStore->setSupportsAlpha(!contentsOpaque());
    m_mainBackingStore->setContentsScale(effectiveContentsScale());
}

void CoordinatedGraphicsLayer::tiledBackingStorePaint(GraphicsContext* context, const IntRect& rect)
{
    if (rect.isEmpty())
        return;
    paintGraphicsLayerContents(*context, rect);
}

void CoordinatedGraphicsLayer::tiledBackingStorePaintEnd(const Vector<IntRect>& updatedRects)
{
}

bool CoordinatedGraphicsLayer::tiledBackingStoreUpdatesAllowed() const
{
    if (!m_inUpdateMode)
        return false;
    return m_CoordinatedGraphicsLayerClient->layerTreeTileUpdatesAllowed();
}

IntRect CoordinatedGraphicsLayer::tiledBackingStoreContentsRect()
{
    return IntRect(0, 0, size().width(), size().height());
}

bool CoordinatedGraphicsLayer::shouldUseTiledBackingStore()
{
    return !selfOrAncestorHaveNonAffineTransforms();
}

IntRect CoordinatedGraphicsLayer::tiledBackingStoreVisibleRect()
{
    if (!shouldUseTiledBackingStore())
        return tiledBackingStoreContentsRect();

    // Non-invertible layers are not visible.
    if (!m_layerTransform.combined().isInvertible())
        return IntRect();

    // Return a projection of the visible rect (surface coordinates) onto the layer's plane (layer coordinates).
    // The resulting quad might be squewed and the visible rect is the bounding box of this quad,
    // so it might spread further than the real visible area (and then even more amplified by the cover rect multiplier).
    return enclosingIntRect(m_layerTransform.combined().inverse().clampedBoundsOfProjectedQuad(FloatQuad(FloatRect(m_CoordinatedGraphicsLayerClient->visibleContentsRect()))));
}

Color CoordinatedGraphicsLayer::tiledBackingStoreBackgroundColor() const
{
    return contentsOpaque() ? Color::white : Color::transparent;
}

PassOwnPtr<WebCore::GraphicsContext> CoordinatedGraphicsLayer::beginContentUpdate(const WebCore::IntSize& size, ShareableSurface::Handle& handle, WebCore::IntPoint& offset)
{
    return m_CoordinatedGraphicsLayerClient->beginContentUpdate(size, contentsOpaque() ? 0 : ShareableBitmap::SupportsAlpha, handle, offset);
}

void CoordinatedGraphicsLayer::createTile(int tileID, const SurfaceUpdateInfo& updateInfo, const IntRect& targetRect)
{
    m_CoordinatedGraphicsLayerClient->createTile(id(), tileID, updateInfo, targetRect);
}

void CoordinatedGraphicsLayer::updateTile(int tileID, const SurfaceUpdateInfo& updateInfo, const IntRect& targetRect)
{
    m_CoordinatedGraphicsLayerClient->updateTile(id(), tileID, updateInfo, targetRect);
}

void CoordinatedGraphicsLayer::removeTile(int tileID)
{
    m_CoordinatedGraphicsLayerClient->removeTile(id(), tileID);
}

void CoordinatedGraphicsLayer::updateContentBuffers()
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

void CoordinatedGraphicsLayer::purgeBackingStores()
{
    m_mainBackingStore.clear();
    m_previousBackingStore.clear();

    if (m_layerInfo.imageBackingStoreID) {
        m_CoordinatedGraphicsLayerClient->releaseImageBackingStore(m_layerInfo.imageBackingStoreID);
        m_layerInfo.imageBackingStoreID = 0;
    }

    didChangeLayerState();
    didChangeChildren();
}

void CoordinatedGraphicsLayer::setCoordinatedGraphicsLayerClient(WebKit::CoordinatedGraphicsLayerClient* client)
{
    if (m_CoordinatedGraphicsLayerClient == client)
        return;

    if (CoordinatedGraphicsLayer* replica = toCoordinatedGraphicsLayer(replicaLayer()))
        replica->setCoordinatedGraphicsLayerClient(client);
    if (CoordinatedGraphicsLayer* mask = toCoordinatedGraphicsLayer(maskLayer()))
        mask->setCoordinatedGraphicsLayerClient(client);
    for (size_t i = 0; i < children().size(); ++i) {
        CoordinatedGraphicsLayer* layer = toCoordinatedGraphicsLayer(this->children()[i]);
        layer->setCoordinatedGraphicsLayerClient(client);
    }

    // We have to release resources on the UI process here if the remote client has changed or is removed.
    if (m_CoordinatedGraphicsLayerClient) {
        purgeBackingStores();
        m_CoordinatedGraphicsLayerClient->detachLayer(this);
    }
    m_CoordinatedGraphicsLayerClient = client;
    if (client)
        client->attachLayer(this);
}

void CoordinatedGraphicsLayer::adjustVisibleRect()
{
    if (m_mainBackingStore)
        m_mainBackingStore->coverWithTilesIfNeeded();
}

void CoordinatedGraphicsLayer::computeTransformedVisibleRect()
{
    if (!m_shouldUpdateVisibleRect)
        return;
    m_shouldUpdateVisibleRect = false;
    m_layerTransform.setLocalTransform(m_effectiveTransform);
    m_layerTransform.setPosition(position());
    m_layerTransform.setAnchorPoint(anchorPoint());
    m_layerTransform.setSize(size());
    m_layerTransform.setFlattening(!preserves3D());
    m_layerTransform.setChildrenTransform(childrenTransform());
    m_layerTransform.combineTransforms(parent() ? toCoordinatedGraphicsLayer(parent())->m_layerTransform.combinedForChildren() : TransformationMatrix());

    // The combined transform will be used in tiledBackingStoreVisibleRect.
    adjustVisibleRect();
    adjustContentsScale();
}

static PassOwnPtr<GraphicsLayer> createCoordinatedGraphicsLayer(GraphicsLayerClient* client)
{
    return adoptPtr(new CoordinatedGraphicsLayer(client));
}

void CoordinatedGraphicsLayer::initFactory()
{
    GraphicsLayer::setGraphicsLayerFactory(createCoordinatedGraphicsLayer);
}

bool CoordinatedGraphicsLayer::selfOrAncestorHaveNonAffineTransforms()
{
    if (!m_layerTransform.combined().isAffine())
        return true;

    return false;
}

bool CoordinatedGraphicsLayer::addAnimation(const KeyframeValueList& valueList, const IntSize& boxSize, const Animation* anim, const String& keyframesName, double timeOffset)
{
    ASSERT(!keyframesName.isEmpty());

    if (!anim || anim->isEmptyOrZeroDuration() || valueList.size() < 2 || (valueList.property() != AnimatedPropertyWebkitTransform && valueList.property() != AnimatedPropertyOpacity))
        return false;

    bool listsMatch = false;
    bool ignoredHasBigRotation;

    if (valueList.property() == AnimatedPropertyWebkitTransform)
        listsMatch = validateTransformOperations(valueList, ignoredHasBigRotation) >= 0;

    m_animations.add(GraphicsLayerAnimation(keyframesName, valueList, boxSize, anim, timeOffset, listsMatch));
    m_animationStartedTimer.startOneShot(0);
    didChangeLayerState();
    return true;
}

void CoordinatedGraphicsLayer::pauseAnimation(const String& animationName, double timeOffset)
{
    m_animations.pause(animationName, timeOffset);
}

void CoordinatedGraphicsLayer::removeAnimation(const String& animationName)
{
    m_animations.remove(animationName);
}

void CoordinatedGraphicsLayer::animationStartedTimerFired(Timer<CoordinatedGraphicsLayer>*)
{
    client()->notifyAnimationStarted(this, /* DOM time */ WTF::currentTime());
}

void CoordinatedGraphicsLayer::setAnimatedTransform(const TransformationMatrix& transform)
{
    m_effectiveTransform = transform;
    didChangeAnimatedProperties();
    m_shouldUpdateVisibleRect = true;
}

void CoordinatedGraphicsLayer::setAnimatedOpacity(float opacity)
{
    m_effectiveOpacity = opacity;
    didChangeAnimatedProperties();
}

}
#endif
