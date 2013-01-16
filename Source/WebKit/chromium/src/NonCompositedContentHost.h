/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NonCompositedContentHost_h
#define NonCompositedContentHost_h

#include "GraphicsLayerClient.h"
#include "IntSize.h"

#include <public/WebLayer.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {
class Color;
class GraphicsLayer;
class GraphicsLayerFactory;
class GraphicsContext;
class IntPoint;
class IntRect;
}

namespace WebKit {
class WebViewImpl;

class NonCompositedContentHost : public WebCore::GraphicsLayerClient {
WTF_MAKE_NONCOPYABLE(NonCompositedContentHost);
public:
    static PassOwnPtr<NonCompositedContentHost> create(WebViewImpl* webView, WebCore::GraphicsLayerFactory* graphicsLayerFactory)
    {
        return adoptPtr(new NonCompositedContentHost(webView, graphicsLayerFactory));
    }
    virtual ~NonCompositedContentHost();

    void invalidateRect(const WebCore::IntRect&);
    void setBackgroundColor(const WebCore::Color&);
    void setOpaque(bool);
    void setScrollLayer(WebCore::GraphicsLayer*);
    void setViewport(const WebCore::IntSize& viewportSize, const WebCore::IntSize& contentsSize, const WebCore::IntPoint& scrollPosition, const WebCore::IntPoint& scrollOrigin);
    WebCore::GraphicsLayer* topLevelRootLayer() const { return m_graphicsLayer.get(); }

    void setShowDebugBorders(bool);

protected:
    NonCompositedContentHost(WebViewImpl*, WebCore::GraphicsLayerFactory*);

private:
    // GraphicsLayerClient
    virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double time);
    virtual void notifyFlushRequired(const WebCore::GraphicsLayer*);
    virtual void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& clipRect);
    virtual bool isTrackingRepaints() const;

    bool haveScrollLayer();
    WebLayer* scrollLayer();

    OwnPtr<WebCore::GraphicsLayer> m_graphicsLayer;
    WebViewImpl* m_webView;
    WebCore::IntSize m_viewportSize;
    WebCore::IntSize m_layerAdjust;

    bool m_showDebugBorders;
};

} // namespace WebKit

#endif // NonCompositedContentHost_h
