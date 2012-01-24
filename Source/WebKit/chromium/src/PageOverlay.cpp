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
#include "PlatformContextSkia.h"
#include "Settings.h"
#include "WebPageOverlay.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"

using namespace WebCore;

namespace WebKit {

namespace {

WebCanvas* ToWebCanvas(GraphicsContext* gc)
{
#if WEBKIT_USING_SKIA
    return gc->platformContext()->canvas();
#elif WEBKIT_USING_CG
    return gc->platformContext();
#endif
}

} // namespace

PassOwnPtr<PageOverlay> PageOverlay::create(WebViewImpl* viewImpl, WebPageOverlay* overlay)
{
    return adoptPtr(new PageOverlay(viewImpl, overlay));
}

PageOverlay::PageOverlay(WebViewImpl* viewImpl, WebPageOverlay* overlay)
    : m_viewImpl(viewImpl)
    , m_overlay(overlay)
    , m_zOrder(0)
{
}

#if USE(ACCELERATED_COMPOSITING)
class OverlayGraphicsLayerClientImpl : public WebCore::GraphicsLayerClient {
public:
    static PassOwnPtr<OverlayGraphicsLayerClientImpl*> create(WebViewImpl* webViewImpl, WebPageOverlay* overlay)
    {
        return adoptPtr(new OverlayGraphicsLayerClientImpl(webViewImpl, overlay));
    }

    virtual ~OverlayGraphicsLayerClientImpl() { }

    virtual void notifyAnimationStarted(const GraphicsLayer*, double time) { }

    virtual void notifySyncRequired(const GraphicsLayer*) { }

    virtual void paintContents(const GraphicsLayer*, GraphicsContext& gc, GraphicsLayerPaintingPhase, const IntRect& inClip)
    {
        gc.save();
        m_overlay->paintPageOverlay(ToWebCanvas(&gc));
        gc.restore();
    }

    virtual float deviceScaleFactor() const
    {
        return m_webViewImpl->deviceScaleFactor();
    }

    virtual float pageScaleFactor() const
    {
        return m_webViewImpl->pageScaleFactor();
    }

    virtual bool showDebugBorders(const GraphicsLayer*) const
    {
        return m_webViewImpl->page()->settings()->showDebugBorders();
    }

    virtual bool showRepaintCounter(const GraphicsLayer*) const
    {
        return m_webViewImpl->page()->settings()->showRepaintCounter();
    }

private:
    OverlayGraphicsLayerClientImpl(WebViewImpl* webViewImpl, WebPageOverlay* overlay)
        : m_overlay(overlay)
        , m_webViewImpl(webViewImpl)
    {
    }

    WebPageOverlay* m_overlay;
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
        m_layerClient = OverlayGraphicsLayerClientImpl::create(m_viewImpl, m_overlay);
        m_layer = GraphicsLayer::create(m_layerClient.get());
        m_layer->setName("WebViewImpl page overlay content");
        m_layer->setDrawsContent(true);
    }

    FloatSize size(m_viewImpl->size());
    if (size != m_layer->size()) {
        // Triggers re-adding to root layer to ensure that we are on top of
        // scrollbars.
        m_layer->removeFromParent();
        m_layer->setSize(size);
    }

    m_viewImpl->setOverlayLayer(m_layer.get());
    m_layer->setNeedsDisplay();
#endif
}

void PageOverlay::paintWebFrame(GraphicsContext& gc)
{
    if (!m_viewImpl->isAcceleratedCompositingActive()) {
        gc.save();
        m_overlay->paintPageOverlay(ToWebCanvas(&gc));
        gc.restore();
    }
}

void PageOverlay::invalidateWebFrame()
{
    // WebPageOverlay does the actual painting of the overlay.
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
