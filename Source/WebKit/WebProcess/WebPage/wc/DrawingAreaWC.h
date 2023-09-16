/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#if USE(GRAPHICS_LAYER_WC)

#include "DrawingArea.h"
#include "GraphicsLayerWC.h"
#include "RemoteWCLayerTreeHostProxy.h"
#include "WCLayerFactory.h"
#include <WebCore/GraphicsLayerFactory.h>
#include <WebCore/Timer.h>

namespace WebKit {

class DrawingAreaWC final
    : public DrawingArea
    , public GraphicsLayerWC::Observer {
public:
    DrawingAreaWC(WebPage&, const WebPageCreationParameters&);
    ~DrawingAreaWC() override;

private:
    // DrawingArea
    WebCore::GraphicsLayerFactory* graphicsLayerFactory() override;
    void setNeedsDisplay() override;
    void setNeedsDisplayInRect(const WebCore::IntRect&) override;
    void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollDelta) override;
    void forceRepaintAsync(WebPage&, CompletionHandler<void()>&&) override;
    void triggerRenderingUpdate() override;
    void didChangeViewportAttributes(WebCore::ViewportAttributes&&) override { }
    void deviceOrPageScaleFactorChanged() override { }
    bool enterAcceleratedCompositingModeIfNeeded() override { return false; }
    void setLayerTreeStateIsFrozen(bool) override;
    bool layerTreeStateIsFrozen() const override { return m_isRenderingSuspended; }
#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER)    
    void updateGeometry(const WebCore::IntSize&, CompletionHandler<void()>&&) override { }
#endif
    void updateGeometryWC(uint64_t, WebCore::IntSize) override;
    void setRootCompositingLayer(WebCore::Frame&, WebCore::GraphicsLayer*) override;
    void addRootFrame(WebCore::FrameIdentifier) override;
    void attachViewOverlayGraphicsLayer(WebCore::FrameIdentifier, WebCore::GraphicsLayer*) override;
    void updatePreferences(const WebPreferencesStore&) override;
    bool shouldUseTiledBackingForFrameView(const WebCore::LocalFrameView&) const override;
    void displayDidRefresh() override;
    // GraphicsLayerWC::Observer
    void graphicsLayerAdded(GraphicsLayerWC&) override;
    void graphicsLayerRemoved(GraphicsLayerWC&) override;
    void commitLayerUpdateInfo(WCLayerUpdateInfo&&) override;
    RefPtr<WebCore::ImageBuffer> createImageBuffer(WebCore::FloatSize) override;

    bool isCompositingMode();
    void updateRendering();
    void sendUpdateAC();
    void sendUpdateNonAC();
    void updateRootLayers();

    struct RootLayerInfo {
        Ref<WebCore::GraphicsLayer> layer;
        RefPtr<WebCore::GraphicsLayer> contentLayer;
        RefPtr<WebCore::GraphicsLayer> viewOverlayRootLayer;
        WebCore::FrameIdentifier frameID;
    };
    RootLayerInfo* rootLayerInfoWithFrameIdentifier(WebCore::FrameIdentifier);

    WebCore::GraphicsLayerClient m_rootLayerClient;
    std::unique_ptr<RemoteWCLayerTreeHostProxy> m_remoteWCLayerTreeHostProxy;
    WCLayerFactory m_layerFactory;
    DoublyLinkedList<GraphicsLayerWC> m_liveGraphicsLayers;
    WebCore::Timer m_updateRenderingTimer;
    bool m_isRenderingSuspended { false };
    bool m_hasDeferredRenderingUpdate { false };
    bool m_inUpdateRendering { false };
    bool m_waitDidUpdate { false };
    bool m_isForceRepaintCompletionHandlerDeferred { false };
    WCUpdateInfo m_updateInfo;
    Vector<RootLayerInfo, 1> m_rootLayers;
    Ref<WorkQueue> m_commitQueue;
    int64_t m_backingStoreStateID { 0 };
    WebCore::Region m_dirtyRegion;
    WebCore::IntRect m_scrollRect;
    WebCore::IntSize m_scrollOffset;
    CompletionHandler<void()> m_forceRepaintCompletionHandler;
};

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
