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

#if USE(ACCELERATED_COMPOSITING)

#include "WebGraphicsLayer.h"

#include "Animation.h"
#include "BackingStore.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "LayerTreeHostProxyMessages.h"
#include "Page.h"
#include "TiledBackingStoreRemoteTile.h"
#include "WebPage.h"
#include "text/CString.h"
#include <HashMap.h>
#include <wtf/CurrentTime.h>

using namespace WebKit;

namespace WebCore {

static const float gTileDimension = 1024.0;

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

void WebGraphicsLayer::notifyChange()
{
    m_modified = true;
    if (client())
        client()->notifySyncRequired(this);
}

WebGraphicsLayer::WebGraphicsLayer(GraphicsLayerClient* client)
    : GraphicsLayer(client)
    , m_needsDisplay(false)
    , m_modified(true)
    , m_contentNeedsDisplay(false)
    , m_hasPendingAnimations(false)
    , m_inUpdateMode(false)
#if USE(TILED_BACKING_STORE)
    , m_layerTreeTileClient(0)
    , m_mainBackingStore(adoptPtr(new TiledBackingStore(this, TiledBackingStoreRemoteTileBackend::create(this))))
    , m_contentsScale(1.f)
#endif
{
    m_mainBackingStore->setContentsScale(1.0);
    static WebLayerID nextLayerID = 1;
    m_layerInfo.id = nextLayerID++;
    layerByIDMap().add(id(), this);
}

WebGraphicsLayer::~WebGraphicsLayer()
{
    layerByIDMap().remove(id());

    // This would tell the UI process to release the backing store.
    setContentsToImage(0);

    if (m_layerTreeTileClient)
        m_layerTreeTileClient->didDeleteLayer(id());
}

bool WebGraphicsLayer::setChildren(const Vector<GraphicsLayer*>& children)
{
    bool ok = GraphicsLayer::setChildren(children);
    if (!ok)
        return false;
    for (int i = 0; i < children.size(); ++i) {
        WebGraphicsLayer* child = toWebGraphicsLayer(children[i]);
        child->setContentsScale(m_contentsScale);
        child->notifyChange();
    }
    notifyChange();
    return true;
}

void WebGraphicsLayer::addChild(GraphicsLayer* layer)
{
    GraphicsLayer::addChild(layer);
    toWebGraphicsLayer(layer)->notifyChange();
    notifyChange();
}

void WebGraphicsLayer::addChildAtIndex(GraphicsLayer* layer, int index)
{
    GraphicsLayer::addChildAtIndex(layer, index);
    toWebGraphicsLayer(layer)->notifyChange();
    notifyChange();
}

void WebGraphicsLayer::addChildAbove(GraphicsLayer* layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildAbove(layer, sibling);
    toWebGraphicsLayer(layer)->notifyChange();
    notifyChange();
}

void WebGraphicsLayer::addChildBelow(GraphicsLayer* layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(layer, sibling);
    toWebGraphicsLayer(layer)->notifyChange();
    notifyChange();
}

bool WebGraphicsLayer::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    bool ok = GraphicsLayer::replaceChild(oldChild, newChild);
    if (!ok)
        return false;
    notifyChange();
    toWebGraphicsLayer(oldChild)->notifyChange();
    toWebGraphicsLayer(newChild)->notifyChange();
    return true;
}

void WebGraphicsLayer::removeFromParent()
{
    if (WebGraphicsLayer* parentLayer = toWebGraphicsLayer(parent()))
        parentLayer->notifyChange();
    GraphicsLayer::removeFromParent();

    notifyChange();
}

void WebGraphicsLayer::setPosition(const FloatPoint& p)
{
    if (position() == p)
        return;

    GraphicsLayer::setPosition(p);
    notifyChange();
}

void WebGraphicsLayer::setAnchorPoint(const FloatPoint3D& p)
{
    if (anchorPoint() == p)
        return;

    GraphicsLayer::setAnchorPoint(p);
    notifyChange();
}

void WebGraphicsLayer::setSize(const FloatSize& size)
{
    if (this->size() == size)
        return;

    GraphicsLayer::setSize(size);
    setNeedsDisplay();
    notifyChange();
}

void WebGraphicsLayer::setTransform(const TransformationMatrix& t)
{
    if (transform() == t)
        return;

    GraphicsLayer::setTransform(t);
    notifyChange();
}

void WebGraphicsLayer::setChildrenTransform(const TransformationMatrix& t)
{
    if (childrenTransform() == t)
        return;

    GraphicsLayer::setChildrenTransform(t);
    notifyChange();
}

void WebGraphicsLayer::setPreserves3D(bool b)
{
    if (preserves3D() == b)
        return;

    GraphicsLayer::setPreserves3D(b);
    notifyChange();
}

void WebGraphicsLayer::setMasksToBounds(bool b)
{
    if (masksToBounds() == b)
        return;
    GraphicsLayer::setMasksToBounds(b);
    notifyChange();
}

void WebGraphicsLayer::setDrawsContent(bool b)
{
    if (drawsContent() == b)
        return;
    GraphicsLayer::setDrawsContent(b);

    if (b)
        setNeedsDisplay();
    notifyChange();
}

void WebGraphicsLayer::setContentsOpaque(bool b)
{
    if (contentsOpaque() == b)
        return;
    GraphicsLayer::setContentsOpaque(b);
    notifyChange();
}

void WebGraphicsLayer::setBackfaceVisibility(bool b)
{
    if (backfaceVisibility() == b)
        return;

    GraphicsLayer::setBackfaceVisibility(b);
    notifyChange();
}

void WebGraphicsLayer::setOpacity(float opacity)
{
    if (this->opacity() == opacity)
        return;

    GraphicsLayer::setOpacity(opacity);
    notifyChange();
}

void WebGraphicsLayer::setContentsRect(const IntRect& r)
{
    if (contentsRect() == r)
        return;

    GraphicsLayer::setContentsRect(r);
    notifyChange();
}

void WebGraphicsLayer::notifyAnimationStarted(double time)
{
    if (client())
        client()->notifyAnimationStarted(this, time);
}

bool WebGraphicsLayer::addAnimation(const KeyframeValueList& valueList, const IntSize& boxSize, const Animation* anim, const String& keyframesName, double timeOffset)
{
    if (!anim || anim->isEmptyOrZeroDuration() || valueList.size() < 2 || (valueList.property() != AnimatedPropertyWebkitTransform && valueList.property() != AnimatedPropertyOpacity))
        return false;

    WebLayerAnimation webAnimation(valueList);
    webAnimation.name = keyframesName;
    webAnimation.operation = WebLayerAnimation::AddAnimation;
    webAnimation.boxSize = boxSize;
    webAnimation.animation = Animation::create(anim);
    webAnimation.startTime = timeOffset;
    m_layerInfo.animations.append(webAnimation);

    m_hasPendingAnimations = true;
    notifyChange();

    return true;
}

void WebGraphicsLayer::pauseAnimation(const String& animationName, double timeOffset)
{
    WebLayerAnimation webAnimation;
    webAnimation.name = animationName;
    webAnimation.operation = WebLayerAnimation::PauseAnimation;
    webAnimation.startTime = WTF::currentTime() - timeOffset;
    m_layerInfo.animations.append(webAnimation);
    notifyChange();
}

void WebGraphicsLayer::removeAnimation(const String& animationName)
{
    WebLayerAnimation webAnimation;
    webAnimation.name = animationName;
    webAnimation.operation = WebLayerAnimation::RemoveAnimation;
    m_layerInfo.animations.append(webAnimation);
    notifyChange();
}

void WebGraphicsLayer::setContentsNeedsDisplay()
{
    RefPtr<Image> image = m_image;
    setContentsToImage(0);
    setContentsToImage(image.get());
}

void WebGraphicsLayer::setContentsToImage(Image* image)
{
    if (image == m_image)
        return;
    WebLayerTreeTileClient* client = layerTreeTileClient();
    int64_t newID = 0;
    if (client) {
        // We adopt first, in case this is the same frame - that way we avoid destroying and recreating the image.
        newID = client->adoptImageBackingStore(image);
        client->releaseImageBackingStore(m_layerInfo.imageBackingStoreID);
        notifyChange();
        if (m_layerInfo.imageBackingStoreID && newID == m_layerInfo.imageBackingStoreID)
            return;
    } else {
        // If client not set yet there should be no backing store ID.
        ASSERT(!m_layerInfo.imageBackingStoreID);
        notifyChange();
    }

    m_layerInfo.imageBackingStoreID = newID;
    m_image = image;
    m_layerInfo.imageIsUpdated = true;
    GraphicsLayer::setContentsToImage(image);
}

void WebGraphicsLayer::setMaskLayer(GraphicsLayer* layer)
{
    GraphicsLayer::setMaskLayer(layer);
    notifyChange();
}

void WebGraphicsLayer::setReplicatedByLayer(GraphicsLayer* layer)
{
    if (layer == replicaLayer())
        return;
    GraphicsLayer::setReplicatedByLayer(layer);
    notifyChange();
}

void WebGraphicsLayer::setNeedsDisplay()
{
    setNeedsDisplayInRect(IntRect(IntPoint::zero(), IntSize(size().width(), size().height())));
}

void WebGraphicsLayer::setNeedsDisplayInRect(const FloatRect& rect)
{
    m_mainBackingStore->invalidate(IntRect(rect));
    notifyChange();
}

WebLayerID WebGraphicsLayer::id() const
{
    return m_layerInfo.id;
}

void WebGraphicsLayer::syncCompositingState(const FloatRect& rect)
{
    for (size_t i = 0; i < children().size(); ++i)
        children()[i]->syncCompositingState(rect);
    if (replicaLayer())
        replicaLayer()->syncCompositingState(rect);
    if (maskLayer())
        maskLayer()->syncCompositingState(rect);
    syncCompositingStateForThisLayerOnly();
}

WebGraphicsLayer* toWebGraphicsLayer(GraphicsLayer* layer)
{
    return static_cast<WebGraphicsLayer*>(layer);
}

void WebGraphicsLayer::syncCompositingStateForThisLayerOnly()
{
    if (!m_layerTreeTileClient)
        m_layerTreeTileClient = layerTreeTileClient();
    updateContentBuffers();

    if (!m_modified)
        return;

    m_layerInfo.name = name();
    m_layerInfo.anchorPoint = anchorPoint();
    m_layerInfo.backfaceVisible = backfaceVisibility();
    m_layerInfo.childrenTransform = childrenTransform();
    m_layerInfo.contentsOpaque = contentsOpaque();
    m_layerInfo.contentsRect = contentsRect();

    // In the shadow layer tree we create in the UI process, layers with directly composited images are always considered to draw content.
    // Otherwise, we'd have to check whether an layer with drawsContent==false has a directly composited image multiple times.
    m_layerInfo.drawsContent = drawsContent() || m_image;
    m_layerInfo.mask = toWebLayerID(maskLayer());
    m_layerInfo.masksToBounds = masksToBounds();
    m_layerInfo.opacity = opacity();
    m_layerInfo.parent = toWebLayerID(parent());
    m_layerInfo.pos = position();
    m_layerInfo.preserves3D = preserves3D();
    m_layerInfo.replica = toWebLayerID(replicaLayer());
    m_layerInfo.size = size();
    m_layerInfo.transform = transform();
    m_contentNeedsDisplay = false;
    m_layerInfo.children.clear();

    for (size_t i = 0; i < children().size(); ++i)
        m_layerInfo.children.append(toWebLayerID(children()[i]));

    if (m_layerInfo.imageIsUpdated && m_image && !m_layerInfo.imageBackingStoreID)
        m_layerInfo.imageBackingStoreID = layerTreeTileClient()->adoptImageBackingStore(m_image.get());

    m_layerTreeTileClient->didSyncCompositingStateForLayer(m_layerInfo);
    m_modified = false;
    m_layerInfo.imageIsUpdated = false;
    if (m_hasPendingAnimations)
        notifyAnimationStarted(WTF::currentTime());
    m_layerInfo.animations.clear();
    m_hasPendingAnimations = false;
}

#if USE(TILED_BACKING_STORE)
void WebGraphicsLayer::tiledBackingStorePaintBegin()
{
}

void WebGraphicsLayer::setContentsScale(float scale)
{
    for (size_t i = 0; i < children().size(); ++i) {
        WebGraphicsLayer* layer = toWebGraphicsLayer(this->children()[i]);
        layer->setContentsScale(scale);
    }

    m_contentsScale = scale;
    if (m_mainBackingStore && m_mainBackingStore->contentsScale() == scale)
        return;

    notifyChange();

    m_previousBackingStore = m_mainBackingStore.release();
    m_mainBackingStore = adoptPtr(new TiledBackingStore(this, TiledBackingStoreRemoteTileBackend::create(this)));
    m_mainBackingStore->setContentsScale(scale);
}

void WebGraphicsLayer::setRootLayer(bool isRoot)
{
    m_layerInfo.isRootLayer = isRoot;
    notifyChange();
}

void WebGraphicsLayer::setVisibleContentRectTrajectoryVector(const FloatPoint& trajectoryVector)
{
    m_mainBackingStore->setVisibleRectTrajectoryVector(trajectoryVector);
}

void WebGraphicsLayer::setVisibleContentRect(const IntRect& rect)
{
    m_visibleContentRect = rect;
    notifyChange();
    m_mainBackingStore->adjustVisibleRect();
}

void WebGraphicsLayer::tiledBackingStorePaint(GraphicsContext* context, const IntRect& rect)
{
    if (rect.isEmpty())
        return;
    m_modified = true;
    paintGraphicsLayerContents(*context, rect);
}

void WebGraphicsLayer::tiledBackingStorePaintEnd(const Vector<IntRect>& updatedRects)
{
}

bool WebGraphicsLayer::tiledBackingStoreUpdatesAllowed() const
{
    if (!m_inUpdateMode)
        return false;
    if (WebLayerTreeTileClient* client = layerTreeTileClient())
        return client->layerTreeTileUpdatesAllowed();
    return false;
}

IntRect WebGraphicsLayer::tiledBackingStoreContentsRect()
{
    if (m_image)
        return IntRect();
    return IntRect(0, 0, size().width(), size().height());
}

IntRect WebGraphicsLayer::tiledBackingStoreVisibleRect()
{
    return m_visibleContentRect;
}

Color WebGraphicsLayer::tiledBackingStoreBackgroundColor() const
{
    return contentsOpaque() ? Color::white : Color::transparent;

}
void WebGraphicsLayer::createTile(int tileID, const UpdateInfo& updateInfo)
{
    m_modified = true;
    if (WebLayerTreeTileClient* client = layerTreeTileClient())
        client->createTile(id(), tileID, updateInfo);
}

void WebGraphicsLayer::updateTile(int tileID, const UpdateInfo& updateInfo)
{
    m_modified = true;
    if (WebLayerTreeTileClient* client = layerTreeTileClient())
        client->updateTile(id(), tileID, updateInfo);
}

void WebGraphicsLayer::removeTile(int tileID)
{
    m_modified = true;
    if (WebLayerTreeTileClient* client = layerTreeTileClient())
        client->removeTile(id(), tileID);
}

void WebGraphicsLayer::updateTileBuffersRecursively()
{
    m_mainBackingStore->updateTileBuffers();
    for (size_t i = 0; i < children().size(); ++i) {
        WebGraphicsLayer* layer = toWebGraphicsLayer(this->children()[i]);
        layer->updateTileBuffersRecursively();
    }
}

WebLayerTreeTileClient* WebGraphicsLayer::layerTreeTileClient() const
{
    if (m_layerTreeTileClient)
        return m_layerTreeTileClient;
    WebGraphicsLayer* parent = toWebGraphicsLayer(this->parent());
    if (!parent)
        return 0;
    return parent->layerTreeTileClient();
}

void WebGraphicsLayer::updateContentBuffers()
{
    // Backing-stores for directly composited images is handled in LayerTreeHost.
    if (m_image)
        return;

    if (!drawsContent())
        return;
    WebLayerTreeTileClient* client = layerTreeTileClient();
    if (!client)
        return;
    m_inUpdateMode = true;
    m_mainBackingStore->updateTileBuffers();
    m_inUpdateMode = false;
}

#endif

static PassOwnPtr<GraphicsLayer> createWebGraphicsLayer(GraphicsLayerClient* client)
{
    return adoptPtr(new WebGraphicsLayer(client));
}

void WebGraphicsLayer::initFactory()
{
    GraphicsLayer::setGraphicsLayerFactory(createWebGraphicsLayer);
}

}
#endif
