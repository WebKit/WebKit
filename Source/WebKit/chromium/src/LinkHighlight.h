/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef LinkHighlight_h
#define LinkHighlight_h

#include "FloatPoint.h"
#include "GraphicsLayerChromium.h"
#include "IntPoint.h"
#include "Path.h"
#include <public/WebAnimationDelegate.h>
#include <public/WebContentLayer.h>
#include <public/WebContentLayerClient.h>
#include <public/WebLayer.h>
#include <wtf/OwnPtr.h>

namespace WebCore {
class RenderLayer;
class Node;
}

namespace WebKit {

struct WebFloatRect;
struct WebRect;
class WebViewImpl;

class LinkHighlight : public WebContentLayerClient, public WebAnimationDelegate, WebCore::LinkHighlightClient {
public:
    static PassOwnPtr<LinkHighlight> create(WebCore::Node*, WebViewImpl*);
    virtual ~LinkHighlight();

    WebContentLayer* contentLayer();
    WebLayer* clipLayer();
    void startHighlightAnimation();
    void updateGeometry();

    // WebContentLayerClient implementation.
    virtual void paintContents(WebCanvas*, const WebRect& clipRect, WebFloatRect& opaque) OVERRIDE;

    // WebAnimationDelegate implementation.
    virtual void notifyAnimationStarted(double time) OVERRIDE;
    virtual void notifyAnimationFinished(double time) OVERRIDE;

    // LinkHighlightClient inplementation.
    virtual void invalidate() OVERRIDE;
    virtual WebLayer* layer() OVERRIDE;
    virtual void clearCurrentGraphicsLayer() OVERRIDE;

private:
    LinkHighlight(WebCore::Node*, WebViewImpl*);

    void releaseResources();

    WebCore::RenderLayer* computeEnclosingCompositingLayer();
    void clearGraphicsLayerLinkHighlightPointer();
    // This function computes the highlight path, and returns true if it has changed
    // size since the last call to this function.
    bool computeHighlightLayerPathAndPosition(WebCore::RenderLayer*);

    WebContentLayer m_contentLayer;
    WebLayer m_clipLayer;
    WebCore::Path m_path;

    RefPtr<WebCore::Node> m_node;
    OwnPtr<WebAnimation> m_animation;
    WebViewImpl* m_owningWebViewImpl;
    WebCore::GraphicsLayerChromium* m_currentGraphicsLayer;

    bool m_geometryNeedsUpdate;
    WebCore::FloatPoint m_graphicsLayerOffset;
};

} // namespace WebKit

#endif
