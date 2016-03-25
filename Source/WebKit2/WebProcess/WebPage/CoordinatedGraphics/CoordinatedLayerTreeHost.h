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

namespace WebCore {
class CoordinatedSurface;
}

namespace WebKit {

class WebPage;

class CoordinatedLayerTreeHost : public LayerTreeHost, public WebCore::CompositingCoordinator::Client
{
public:
    static Ref<CoordinatedLayerTreeHost> create(WebPage*);
    virtual ~CoordinatedLayerTreeHost();

    const LayerTreeContext& layerTreeContext() override { return m_layerTreeContext; }
    void setLayerFlushSchedulingEnabled(bool) override;
    void scheduleLayerFlush() override;
    void setShouldNotifyAfterNextScheduledLayerFlush(bool) override;
    void setRootCompositingLayer(WebCore::GraphicsLayer*) override;
    void invalidate() override;

    void setNonCompositedContentsNeedDisplay() override { }
    void setNonCompositedContentsNeedDisplayInRect(const WebCore::IntRect&) override { }
    void scrollNonCompositedContents(const WebCore::IntRect&) override { }
    void forceRepaint() override;
    bool forceRepaintAsync(uint64_t callbackID) override;
    void sizeDidChange(const WebCore::IntSize& newSize) override;

    void pauseRendering() override { m_isSuspended = true; }
    void resumeRendering() override { m_isSuspended = false; scheduleLayerFlush(); }
    void deviceOrPageScaleFactorChanged() override;
    void pageBackgroundTransparencyChanged() override;

    void didReceiveCoordinatedLayerTreeHostMessage(IPC::Connection&, IPC::MessageDecoder&) override;
    WebCore::GraphicsLayerFactory* graphicsLayerFactory() override;
    WebCore::CoordinatedGraphicsLayer* mainContentsLayer();

#if ENABLE(REQUEST_ANIMATION_FRAME)
    void scheduleAnimation() override;
#endif

    void setViewOverlayRootLayer(WebCore::GraphicsLayer*) override;

    static RefPtr<WebCore::CoordinatedSurface> createCoordinatedSurface(const WebCore::IntSize&, WebCore::CoordinatedSurface::Flags);

protected:
    explicit CoordinatedLayerTreeHost(WebPage*);

private:
    // CoordinatedLayerTreeHost
    void cancelPendingLayerFlush();
    void performScheduledLayerFlush();
    void setVisibleContentsRect(const WebCore::FloatRect&, const WebCore::FloatPoint&);
    void renderNextFrame();
    void purgeBackingStores();
    void commitScrollOffset(uint32_t layerID, const WebCore::IntSize& offset);

    void layerFlushTimerFired();

    void updateRootLayers();

    // CompositingCoordinator::Client
    void didFlushRootLayer(const WebCore::FloatRect& visibleContentRect) override;
    void notifyFlushRequired() override { scheduleLayerFlush(); };
    void commitSceneState(const WebCore::CoordinatedGraphicsState&) override;
    void paintLayerContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, const WebCore::IntRect& clipRect) override;

    std::unique_ptr<WebCore::CompositingCoordinator> m_coordinator;

    bool m_notifyAfterScheduledLayerFlush;
    bool m_isValid;
    bool m_isSuspended;
    bool m_isWaitingForRenderer;

    LayerTreeContext m_layerTreeContext;

    WebCore::Timer m_layerFlushTimer;
    bool m_layerFlushSchedulingEnabled;
    uint64_t m_forceRepaintAsyncCallbackID;

    WebCore::GraphicsLayer* m_contentLayer;
    WebCore::GraphicsLayer* m_viewOverlayRootLayer;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

#endif // CoordinatedLayerTreeHost_h
