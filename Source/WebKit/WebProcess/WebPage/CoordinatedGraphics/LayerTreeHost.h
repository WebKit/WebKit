/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2019 Igalia S.L.
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

#pragma once

#include "AcceleratedSurface.h"
#include "CallbackID.h"
#include "CompositingCoordinator.h"
#include "LayerTreeContext.h"
#include "OptionalCallbackID.h"
#include "SimpleViewportController.h"
#include "ThreadedCompositor.h"
#include "ThreadedDisplayRefreshMonitor.h"
#include <WebCore/PlatformScreen.h>
#include <wtf/Forward.h>
#include <wtf/OptionSet.h>
#include <wtf/RunLoop.h>

namespace WebCore {
class IntRect;
class IntSize;
class GraphicsLayer;
class GraphicsLayerFactory;
struct CoordinatedGraphicsState;
struct ViewportAttributes;
}

namespace WebKit {

class WebPage;

class LayerTreeHost
#if USE(COORDINATED_GRAPHICS)
    final : public CompositingCoordinator::Client, public AcceleratedSurface::Client
#endif
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit LayerTreeHost(WebPage&);
    ~LayerTreeHost();

    const LayerTreeContext& layerTreeContext() const { return m_layerTreeContext; }
    void setLayerFlushSchedulingEnabled(bool);
    void setShouldNotifyAfterNextScheduledLayerFlush(bool);

    void scheduleLayerFlush();
    void cancelPendingLayerFlush();
    void setRootCompositingLayer(WebCore::GraphicsLayer*);
    void setViewOverlayRootLayer(WebCore::GraphicsLayer*);
    void invalidate();

    void scrollNonCompositedContents(const WebCore::IntRect&);
    void forceRepaint();
    bool forceRepaintAsync(CallbackID);
    void sizeDidChange(const WebCore::IntSize& newSize);

    void pauseRendering();
    void resumeRendering();

    WebCore::GraphicsLayerFactory* graphicsLayerFactory();

    void contentsSizeChanged(const WebCore::IntSize&);
    void didChangeViewportAttributes(WebCore::ViewportAttributes&&);

    void setIsDiscardable(bool);

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(GTK)
    void setNativeSurfaceHandleForCompositing(uint64_t);
#endif

    void deviceOrPageScaleFactorChanged();

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    RefPtr<WebCore::DisplayRefreshMonitor> createDisplayRefreshMonitor(WebCore::PlatformDisplayID);
#endif

private:
#if USE(COORDINATED_GRAPHICS)
    void layerFlushTimerFired();
    void didChangeViewport();
    void renderNextFrame(bool);

    // CompositingCoordinator::Client
    void didFlushRootLayer(const WebCore::FloatRect& visibleContentRect) override;
    void notifyFlushRequired() override { scheduleLayerFlush(); };
    void commitSceneState(const WebCore::CoordinatedGraphicsState&) override;

    // AcceleratedSurface::Client
    void frameComplete() override;

    uint64_t nativeSurfaceHandleForCompositing();
    void didDestroyGLContext();
    void willRenderFrame();
    void didRenderFrame();
    void requestDisplayRefreshMonitorUpdate();
    void handleDisplayRefreshMonitorUpdate(bool);

    class CompositorClient final : public ThreadedCompositor::Client, public ThreadedDisplayRefreshMonitor::Client  {
        WTF_MAKE_NONCOPYABLE(CompositorClient);
    public:
        CompositorClient(LayerTreeHost& layerTreeHost)
            : m_layerTreeHost(layerTreeHost)
        {
        }

    private:
        uint64_t nativeSurfaceHandleForCompositing() override
        {
            return m_layerTreeHost.nativeSurfaceHandleForCompositing();
        }

        void didDestroyGLContext() override
        {
            m_layerTreeHost.didDestroyGLContext();
        }

        void resize(const WebCore::IntSize& size)
        {
            if (m_layerTreeHost.m_surface)
                m_layerTreeHost.m_surface->clientResize(size);
        }

        void willRenderFrame() override
        {
            m_layerTreeHost.willRenderFrame();
        }

        void didRenderFrame() override
        {
            m_layerTreeHost.didRenderFrame();
        }

        void requestDisplayRefreshMonitorUpdate() override
        {
            m_layerTreeHost.requestDisplayRefreshMonitorUpdate();
        }

        void handleDisplayRefreshMonitorUpdate(bool hasBeenRescheduled)
        {
            m_layerTreeHost.handleDisplayRefreshMonitorUpdate(hasBeenRescheduled);
        }

        LayerTreeHost& m_layerTreeHost;
    };

    enum class DiscardableSyncActions {
        UpdateSize = 1 << 1,
        UpdateViewport = 1 << 2,
        UpdateScale = 1 << 3
    };
#endif // USE(COORDINATED_GRAPHICS)

    WebPage& m_webPage;
    LayerTreeContext m_layerTreeContext;
#if USE(COORDINATED_GRAPHICS)
    bool m_layerFlushSchedulingEnabled { true };
    bool m_notifyAfterScheduledLayerFlush { false };
    bool m_isSuspended { false };
    bool m_isValid { true };
    bool m_isWaitingForRenderer { false };
    bool m_scheduledWhileWaitingForRenderer { false };
    float m_lastPageScaleFactor { 1 };
    WebCore::IntPoint m_lastScrollPosition;
    bool m_isDiscardable { false };
    OptionSet<DiscardableSyncActions> m_discardableSyncActions;
    WebCore::GraphicsLayer* m_viewOverlayRootLayer { nullptr };
    CompositingCoordinator m_coordinator;
    CompositorClient m_compositorClient;
    std::unique_ptr<AcceleratedSurface> m_surface;
    RefPtr<ThreadedCompositor> m_compositor;
    SimpleViewportController m_viewportController;
    struct {
        OptionalCallbackID callbackID;
        bool needsFreshFlush { false };
    } m_forceRepaintAsync;
    RunLoop::Timer<LayerTreeHost> m_layerFlushTimer;
#endif // USE(COORDINATED_GRAPHICS)
};

#if !USE(COORDINATED_GRAPHICS)
inline LayerTreeHost::LayerTreeHost(WebPage& webPage) : m_webPage(webPage) { }
inline LayerTreeHost::~LayerTreeHost() { }
inline void LayerTreeHost::setLayerFlushSchedulingEnabled(bool) { }
inline void LayerTreeHost::setShouldNotifyAfterNextScheduledLayerFlush(bool) { }
inline void LayerTreeHost::scheduleLayerFlush() { }
inline void LayerTreeHost::cancelPendingLayerFlush() { }
inline void LayerTreeHost::setRootCompositingLayer(WebCore::GraphicsLayer*) { }
inline void LayerTreeHost::setViewOverlayRootLayer(WebCore::GraphicsLayer*) { }
inline void LayerTreeHost::invalidate() { }
inline void LayerTreeHost::scrollNonCompositedContents(const WebCore::IntRect&) { }
inline void LayerTreeHost::forceRepaint() { }
inline bool LayerTreeHost::forceRepaintAsync(CallbackID) { return false; }
inline void LayerTreeHost::sizeDidChange(const WebCore::IntSize&) { }
inline void LayerTreeHost::pauseRendering() { }
inline void LayerTreeHost::resumeRendering() { }
inline WebCore::GraphicsLayerFactory* LayerTreeHost::graphicsLayerFactory() { return nullptr; }
inline void LayerTreeHost::contentsSizeChanged(const WebCore::IntSize&) { }
inline void LayerTreeHost::didChangeViewportAttributes(WebCore::ViewportAttributes&&) { }
inline void LayerTreeHost::setIsDiscardable(bool) { }
inline void LayerTreeHost::deviceOrPageScaleFactorChanged() { }
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
inline RefPtr<WebCore::DisplayRefreshMonitor> LayerTreeHost::createDisplayRefreshMonitor(WebCore::PlatformDisplayID) { return nullptr; }
#endif
#endif

} // namespace WebKit
