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
#include "LayerTreeContext.h"
#include "LayerTreeHost.h"
#include "Timer.h"
#include "UpdateAtlas.h"
#include <WebCore/GraphicsLayerClient.h>
#include <wtf/OwnPtr.h>

namespace WebKit {

class UpdateInfo;
class WebPage;

class LayerTreeCoordinator : public LayerTreeHost, WebCore::GraphicsLayerClient
                           , public CoordinatedGraphicsLayerClient {
public:
    static PassRefPtr<LayerTreeCoordinator> create(WebPage*);
    virtual ~LayerTreeCoordinator();

    static bool supportsAcceleratedCompositing();

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
    virtual int64_t adoptImageBackingStore(WebCore::Image*);
    virtual void releaseImageBackingStore(int64_t);

    virtual void createTile(WebLayerID, int tileID, const SurfaceUpdateInfo&, const WebCore::IntRect&);
    virtual void updateTile(WebLayerID, int tileID, const SurfaceUpdateInfo&, const WebCore::IntRect&);
    virtual void removeTile(WebLayerID, int tileID);
    virtual WebCore::IntRect visibleContentsRect() const;
    virtual void renderNextFrame();
    virtual void purgeBackingStores();
    virtual bool layerTreeTileUpdatesAllowed() const;
    virtual void setVisibleContentsRect(const WebCore::IntRect&, float scale, const WebCore::FloatPoint&);
    virtual void didReceiveLayerTreeCoordinatorMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);

    virtual void syncLayerState(WebLayerID, const WebLayerInfo&);
    virtual void syncLayerChildren(WebLayerID, const Vector<WebLayerID>&);
    virtual void setLayerAnimatedOpacity(WebLayerID, float);
    virtual void setLayerAnimatedTransform(WebLayerID, const WebCore::TransformationMatrix&);
#if ENABLE(CSS_FILTERS)
    virtual void syncLayerFilters(WebLayerID, const WebCore::FilterOperations&);
#endif
    virtual void syncCanvas(WebLayerID, const WebCore::IntSize& canvasSize, uint32_t graphicsSurfaceToken) OVERRIDE;
    virtual void attachLayer(WebCore::CoordinatedGraphicsLayer*);
    virtual void detachLayer(WebCore::CoordinatedGraphicsLayer*);
    virtual void syncFixedLayers();

    virtual PassOwnPtr<WebCore::GraphicsContext> beginContentUpdate(const WebCore::IntSize&, ShareableBitmap::Flags, ShareableSurface::Handle&, WebCore::IntPoint&);
    virtual void scheduleAnimation() OVERRIDE;

protected:
    explicit LayerTreeCoordinator(WebPage*);

private:
    // GraphicsLayerClient
    virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double time);
    virtual void notifySyncRequired(const WebCore::GraphicsLayer*);
    virtual void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& clipRect);
    virtual bool showDebugBorders(const WebCore::GraphicsLayer*) const;
    virtual bool showRepaintCounter(const WebCore::GraphicsLayer*) const;

    // LayerTreeCoordinator
    void createPageOverlayLayer();
    void destroyPageOverlayLayer();
    bool flushPendingLayerChanges();
    void cancelPendingLayerFlush();
    void performScheduledLayerFlush();
    void didPerformScheduledLayerFlush();
    void syncDisplayState();

    OwnPtr<WebCore::GraphicsLayer> m_rootLayer;

    // The layer which contains all non-composited content.
    OwnPtr<WebCore::GraphicsLayer> m_nonCompositedContentLayer;

    // The page overlay layer. Will be null if there's no page overlay.
    OwnPtr<WebCore::GraphicsLayer> m_pageOverlayLayer;

    HashSet<WebCore::CoordinatedGraphicsLayer*> m_registeredLayers;
    HashMap<int64_t, int> m_directlyCompositedImageRefCounts;
    Vector<OwnPtr<UpdateAtlas> > m_updateAtlases;

    bool m_notifyAfterScheduledLayerFlush;
    bool m_isValid;

    bool m_waitingForUIProcess;
    bool m_isSuspended;
    WebCore::IntRect m_visibleContentsRect;
    float m_contentsScale;
    bool m_shouldSendScrollPositionUpdate;

    LayerTreeContext m_layerTreeContext;
    bool m_shouldSyncFrame;
    bool m_shouldSyncRootLayer;
    void layerFlushTimerFired(WebCore::Timer<LayerTreeCoordinator>*);
    WebCore::Timer<LayerTreeCoordinator> m_layerFlushTimer;
    bool m_layerFlushSchedulingEnabled;
    uint64_t m_forceRepaintAsyncCallbackID;
};

}

#endif

#endif // LayerTreeCoordinator_h
