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
#include "OptionalCallbackID.h"
#include <wtf/RunLoop.h>

namespace WebCore {
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
    bool forceRepaintAsync(CallbackID) override;
    void sizeDidChange(const WebCore::IntSize& newSize) override;

    void deviceOrPageScaleFactorChanged() override;

    void setVisibleContentsRect(const WebCore::FloatRect&);
    void renderNextFrame(bool);

    WebCore::GraphicsLayerFactory* graphicsLayerFactory() override;

    void scheduleAnimation() override;

    void setViewOverlayRootLayer(WebCore::GraphicsLayer*) override;

    // CompositingCoordinator::Client
    void didFlushRootLayer(const WebCore::FloatRect& visibleContentRect) override;
    void notifyFlushRequired() override { scheduleLayerFlush(); };
    void commitSceneState(const WebCore::CoordinatedGraphicsState&) override;

    void flushLayersAndForceRepaint();

private:
    void layerFlushTimerFired();

    CompositingCoordinator m_coordinator;
    bool m_isWaitingForRenderer { false };
    bool m_scheduledWhileWaitingForRenderer { false };
    struct {
        OptionalCallbackID callbackID;
        bool needsFreshFlush { false };
    } m_forceRepaintAsync;
    RunLoop::Timer<CoordinatedLayerTreeHost> m_layerFlushTimer;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

#endif // CoordinatedLayerTreeHost_h
