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
#include "DrawingAreaMessages.h"
#include "DrawingAreaProxyMessages.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "Page.h"
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

static Vector<WebLayerID>& deletedLayerIDs()
{
    static Vector<WebLayerID> layers;
    return layers;
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
    , m_modified(false)
    , m_contentNeedsDisplay(false)
    , m_hasPendingAnimations(false)
    , m_contentScale(1.f)
{
    // Starting layer IDs from 1000, to help detect faulty IDs easily.
    static WebLayerID sID = 1000;
    m_layerInfo.id = ++sID;
    layerByIDMap().add(id(), this);
}

WebGraphicsLayer::~WebGraphicsLayer()
{
    layerByIDMap().remove(id());
    deletedLayerIDs().append(id());
}

bool WebGraphicsLayer::setChildren(const Vector<GraphicsLayer*>& children)
{
    bool ok = GraphicsLayer::setChildren(children);
    if (ok)
        notifyChange();
    return ok;
}

void WebGraphicsLayer::addChild(GraphicsLayer* layer)
{
    GraphicsLayer::addChild(layer);
    notifyChange();
}

void WebGraphicsLayer::addChildAtIndex(GraphicsLayer* layer, int index)
{
    GraphicsLayer::addChildAtIndex(layer, index);
    notifyChange();
}

void WebGraphicsLayer::addChildAbove(GraphicsLayer* layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildAbove(layer, sibling);
    notifyChange();
}

void WebGraphicsLayer::addChildBelow(GraphicsLayer* layer, GraphicsLayer* sibling)
{
    GraphicsLayer::addChildBelow(layer, sibling);
    notifyChange();
}

bool WebGraphicsLayer::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    bool ok = GraphicsLayer::replaceChild(oldChild, newChild);

    if (ok)
        notifyChange();
    return ok;
}

void WebGraphicsLayer::removeFromParent()
{
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
    if (m_image)
        setNeedsDisplay();

    GraphicsLayer::setContentsRect(r);
    notifyChange();
}

void WebGraphicsLayer::notifyAnimationStarted(double time)
{
    if (client())
        client()->notifyAnimationStarted(this, time);
}

// We put these stub implementations here for when we implement accelerated cross-process animations.
bool WebGraphicsLayer::addAnimation(const KeyframeValueList& valueList, const IntSize& boxSize, const Animation* anim, const String& keyframesName, double timeOffset)
{
    return false;
}

void WebGraphicsLayer::pauseAnimation(const String& animationName, double timeOffset)
{
}

void WebGraphicsLayer::removeAnimation(const String& animationName)
{
}

void WebGraphicsLayer::setContentsToImage(Image* image)
{
    if (image == m_image.get())
        return;

    m_image = image;
    setNeedsDisplay();
    GraphicsLayer::setContentsToImage(image);
    notifyChange();
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
    m_needsDisplay = true;
    notifyChange();
}

void WebGraphicsLayer::setNeedsDisplayInRect(const FloatRect& rect)
{
    m_needsDisplayRect.unite(rect);
    notifyChange();
}

void WebGraphicsLayer::setContentsNeedsDisplay()
{
    setNeedsDisplay();
}

WebLayerID WebGraphicsLayer::id() const
{
    return m_layerInfo.id;
}

void WebGraphicsLayer::didSynchronize()
{
    m_needsDisplay = false;
    m_modified = false;
    m_needsDisplayRect = FloatRect();
    if (m_hasPendingAnimations)
        notifyAnimationStarted(WTF::currentTime());
    m_hasPendingAnimations = false;
}

const WebLayerInfo& WebGraphicsLayer::layerInfo() const
{
    return m_layerInfo;
}

void WebGraphicsLayer::syncCompositingState()
{
    syncCompositingStateForThisLayerOnly();
    for (size_t i = 0; i < children().size(); ++i)
        children()[i]->syncCompositingState();
    if (replicaLayer())
        replicaLayer()->syncCompositingState();
    if (maskLayer())
        maskLayer()->syncCompositingState();
}

WebGraphicsLayer* toWebGraphicsLayer(GraphicsLayer* layer)
{
    return static_cast<WebGraphicsLayer*>(layer);
}

FloatRect WebGraphicsLayer::needsDisplayRect() const
{
    FloatRect rect = FloatRect(0, 0, size().width(), size().height());
    if (!m_needsDisplay)
        rect.intersect(m_needsDisplayRect);
    return rect;
}

Vector<WebKit::WebLayerID> WebGraphicsLayer::takeLayersToDelete()
{
    Vector<WebKit::WebLayerID> layers = deletedLayerIDs();
    deletedLayerIDs().clear();
    return layers;
}

void WebGraphicsLayer::syncCompositingStateForThisLayerOnly()
{
    m_layerInfo.name = name();
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
    m_layerInfo.size = FloatSize(size().width() * m_contentScale, size().height() * m_contentScale);
    m_layerInfo.transform = transform();
    m_contentNeedsDisplay = false;
    m_layerInfo.children.clear();

    for (size_t i = 0; i < children().size(); ++i)
        m_layerInfo.children.append(toWebLayerID(children()[i]));
}

static Vector<FloatRect> divideIntoTiles(const FloatRect& rect)
{
    Vector<FloatRect> tiles;
    for (float y = rect.y(); y < rect.maxY(); y += gTileDimension) {
        for (float x = rect.x(); x < rect.maxX(); x += gTileDimension) {
            FloatRect tile(x, y, gTileDimension, gTileDimension);
            tile.intersect(rect);
            tiles.append(tile);
        }
    }
    return tiles;
}

static void collectCompositingInfoForThisLayer(GraphicsLayer* layer, WebLayerTreeInfo& outInfo, Vector<WebGraphicsLayer*>& outLayers)
{
    WebGraphicsLayer* layer = toWebGraphicsLayer(layer);
    if (!layer)
        return;
    if (!layer->isModified())
        return;

    outLayers.append(layer);
    outInfo.layers.append(layer->layerInfo());

    WebLayerInfo& layerInfo = outInfo.layers[outInfo.layers.size() - 1];
    if (layer->image())
        layerInfo.drawsContent = true;
}

static void collectCompositingInfoRecursively(GraphicsLayer* rootLayer, WebLayerTreeInfo& outInfo, Vector<WebGraphicsLayer*>& outLayers)
{
    if (!rootLayer)
        return;

    collectCompositingInfoForThisLayer(rootLayer, outInfo, outLayers);
    collectCompositingInfoForThisLayer(rootLayer->maskLayer(), outInfo, outLayers);
    collectCompositingInfoRecursively(rootLayer->replicaLayer(), outInfo, outLayers);

    for (size_t i = 0; i < rootLayer->children().size(); ++i)
        collectCompositingInfoRecursively(rootLayer->children()[i], outInfo, outLayers);
}

void WebGraphicsLayer::sendLayersToUIProcess(WebCore::GraphicsLayer* rootLayer, WebPage* webPage)
{
    webPage->layoutIfNeeded();
    webPage->corePage()->mainFrame()->view()->syncCompositingStateIncludingSubframes();
    WebLayerTreeInfo compositingInfo;
    compositingInfo.rootLayerID = rootLayer ? toWebGraphicsLayer(rootLayer)->id() : 0;
    compositingInfo.contentScale = 1.0;
    compositingInfo.deletedLayerIDs = WebGraphicsLayer::takeLayersToDelete();
    Vector<WebGraphicsLayer*> layers;
    if (rootLayer)
        collectCompositingInfoRecursively(rootLayer, compositingInfo, layers);

    if (compositingInfo.deletedLayerIDs.isEmpty() && compositingInfo.layers.isEmpty())
        return;

    webPage->send(Messages::DrawingAreaProxy::SyncCompositingLayers(compositingInfo));

    for (size_t i = 0; i < layers.size(); ++i) {
        WebGraphicsLayer* layer = layers[i];
        FloatRect updateRect = layer->needsDisplayRect();

        layer->didSynchronize();

        if (updateRect.isEmpty())
            continue;

        RefPtr<Image> directImage = layer->image();
        if (directImage)
            updateRect = layer->contentsRect();
        else if (!layer->drawsContent())
            continue;

        Vector<FloatRect> tileRects = divideIntoTiles(updateRect);
        for (size_t tileIndex = 0; tileIndex < tileRects.size(); ++tileIndex) {
            const FloatRect& tileRect = tileRects[tileIndex];
            IntRect enclosingUpdateRect = enclosingIntRect(tileRect);

            WebLayerUpdateInfo chunk(enclosingUpdateRect);
            chunk.layerID = uint32_t(layer->id());
            RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(chunk.rect.size(), ShareableBitmap::SupportsAlpha);
            if (!bitmap)
                return;

            if (!bitmap->createHandle(chunk.bitmapHandle))
                return;

#if PLATFORM(QT)
            QImage tileImage = bitmap->createQImage();
            tileImage.fill(0);
            {
                QPainter painter(&tileImage);
                painter.translate(-tileRect.x(), -tileRect.y());
                GraphicsContext context(&painter);
                if (directImage)
                    context.drawImage(directImage.get(), ColorSpaceDeviceRGB, layer->contentsRect(), FloatRect(0, 0, directImage->width(), directImage->height()));
                else
                    layer->paintGraphicsLayerContents(context, enclosingUpdateRect);
            }
#endif

            webPage->send(Messages::DrawingAreaProxy::UpdateCompositingLayerContent(chunk));
        }
    }
}

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
