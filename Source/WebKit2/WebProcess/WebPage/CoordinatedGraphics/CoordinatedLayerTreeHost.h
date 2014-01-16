/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef CoordinatedLayerTreeHost_h
#define CoordinatedLayerTreeHost_h

#if USE(COORDINATED_GRAPHICS)

#include "LayerTreeContext.h"
#include "LayerTreeHost.h"
#include <WebCore/CompositingCoordinator.h>
#include <WebCore/GraphicsLayerFactory.h>
#include <wtf/HashSet.h>

#if ENABLE(CSS_SHADERS)
#include "WebCustomFilterProgramProxy.h"
#endif

namespace WebCore {
class CoordinatedSurface;
}

namespace WebKit {

class WebPage;

class CoordinatedLayerTreeHost : public LayerTreeHost, public WebCore::CompositingCoordinator::Client
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

    virtual void setNonCompositedContentsNeedDisplay() override { }
    virtual void setNonCompositedContentsNeedDisplayInRect(const WebCore::IntRect&) override { }
    virtual void scrollNonCompositedContents(const WebCore::IntRect&) override { }
    virtual void forceRepaint();
    virtual bool forceRepaintAsync(uint64_t callbackID);
    virtual void sizeDidChange(const WebCore::IntSize& newSize);

    virtual void didInstallPageOverlay(PageOverlay*);
    virtual void didUninstallPageOverlay(PageOverlay*);
    virtual void setPageOverlayNeedsDisplay(PageOverlay*, const WebCore::IntRect&);
    virtual void setPageOverlayOpacity(PageOverlay*, float);

    virtual void pauseRendering() { m_isSuspended = true; }
    virtual void resumeRendering() { m_isSuspended = false; scheduleLayerFlush(); }
    virtual void deviceOrPageScaleFactorChanged() override;
    virtual void pageBackgroundTransparencyChanged() override;

    virtual void didReceiveCoordinatedLayerTreeHostMessage(IPC::Connection*, IPC::MessageDecoder&);
    virtual WebCore::GraphicsLayerFactory* graphicsLayerFactory() override;
    WebCore::CoordinatedGraphicsLayer* mainContentsLayer();

#if ENABLE(REQUEST_ANIMATION_FRAME)
    virtual void scheduleAnimation() override;
#endif
    virtual void setBackgroundColor(const WebCore::Color&) override;

    static PassRefPtr<WebCore::CoordinatedSurface> createCoordinatedSurface(const WebCore::IntSize&, WebCore::CoordinatedSurface::Flags);

protected:
    explicit CoordinatedLayerTreeHost(WebPage*);

private:
    // CoordinatedLayerTreeHost
    void createPageOverlayLayer();
    void destroyPageOverlayLayer();
    void cancelPendingLayerFlush();
    void performScheduledLayerFlush();
    void setVisibleContentsRect(const WebCore::FloatRect&, const WebCore::FloatPoint&);
    void renderNextFrame();
    void purgeBackingStores();
    void commitScrollOffset(uint32_t layerID, const WebCore::IntSize& offset);

    void layerFlushTimerFired(WebCore::Timer<CoordinatedLayerTreeHost>*);

    // CompositingCoordinator::Client
    virtual void didFlushRootLayer() override;
    virtual void willSyncLayerState(WebCore::CoordinatedGraphicsLayerState&) override;
    virtual void notifyFlushRequired() override { scheduleLayerFlush(); };
    virtual void commitSceneState(const WebCore::CoordinatedGraphicsState&) override;
    virtual void paintLayerContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, const WebCore::IntRect& clipRect) override;

#if ENABLE(CSS_SHADERS)
    void prepareCustomFilterProxiesForAnimations(WebCore::GraphicsLayerAnimations&);

    // WebCustomFilterProgramProxyClient
    void removeCustomFilterProgramProxy(WebCustomFilterProgramProxy*);

    void checkCustomFilterProgramProxies(const WebCore::FilterOperations&);
    void disconnectCustomFilterPrograms();
#endif

    std::unique_ptr<WebCore::CompositingCoordinator> m_coordinator;

    // The page overlay layer. Will be null if there's no page overlay.
    std::unique_ptr<WebCore::GraphicsLayer> m_pageOverlayLayer;
    RefPtr<PageOverlay> m_pageOverlay;

#if ENABLE(CSS_SHADERS)
    HashSet<WebCustomFilterProgramProxy*> m_customFilterPrograms;
#endif

    bool m_notifyAfterScheduledLayerFlush;
    bool m_isValid;
    bool m_isSuspended;
    bool m_isWaitingForRenderer;

    LayerTreeContext m_layerTreeContext;

    WebCore::Timer<CoordinatedLayerTreeHost> m_layerFlushTimer;
    bool m_layerFlushSchedulingEnabled;
    uint64_t m_forceRepaintAsyncCallbackID;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

#endif // CoordinatedLayerTreeHost_h
