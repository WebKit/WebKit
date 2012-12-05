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

#ifndef LayerTreeCoordinator_h
#define LayerTreeCoordinator_h

#if USE(COORDINATED_GRAPHICS)

#include "CoordinatedGraphicsLayer.h"
#include "CoordinatedImageBacking.h"
#include "LayerTreeContext.h"
#include "LayerTreeHost.h"
#include "Timer.h"
#include "UpdateAtlas.h"
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/GraphicsLayerFactory.h>
#include <wtf/OwnPtr.h>

#if ENABLE(CSS_SHADERS)
#include "WebCustomFilterProgramProxy.h"
#endif

namespace WebKit {

class UpdateInfo;
class WebPage;

class LayerTreeCoordinator : public LayerTreeHost, WebCore::GraphicsLayerClient
    , public CoordinatedGraphicsLayerClient
    , public CoordinatedImageBacking::Coordinator
    , public UpdateAtlasClient
    , public WebCore::GraphicsLayerFactory
#if ENABLE(CSS_SHADERS)
    , WebCustomFilterProgramProxyClient
#endif
{
public:
    static PassRefPtr<LayerTreeCoordinator> create(WebPage*);
    virtual ~LayerTreeCoordinator();

    virtual const LayerTreeContext& layerTreeContext() { return m_layerTreeContext; }
    virtual void setLayerFlushSchedulingEnabled(bool);
    virtual void scheduleLayerFlush();
    virtual void setShouldNotifyAfterNextScheduledLayerFlush(bool);
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*);
    virtual void invalidate();

    virtual void setNonCompositedContentsNeedDisplay(const WebCore::IntRect&);
    virtual void scrollNonCompositedContents(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset);
    virtual void forceRepaint();
    virtual bool forceRepaintAsync(uint64_t callbackID);
    virtual void sizeDidChange(const WebCore::IntSize& newSize);

    virtual void didInstallPageOverlay();
    virtual void didUninstallPageOverlay();
    virtual void setPageOverlayNeedsDisplay(const WebCore::IntRect&);
    virtual void setPageOverlayOpacity(float);
    virtual bool pageOverlayShouldApplyFadeWhenPainting() const { return false; }

    virtual void pauseRendering() { m_isSuspended = true; }
    virtual void resumeRendering() { m_isSuspended = false; scheduleLayerFlush(); }
    virtual void deviceScaleFactorDidChange() { }
    virtual PassRefPtr<CoordinatedImageBacking> createImageBackingIfNeeded(WebCore::Image*) OVERRIDE;

    virtual void createTile(WebLayerID, uint32_t tileID, const SurfaceUpdateInfo&, const WebCore::IntRect&);
    virtual void updateTile(WebLayerID, uint32_t tileID, const SurfaceUpdateInfo&, const WebCore::IntRect&);
    virtual void removeTile(WebLayerID, uint32_t tileID);
    virtual WebCore::IntRect visibleContentsRect() const;
    virtual void renderNextFrame();
    virtual void purgeBackingStores();
    virtual bool layerTreeTileUpdatesAllowed() const;
    virtual void setVisibleContentsRect(const WebCore::IntRect&, float scale, const WebCore::FloatPoint&);
    virtual void didReceiveLayerTreeCoordinatorMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);
    virtual WebCore::GraphicsLayerFactory* graphicsLayerFactory() OVERRIDE;

    virtual void syncLayerState(WebLayerID, const WebLayerInfo&);
    virtual void syncLayerChildren(WebLayerID, const Vector<WebLayerID>&);
    virtual void setLayerAnimations(WebLayerID, const WebCore::GraphicsLayerAnimations&);
#if ENABLE(CSS_FILTERS)
    virtual void syncLayerFilters(WebLayerID, const WebCore::FilterOperations&);
#endif
#if USE(GRAPHICS_SURFACE)
    virtual void createCanvas(WebLayerID, WebCore::PlatformLayer*) OVERRIDE;
    virtual void syncCanvas(WebLayerID, WebCore::PlatformLayer*) OVERRIDE;
    virtual void destroyCanvas(WebLayerID) OVERRIDE;
#endif
    virtual void detachLayer(WebCore::CoordinatedGraphicsLayer*);
    virtual void syncFixedLayers();

    virtual PassOwnPtr<WebCore::GraphicsContext> beginContentUpdate(const WebCore::IntSize&, ShareableBitmap::Flags, int& atlasID, WebCore::IntPoint&);

    // UpdateAtlasClient
    virtual void createUpdateAtlas(int atlasID, const ShareableSurface::Handle&);
    virtual void removeUpdateAtlas(int atlasID);

#if ENABLE(REQUEST_ANIMATION_FRAME)
    virtual void scheduleAnimation() OVERRIDE;
#endif
    virtual void setBackgroundColor(const WebCore::Color&) OVERRIDE;

protected:
    explicit LayerTreeCoordinator(WebPage*);

private:
    // GraphicsLayerClient
    virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double time);
    virtual void notifyFlushRequired(const WebCore::GraphicsLayer*);
    virtual void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& clipRect);

    // CoordinatedImageBacking::Coordinator
    virtual void createImageBacking(CoordinatedImageBackingID) OVERRIDE;
    virtual void updateImageBacking(CoordinatedImageBackingID, const ShareableSurface::Handle&) OVERRIDE;
    virtual void clearImageBackingContents(CoordinatedImageBackingID) OVERRIDE;
    virtual void removeImageBacking(CoordinatedImageBackingID) OVERRIDE;

    void flushPendingImageBackingChanges();

    // GraphicsLayerFactory
    virtual PassOwnPtr<WebCore::GraphicsLayer> createGraphicsLayer(WebCore::GraphicsLayerClient*) OVERRIDE;

    // LayerTreeCoordinator
    void createPageOverlayLayer();
    void destroyPageOverlayLayer();
    bool flushPendingLayerChanges();
    void cancelPendingLayerFlush();
    void performScheduledLayerFlush();
    void didPerformScheduledLayerFlush();
    void syncDisplayState();
    void lockAnimations();
    void unlockAnimations();

    void layerFlushTimerFired(WebCore::Timer<LayerTreeCoordinator>*);

    void scheduleReleaseInactiveAtlases();
#if ENABLE(REQUEST_ANIMATION_FRAME)
    void animationFrameReady();
#endif

    void releaseInactiveAtlasesTimerFired(WebCore::Timer<LayerTreeCoordinator>*);

#if ENABLE(CSS_SHADERS)
    // WebCustomFilterProgramProxyClient
    void removeCustomFilterProgramProxy(WebCustomFilterProgramProxy*);

    void checkCustomFilterProgramProxies(const WebCore::FilterOperations&);
    void disconnectCustomFilterPrograms();
#endif

    OwnPtr<WebCore::GraphicsLayer> m_rootLayer;

    // The layer which contains all non-composited content.
    OwnPtr<WebCore::GraphicsLayer> m_nonCompositedContentLayer;

    // The page overlay layer. Will be null if there's no page overlay.
    OwnPtr<WebCore::GraphicsLayer> m_pageOverlayLayer;

    HashSet<WebCore::CoordinatedGraphicsLayer*> m_registeredLayers;
    Vector<WebLayerID> m_detachedLayers;
    typedef HashMap<CoordinatedImageBackingID, RefPtr<CoordinatedImageBacking> > ImageBackingMap;
    ImageBackingMap m_imageBackings;
    Vector<OwnPtr<UpdateAtlas> > m_updateAtlases;

#if ENABLE(CSS_SHADERS)
    HashSet<WebCustomFilterProgramProxy*> m_customFilterPrograms;
#endif

    bool m_notifyAfterScheduledLayerFlush;
    bool m_isValid;
    // We don't send the messages related to releasing resources to UI Process during purging, because UI Process already had removed all resources.
    bool m_isPurging;

    bool m_waitingForUIProcess;
    bool m_isSuspended;
    WebCore::IntRect m_visibleContentsRect;
    float m_contentsScale;
    bool m_shouldSendScrollPositionUpdate;

    LayerTreeContext m_layerTreeContext;
    bool m_shouldSyncFrame;
    bool m_shouldSyncRootLayer;
    WebCore::Timer<LayerTreeCoordinator> m_layerFlushTimer;
    WebCore::Timer<LayerTreeCoordinator> m_releaseInactiveAtlasesTimer;
    bool m_layerFlushSchedulingEnabled;
    uint64_t m_forceRepaintAsyncCallbackID;
    bool m_animationsLocked;
};

}

#endif

#endif // LayerTreeCoordinator_h
