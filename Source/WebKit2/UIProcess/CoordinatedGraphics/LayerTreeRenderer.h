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
#include "CoordinatedLayerInfo.h"
#include "CoordinatedSurface.h"
#include "TextureMapper.h"
#include "TextureMapperBackingStore.h"
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

namespace WebCore {
class CustomFilterProgram;
class CustomFilterProgramInfo;
class TextureMapperLayer;
}

namespace WebKit {

class CoordinatedBackingStore;
class CoordinatedLayerTreeHostProxy;
class CoordinatedLayerInfo;

class LayerTreeRenderer : public ThreadSafeRefCounted<LayerTreeRenderer>, public WebCore::GraphicsLayerClient {
public:
    struct TileUpdate {
        WebCore::IntRect sourceRect;
        WebCore::IntRect tileRect;
        RefPtr<CoordinatedSurface> surface;
        WebCore::IntPoint offset;
        TileUpdate(const WebCore::IntRect& source, const WebCore::IntRect& tile, PassRefPtr<CoordinatedSurface> newSurface, const WebCore::IntPoint& newOffset)
            : sourceRect(source)
            , tileRect(tile)
            , surface(newSurface)
            , offset(newOffset)
        {
        }
    };
    explicit LayerTreeRenderer(CoordinatedLayerTreeHostProxy*);
    virtual ~LayerTreeRenderer();
    void paintToCurrentGLContext(const WebCore::TransformationMatrix&, float, const WebCore::FloatRect&, WebCore::TextureMapper::PaintFlags = 0);
    void paintToGraphicsContext(BackingStore::PlatformGraphicsContext);
    void setContentsSize(const WebCore::FloatSize&);
    void setVisibleContentsRect(const WebCore::FloatRect&);
    void didChangeScrollPosition(const WebCore::FloatPoint& position);
#if USE(GRAPHICS_SURFACE)
    void createCanvas(CoordinatedLayerID, const WebCore::IntSize&, PassRefPtr<WebCore::GraphicsSurface>);
    void syncCanvas(CoordinatedLayerID, uint32_t frontBuffer);
    void destroyCanvas(CoordinatedLayerID);
#endif

    void detach();
    void appendUpdate(const Function<void()>&);

    // The painting thread must lock the main thread to use below two methods, because two methods access members that the main thread manages. See m_coordinatedLayerTreeHostProxy.
    // Currently, QQuickWebPage::updatePaintNode() locks the main thread before calling both methods.
    void purgeGLResources();
    void setActive(bool);

    void deleteLayer(CoordinatedLayerID);
    void setRootLayerID(CoordinatedLayerID);
    void setLayerChildren(CoordinatedLayerID, const Vector<CoordinatedLayerID>&);
    void setLayerState(CoordinatedLayerID, const CoordinatedLayerInfo&);
#if ENABLE(CSS_FILTERS)
    void setLayerFilters(CoordinatedLayerID, const WebCore::FilterOperations&);
#endif
#if ENABLE(CSS_SHADERS)
    void injectCachedCustomFilterPrograms(const WebCore::FilterOperations& filters) const;
    void createCustomFilterProgram(int id, const WebCore::CustomFilterProgramInfo&);
    void removeCustomFilterProgram(int id);
#endif

    void createTile(CoordinatedLayerID, uint32_t tileID, float scale);
    void removeTile(CoordinatedLayerID, uint32_t tileID);
    void updateTile(CoordinatedLayerID, uint32_t tileID, const TileUpdate&);
    void flushLayerChanges();
    void createImageBacking(CoordinatedImageBackingID);
    void updateImageBacking(CoordinatedImageBackingID, PassRefPtr<CoordinatedSurface>);
    void clearImageBackingContents(CoordinatedImageBackingID);
    void removeImageBacking(CoordinatedImageBackingID);
    void setLayerAnimations(CoordinatedLayerID, const WebCore::GraphicsLayerAnimations&);
    void setAnimationsLocked(bool);
    void setBackgroundColor(const WebCore::Color&);
    void setDrawsBackground(bool enable) { m_setDrawsBackground = enable; }

#if ENABLE(REQUEST_ANIMATION_FRAME)
    void requestAnimationFrame();
#endif

private:
    PassOwnPtr<WebCore::GraphicsLayer> createLayer(CoordinatedLayerID);

    WebCore::GraphicsLayer* layerByID(CoordinatedLayerID id) { return (id == InvalidCoordinatedLayerID) ? 0 : m_layers.get(id); }
    WebCore::GraphicsLayer* rootLayer() { return m_rootLayer.get(); }

    void syncRemoteContent();
    void adjustPositionForFixedLayers();

    // Reimplementations from WebCore::GraphicsLayerClient.
    virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double) { }
    virtual void notifyFlushRequired(const WebCore::GraphicsLayer*) { }
    virtual void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect&) OVERRIDE { }

    void dispatchOnMainThread(const Function<void()>&);
    void updateViewport();
#if ENABLE(REQUEST_ANIMATION_FRAME)
    void animationFrameReady();
#endif
    void renderNextFrame();
    void purgeBackingStores();

    void assignImageBackingToLayer(WebCore::GraphicsLayer*, CoordinatedImageBackingID);
    void removeReleasedImageBackingsIfNeeded();
    void ensureRootLayer();
    WebCore::GraphicsLayer* ensureLayer(CoordinatedLayerID);
    void commitPendingBackingStoreOperations();

    CoordinatedBackingStore* getBackingStore(WebCore::GraphicsLayer*);
    void prepareContentBackingStore(WebCore::GraphicsLayer*);
    void createBackingStoreIfNeeded(WebCore::GraphicsLayer*);
    void removeBackingStoreIfNeeded(WebCore::GraphicsLayer*);
    void resetBackingStoreSizeToLayerSize(WebCore::GraphicsLayer*);

    WebCore::FloatSize m_contentsSize;
    WebCore::FloatRect m_visibleContentsRect;

    // Render queue can be accessed ony from main thread or updatePaintNode call stack!
    Vector<Function<void()> > m_renderQueue;
    Mutex m_renderQueueMutex;

    OwnPtr<WebCore::TextureMapper> m_textureMapper;

    typedef HashMap<CoordinatedImageBackingID, RefPtr<CoordinatedBackingStore> > ImageBackingMap;
    ImageBackingMap m_imageBackings;
    Vector<RefPtr<CoordinatedBackingStore> > m_releasedImageBackings;

    typedef HashMap<WebCore::TextureMapperLayer*, RefPtr<CoordinatedBackingStore> > BackingStoreMap;
    BackingStoreMap m_pendingSyncBackingStores;

    HashSet<RefPtr<CoordinatedBackingStore> > m_backingStoresWithPendingBuffers;

#if USE(GRAPHICS_SURFACE)
    typedef HashMap<CoordinatedLayerID, RefPtr<WebCore::TextureMapperSurfaceBackingStore> > SurfaceBackingStoreMap;
    SurfaceBackingStoreMap m_surfaceBackingStores;
#endif

    // Below two members are accessed by only the main thread. The painting thread must lock the main thread to access both members.
    CoordinatedLayerTreeHostProxy* m_coordinatedLayerTreeHostProxy;
    bool m_isActive;

    OwnPtr<WebCore::GraphicsLayer> m_rootLayer;

    typedef HashMap<CoordinatedLayerID, OwnPtr<WebCore::GraphicsLayer> > LayerMap;
    LayerMap m_layers;
    typedef HashMap<CoordinatedLayerID, WebCore::GraphicsLayer*> LayerRawPtrMap;
    LayerRawPtrMap m_fixedLayers;
    CoordinatedLayerID m_rootLayerID;
    WebCore::FloatPoint m_renderedContentsScrollPosition;
    WebCore::FloatPoint m_pendingRenderedContentsScrollPosition;
    bool m_animationsLocked;
#if ENABLE(REQUEST_ANIMATION_FRAME)
    bool m_animationFrameRequested;
#endif
    WebCore::Color m_backgroundColor;
    bool m_setDrawsBackground;

#if ENABLE(CSS_SHADERS)
    typedef HashMap<int, RefPtr<WebCore::CustomFilterProgram> > CustomFilterProgramMap;
    CustomFilterProgramMap m_customFilterPrograms;
#endif
};

};

#endif // USE(COORDINATED_GRAPHICS)

#endif // LayerTreeRenderer_h


