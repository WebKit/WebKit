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
#include "SimpleViewportController.h"
#include "ThreadedCompositor.h"
#include "ThreadedDisplayRefreshMonitor.h"
#include <WebCore/PlatformScreen.h>
#include <wtf/Forward.h>
#include <wtf/OptionSet.h>
#include <wtf/RunLoop.h>

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)

#include "LayerTreeHostTextureMapper.h"

#else // USE(GRAPHICS_LAYER_TEXTURE_MAPPER)

namespace WebCore {
class CoordinatedGraphicsLayer;
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
    final : public CompositingCoordinator::Client, public AcceleratedSurface::Client, public ThreadedCompositor::Client, public ThreadedDisplayRefreshMonitor::Client
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

    void scrollNonCompositedContents(const WebCore::IntRect&);
    void forceRepaint();
    void forceRepaintAsync(CompletionHandler<void()>&&);
    void sizeDidChange(const WebCore::IntSize& newSize);
    void targetRefreshRateDidChange(unsigned);

    void pauseRendering();
    void resumeRendering();

    WebCore::GraphicsLayerFactory* graphicsLayerFactory();

    void contentsSizeChanged(const WebCore::IntSize&);
    void didChangeViewportAttributes(WebCore::ViewportAttributes&&);

    void setIsDiscardable(bool);

    void deviceOrPageScaleFactorChanged();

    RefPtr<WebCore::DisplayRefreshMonitor> createDisplayRefreshMonitor(WebCore::PlatformDisplayID);

    WebCore::PlatformDisplayID displayID() const { return m_displayID; }

#if PLATFORM(GTK)
    void adjustTransientZoom(double, WebCore::FloatPoint);
    void commitTransientZoom(double, WebCore::FloatPoint);
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
    void updateScene() override;

    // AcceleratedSurface::Client
    void frameComplete() override;

    // ThreadedCompositor::Client
    uint64_t nativeSurfaceHandleForCompositing() override;
    void didDestroyGLContext() override;
    void resize(const WebCore::IntSize&) override;
    void willRenderFrame() override;
    void didRenderFrame() override;

    // ThreadedDisplayRefreshMonitor::Client
    void requestDisplayRefreshMonitorUpdate() override;
    void handleDisplayRefreshMonitorUpdate(bool hasBeenRescheduled) override;

#if PLATFORM(GTK)
    WebCore::FloatPoint constrainTransientZoomOrigin(double, WebCore::FloatPoint) const;
    WebCore::CoordinatedGraphicsLayer* layerForTransientZoom() const;
    void applyTransientZoomToLayers(double, WebCore::FloatPoint);
#endif

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
    bool m_isWaitingForRenderer { false };
    bool m_scheduledWhileWaitingForRenderer { false };
    float m_lastPageScaleFactor { 1 };
    WebCore::IntPoint m_lastScrollPosition;
    bool m_isDiscardable { false };
    OptionSet<DiscardableSyncActions> m_discardableSyncActions;
    WebCore::GraphicsLayer* m_viewOverlayRootLayer { nullptr };
    std::unique_ptr<AcceleratedSurface> m_surface;
    RefPtr<ThreadedCompositor> m_compositor;
    SimpleViewportController m_viewportController;
    struct {
        CompletionHandler<void()> callback;
        bool needsFreshFlush { false };
    } m_forceRepaintAsync;
    RunLoop::Timer<LayerTreeHost> m_layerFlushTimer;
    CompositingCoordinator m_coordinator;
#endif // USE(COORDINATED_GRAPHICS)
    WebCore::PlatformDisplayID m_displayID;

#if PLATFORM(GTK)
    bool m_transientZoom { false };
    double m_transientZoomScale { 1 };
    WebCore::FloatPoint m_transientZoomOrigin;
#endif
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
inline void LayerTreeHost::scrollNonCompositedContents(const WebCore::IntRect&) { }
inline void LayerTreeHost::forceRepaint() { }
inline void LayerTreeHost::forceRepaintAsync(CompletionHandler<void()>&&) { }
inline void LayerTreeHost::sizeDidChange(const WebCore::IntSize&) { }
inline void LayerTreeHost::targetRefreshRateDidChange(unsigned) { }
inline void LayerTreeHost::pauseRendering() { }
inline void LayerTreeHost::resumeRendering() { }
inline WebCore::GraphicsLayerFactory* LayerTreeHost::graphicsLayerFactory() { return nullptr; }
inline void LayerTreeHost::contentsSizeChanged(const WebCore::IntSize&) { }
inline void LayerTreeHost::didChangeViewportAttributes(WebCore::ViewportAttributes&&) { }
inline void LayerTreeHost::setIsDiscardable(bool) { }
inline void LayerTreeHost::deviceOrPageScaleFactorChanged() { }
inline RefPtr<WebCore::DisplayRefreshMonitor> LayerTreeHost::createDisplayRefreshMonitor(WebCore::PlatformDisplayID) { return nullptr; }
#if PLATFORM(GTK)
inline void LayerTreeHost::adjustTransientZoom(double, WebCore::FloatPoint) { }
inline void LayerTreeHost::commitTransientZoom(double, WebCore::FloatPoint) { }
#endif
#endif

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
