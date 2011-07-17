/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PageOverlay.h"

#include "GraphicsLayer.h"
#include "GraphicsLayerClient.h"
#include "Page.h"
#include "Settings.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"

using namespace WebCore;

namespace WebKit {

PassOwnPtr<PageOverlay> PageOverlay::create(WebViewImpl* viewImpl, PageOverlayClient* client)
{
    return adoptPtr(new PageOverlay(viewImpl, client));
}

PageOverlay::PageOverlay(WebViewImpl* viewImpl, PageOverlayClient* client)
    : m_viewImpl(viewImpl)
    , m_client(client)
{
}

#if USE(ACCELERATED_COMPOSITING)
class OverlayGraphicsLayerClientImpl : public WebCore::GraphicsLayerClient {
public:
    static PassOwnPtr<OverlayGraphicsLayerClientImpl*> create(WebViewImpl* webViewImpl, PageOverlay::PageOverlayClient* pageOverlayClient)
    {
        return adoptPtr(new OverlayGraphicsLayerClientImpl(webViewImpl, pageOverlayClient));
    }

    virtual ~OverlayGraphicsLayerClientImpl() { }

    virtual void notifyAnimationStarted(const GraphicsLayer*, double time) { }

    virtual void notifySyncRequired(const GraphicsLayer*) { }

    virtual void paintContents(const GraphicsLayer*, GraphicsContext& context, GraphicsLayerPaintingPhase, const IntRect& inClip)
    {
        m_pageOverlayClient->paintPageOverlay(context);
    }

    virtual bool showDebugBorders() const
    {
        return m_webViewImpl->page()->settings()->showDebugBorders();
    }

    virtual bool showRepaintCounter() const
    {
        return m_webViewImpl->page()->settings()->showRepaintCounter();
    }

private:
    explicit OverlayGraphicsLayerClientImpl(WebViewImpl* webViewImpl, PageOverlay::PageOverlayClient* pageOverlayClient)
        : m_pageOverlayClient(pageOverlayClient)
        , m_webViewImpl(webViewImpl)
    {
    }

    PageOverlay::PageOverlayClient* m_pageOverlayClient;
    WebViewImpl* m_webViewImpl;
};
#endif

void PageOverlay::clear()
{
    invalidateWebFrame();

#if USE(ACCELERATED_COMPOSITING)
    if (m_layer) {
        m_layer->removeFromParent();
        m_layer = nullptr;
        m_layerClient = nullptr;
    }
#endif
}

void PageOverlay::update()
{
    invalidateWebFrame();

#if USE(ACCELERATED_COMPOSITING)
    if (!m_layer) {
        m_layerClient = OverlayGraphicsLayerClientImpl::create(m_viewImpl, m_client);
        m_layer = GraphicsLayer::create(m_layerClient.get());
        m_layer->setName("WebViewImpl page overlay content");
        m_layer->setDrawsContent(true);
        const WebSize& size = m_viewImpl->size();
        m_layer->setSize(IntSize(size.width, size.height));
    }
    m_viewImpl->setOverlayLayer(m_layer.get());
    m_layer->setNeedsDisplay();
#endif
}

void PageOverlay::paintWebFrame(GraphicsContext& gc)
{
    if (!m_viewImpl->isAcceleratedCompositingActive())
        m_client->paintPageOverlay(gc);
}

void PageOverlay::invalidateWebFrame()
{
    // PageOverlayClient does the actual painting of the overlay.
    // Here we just make sure to invalidate.
    if (!m_viewImpl->isAcceleratedCompositingActive()) {
        // FIXME: able to invalidate a smaller rect.
        // FIXME: Is it important to just invalidate a smaller rect given that
        // this is not on a critical codepath? In order to do so, we'd
        // have to take scrolling into account.
        const WebSize& size = m_viewImpl->size();
        WebRect damagedRect(0, 0, size.width, size.height);
        if (m_viewImpl->client())
            m_viewImpl->client()->didInvalidateRect(damagedRect);
    }
}

} // namespace WebKit
