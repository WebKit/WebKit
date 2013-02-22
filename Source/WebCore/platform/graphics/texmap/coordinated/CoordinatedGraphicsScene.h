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

#ifndef CoordinatedGraphicsScene_h
#define CoordinatedGraphicsScene_h

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedLayerInfo.h"
#include "CoordinatedSurface.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerAnimation.h"
#include "GraphicsSurface.h"
#include "IntRect.h"
#include "IntSize.h"
#include "RunLoop.h"
#include "TextureMapper.h"
#include "TextureMapperBackingStore.h"
#include "TextureMapperFPSCounter.h"
#include "Timer.h"
#include <wtf/Functional.h>
#include <wtf/HashSet.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/Vector.h>

#if USE(GRAPHICS_SURFACE)
#include "TextureMapperSurfaceBackingStore.h"
#endif

namespace WebCore {

class CoordinatedBackingStore;
class CoordinatedLayerInfo;
class CustomFilterProgram;
class CustomFilterProgramInfo;
class TextureMapperLayer;

class CoordinatedGraphicsSceneClient {
public:
    virtual ~CoordinatedGraphicsSceneClient() { }
#if ENABLE(REQUEST_ANIMATION_FRAME)
    virtual void animationFrameReady() = 0;
#endif
    virtual void purgeBackingStores() = 0;
    virtual void renderNextFrame() = 0;
    virtual void updateViewport() = 0;
};

class CoordinatedGraphicsScene : public ThreadSafeRefCounted<CoordinatedGraphicsScene>, public GraphicsLayerClient {
public:
    struct TileUpdate {
        IntRect sourceRect;
        IntRect tileRect;
        uint32_t atlasID;
        IntPoint offset;
        TileUpdate(const IntRect& source, const IntRect& tile, uint32_t atlas, const IntPoint& newOffset)
            : sourceRect(source)
            , tileRect(tile)
            , atlasID(atlas)
            , offset(newOffset)
        {
        }
    };
    explicit CoordinatedGraphicsScene(CoordinatedGraphicsSceneClient*);
    virtual ~CoordinatedGraphicsScene();
    void paintToCurrentGLContext(const TransformationMatrix&, float, const FloatRect&, TextureMapper::PaintFlags = 0);
    void paintToGraphicsContext(PlatformGraphicsContext*);
    void setScrollPosition(const FloatPoint&);
#if USE(GRAPHICS_SURFACE)
    void createCanvas(CoordinatedLayerID, const IntSize&, PassRefPtr<GraphicsSurface>);
    void syncCanvas(CoordinatedLayerID, uint32_t frontBuffer);
    void destroyCanvas(CoordinatedLayerID);
#endif
    void setLayerRepaintCount(CoordinatedLayerID, int value);

    void detach();
    void appendUpdate(const Function<void()>&);

    // The painting thread must lock the main thread to use below two methods, because two methods access members that the main thread manages. See m_client.
    // Currently, QQuickWebPage::updatePaintNode() locks the main thread before calling both methods.
    void purgeGLResources();
    void setActive(bool);

    void createLayers(const Vector<CoordinatedLayerID>&);
    void deleteLayers(const Vector<CoordinatedLayerID>&);
    void setRootLayerID(CoordinatedLayerID);
    void setLayerChildren(CoordinatedLayerID, const Vector<CoordinatedLayerID>&);
    void setLayerState(CoordinatedLayerID, const CoordinatedLayerInfo&);
#if ENABLE(CSS_FILTERS)
    void setLayerFilters(CoordinatedLayerID, const FilterOperations&);
#endif
#if ENABLE(CSS_SHADERS)
    void injectCachedCustomFilterPrograms(const FilterOperations& filters) const;
    void createCustomFilterProgram(int id, const CustomFilterProgramInfo&);
    void removeCustomFilterProgram(int id);
#endif

    void createTile(CoordinatedLayerID, uint32_t tileID, float scale);
    void removeTile(CoordinatedLayerID, uint32_t tileID);
    void updateTile(CoordinatedLayerID, uint32_t tileID, const TileUpdate&);
    void createUpdateAtlas(uint32_t atlasID, PassRefPtr<CoordinatedSurface>);
    void removeUpdateAtlas(uint32_t atlasID);
    void flushLayerChanges(const FloatPoint& scrollPosition);
    void createImageBacking(CoordinatedImageBackingID);
    void updateImageBacking(CoordinatedImageBackingID, PassRefPtr<CoordinatedSurface>);
    void clearImageBackingContents(CoordinatedImageBackingID);
    void removeImageBacking(CoordinatedImageBackingID);
    void setLayerAnimations(CoordinatedLayerID, const GraphicsLayerAnimations&);
    void setAnimationsLocked(bool);
    void setBackgroundColor(const Color&);
    void setDrawsBackground(bool enable) { m_setDrawsBackground = enable; }

#if ENABLE(REQUEST_ANIMATION_FRAME)
    void requestAnimationFrame();
#endif

private:
    GraphicsLayer* layerByID(CoordinatedLayerID id)
    {
        ASSERT(m_layers.contains(id));
        ASSERT(id != InvalidCoordinatedLayerID);
        return m_layers.get(id);
    }
    GraphicsLayer* getLayerByIDIfExists(CoordinatedLayerID);
    GraphicsLayer* rootLayer() { return m_rootLayer.get(); }

    void syncRemoteContent();
    void adjustPositionForFixedLayers();

    // Reimplementations from GraphicsLayerClient.
    virtual void notifyAnimationStarted(const GraphicsLayer*, double) { }
    virtual void notifyFlushRequired(const GraphicsLayer*) { }
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect&) OVERRIDE { }

    void dispatchOnMainThread(const Function<void()>&);
    void updateViewport();
#if ENABLE(REQUEST_ANIMATION_FRAME)
    void animationFrameReady();
#endif
    void renderNextFrame();
    void purgeBackingStores();

    void createLayer(CoordinatedLayerID);
    void deleteLayer(CoordinatedLayerID);

    void assignImageBackingToLayer(CoordinatedLayerID, GraphicsLayer*, CoordinatedImageBackingID);
    void removeReleasedImageBackingsIfNeeded();
    void ensureRootLayer();
    void commitPendingBackingStoreOperations();

    void prepareContentBackingStore(GraphicsLayer*);
    void createBackingStoreIfNeeded(GraphicsLayer*);
    void removeBackingStoreIfNeeded(GraphicsLayer*);
    void resetBackingStoreSizeToLayerSize(GraphicsLayer*);

    // Render queue can be accessed ony from main thread or updatePaintNode call stack!
    Vector<Function<void()> > m_renderQueue;
    Mutex m_renderQueueMutex;

    OwnPtr<TextureMapper> m_textureMapper;

    typedef HashMap<CoordinatedImageBackingID, RefPtr<CoordinatedBackingStore> > ImageBackingMap;
    ImageBackingMap m_imageBackings;
    Vector<RefPtr<CoordinatedBackingStore> > m_releasedImageBackings;

    typedef HashMap<GraphicsLayer*, RefPtr<CoordinatedBackingStore> > BackingStoreMap;
    BackingStoreMap m_backingStores;

    HashSet<RefPtr<CoordinatedBackingStore> > m_backingStoresWithPendingBuffers;

#if USE(GRAPHICS_SURFACE)
    typedef HashMap<CoordinatedLayerID, RefPtr<TextureMapperSurfaceBackingStore> > SurfaceBackingStoreMap;
    SurfaceBackingStoreMap m_surfaceBackingStores;
#endif

    typedef HashMap<uint32_t /* atlasID */, RefPtr<CoordinatedSurface> > SurfaceMap;
    SurfaceMap m_surfaces;

    // Below two members are accessed by only the main thread. The painting thread must lock the main thread to access both members.
    CoordinatedGraphicsSceneClient* m_client;
    bool m_isActive;

    OwnPtr<GraphicsLayer> m_rootLayer;

    typedef HashMap<CoordinatedLayerID, OwnPtr<GraphicsLayer> > LayerMap;
    LayerMap m_layers;
    typedef HashMap<CoordinatedLayerID, GraphicsLayer*> LayerRawPtrMap;
    LayerRawPtrMap m_fixedLayers;
    CoordinatedLayerID m_rootLayerID;
    FloatPoint m_scrollPosition;
    FloatPoint m_renderedContentsScrollPosition;
    bool m_animationsLocked;
#if ENABLE(REQUEST_ANIMATION_FRAME)
    bool m_animationFrameRequested;
#endif
    Color m_backgroundColor;
    bool m_setDrawsBackground;

#if ENABLE(CSS_SHADERS)
    typedef HashMap<int, RefPtr<CustomFilterProgram> > CustomFilterProgramMap;
    CustomFilterProgramMap m_customFilterPrograms;
#endif

    TextureMapperFPSCounter m_fpsCounter;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)

#endif // CoordinatedGraphicsScene_h


