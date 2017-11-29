/*
    Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2013 Company 100, Inc.

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

#include <WebCore/CoordinatedGraphicsState.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <WebCore/TextureMapper.h>
#include <WebCore/TextureMapperBackingStore.h>
#include <WebCore/TextureMapperFPSCounter.h>
#include <WebCore/TextureMapperLayer.h>
#include <WebCore/Timer.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/Vector.h>

#if USE(COORDINATED_GRAPHICS_THREADED)
#include <WebCore/TextureMapperPlatformLayerProxy.h>
#endif

namespace Nicosia {
class Buffer;
}

namespace WebCore {
class TextureMapperGL;
}

namespace WebKit {

class CoordinatedBackingStore;

class CoordinatedGraphicsSceneClient {
public:
    virtual ~CoordinatedGraphicsSceneClient() { }
    virtual void renderNextFrame() = 0;
    virtual void updateViewport() = 0;
    virtual void commitScrollOffset(uint32_t layerID, const WebCore::IntSize& offset) = 0;
};

class CoordinatedGraphicsScene : public ThreadSafeRefCounted<CoordinatedGraphicsScene>, public WebCore::TextureMapperLayer::ScrollingClient
#if USE(COORDINATED_GRAPHICS_THREADED)
    , public WebCore::TextureMapperPlatformLayerProxy::Compositor
#endif
{
public:
    explicit CoordinatedGraphicsScene(CoordinatedGraphicsSceneClient*);
    virtual ~CoordinatedGraphicsScene();

    void applyStateChanges(const Vector<WebCore::CoordinatedGraphicsState>&);
    void paintToCurrentGLContext(const WebCore::TransformationMatrix&, float, const WebCore::FloatRect&, const WebCore::Color& backgroundColor, bool drawsBackground, const WebCore::FloatPoint&, WebCore::TextureMapper::PaintFlags = 0);
    void detach();

    WebCore::TextureMapperLayer* findScrollableContentsLayerAt(const WebCore::FloatPoint&);

    void commitScrollOffset(uint32_t layerID, const WebCore::IntSize& offset) override;

    // The painting thread must lock the main thread to use below two methods, because two methods access members that the main thread manages. See m_client.
    // Currently, QQuickWebPage::updatePaintNode() locks the main thread before calling both methods.
    void purgeGLResources();

    bool isActive() const { return m_isActive; }
    void setActive(bool);

    void commitSceneState(const WebCore::CoordinatedGraphicsState&);
    void renderNextFrame();

    void setViewBackgroundColor(const WebCore::Color& color) { m_viewBackgroundColor = color; }
    WebCore::Color viewBackgroundColor() const { return m_viewBackgroundColor; }

    void releaseUpdateAtlases(const Vector<uint32_t>&);

private:
    void setRootLayerID(WebCore::CoordinatedLayerID);
    void createLayers(const Vector<WebCore::CoordinatedLayerID>&);
    void deleteLayers(const Vector<WebCore::CoordinatedLayerID>&);
    void setLayerState(WebCore::CoordinatedLayerID, const WebCore::CoordinatedGraphicsLayerState&);
    void setLayerChildrenIfNeeded(WebCore::TextureMapperLayer*, const WebCore::CoordinatedGraphicsLayerState&);
    void updateTilesIfNeeded(WebCore::TextureMapperLayer*, const WebCore::CoordinatedGraphicsLayerState&);
    void createTilesIfNeeded(WebCore::TextureMapperLayer*, const WebCore::CoordinatedGraphicsLayerState&);
    void removeTilesIfNeeded(WebCore::TextureMapperLayer*, const WebCore::CoordinatedGraphicsLayerState&);
    void setLayerFiltersIfNeeded(WebCore::TextureMapperLayer*, const WebCore::CoordinatedGraphicsLayerState&);
    void setLayerAnimationsIfNeeded(WebCore::TextureMapperLayer*, const WebCore::CoordinatedGraphicsLayerState&);
    void syncPlatformLayerIfNeeded(WebCore::TextureMapperLayer*, const WebCore::CoordinatedGraphicsLayerState&);
    void setLayerRepaintCountIfNeeded(WebCore::TextureMapperLayer*, const WebCore::CoordinatedGraphicsLayerState&);

    void syncUpdateAtlases(const WebCore::CoordinatedGraphicsState&);
    void createUpdateAtlas(uint32_t atlasID, RefPtr<Nicosia::Buffer>&&);
    void removeUpdateAtlas(uint32_t atlasID);

    void syncImageBackings(const WebCore::CoordinatedGraphicsState&);
    void createImageBacking(WebCore::CoordinatedImageBackingID);
    void updateImageBacking(WebCore::CoordinatedImageBackingID, RefPtr<Nicosia::Buffer>&&);
    void clearImageBackingContents(WebCore::CoordinatedImageBackingID);
    void removeImageBacking(WebCore::CoordinatedImageBackingID);

    WebCore::TextureMapperLayer* layerByID(WebCore::CoordinatedLayerID id)
    {
        ASSERT(m_layers.contains(id));
        ASSERT(id != WebCore::InvalidCoordinatedLayerID);
        return m_layers.get(id);
    }
    WebCore::TextureMapperLayer* getLayerByIDIfExists(WebCore::CoordinatedLayerID);
    WebCore::TextureMapperLayer* rootLayer() { return m_rootLayer.get(); }

    void adjustPositionForFixedLayers(const WebCore::FloatPoint& contentPosition);

    void dispatchOnMainThread(Function<void()>&&);
    void dispatchOnClientRunLoop(Function<void()>&&);
    void updateViewport();

    void createLayer(WebCore::CoordinatedLayerID);
    void deleteLayer(WebCore::CoordinatedLayerID);

    void assignImageBackingToLayer(WebCore::TextureMapperLayer*, WebCore::CoordinatedImageBackingID);
    void removeReleasedImageBackingsIfNeeded();
    void ensureRootLayer();
    void commitPendingBackingStoreOperations();

    void prepareContentBackingStore(WebCore::TextureMapperLayer*);
    void createBackingStoreIfNeeded(WebCore::TextureMapperLayer*);
    void removeBackingStoreIfNeeded(WebCore::TextureMapperLayer*);
    void resetBackingStoreSizeToLayerSize(WebCore::TextureMapperLayer*);

#if USE(COORDINATED_GRAPHICS_THREADED)
    void onNewBufferAvailable() override;
    WebCore::TextureMapperGL* texmapGL() override;
#endif

    std::unique_ptr<WebCore::TextureMapper> m_textureMapper;

    HashMap<WebCore::CoordinatedImageBackingID, RefPtr<CoordinatedBackingStore>> m_imageBackings;
    Vector<RefPtr<CoordinatedBackingStore>> m_releasedImageBackings;

    HashMap<WebCore::TextureMapperLayer*, RefPtr<CoordinatedBackingStore>> m_backingStores;
    HashSet<RefPtr<CoordinatedBackingStore>> m_backingStoresWithPendingBuffers;

#if USE(COORDINATED_GRAPHICS_THREADED)
    HashMap<WebCore::TextureMapperLayer*, RefPtr<WebCore::TextureMapperPlatformLayerProxy>> m_platformLayerProxies;
#endif

    HashMap<uint32_t /* atlasID */, RefPtr<Nicosia::Buffer>> m_surfaces;

    // Below two members are accessed by only the main thread. The painting thread must lock the main thread to access both members.
    CoordinatedGraphicsSceneClient* m_client;
    bool m_isActive;

    std::unique_ptr<WebCore::TextureMapperLayer> m_rootLayer;

    HashMap<WebCore::CoordinatedLayerID, std::unique_ptr<WebCore::TextureMapperLayer>> m_layers;
    HashMap<WebCore::CoordinatedLayerID, WebCore::TextureMapperLayer*> m_fixedLayers;
    WebCore::CoordinatedLayerID m_rootLayerID;
    WebCore::FloatPoint m_scrollPosition;
    WebCore::FloatPoint m_renderedContentsScrollPosition;
    WebCore::Color m_viewBackgroundColor;

    WebCore::TextureMapperFPSCounter m_fpsCounter;

    RunLoop& m_clientRunLoop;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

#endif // CoordinatedGraphicsScene_h


