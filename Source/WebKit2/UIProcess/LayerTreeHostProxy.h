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

#ifndef LayerTreeHostProxy_h
#define LayerTreeHostProxy_h

#include "BackingStore.h"
#include "DrawingAreaProxy.h"
#include "MessageQueue.h"
#include "Region.h"
#include "RunLoop.h"
#include "WebLayerTreeInfo.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <wtf/HashSet.h>

#if USE(TEXTURE_MAPPER)
#include "TextureMapper.h"
#include "TextureMapperNode.h"
#endif

namespace WebKit {

class WebLayerInfo;
class WebLayerUpdateInfo;

using namespace WebCore;

class LayerTreeMessageToRenderer;

class LayerTreeHostProxy : public GraphicsLayerClient {
public:
    LayerTreeHostProxy(DrawingAreaProxy*);
    virtual ~LayerTreeHostProxy();
    void syncCompositingLayerState(const WebLayerInfo&);
    void deleteCompositingLayer(WebLayerID);
    void setRootCompositingLayer(WebLayerID);
    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void paintToCurrentGLContext(const TransformationMatrix&, float);
    void purgeGLResources();
    void setVisibleContentsRectAndScale(const IntRect&, float);
    void setVisibleContentRectTrajectoryVector(const FloatPoint&);
#if USE(TILED_BACKING_STORE)
    void syncRemoteContent();
    void swapContentBuffers();
    void didRenderFrame();
    void createTileForLayer(int layerID, int tileID, const WebKit::UpdateInfo&);
    void updateTileForLayer(int layerID, int tileID, const WebKit::UpdateInfo&);
    void removeTileForLayer(int layerID, int tileID);
    void createDirectlyCompositedImage(int64_t, const WebKit::ShareableBitmap::Handle&);
    void destroyDirectlyCompositedImage(int64_t);
    void didReceiveLayerTreeHostProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void setVisibleContentsRectForLayer(WebLayerID, const IntRect&);
    void updateViewport();
#endif

protected:
    PassOwnPtr<GraphicsLayer> createLayer(WebLayerID);

    GraphicsLayer* layerByID(WebLayerID id) { return (id == InvalidWebLayerID) ? 0 : m_layers.get(id); }
    GraphicsLayer* rootLayer() { return m_rootLayer.get(); }

    // Reimplementations from WebCore::GraphicsLayerClient.
    virtual void notifyAnimationStarted(const GraphicsLayer*, double) { }
    virtual void notifySyncRequired(const GraphicsLayer*) { }
    virtual bool showDebugBorders() const { return false; }
    virtual bool showRepaintCounter() const { return false; }
    void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect&) { }

    RunLoop::Timer<LayerTreeHostProxy> m_animationTimer;
    DrawingAreaProxy* m_drawingAreaProxy;

    typedef HashMap<WebLayerID, WebCore::GraphicsLayer*> LayerMap;
    IntRect m_visibleContentsRect;
    float m_contentsScale;

    MessageQueue<LayerTreeMessageToRenderer> m_messagesToRenderer;
    void pushUpdateToQueue(PassOwnPtr<LayerTreeMessageToRenderer>);

#if USE(TEXTURE_MAPPER)
    OwnPtr<TextureMapper> m_textureMapper;
#endif

#if PLATFORM(QT)
    typedef HashMap<IntPoint, RefPtr<BitmapTexture> > TiledImage;
    TextureMapperNode::NodeRectMap m_nodeVisualContentsRectMap;
    HashMap<int, int> m_tileToNodeTile;
    int remoteTileIDToNodeTileID(int tileID) const;
    HashMap<int64_t, TiledImage> m_directlyCompositedImages;

    void scheduleWebViewUpdate();
    void synchronizeViewport();
    void deleteLayer(WebLayerID);
    void setRootLayerID(WebLayerID);
    void syncLayerParameters(const WebLayerInfo&);
    void createTile(WebLayerID, int, float scale);
    void removeTile(WebLayerID, int);
    void updateTile(WebLayerID, int, const IntRect&, const IntRect&, const QImage&);
    void createImage(int64_t, const QImage&);
    void destroyImage(int64_t);
    void assignImageToLayer(WebCore::GraphicsLayer*, int64_t imageID);
    void flushLayerChanges();
    void ensureRootLayer();
    void ensureLayer(WebLayerID);

#endif

    OwnPtr<GraphicsLayer> m_rootLayer;
    Vector<WebLayerID> m_layersToDelete;

#if PLATFORM(QT)
    void didFireViewportUpdateTimer(Timer<LayerTreeHostProxy>*);
    Timer<LayerTreeHostProxy> m_viewportUpdateTimer;
#endif

    LayerMap m_layers;
    WebLayerID m_rootLayerID;
    int m_id;
};

}

#endif // LayerTreeHostProxy_h
