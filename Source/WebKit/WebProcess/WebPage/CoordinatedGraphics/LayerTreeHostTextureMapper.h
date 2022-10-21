/*
 * Copyright (C) 2014 Apple, Inc.
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)

#include "CallbackID.h"
#include "LayerTreeContext.h"
#include <WebCore/DisplayRefreshMonitor.h>
#include <WebCore/GLContext.h>
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/TextureMapperFPSCounter.h>
#include <WebCore/Timer.h>
#include <wtf/Forward.h>

namespace WebCore {
class GraphicsLayer;
class GraphicsLayerFactory;
class IntRect;
class IntSize;
class Page;
struct ViewportAttributes;
}

namespace WebKit {

class WebPage;

class LayerTreeHost : public WebCore::GraphicsLayerClient {
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
    void setNonCompositedContentsNeedDisplay(const WebCore::IntRect&);
    void scrollNonCompositedContents(const WebCore::IntRect&);
    void forceRepaint();
    void forceRepaintAsync(CompletionHandler<void()>&&);
    void sizeDidChange(const WebCore::IntSize& newSize);
    void pauseRendering();
    void resumeRendering();
    WebCore::GraphicsLayerFactory* graphicsLayerFactory();
    void contentsSizeChanged(const WebCore::IntSize&);
    void didChangeViewportAttributes(WebCore::ViewportAttributes&&);
    void setIsDiscardable(bool);
    void deviceOrPageScaleFactorChanged();
    RefPtr<WebCore::DisplayRefreshMonitor> createDisplayRefreshMonitor(WebCore::PlatformDisplayID);
    WebCore::PlatformDisplayID displayID() const { return m_displayID; }

private:
    // GraphicsLayerClient
    void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, const WebCore::FloatRect& rectToPaint, WebCore::GraphicsLayerPaintBehavior) override;
    float deviceScaleFactor() const override;

    void initialize();
    GLNativeWindowType window();
    bool enabled();
    void compositeLayersToContext();
    void flushAndRenderLayers();
    bool flushPendingLayerChanges();
    void scrollNonCompositedContents(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset);
    void layerFlushTimerFired();
    bool prepareForRendering();
    void applyDeviceScaleFactor();

    WebPage& m_webPage;
    std::unique_ptr<WebCore::GLContext> m_context;
    LayerTreeContext m_layerTreeContext;
    WebCore::PlatformDisplayID m_displayID;
    RefPtr<WebCore::GraphicsLayer> m_rootLayer;
    WebCore::GraphicsLayer* m_rootCompositingLayer { nullptr };
    WebCore::GraphicsLayer* m_overlayCompositingLayer { nullptr };
    std::unique_ptr<WebCore::TextureMapper> m_textureMapper;
    WebCore::TextureMapperFPSCounter m_fpsCounter;
    WebCore::Timer m_layerFlushTimer;
    bool m_notifyAfterScheduledLayerFlush { false };
    bool m_isSuspended { false };
};

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
