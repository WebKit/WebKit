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

#include "CompositingCoordinator.h"
#include "LayerTreeHost.h"
#include <wtf/RunLoop.h>

namespace WebCore {
class CoordinatedSurface;
class GraphicsLayerFactory;
}

namespace WebKit {

class WebPage;

class CoordinatedLayerTreeHost : public LayerTreeHost, public CompositingCoordinator::Client
{
public:
    static Ref<CoordinatedLayerTreeHost> create(WebPage&);
    virtual ~CoordinatedLayerTreeHost();

protected:
    explicit CoordinatedLayerTreeHost(WebPage&);

    void scheduleLayerFlush() override;
    void cancelPendingLayerFlush() override;
    void setRootCompositingLayer(WebCore::GraphicsLayer*) override;
    void invalidate() override;

    void forceRepaint() override;
    bool forceRepaintAsync(uint64_t callbackID) override;
    void sizeDidChange(const WebCore::IntSize& newSize) override;

    void deviceOrPageScaleFactorChanged() override;
    void pageBackgroundTransparencyChanged() override;

    void setVisibleContentsRect(const WebCore::FloatRect&, const WebCore::FloatPoint&);
    void renderNextFrame();
    void commitScrollOffset(uint32_t layerID, const WebCore::IntSize& offset);

    WebCore::GraphicsLayerFactory* graphicsLayerFactory() override;

#if ENABLE(REQUEST_ANIMATION_FRAME)
    void scheduleAnimation() override;
#endif

    void setViewOverlayRootLayer(WebCore::GraphicsLayer*) override;

    // CompositingCoordinator::Client
    void didFlushRootLayer(const WebCore::FloatRect& visibleContentRect) override;
    void notifyFlushRequired() override { scheduleLayerFlush(); };
    void commitSceneState(const WebCore::CoordinatedGraphicsState&) override;
    void paintLayerContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, const WebCore::IntRect& clipRect) override;

private:
    void layerFlushTimerFired();

#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)
    void didReceiveCoordinatedLayerTreeHostMessage(IPC::Connection&, IPC::Decoder&) override;
#endif

    static RefPtr<WebCore::CoordinatedSurface> createCoordinatedSurface(const WebCore::IntSize&, WebCore::CoordinatedSurface::Flags);

    CompositingCoordinator m_coordinator;
    bool m_isWaitingForRenderer { true };
    uint64_t m_forceRepaintAsyncCallbackID { 0 };
    RunLoop::Timer<CoordinatedLayerTreeHost> m_layerFlushTimer;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

#endif // CoordinatedLayerTreeHost_h
