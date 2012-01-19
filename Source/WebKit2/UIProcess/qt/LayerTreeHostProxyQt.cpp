/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)

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
#include "LayerTreeHostProxy.h"

#include "LayerTreeHostMessages.h"
#include "MainThread.h"
#include "MessageID.h"
#include "ShareableBitmap.h"
#include "TextureMapperGL.h"
#include "UpdateInfo.h"
#include "WebCoreArgumentCoders.h"
#include "WebLayerTreeInfo.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include "texmap/GraphicsLayerTextureMapper.h"
#include "texmap/TextureMapper.h"
#include "texmap/TextureMapperNode.h"
#include <QDateTime>
#include <cairo/OpenGLShims.h>

namespace WebKit {

class LayerTreeMessageToRenderer {
public:
    enum Type {
        DeleteLayer,
        CreateTile,
        RemoveTile,
        UpdateTile,
        CreateImage,
        DestroyImage,
        SyncLayerParameters,
        FlushLayerChanges,
        SetRootLayer
    };
    virtual ~LayerTreeMessageToRenderer() { }
    virtual Type type() const = 0;
};

using namespace WebCore;

template<class MessageData, LayerTreeMessageToRenderer::Type messageType>
class LayerTreeMessageToRendererWithData : public LayerTreeMessageToRenderer {
public:
    virtual Type type() const { return messageType; }

    static PassOwnPtr<LayerTreeMessageToRenderer> create(const MessageData& data = MessageData())
    {
        return adoptPtr(new LayerTreeMessageToRendererWithData(data));
    }

    const MessageData& data() const
    {
        return m_data;
    }

private:
    LayerTreeMessageToRendererWithData(const MessageData& data)
        : m_data(data)
    {
    }

    MessageData m_data;
};


namespace {
struct CreateTileMessageData {
    WebLayerID layerID;
    int remoteTileID;
    float scale;
};

struct UpdateTileMessageData {
    WebLayerID layerID;
    int remoteTileID;
    IntRect sourceRect;
    IntRect targetRect;
    QImage image;
};

struct RemoveTileMessageData {
    WebLayerID layerID;
    int remoteTileID;
};

struct CreateImageMessageData {
    int64_t imageID;
    QImage image;
};

struct DestroyImageMessageData {
    int64_t imageID;
};

struct SyncLayerParametersMessageData {
    WebLayerInfo layerInfo;
};

struct EmptyMessageData { };
struct DeleteLayerMessageData {
    WebLayerID layerID;
};
struct SetRootLayerMessageData {
    WebLayerID layerID;
};

class CreateTileMessage
        : public LayerTreeMessageToRendererWithData<CreateTileMessageData, LayerTreeMessageToRenderer::CreateTile> { };
class UpdateTileMessage
        : public LayerTreeMessageToRendererWithData<UpdateTileMessageData, LayerTreeMessageToRenderer::UpdateTile> { };
class RemoveTileMessage
        : public LayerTreeMessageToRendererWithData<RemoveTileMessageData, LayerTreeMessageToRenderer::RemoveTile> { };
class CreateImageMessage
        : public LayerTreeMessageToRendererWithData<CreateImageMessageData, LayerTreeMessageToRenderer::CreateImage> { };
class DestroyImageMessage
        : public LayerTreeMessageToRendererWithData<DestroyImageMessageData, LayerTreeMessageToRenderer::DestroyImage> { };
class FlushLayerChangesMessage
        : public LayerTreeMessageToRendererWithData<EmptyMessageData, LayerTreeMessageToRenderer::FlushLayerChanges> { };
class SyncLayerParametersMessage
        : public LayerTreeMessageToRendererWithData<SyncLayerParametersMessageData, LayerTreeMessageToRenderer::SyncLayerParameters> { };
class DeleteLayerMessage
        : public LayerTreeMessageToRendererWithData<DeleteLayerMessageData, LayerTreeMessageToRenderer::DeleteLayer> { };
class SetRootLayerMessage
        : public LayerTreeMessageToRendererWithData<SetRootLayerMessageData, LayerTreeMessageToRenderer::SetRootLayer> { };
}

PassOwnPtr<GraphicsLayer> LayerTreeHostProxy::createLayer(WebLayerID layerID)
{
    GraphicsLayer* newLayer = new GraphicsLayerTextureMapper(this);
    TextureMapperNode* node = toTextureMapperNode(newLayer);
    node->setID(layerID);
    node->setTileOwnership(TextureMapperNode::ExternallyManagedTiles);
    return adoptPtr(newLayer);
}

LayerTreeHostProxy::LayerTreeHostProxy(DrawingAreaProxy* drawingAreaProxy)
    : m_animationTimer(RunLoop::main(), this, &LayerTreeHostProxy::updateViewport)
    , m_drawingAreaProxy(drawingAreaProxy)
    , m_viewportUpdateTimer(this, &LayerTreeHostProxy::didFireViewportUpdateTimer)
    , m_rootLayerID(0)
{
}

LayerTreeHostProxy::~LayerTreeHostProxy()
{
}

// This function needs to be reentrant.
void LayerTreeHostProxy::paintToCurrentGLContext(const TransformationMatrix& matrix, float opacity)
{
    if (!m_textureMapper)
        m_textureMapper = TextureMapperGL::create();

    syncRemoteContent();
    GraphicsLayer* currentRootLayer = rootLayer();
    if (!currentRootLayer)
        return;

    TextureMapperNode* node = toTextureMapperNode(currentRootLayer);

    if (!node)
        return;

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    m_textureMapper->setViewportSize(IntSize(viewport[2], viewport[3]));
    node->setTextureMapper(m_textureMapper.get());
    m_textureMapper->beginPainting();
    m_textureMapper->bindSurface(0);

    if (currentRootLayer->opacity() != opacity || currentRootLayer->transform() != matrix) {
        currentRootLayer->setOpacity(opacity);
        currentRootLayer->setTransform(matrix);
        currentRootLayer->syncCompositingStateForThisLayerOnly();
    }

    node->paint();
    m_textureMapper->endPainting();

    if (node->descendantsOrSelfHaveRunningAnimations()) {
        node->syncAnimationsRecursively();
        m_viewportUpdateTimer.startOneShot(0);
    }
}

void LayerTreeHostProxy::didFireViewportUpdateTimer(Timer<LayerTreeHostProxy>*)
{
    updateViewport();
}

void LayerTreeHostProxy::updateViewport()
{
    m_drawingAreaProxy->updateViewport();
}

int LayerTreeHostProxy::remoteTileIDToNodeTileID(int tileID) const
{
    HashMap<int, int>::const_iterator it = m_tileToNodeTile.find(tileID);
    if (it == m_tileToNodeTile.end())
        return 0;
    return it->second;
}

void LayerTreeHostProxy::syncLayerParameters(const WebLayerInfo& layerInfo)
{
    WebLayerID id = layerInfo.id;
    ensureLayer(id);
    LayerMap::iterator it = m_layers.find(id);
    GraphicsLayer* layer = it->second;
    bool needsToUpdateImageTiles = layerInfo.imageIsUpdated || layerInfo.contentsRect != layer->contentsRect();

    layer->setName(layerInfo.name);

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

    if (needsToUpdateImageTiles)
        assignImageToLayer(layer, layerInfo.imageBackingStoreID);

    // Never make the root layer clip.
    layer->setMasksToBounds(layerInfo.isRootLayer ? false : layerInfo.masksToBounds);
    layer->setOpacity(layerInfo.opacity);
    layer->setPreserves3D(layerInfo.preserves3D);
    Vector<GraphicsLayer*> children;

    for (size_t i = 0; i < layerInfo.children.size(); ++i) {
        WebLayerID childID = layerInfo.children[i];
        GraphicsLayer* child = layerByID(childID);
        if (!child) {
            child = createLayer(childID).leakPtr();
            m_layers.add(childID, child);
        }
        children.append(child);
    }
    layer->setChildren(children);

    for (size_t i = 0; i < layerInfo.animations.size(); ++i) {
        const WebKit::WebLayerAnimation anim = layerInfo.animations[i];

        switch (anim.operation) {
        case WebKit::WebLayerAnimation::AddAnimation: {
            const IntSize boxSize = anim.boxSize;
            layer->addAnimation(anim.keyframeList, boxSize, anim.animation.get(), anim.name, anim.startTime);
            break;
        }
        case WebKit::WebLayerAnimation::RemoveAnimation:
            layer->removeAnimation(anim.name);
            break;
        case WebKit::WebLayerAnimation::PauseAnimation:
            double offset = WTF::currentTime() - anim.startTime;
            layer->pauseAnimation(anim.name, offset);
            break;
        }
    }

    if (layerInfo.isRootLayer && m_rootLayerID != id)
        setRootLayerID(id);
}

void LayerTreeHostProxy::deleteLayer(WebLayerID layerID)
{
    GraphicsLayer* layer = layerByID(layerID);
    if (!layer)
        return;

    layer->removeFromParent();
    m_layers.remove(layerID);
    delete layer;
}


void LayerTreeHostProxy::ensureLayer(WebLayerID id)
{
    // We have to leak the new layer's pointer and manage it ourselves,
    // because OwnPtr is not copyable.
    if (m_layers.find(id) == m_layers.end())
        m_layers.add(id, createLayer(id).leakPtr());
}

void LayerTreeHostProxy::setRootLayerID(WebLayerID layerID)
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

void LayerTreeHostProxy::createTile(WebLayerID layerID, int tileID, float scale)
{
    ensureLayer(layerID);
    TextureMapperNode* node = toTextureMapperNode(layerByID(layerID));

    int nodeTileID = node->createContentsTile(scale);
    m_tileToNodeTile.add(tileID, nodeTileID);
}

void LayerTreeHostProxy::removeTile(WebLayerID layerID, int tileID)
{
    TextureMapperNode* node = toTextureMapperNode(layerByID(layerID));
    if (!node)
        return;

    int nodeTileID = remoteTileIDToNodeTileID(tileID);
    if (!nodeTileID)
        return;

    node->removeContentsTile(nodeTileID);
    m_tileToNodeTile.remove(tileID);
}

void LayerTreeHostProxy::updateTile(WebLayerID layerID, int tileID, const IntRect& sourceRect, const IntRect& targetRect, const QImage& image)
{
    ensureLayer(layerID);
    TextureMapperNode* node = toTextureMapperNode(layerByID(layerID));
    if (!node)
        return;

    int nodeTileID = remoteTileIDToNodeTileID(tileID);
    if (!nodeTileID)
        return;

    QImage imageRef(image);
    node->setTextureMapper(m_textureMapper.get());
    node->setContentsTileBackBuffer(nodeTileID, sourceRect, targetRect, imageRef.bits(), BitmapTexture::BGRAFormat);
}

void LayerTreeHostProxy::createImage(int64_t imageID, const QImage& image)
{
    TiledImage tiledImage;
    static const int TileDimension = 1024;
    bool imageHasAlpha = image.hasAlphaChannel();
    IntRect imageRect(0, 0, image.width(), image.height());
    for (int y = 0; y < image.height(); y += TileDimension) {
        for (int x = 0; x < image.width(); x += TileDimension) {
            QImage subImage;
            IntRect rect(x, y, TileDimension, TileDimension);
            rect.intersect(imageRect);
            if (QSize(rect.size()) == image.size())
                subImage = image;
            else
                subImage = image.copy(rect);
            RefPtr<BitmapTexture> texture = m_textureMapper->createTexture();
            texture->reset(rect.size(), !imageHasAlpha);
            texture->updateContents(imageHasAlpha ? BitmapTexture::BGRAFormat : BitmapTexture::BGRFormat, IntRect(IntPoint::zero(), rect.size()), subImage.bits());
            tiledImage.add(rect.location(), texture);
        }
    }

    m_directlyCompositedImages.remove(imageID);
    m_directlyCompositedImages.add(imageID, tiledImage);
}

void LayerTreeHostProxy::destroyImage(int64_t imageID)
{
    m_directlyCompositedImages.remove(imageID);
}

void LayerTreeHostProxy::assignImageToLayer(GraphicsLayer* layer, int64_t imageID)
{
    TextureMapperNode* node = toTextureMapperNode(layer);
    if (!node)
        return;

    if (!imageID) {
        node->clearAllDirectlyCompositedImageTiles();
        return;
    }

    FloatSize size(layer->size());
    FloatRect contentsRect(layer->contentsRect());
    float horizontalFactor = contentsRect.width() / size.width();
    float verticalFactor = contentsRect.height() / size.height();
    HashMap<int64_t, TiledImage>::iterator it = m_directlyCompositedImages.find(imageID);
    if (it == m_directlyCompositedImages.end())
        return;

    TiledImage::iterator endTileIterator = it->second.end();
    for (TiledImage::iterator tileIt = it->second.begin(); tileIt != endTileIterator; ++tileIt) {
        FloatRect sourceRect(FloatPoint(tileIt->first), FloatSize(tileIt->second->size()));
        FloatRect targetRect(sourceRect.x() * horizontalFactor + contentsRect.x(),
                             sourceRect.y() * verticalFactor + contentsRect.y(),
                             sourceRect.width() * horizontalFactor,
                             sourceRect.height() * verticalFactor);
        int newTileID = node->createContentsTile(1.0);
        node->setTileBackBufferTextureForDirectlyCompositedImage(newTileID, IntRect(sourceRect), targetRect, tileIt->second.get());
    }
}

void LayerTreeHostProxy::flushLayerChanges()
{
    m_rootLayer->syncCompositingState(FloatRect());
}

void LayerTreeHostProxy::ensureRootLayer()
{
    if (m_rootLayer)
        return;
    m_rootLayer = createLayer(InvalidWebLayerID);
    m_rootLayer->setMasksToBounds(false);
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setAnchorPoint(FloatPoint3D(0, 0, 0));

    // The root layer should not have zero size, or it would be optimized out.
    m_rootLayer->setSize(FloatSize(1.0, 1.0));
    if (!m_textureMapper)
        m_textureMapper = TextureMapperGL::create();
    toTextureMapperNode(m_rootLayer.get())->setTextureMapper(m_textureMapper.get());
}

void LayerTreeHostProxy::syncRemoteContent()
{
    // We enqueue messages and execute them during paint, as they require an active GL context.
    ensureRootLayer();

    while (OwnPtr<LayerTreeMessageToRenderer> nextMessage = m_messagesToRenderer.tryGetMessage()) {
        switch (nextMessage->type()) {
        case LayerTreeMessageToRenderer::SetRootLayer: {
            const SetRootLayerMessageData& data = static_cast<SetRootLayerMessage*>(nextMessage.get())->data();
            setRootLayerID(data.layerID);
            break;
        }

        case LayerTreeMessageToRenderer::DeleteLayer: {
            const DeleteLayerMessageData& data = static_cast<DeleteLayerMessage*>(nextMessage.get())->data();
            deleteLayer(data.layerID);
            break;
        }

        case LayerTreeMessageToRenderer::SyncLayerParameters: {
            const SyncLayerParametersMessageData& data = static_cast<SyncLayerParametersMessage*>(nextMessage.get())->data();
            syncLayerParameters(data.layerInfo);
            break;
        }

        case LayerTreeMessageToRenderer::CreateTile: {
            const CreateTileMessageData& data = static_cast<CreateTileMessage*>(nextMessage.get())->data();
            createTile(data.layerID, data.remoteTileID, data.scale);
            break;
        }

        case LayerTreeMessageToRenderer::RemoveTile: {
            const RemoveTileMessageData& data = static_cast<RemoveTileMessage*>(nextMessage.get())->data();
            removeTile(data.layerID, data.remoteTileID);
            break;
        }

        case LayerTreeMessageToRenderer::UpdateTile: {
            const UpdateTileMessageData& data = static_cast<UpdateTileMessage*>(nextMessage.get())->data();
            updateTile(data.layerID, data.remoteTileID, data.sourceRect, data.targetRect, data.image);
            break;
        }

        case LayerTreeMessageToRenderer::CreateImage: {
            const CreateImageMessageData& data = static_cast<CreateImageMessage*>(nextMessage.get())->data();
            createImage(data.imageID, data.image);
            break;
        }

        case LayerTreeMessageToRenderer::DestroyImage: {
            const CreateImageMessageData& data = static_cast<CreateImageMessage*>(nextMessage.get())->data();
            destroyImage(data.imageID);
            break;
        }

        case LayerTreeMessageToRenderer::FlushLayerChanges:
            flushLayerChanges();
            break;
        }
    }
}

void LayerTreeHostProxy::pushUpdateToQueue(PassOwnPtr<LayerTreeMessageToRenderer> message)
{
    m_messagesToRenderer.append(message);
    updateViewport();
}

void LayerTreeHostProxy::createTileForLayer(int layerID, int tileID, const WebKit::UpdateInfo& updateInfo)
{
    CreateTileMessageData data;
    data.layerID = layerID;
    data.remoteTileID = tileID;
    data.scale = updateInfo.updateScaleFactor;
    pushUpdateToQueue(CreateTileMessage::create(data));
    updateTileForLayer(layerID, tileID, updateInfo);
}

void LayerTreeHostProxy::updateTileForLayer(int layerID, int tileID, const WebKit::UpdateInfo& updateInfo)
{
    UpdateTileMessageData data;
    data.layerID = layerID;
    data.remoteTileID = tileID;
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(updateInfo.bitmapHandle);
    data.image = bitmap->createQImage().copy();
    data.sourceRect = IntRect(IntPoint::zero(), updateInfo.updateRectBounds.size());
    data.targetRect = updateInfo.updateRectBounds;
    pushUpdateToQueue(UpdateTileMessage::create(data));
}

void LayerTreeHostProxy::removeTileForLayer(int layerID, int tileID)
{
    RemoveTileMessageData data;
    data.layerID = layerID;
    data.remoteTileID = tileID;
    pushUpdateToQueue(RemoveTileMessage::create(data));
}


void LayerTreeHostProxy::deleteCompositingLayer(WebLayerID id)
{
    DeleteLayerMessageData data;
    data.layerID = id;
    pushUpdateToQueue(DeleteLayerMessage::create(data));
}

void LayerTreeHostProxy::setRootCompositingLayer(WebLayerID id)
{
    SetRootLayerMessageData data;
    data.layerID = id;
    pushUpdateToQueue(SetRootLayerMessage::create(data));
}

void LayerTreeHostProxy::syncCompositingLayerState(const WebLayerInfo& info)
{
    SyncLayerParametersMessageData data;
    data.layerInfo = info;
    pushUpdateToQueue(SyncLayerParametersMessage::create(data));
}

void LayerTreeHostProxy::didRenderFrame()
{
    m_drawingAreaProxy->page()->process()->send(Messages::LayerTreeHost::RenderNextFrame(), m_drawingAreaProxy->page()->pageID());
    pushUpdateToQueue(FlushLayerChangesMessage::create());
    updateViewport();
}

void LayerTreeHostProxy::createDirectlyCompositedImage(int64_t key, const WebKit::ShareableBitmap::Handle& handle)
{
    CreateImageMessageData data;
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(handle);
    data.imageID = key;
    data.image = bitmap->createQImage().copy();
    pushUpdateToQueue(CreateImageMessage::create(data));
}

void LayerTreeHostProxy::destroyDirectlyCompositedImage(int64_t key)
{
    DestroyImageMessageData data;
    data.imageID = key;
    pushUpdateToQueue(DestroyImageMessage::create(data));
}

void LayerTreeHostProxy::setVisibleContentRectTrajectoryVector(const FloatPoint& trajectoryVector)
{
    m_drawingAreaProxy->page()->process()->send(Messages::LayerTreeHost::SetVisibleContentRectTrajectoryVector(trajectoryVector), m_drawingAreaProxy->page()->pageID());
}

void LayerTreeHostProxy::setVisibleContentsRectAndScale(const IntRect& rect, float scale)
{
    m_visibleContentsRect = rect;
    m_contentsScale = scale;
    m_drawingAreaProxy->page()->process()->send(Messages::LayerTreeHost::SetVisibleContentRectAndScale(rect, scale), m_drawingAreaProxy->page()->pageID());
}

void LayerTreeHostProxy::purgeGLResources()
{
    TextureMapperNode* node = toTextureMapperNode(rootLayer());

    if (node)
        node->purgeNodeTexturesRecursive();

    m_directlyCompositedImages.clear();

    m_textureMapper.clear();

    m_drawingAreaProxy->page()->process()->send(Messages::LayerTreeHost::PurgeBackingStores(), m_drawingAreaProxy->page()->pageID());
}

}
#endif
