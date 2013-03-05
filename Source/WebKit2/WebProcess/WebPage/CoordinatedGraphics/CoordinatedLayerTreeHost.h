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

#ifndef CoordinatedLayerTreeHost_h
#define CoordinatedLayerTreeHost_h

#if USE(COORDINATED_GRAPHICS)

#include "LayerTreeContext.h"
#include "LayerTreeHost.h"
#include "Timer.h"
#include <WebCore/CoordinatedGraphicsLayer.h>
#include <WebCore/CoordinatedGraphicsState.h>
#include <WebCore/CoordinatedImageBacking.h>
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/GraphicsLayerFactory.h>
#include <WebCore/UpdateAtlas.h>
#include <wtf/OwnPtr.h>

#if ENABLE(CSS_SHADERS)
#include "WebCustomFilterProgramProxy.h"
#endif

namespace WebCore {
class CoordinatedSurface;
}

namespace WebKit {

class WebPage;

class CoordinatedLayerTreeHost : public LayerTreeHost, WebCore::GraphicsLayerClient
    , public WebCore::CoordinatedGraphicsLayerClient
    , public WebCore::CoordinatedImageBacking::Coordinator
    , public WebCore::UpdateAtlasClient
    , public WebCore::GraphicsLayerFactory
#if ENABLE(CSS_SHADERS)
    , WebCustomFilterProgramProxyClient
#endif
{
public:
    static PassRefPtr<CoordinatedLayerTreeHost> create(WebPage*);
    virtual ~CoordinatedLayerTreeHost();

    virtual const LayerTreeContext& layerTreeContext() { return m_layerTreeContext; }
    virtual void setLayerFlushSchedulingEnabled(bool);
    virtual void scheduleLayerFlush();
    virtual void setShouldNotifyAfterNextScheduledLayerFlush(bool);
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*);
    virtual void invalidate();

    virtual void setNonCompositedContentsNeedDisplay() OVERRIDE;
    virtual void setNonCompositedContentsNeedDisplayInRect(const WebCore::IntRect&) OVERRIDE;
    virtual void scrollNonCompositedContents(const WebCore::IntRect&) OVERRIDE;
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
    virtual void deviceOrPageScaleFactorChanged() OVERRIDE;

    virtual void renderNextFrame();
    virtual void purgeBackingStores();
    virtual void setVisibleContentsRect(const WebCore::FloatRect&, const WebCore::FloatPoint&);
    virtual void didReceiveCoordinatedLayerTreeHostMessage(CoreIPC::Connection*, CoreIPC::MessageDecoder&);
    virtual WebCore::GraphicsLayerFactory* graphicsLayerFactory() OVERRIDE;

#if ENABLE(REQUEST_ANIMATION_FRAME)
    virtual void scheduleAnimation() OVERRIDE;
#endif
    virtual void setBackgroundColor(const WebCore::Color&) OVERRIDE;

    static PassRefPtr<WebCore::CoordinatedSurface> createCoordinatedSurface(const WebCore::IntSize&, WebCore::CoordinatedSurface::Flags);

protected:
    explicit CoordinatedLayerTreeHost(WebPage*);

private:
    // GraphicsLayerClient
    virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double time);
    virtual void notifyFlushRequired(const WebCore::GraphicsLayer*);
    virtual void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& clipRect);
    virtual float deviceScaleFactor() const OVERRIDE;
    virtual float pageScaleFactor() const OVERRIDE;

    // CoordinatedImageBacking::Coordinator
    virtual void createImageBacking(WebCore::CoordinatedImageBackingID) OVERRIDE;
    virtual bool updateImageBacking(WebCore::CoordinatedImageBackingID, PassRefPtr<WebCore::CoordinatedSurface>) OVERRIDE;
    virtual void clearImageBackingContents(WebCore::CoordinatedImageBackingID) OVERRIDE;
    virtual void removeImageBacking(WebCore::CoordinatedImageBackingID) OVERRIDE;

    void flushPendingImageBackingChanges();

    // CoordinatedGraphicsLayerClient
    virtual bool isFlushingLayerChanges() const OVERRIDE { return m_isFlushingLayerChanges; }
    virtual WebCore::FloatRect visibleContentsRect() const;
    virtual PassRefPtr<WebCore::CoordinatedImageBacking> createImageBackingIfNeeded(WebCore::Image*) OVERRIDE;
    virtual void detachLayer(WebCore::CoordinatedGraphicsLayer*);
    virtual PassOwnPtr<WebCore::GraphicsContext> beginContentUpdate(const WebCore::IntSize&, WebCore::CoordinatedSurface::Flags, uint32_t& atlasID, WebCore::IntPoint&);
    virtual void syncLayerState(WebCore::CoordinatedLayerID, WebCore::CoordinatedGraphicsLayerState&);

    // UpdateAtlasClient
    virtual bool createUpdateAtlas(uint32_t atlasID, PassRefPtr<WebCore::CoordinatedSurface>) OVERRIDE;
    virtual void removeUpdateAtlas(uint32_t atlasID);

    // GraphicsLayerFactory
    virtual PassOwnPtr<WebCore::GraphicsLayer> createGraphicsLayer(WebCore::GraphicsLayerClient*) OVERRIDE;

    // CoordinatedLayerTreeHost
    void initializeRootCompositingLayerIfNeeded();
    void createPageOverlayLayer();
    void destroyPageOverlayLayer();
    bool flushPendingLayerChanges();
    void createCompositingLayers();
    void deleteCompositingLayers();
    void cancelPendingLayerFlush();
    void performScheduledLayerFlush();
    void didPerformScheduledLayerFlush();
    void syncDisplayState();
    void lockAnimations();
    void unlockAnimations();

    void layerFlushTimerFired(WebCore::Timer<CoordinatedLayerTreeHost>*);

    void scheduleReleaseInactiveAtlases();
#if ENABLE(REQUEST_ANIMATION_FRAME)
    void animationFrameReady();
#endif

    void releaseInactiveAtlasesTimerFired(WebCore::Timer<CoordinatedLayerTreeHost>*);

#if ENABLE(CSS_SHADERS)
    void prepareCustomFilterProxiesIfNeeded(WebCore::CoordinatedGraphicsLayerState&);

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

    WebCore::CoordinatedGraphicsState m_state;

    HashSet<WebCore::CoordinatedGraphicsLayer*> m_registeredLayers;
    Vector<WebCore::CoordinatedLayerID> m_layersToCreate;
    Vector<WebCore::CoordinatedLayerID> m_layersToDelete;
    typedef HashMap<WebCore::CoordinatedImageBackingID, RefPtr<WebCore::CoordinatedImageBacking> > ImageBackingMap;
    ImageBackingMap m_imageBackings;
    Vector<OwnPtr<WebCore::UpdateAtlas> > m_updateAtlases;

#if ENABLE(CSS_SHADERS)
    HashSet<WebCustomFilterProgramProxy*> m_customFilterPrograms;
#endif

    bool m_notifyAfterScheduledLayerFlush;
    bool m_isValid;
    // We don't send the messages related to releasing resources to UI Process during purging, because UI Process already had removed all resources.
    bool m_isPurging;
    bool m_isFlushingLayerChanges;

    bool m_waitingForUIProcess;
    bool m_isSuspended;
    WebCore::FloatRect m_visibleContentsRect;

    LayerTreeContext m_layerTreeContext;
    bool m_shouldSyncFrame;
    bool m_didInitializeRootCompositingLayer;
    WebCore::Timer<CoordinatedLayerTreeHost> m_layerFlushTimer;
    WebCore::Timer<CoordinatedLayerTreeHost> m_releaseInactiveAtlasesTimer;
    bool m_layerFlushSchedulingEnabled;
    uint64_t m_forceRepaintAsyncCallbackID;
    bool m_animationsLocked;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

#endif // CoordinatedLayerTreeHost_h
