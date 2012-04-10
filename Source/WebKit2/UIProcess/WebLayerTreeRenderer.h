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

#ifndef WebLayerTreeRenderer_h
#define WebLayerTreeRenderer_h

#if USE(UI_SIDE_COMPOSITING)

#include "BackingStore.h"
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
#include <wtf/ThreadingPrimitives.h>

namespace WebKit {

class LayerBackingStore;
class LayerTreeHostProxy;
class WebLayerInfo;
class WebLayerUpdateInfo;

class WebLayerTreeRenderer : public ThreadSafeRefCounted<WebLayerTreeRenderer>, public WebCore::GraphicsLayerClient {
public:
    struct TileUpdate {
        WebCore::IntRect sourceRect;
        WebCore::IntRect targetRect;
        RefPtr<ShareableBitmap> bitmap;
        WebCore::IntPoint offset;
        TileUpdate(const WebCore::IntRect& source, const WebCore::IntRect& target, PassRefPtr<ShareableBitmap> newBitmap, const WebCore::IntPoint& newOffset)
            : sourceRect(source)
            , targetRect(target)
            , bitmap(newBitmap)
            , offset(newOffset)
        {
        }
    };
    WebLayerTreeRenderer(LayerTreeHostProxy*);
    virtual ~WebLayerTreeRenderer();
    void purgeGLResources();
    void paintToCurrentGLContext(const WebCore::TransformationMatrix&, float, const WebCore::FloatRect&);
    void paintToGraphicsContext(BackingStore::PlatformGraphicsContext);
    void syncRemoteContent();
    void setVisibleContentsRect(const WebCore::IntRect&, float scale);

    void detach();
    void appendUpdate(const Function<void()>&);
    void updateViewport();

    void deleteLayer(WebLayerID);
    void setRootLayerID(WebLayerID);
    void setLayerChildren(WebLayerID, const Vector<WebLayerID>&);
    void setLayerState(WebLayerID, const WebLayerInfo&);
    void createTile(WebLayerID, int, float scale);
    void removeTile(WebLayerID, int);
    void updateTile(WebLayerID, int, const TileUpdate&);
    void flushLayerChanges();
    void createImage(int64_t, PassRefPtr<ShareableBitmap>);
    void destroyImage(int64_t);

private:
    PassOwnPtr<WebCore::GraphicsLayer> createLayer(WebLayerID);

    WebCore::GraphicsLayer* layerByID(WebLayerID id) { return (id == InvalidWebLayerID) ? 0 : m_layers.get(id); }
    WebCore::GraphicsLayer* rootLayer() { return m_rootLayer.get(); }

    // Reimplementations from WebCore::GraphicsLayerClient.
    virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double) { }
    virtual void notifySyncRequired(const WebCore::GraphicsLayer*) { }
    virtual bool showDebugBorders(const WebCore::GraphicsLayer*) const { return false; }
    virtual bool showRepaintCounter(const WebCore::GraphicsLayer*) const { return false; }
    void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect&) { }
    void callOnMainTread(const Function<void()>&);

    typedef HashMap<WebLayerID, WebCore::GraphicsLayer*> LayerMap;
    WebCore::IntRect m_visibleContentsRect;
    float m_contentsScale;

    // Render queue can be accessed ony from main thread or updatePaintNode call stack!
    Vector<Function<void()> > m_renderQueue;

#if USE(TEXTURE_MAPPER)
    OwnPtr<WebCore::TextureMapper> m_textureMapper;
    PassRefPtr<LayerBackingStore> getBackingStore(WebLayerID);
    HashMap<int64_t, RefPtr<WebCore::TextureMapperBackingStore> > m_directlyCompositedImages;
    HashSet<RefPtr<LayerBackingStore> > m_backingStoresWithPendingBuffers;
#endif

    void scheduleWebViewUpdate();
    void synchronizeViewport();
    void assignImageToLayer(WebCore::GraphicsLayer*, int64_t imageID);
    void ensureRootLayer();
    void ensureLayer(WebLayerID);
    void commitTileOperations();
    void syncAnimations();
    void renderNextFrame();
    void purgeBackingStores();

    LayerTreeHostProxy* m_layerTreeHostProxy;
    OwnPtr<WebCore::GraphicsLayer> m_rootLayer;
    Vector<WebLayerID> m_layersToDelete;

    LayerMap m_layers;
    WebLayerID m_rootLayerID;
};

};

#endif // USE(UI_SIDE_COMPOSITING)

#endif // WebLayerTreeRenderer_h


