/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Company 100, Inc.
 * Copyright (C) 2014 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ThreadedCoordinatedLayerTreeHost_h
#define ThreadedCoordinatedLayerTreeHost_h

#if USE(COORDINATED_GRAPHICS_THREADED)

#include "LayerTreeContext.h"
#include "LayerTreeHost.h"
#include "ThreadedCompositor.h"
#include <WebCore/CompositingCoordinator.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/FloatRect.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <WebCore/PageOverlay.h>
#include <WebCore/Timer.h>
#include <wtf/RunLoop.h>
#include <wtf/Threading.h>

namespace WebCore {
class CoordinatedSurface;
class GraphicsContext;
class GraphicsLayer;
class GraphicsLayerFactory;
class GraphicsLayerFactory;
struct CoordinatedGraphicsLayerState;
struct CoordinatedGraphicsState;
}

namespace WebKit {

class WebPage;

class ThreadedCoordinatedLayerTreeHost : public LayerTreeHost, public WebCore::CompositingCoordinator::Client, public ThreadedCompositor::Client {
    WTF_MAKE_NONCOPYABLE(ThreadedCoordinatedLayerTreeHost); WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ThreadedCoordinatedLayerTreeHost> create(WebPage*);
    virtual ~ThreadedCoordinatedLayerTreeHost();

    const LayerTreeContext& layerTreeContext() override { return m_layerTreeContext; };

    void scheduleLayerFlush() override;
    void setLayerFlushSchedulingEnabled(bool) override;
    void setShouldNotifyAfterNextScheduledLayerFlush(bool) override;
    void setRootCompositingLayer(WebCore::GraphicsLayer*) override;
    void invalidate() override;

    void setNonCompositedContentsNeedDisplay() override { };
    void setNonCompositedContentsNeedDisplayInRect(const WebCore::IntRect&) override { };
    void scrollNonCompositedContents(const WebCore::IntRect& scrollRect) override;
    void forceRepaint() override;
    bool forceRepaintAsync(uint64_t /*callbackID*/) override;
    void sizeDidChange(const WebCore::IntSize& newSize) override;
    void deviceOrPageScaleFactorChanged() override;

    void pauseRendering() override;
    void resumeRendering() override;

    WebCore::GraphicsLayerFactory* graphicsLayerFactory() override;
    void pageBackgroundTransparencyChanged() override { };

    void viewportSizeChanged(const WebCore::IntSize&) override;
    void didChangeViewportProperties(const WebCore::ViewportAttributes&) override;

#if PLATFORM(GTK)
    void setNativeSurfaceHandleForCompositing(uint64_t) override;
#endif

#if ENABLE(REQUEST_ANIMATION_FRAME)
    void scheduleAnimation() override;
#endif

    void setViewOverlayRootLayer(WebCore::GraphicsLayer*);
    static PassRefPtr<WebCore::CoordinatedSurface> createCoordinatedSurface(const WebCore::IntSize&, WebCore::CoordinatedSurface::Flags);

protected:
    explicit ThreadedCoordinatedLayerTreeHost(WebPage*);

private:

    void compositorDidFlushLayers();
    void didScaleFactorChanged(float scale, const WebCore::IntPoint& origin);

    void updateRootLayers();

    void cancelPendingLayerFlush();
    void performScheduledLayerFlush();

    WebCore::GraphicsLayer* rootLayer() { return m_coordinator->rootLayer(); }

    // ThreadedCompositor::Client
    void setVisibleContentsRect(const WebCore::FloatRect&, const WebCore::FloatPoint&, float) override;
    void purgeBackingStores() override;
    void renderNextFrame() override;
    void commitScrollOffset(uint32_t layerID, const WebCore::IntSize& offset) override;

    // CompositingCoordinator::Client
    void didFlushRootLayer(const WebCore::FloatRect&) override { }
    void notifyFlushRequired() override;
    void commitSceneState(const WebCore::CoordinatedGraphicsState&) override;
    void paintLayerContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, const WebCore::IntRect& clipRect) override;

    LayerTreeContext m_layerTreeContext;
    uint64_t m_forceRepaintAsyncCallbackID;

    WebCore::IntPoint m_prevScrollPosition;

    WebCore::GraphicsLayer* m_contentLayer;
    WebCore::GraphicsLayer* m_viewOverlayRootLayer;

    std::unique_ptr<WebCore::CompositingCoordinator> m_coordinator;
    RefPtr<ThreadedCompositor> m_compositor;

    bool m_notifyAfterScheduledLayerFlush;
    bool m_isSuspended;
    bool m_isWaitingForRenderer;

    float m_lastScaleFactor;
    WebCore::IntPoint m_lastScrollPosition;

    RunLoop::Timer<ThreadedCoordinatedLayerTreeHost> m_layerFlushTimer;
    bool m_layerFlushSchedulingEnabled;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS_THREADED)

#endif // ThreadedCoordinatedLayerTreeHost_h
