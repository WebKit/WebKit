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
#include <WebCore/FloatPoint.h>
#include <WebCore/PlatformScreen.h>
#include <wtf/Forward.h>
#include <wtf/OptionSet.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>

#if !HAVE(DISPLAY_LINK)
#include "ThreadedDisplayRefreshMonitor.h"
#endif

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)

#include "LayerTreeHostTextureMapper.h"

#else // USE(GRAPHICS_LAYER_TEXTURE_MAPPER)

namespace WebCore {
class CoordinatedGraphicsLayer;
class Damage;
class IntRect;
class IntSize;
class GraphicsLayer;
class GraphicsLayerFactory;
struct ViewportAttributes;
}

namespace WebKit {

class WebPage;

class LayerTreeHost
#if USE(COORDINATED_GRAPHICS)
    final : public CompositingCoordinator::Client, public AcceleratedSurface::Client, public ThreadedCompositor::Client
#if !HAVE(DISPLAY_LINK)
    , public ThreadedDisplayRefreshMonitor::Client
#endif
#endif
{
    WTF_MAKE_TZONE_ALLOCATED(LayerTreeHost);
public:
#if HAVE(DISPLAY_LINK)
    explicit LayerTreeHost(WebPage&);
#else
    LayerTreeHost(WebPage&, WebCore::PlatformDisplayID);
#endif
    ~LayerTreeHost();

    const LayerTreeContext& layerTreeContext() const { return m_layerTreeContext; }
    void setLayerFlushSchedulingEnabled(bool);

    void scheduleLayerFlush();
    void cancelPendingLayerFlush();
    void setRootCompositingLayer(WebCore::GraphicsLayer*);
    void setViewOverlayRootLayer(WebCore::GraphicsLayer*);

    void scrollNonCompositedContents(const WebCore::IntRect&);
    void forceRepaint();
    void forceRepaintAsync(CompletionHandler<void()>&&);
    void sizeDidChange(const WebCore::IntSize& newSize);

    void pauseRendering();
    void resumeRendering();

    WebCore::GraphicsLayerFactory* graphicsLayerFactory();

    void contentsSizeChanged(const WebCore::IntSize&);
    void didChangeViewportAttributes(WebCore::ViewportAttributes&&);

    void deviceOrPageScaleFactorChanged();
    void backgroundColorDidChange();

#if !HAVE(DISPLAY_LINK)
    RefPtr<WebCore::DisplayRefreshMonitor> createDisplayRefreshMonitor(WebCore::PlatformDisplayID);
    WebCore::PlatformDisplayID displayID() const { return m_displayID; }
#endif

#if PLATFORM(GTK)
    void adjustTransientZoom(double, WebCore::FloatPoint);
    void commitTransientZoom(double, WebCore::FloatPoint);
#endif

#if PLATFORM(WPE) && USE(GBM) && ENABLE(WPE_PLATFORM)
    void preferredBufferFormatsDidChange();
#endif
private:
#if USE(COORDINATED_GRAPHICS)
    void layerFlushTimerFired();
    void didChangeViewport();
#if HAVE(DISPLAY_LINK)
    void didRenderFrameTimerFired();
#endif
    void renderNextFrame(bool);

    // CompositingCoordinator::Client
    void didFlushRootLayer(const WebCore::FloatRect& visibleContentRect) override;
    void commitSceneState(const RefPtr<Nicosia::Scene>&) override;
    void updateScene() override;

    // AcceleratedSurface::Client
    void frameComplete() override;

    // ThreadedCompositor::Client
    uint64_t nativeSurfaceHandleForCompositing() override;
    void didCreateGLContext() override;
    void willDestroyGLContext() override;
    void didDestroyGLContext() override;
    void resize(const WebCore::IntSize&) override;
    void willRenderFrame() override;
    void clearIfNeeded() override;
    void didRenderFrame(uint32_t, const WebCore::Damage&) override;
    void displayDidRefresh(WebCore::PlatformDisplayID) override;

#if !HAVE(DISPLAY_LINK)
    // ThreadedDisplayRefreshMonitor::Client
    void requestDisplayRefreshMonitorUpdate() override;
    void handleDisplayRefreshMonitorUpdate(bool hasBeenRescheduled) override;
#endif

#if PLATFORM(GTK)
    WebCore::FloatPoint constrainTransientZoomOrigin(double, WebCore::FloatPoint) const;
    WebCore::CoordinatedGraphicsLayer* layerForTransientZoom() const;
    void applyTransientZoomToLayers(double, WebCore::FloatPoint);
#endif

#endif // USE(COORDINATED_GRAPHICS)

    WebPage& m_webPage;
    LayerTreeContext m_layerTreeContext;
#if USE(COORDINATED_GRAPHICS)
    bool m_layerFlushSchedulingEnabled { true };
    bool m_isSuspended { false };
    bool m_isWaitingForRenderer { false };
    bool m_scheduledWhileWaitingForRenderer { false };
    float m_lastPageScaleFactor { 1 };
    WebCore::IntPoint m_lastScrollPosition;
    bool m_scrolledSinceLastFrame { false };
    WebCore::GraphicsLayer* m_viewOverlayRootLayer { nullptr };
    std::unique_ptr<AcceleratedSurface> m_surface;
    RefPtr<ThreadedCompositor> m_compositor;
    SimpleViewportController m_viewportController;
    struct {
        CompletionHandler<void()> callback;
        bool needsFreshFlush { false };
    } m_forceRepaintAsync;
    RunLoop::Timer m_layerFlushTimer;
#if HAVE(DISPLAY_LINK)
    RunLoop::Timer m_didRenderFrameTimer;
#endif
    CompositingCoordinator m_coordinator;
#endif // USE(COORDINATED_GRAPHICS)
#if !HAVE(DISPLAY_LINK)
    WebCore::PlatformDisplayID m_displayID;
#endif

#if PLATFORM(GTK)
    bool m_transientZoom { false };
    double m_transientZoomScale { 1 };
    WebCore::FloatPoint m_transientZoomOrigin;
#endif

    uint32_t m_compositionRequestID { 0 };
    uint32_t m_compositionResponseID { 0 };
};

#if !USE(COORDINATED_GRAPHICS)
#if HAVE(DISPLAY_LINK)
inline LayerTreeHost::LayerTreeHost(WebPage& webPage) : m_webPage(webPage) { }
#else
inline LayerTreeHost::LayerTreeHost(WebPage& webPage, WebCore::PlatformDisplayID displayID) : m_webPage(webPage), m_displayID(displayID) { }
#endif
inline LayerTreeHost::~LayerTreeHost() { }
inline void LayerTreeHost::setLayerFlushSchedulingEnabled(bool) { }
inline void LayerTreeHost::scheduleLayerFlush() { }
inline void LayerTreeHost::cancelPendingLayerFlush() { }
inline void LayerTreeHost::setRootCompositingLayer(WebCore::GraphicsLayer*) { }
inline void LayerTreeHost::setViewOverlayRootLayer(WebCore::GraphicsLayer*) { }
inline void LayerTreeHost::scrollNonCompositedContents(const WebCore::IntRect&) { }
inline void LayerTreeHost::forceRepaint() { }
inline void LayerTreeHost::forceRepaintAsync(CompletionHandler<void()>&&) { }
inline void LayerTreeHost::sizeDidChange(const WebCore::IntSize&) { }
inline void LayerTreeHost::pauseRendering() { }
inline void LayerTreeHost::resumeRendering() { }
inline WebCore::GraphicsLayerFactory* LayerTreeHost::graphicsLayerFactory() { return nullptr; }
inline void LayerTreeHost::contentsSizeChanged(const WebCore::IntSize&) { }
inline void LayerTreeHost::didChangeViewportAttributes(WebCore::ViewportAttributes&&) { }
inline void LayerTreeHost::deviceOrPageScaleFactorChanged() { }
#if PLATFORM(GTK)
inline void LayerTreeHost::adjustTransientZoom(double, WebCore::FloatPoint) { }
inline void LayerTreeHost::commitTransientZoom(double, WebCore::FloatPoint) { }
#else
inline RefPtr<WebCore::DisplayRefreshMonitor> LayerTreeHost::createDisplayRefreshMonitor(WebCore::PlatformDisplayID) { return nullptr; }
#endif
#endif

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
