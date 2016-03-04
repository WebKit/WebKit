/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Igalia S.L.
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

#ifndef LayerTreeHostGtk_h
#define LayerTreeHostGtk_h

#if USE(TEXTURE_MAPPER_GL)

#include "LayerTreeContext.h"
#include "LayerTreeHost.h"
#include "TextureMapperLayer.h"
#include <WebCore/GLContext.h>
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/RunLoop.h>

namespace WebKit {

class LayerTreeHostGtk final : public LayerTreeHost, WebCore::GraphicsLayerClient {
public:
    static PassRefPtr<LayerTreeHostGtk> create(WebPage*);
    virtual ~LayerTreeHostGtk();

protected:
    explicit LayerTreeHostGtk(WebPage*);

    WebCore::GraphicsLayer* rootLayer() const { return m_rootLayer.get(); }

    void initialize();

    // LayerTreeHost
    void scheduleLayerFlush() override;
    void setLayerFlushSchedulingEnabled(bool layerFlushingEnabled) override;
    void setRootCompositingLayer(WebCore::GraphicsLayer*) override;
    void invalidate() override;

    void forceRepaint() override;
    void sizeDidChange(const WebCore::IntSize& newSize) override;
    void deviceOrPageScaleFactorChanged() override;
    void pageBackgroundTransparencyChanged() override;

    void setNativeSurfaceHandleForCompositing(uint64_t) override;

private:

    class RenderFrameScheduler {
    public:
        RenderFrameScheduler(std::function<bool()>);
        ~RenderFrameScheduler();

        void start();
        void stop();

    private:
        void renderFrame();
        void nextFrame();

        std::function<bool()> m_renderer;
        RunLoop::Timer<RenderFrameScheduler> m_timer;
        double m_fireTime { 0 };
        double m_lastImmediateFlushTime { 0 };
    };

    // LayerTreeHost
    const LayerTreeContext& layerTreeContext() override;
    void setShouldNotifyAfterNextScheduledLayerFlush(bool) override;

    void setNonCompositedContentsNeedDisplay() override;
    void setNonCompositedContentsNeedDisplayInRect(const WebCore::IntRect&) override;
    void scrollNonCompositedContents(const WebCore::IntRect& scrollRect) override;
    void setViewOverlayRootLayer(WebCore::GraphicsLayer*) override;

    // GraphicsLayerClient
    void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::FloatRect& clipRect) override;
    float deviceScaleFactor() const override;
    float pageScaleFactor() const override;

    bool flushPendingLayerChanges();

    enum CompositePurpose { ForResize, NotForResize };
    void compositeLayersToContext(CompositePurpose = NotForResize);

    void flushAndRenderLayers();
    void cancelPendingLayerFlush();

    bool renderFrame();

    bool makeContextCurrent();

    LayerTreeContext m_layerTreeContext;
    bool m_isValid;
    bool m_notifyAfterScheduledLayerFlush;
    std::unique_ptr<WebCore::GraphicsLayer> m_rootLayer;
    std::unique_ptr<WebCore::GraphicsLayer> m_nonCompositedContentLayer;
    std::unique_ptr<WebCore::TextureMapper> m_textureMapper;
    std::unique_ptr<WebCore::GLContext> m_context;
    bool m_layerFlushSchedulingEnabled;
    WebCore::GraphicsLayer* m_viewOverlayRootLayer;
    WebCore::TransformationMatrix m_scaleMatrix;
    RenderFrameScheduler m_renderFrameScheduler;
};

} // namespace WebKit

#endif

#endif // LayerTreeHostGtk_h
