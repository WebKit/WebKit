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

#if USE(UI_SIDE_COMPOSITING)

#include "BackingStore.h"
#include "DrawingAreaProxy.h"
#include "Region.h"
#include "TextureMapper.h"
#include "TextureMapperBackingStore.h"
#include "WebLayerTreeInfo.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <WebCore/RunLoop.h>
#include <WebCore/Timer.h>
#include <wtf/Functional.h>
#include <wtf/HashSet.h>


namespace WebKit {

class LayerBackingStore;
class WebLayerInfo;
class WebLayerUpdateInfo;

class LayerTreeHostProxy : public WebCore::GraphicsLayerClient {
public:
    LayerTreeHostProxy(DrawingAreaProxy*);
    virtual ~LayerTreeHostProxy();
    void syncCompositingLayerState(const WebLayerInfo&);
    void deleteCompositingLayer(WebLayerID);
    void setRootCompositingLayer(WebLayerID);
    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void paintToCurrentGLContext(const WebCore::TransformationMatrix&, float, const WebCore::FloatRect&);
    void paintToGraphicsContext(BackingStore::PlatformGraphicsContext);
    void purgeGLResources();
    void setVisibleContentsRectForScaling(const WebCore::IntRect&, float);
    void setVisibleContentsRectForPanning(const WebCore::IntRect&, const WebCore::FloatPoint&);
    void syncRemoteContent();
    void swapContentBuffers();
    void didRenderFrame();
    void createTileForLayer(int layerID, int tileID, const WebKit::UpdateInfo&);
    void updateTileForLayer(int layerID, int tileID, const WebKit::UpdateInfo&);
    void removeTileForLayer(int layerID, int tileID);
    void createDirectlyCompositedImage(int64_t, const WebKit::ShareableBitmap::Handle&);
    void destroyDirectlyCompositedImage(int64_t);
    void didReceiveLayerTreeHostProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void updateViewport();

protected:
    PassOwnPtr<WebCore::GraphicsLayer> createLayer(WebLayerID);

    WebCore::GraphicsLayer* layerByID(WebLayerID id) { return (id == InvalidWebLayerID) ? 0 : m_layers.get(id); }
    WebCore::GraphicsLayer* rootLayer() { return m_rootLayer.get(); }

    // Reimplementations from WebCore::GraphicsLayerClient.
    virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double) { }
    virtual void notifySyncRequired(const WebCore::GraphicsLayer*) { }
    virtual bool showDebugBorders(const WebCore::GraphicsLayer*) const { return false; }
    virtual bool showRepaintCounter(const WebCore::GraphicsLayer*) const { return false; }
    void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect&) { }

    DrawingAreaProxy* m_drawingAreaProxy;

    typedef HashMap<WebLayerID, WebCore::GraphicsLayer*> LayerMap;
    WebCore::IntRect m_visibleContentsRect;
    float m_contentsScale;

    Vector<Function<void()> > m_renderQueue;
    void dispatchUpdate(const Function<void()>&);

#if USE(TEXTURE_MAPPER)
    OwnPtr<WebCore::TextureMapper> m_textureMapper;
    PassRefPtr<LayerBackingStore> getBackingStore(WebLayerID);
    HashMap<int64_t, RefPtr<WebCore::TextureMapperBackingStore> > m_directlyCompositedImages;
    HashSet<RefPtr<LayerBackingStore> > m_backingStoresWithPendingBuffers;
#endif

    void scheduleWebViewUpdate();
    void synchronizeViewport();
    void deleteLayer(WebLayerID);
    void setRootLayerID(WebLayerID);
    void syncLayerParameters(const WebLayerInfo&);
    void createTile(WebLayerID, int, float scale);
    void removeTile(WebLayerID, int);
    void updateTile(WebLayerID, int, const WebCore::IntRect&, const WebCore::IntRect&, PassRefPtr<ShareableBitmap>);
    void createImage(int64_t, PassRefPtr<ShareableBitmap>);
    void destroyImage(int64_t);
    void assignImageToLayer(WebCore::GraphicsLayer*, int64_t imageID);
    void flushLayerChanges();
    void ensureRootLayer();
    void ensureLayer(WebLayerID);
    void swapBuffers();
    void syncAnimations();

    OwnPtr<WebCore::GraphicsLayer> m_rootLayer;
    Vector<WebLayerID> m_layersToDelete;

    LayerMap m_layers;
    WebLayerID m_rootLayerID;
    int m_id;
};

}

#endif

#endif // LayerTreeHostProxy_h
