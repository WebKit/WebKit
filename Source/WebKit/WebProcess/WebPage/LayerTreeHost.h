/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef LayerTreeHost_h
#define LayerTreeHost_h

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)

#include "CallbackID.h"
#include "LayerTreeContext.h"
#include <WebCore/DisplayRefreshMonitor.h>
#include <WebCore/PlatformScreen.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace IPC {
class Connection;
}

namespace WebCore {
class IntRect;
class IntSize;
class GraphicsLayer;
class GraphicsLayerFactory;
#if USE(COORDINATED_GRAPHICS_THREADED)
struct ViewportAttributes;
#endif
}

namespace WebKit {

class WebPage;

class LayerTreeHost : public RefCounted<LayerTreeHost> {
public:
    static RefPtr<LayerTreeHost> create(WebPage&);
    virtual ~LayerTreeHost();

    const LayerTreeContext& layerTreeContext() const { return m_layerTreeContext; }
    void setLayerFlushSchedulingEnabled(bool);
    void setShouldNotifyAfterNextScheduledLayerFlush(bool notifyAfterScheduledLayerFlush) { m_notifyAfterScheduledLayerFlush = notifyAfterScheduledLayerFlush; }

    virtual void scheduleLayerFlush() = 0;
    virtual void cancelPendingLayerFlush() = 0;
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*) = 0;
    virtual void invalidate();

    virtual void setNonCompositedContentsNeedDisplay() { };
    virtual void setNonCompositedContentsNeedDisplayInRect(const WebCore::IntRect&) { };
    virtual void scrollNonCompositedContents(const WebCore::IntRect&) { };
    virtual void forceRepaint() = 0;
    virtual bool forceRepaintAsync(CallbackID) { return false; }
    virtual void sizeDidChange(const WebCore::IntSize& newSize) = 0;

    virtual void pauseRendering();
    virtual void resumeRendering();

    virtual WebCore::GraphicsLayerFactory* graphicsLayerFactory() { return nullptr; }

#if USE(COORDINATED_GRAPHICS_THREADED)
    virtual void contentsSizeChanged(const WebCore::IntSize&) { };
    virtual void didChangeViewportAttributes(WebCore::ViewportAttributes&&) { };
#endif

#if USE(COORDINATED_GRAPHICS)
    virtual void scheduleAnimation() = 0;
    virtual void setIsDiscardable(bool) { };
#endif

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(GTK)
    virtual void setNativeSurfaceHandleForCompositing(uint64_t) { };
#endif

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    virtual void deviceOrPageScaleFactorChanged() = 0;
#endif

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    virtual RefPtr<WebCore::DisplayRefreshMonitor> createDisplayRefreshMonitor(WebCore::PlatformDisplayID) { return nullptr; }
#endif

    virtual void setViewOverlayRootLayer(WebCore::GraphicsLayer* viewOverlayRootLayer) { m_viewOverlayRootLayer = viewOverlayRootLayer; }

protected:
    explicit LayerTreeHost(WebPage&);

    WebPage& m_webPage;
    LayerTreeContext m_layerTreeContext;
    bool m_layerFlushSchedulingEnabled { true };
    bool m_notifyAfterScheduledLayerFlush { false };
    bool m_isSuspended { false };
    bool m_isValid { true };
    WebCore::GraphicsLayer* m_viewOverlayRootLayer { nullptr };
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)

#endif // LayerTreeHost_h
