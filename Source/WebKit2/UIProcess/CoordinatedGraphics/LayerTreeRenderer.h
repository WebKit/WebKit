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

#ifndef LayerTreeRenderer_h
#define LayerTreeRenderer_h

#if USE(COORDINATED_GRAPHICS)
#include "BackingStore.h"
#include "ShareableSurface.h"
#include "TextureMapper.h"
#include "TextureMapperBackingStore.h"
#include "WebLayerTreeInfo.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/GraphicsLayerAnimation.h>
#include <WebCore/GraphicsSurface.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <WebCore/RunLoop.h>
#include <WebCore/Timer.h>
#include <wtf/Functional.h>
#include <wtf/HashSet.h>
#include <wtf/ThreadingPrimitives.h>

namespace WebKit {

class CoordinatedBackingStore;
class LayerTreeCoordinatorProxy;
class WebLayerInfo;
class WebLayerUpdateInfo;

class LayerTreeRenderer : public ThreadSafeRefCounted<LayerTreeRenderer>, public WebCore::GraphicsLayerClient {
public:
    struct TileUpdate {
        WebCore::IntRect sourceRect;
        WebCore::IntRect tileRect;
        RefPtr<ShareableSurface> surface;
        WebCore::IntPoint offset;
        TileUpdate(const WebCore::IntRect& source, const WebCore::IntRect& tile, PassRefPtr<ShareableSurface> newSurface, const WebCore::IntPoint& newOffset)
            : sourceRect(source)
            , tileRect(tile)
            , surface(newSurface)
            , offset(newOffset)
        {
        }
    };
    explicit LayerTreeRenderer(LayerTreeCoordinatorProxy*);
    virtual ~LayerTreeRenderer();
    void purgeGLResources();
    void paintToCurrentGLContext(const WebCore::TransformationMatrix&, float, const WebCore::FloatRect&, WebCore::TextureMapper::PaintFlags = 0);
    void paintToGraphicsContext(BackingStore::PlatformGraphicsContext);
    void syncRemoteContent();
    void setContentsSize(const WebCore::FloatSize&);
    void setVisibleContentsRect(const WebCore::FloatRect&);
    void didChangeScrollPosition(const WebCore::IntPoint& position);
#if USE(GRAPHICS_SURFACE)
    void syncCanvas(uint32_t id, const WebCore::IntSize& canvasSize, const WebCore::GraphicsSurfaceToken&, uint32_t frontBuffer);
#endif

    void detach();
    void appendUpdate(const Function<void()>&);
    void setActive(bool);

    void deleteLayer(WebLayerID);
    void setRootLayerID(WebLayerID);
    void setLayerChildren(WebLayerID, const Vector<WebLayerID>&);
    void setLayerState(WebLayerID, const WebLayerInfo&);
#if ENABLE(CSS_FILTERS)
    void setLayerFilters(WebLayerID, const WebCore::FilterOperations&);
#endif

    void createTile(WebLayerID, int, float scale, const WebCore::IntSize&);
    void removeTile(WebLayerID, int);
    void updateTile(WebLayerID, int, const TileUpdate&);
    void flushLayerChanges();
    void createImage(int64_t, PassRefPtr<ShareableBitmap>);
    void destroyImage(int64_t);
    void setLayerAnimations(WebLayerID, const WebCore::GraphicsLayerAnimations&);
    void setAnimationsLocked(bool);

#if ENABLE(REQUEST_ANIMATION_FRAME)
    void requestAnimationFrame();
    void animationFrameReady();
#endif

private:
    PassOwnPtr<WebCore::GraphicsLayer> createLayer(WebLayerID);

    WebCore::GraphicsLayer* layerByID(WebLayerID id) { return (id == InvalidWebLayerID) ? 0 : m_layers.get(id); }
    WebCore::GraphicsLayer* rootLayer() { return m_rootLayer.get(); }

    // Reimplementations from WebCore::GraphicsLayerClient.
    virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double) { }
    virtual void notifyFlushRequired(const WebCore::GraphicsLayer*) { }
    void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect&) { }
    void updateViewport();
    void dispatchOnMainThread(const Function<void()>&);
    void adjustPositionForFixedLayers();

    void assignImageToLayer(WebCore::GraphicsLayer*, int64_t imageID);
    void ensureRootLayer();
    void ensureLayer(WebLayerID);
    void commitTileOperations();
    void renderNextFrame();
    void purgeBackingStores();

    PassRefPtr<CoordinatedBackingStore> getBackingStore(WebLayerID);
    void removeBackingStoreIfNeeded(WebLayerID, int tileID);

    typedef HashMap<WebLayerID, WebCore::GraphicsLayer*> LayerMap;
    WebCore::FloatSize m_contentsSize;
    WebCore::FloatRect m_visibleContentsRect;

    // Render queue can be accessed ony from main thread or updatePaintNode call stack!
    Vector<Function<void()> > m_renderQueue;
    Mutex m_renderQueueMutex;

    OwnPtr<WebCore::TextureMapper> m_textureMapper;
    HashMap<int64_t, RefPtr<WebCore::TextureMapperBackingStore> > m_directlyCompositedImages;
    HashSet<RefPtr<CoordinatedBackingStore> > m_backingStoresWithPendingBuffers;

#if USE(GRAPHICS_SURFACE)
    typedef HashMap<WebLayerID, RefPtr<WebCore::TextureMapperSurfaceBackingStore> > SurfaceBackingStoreMap;
    SurfaceBackingStoreMap m_surfaceBackingStores;
#endif

    LayerTreeCoordinatorProxy* m_layerTreeCoordinatorProxy;
    OwnPtr<WebCore::GraphicsLayer> m_rootLayer;

    LayerMap m_layers;
    LayerMap m_fixedLayers;
    WebLayerID m_rootLayerID;
    WebCore::IntPoint m_renderedContentsScrollPosition;
    WebCore::IntPoint m_pendingRenderedContentsScrollPosition;
    bool m_isActive;
    bool m_animationsLocked;
#if ENABLE(REQUEST_ANIMATION_FRAME)
    bool m_animationFrameRequested;
#endif
};

};

#endif // USE(COORDINATED_GRAPHICS)

#endif // LayerTreeRenderer_h


