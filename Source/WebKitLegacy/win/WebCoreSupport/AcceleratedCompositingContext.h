/*
 * Copyright (C) 2014 Apple, Inc.
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

#ifndef AcceleratedCompositingContext_h
#define AcceleratedCompositingContext_h

#if USE(TEXTURE_MAPPER_GL)

#include <WebCore/FloatRect.h>
#include <WebCore/GLContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <WebCore/TextureMapperFPSCounter.h>
#include <WebCore/Timer.h>

class WebView;

class AcceleratedCompositingContext : public WebCore::GraphicsLayerClient {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(AcceleratedCompositingContext);
public:
    explicit AcceleratedCompositingContext(WebView&);
    virtual ~AcceleratedCompositingContext();

    void setRootCompositingLayer(WebCore::GraphicsLayer*);
    void setNonCompositedContentsNeedDisplay(const WebCore::IntRect&);
    void scheduleLayerFlush();
    void resizeRootLayer(const WebCore::IntSize&);
    bool enabled();

    // GraphicsLayerClient
    void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, const WebCore::FloatRect& rectToPaint, WebCore::GraphicsLayerPaintBehavior) override;
    float deviceScaleFactor() const override;

    void initialize();

    enum CompositePurpose { ForResize, NotForResize };
    void compositeLayersToContext(CompositePurpose = NotForResize);
    void paint(HDC);

    bool flushPendingLayerChanges();
    bool flushPendingLayerChangesSoon();
    void scrollNonCompositedContents(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset);

    String layerTreeAsString() const;

private:
    WebView& m_webView;
    std::unique_ptr<WebCore::GLContext> m_context;
    HWND m_window;
    RefPtr<WebCore::GraphicsLayer> m_rootLayer;
    RefPtr<WebCore::GraphicsLayer> m_nonCompositedContentLayer;
    std::unique_ptr<WebCore::TextureMapper> m_textureMapper;
    WebCore::TextureMapperFPSCounter m_fpsCounter;

    class LayerFlushTimer : public WebCore::TimerBase {
    public:
        LayerFlushTimer(AcceleratedCompositingContext& context)
            : m_context(context)
        {
        }

    private:
        virtual void fired()
        {
            m_context.layerFlushTimerFired();
        }

        AcceleratedCompositingContext& m_context;
    } m_layerFlushTimer;

    void flushAndRenderLayers();
    void layerFlushTimerFired();
    void stopAnyPendingLayerFlush();
    bool prepareForRendering();
    bool startedAnimation(WebCore::GraphicsLayer*);
    void applyDeviceScaleFactor();
};

#endif // TEXTURE_MAPPER_GL

#endif // AcceleratedCompositingContext_h
