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

#include "config.h"

#include "NonCompositedContentHost.h"

#include "FloatPoint.h"
#include "FloatRect.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerChromium.h"
#include "PlatformContextSkia.h"
#include "Settings.h"
#include "WebViewImpl.h"
#include <public/WebContentLayer.h>
#include <public/WebFloatPoint.h>

namespace WebKit {

NonCompositedContentHost::NonCompositedContentHost(WebViewImpl* webView)
    : m_webView(webView)
    , m_showDebugBorders(false)
{
    m_graphicsLayer = WebCore::GraphicsLayer::create(0, this);
#ifndef NDEBUG
    m_graphicsLayer->setName("non-composited content");
#endif
    m_graphicsLayer->setDrawsContent(true);
    m_graphicsLayer->setAppliesPageScale(!m_webView->page()->settings()->applyPageScaleFactorInCompositor());
    m_graphicsLayer->setContentsOpaque(true);
    // FIXME: Remove LCD text setting after it is implemented in chromium.
    WebContentLayer* layer = static_cast<WebCore::GraphicsLayerChromium*>(m_graphicsLayer.get())->contentLayer();
    layer->setUseLCDText(true);
#if !OS(ANDROID)
    layer->setDrawCheckerboardForMissingTiles(true);
#endif
}

NonCompositedContentHost::~NonCompositedContentHost()
{
}

void NonCompositedContentHost::setBackgroundColor(const WebCore::Color& color)
{
    m_graphicsLayer->platformLayer()->setBackgroundColor(color.rgb());
}

void NonCompositedContentHost::setOpaque(bool opaque)
{
    m_graphicsLayer->setContentsOpaque(opaque);
}

void NonCompositedContentHost::setScrollLayer(WebCore::GraphicsLayer* layer)
{
    m_graphicsLayer->setNeedsDisplay();

    if (!layer) {
        m_graphicsLayer->removeFromParent();
        return;
    }

    if (layer->platformLayer() == scrollLayer())
        return;

    layer->addChildAtIndex(m_graphicsLayer.get(), 0);
    ASSERT(haveScrollLayer());
}

static void setScrollbarBoundsContainPageScale(WebCore::GraphicsLayer* layer, WebCore::GraphicsLayer* clipLayer)
{
    // Scrollbars are attached outside the root clip rect, so skip the
    // clipLayer subtree.
    if (layer == clipLayer)
        return;

    for (size_t i = 0; i < layer->children().size(); ++i)
        setScrollbarBoundsContainPageScale(layer->children()[i], clipLayer);

    if (layer->children().isEmpty())
        layer->setAppliesPageScale(true);
}

void NonCompositedContentHost::setViewport(const WebCore::IntSize& viewportSize, const WebCore::IntSize& contentsSize, const WebCore::IntPoint& scrollPosition, const WebCore::IntPoint& scrollOrigin)
{
    if (!haveScrollLayer())
        return;

    bool visibleRectChanged = m_viewportSize != viewportSize;

    m_viewportSize = viewportSize;
    WebLayer* layer = scrollLayer();
    layer->setScrollPosition(scrollPosition + scrollOrigin);
    layer->setPosition(WebFloatPoint(-scrollPosition));
    // Due to the possibility of pinch zoom, the noncomposited layer is always
    // assumed to be scrollable.
    layer->setScrollable(true);
    m_graphicsLayer->setSize(contentsSize);

    // In RTL-style pages, the origin of the initial containing block for the
    // root layer may be positive; translate the layer to avoid negative
    // coordinates.
    m_layerAdjust = -toIntSize(scrollOrigin);
    if (m_graphicsLayer->transform().m41() != m_layerAdjust.width() || m_graphicsLayer->transform().m42() != m_layerAdjust.height()) {
        WebCore::TransformationMatrix transform = m_graphicsLayer->transform();
        transform.setM41(m_layerAdjust.width());
        transform.setM42(m_layerAdjust.height());
        m_graphicsLayer->setTransform(transform);

        // If a tiled layer is shifted left or right, the content that goes into
        // each tile will change. Invalidate the entire layer when this happens.
        m_graphicsLayer->setNeedsDisplay();
    } else if (visibleRectChanged)
        m_graphicsLayer->setNeedsDisplay();

    WebCore::GraphicsLayer* clipLayer = m_graphicsLayer->parent()->parent();
    WebCore::GraphicsLayer* rootLayer = clipLayer;
    while (rootLayer->parent())
        rootLayer = rootLayer->parent();
    setScrollbarBoundsContainPageScale(rootLayer, clipLayer);
}

bool NonCompositedContentHost::haveScrollLayer()
{
    return m_graphicsLayer->parent();
}

WebLayer* NonCompositedContentHost::scrollLayer()
{
    if (!m_graphicsLayer->parent())
        return 0;
    return m_graphicsLayer->parent()->platformLayer();
}

void NonCompositedContentHost::invalidateRect(const WebCore::IntRect& rect)
{
    WebCore::IntRect layerRect = rect;
    layerRect.move(-m_layerAdjust);
    m_graphicsLayer->setNeedsDisplayInRect(WebCore::FloatRect(layerRect));
}

void NonCompositedContentHost::notifyAnimationStarted(const WebCore::GraphicsLayer*, double /* time */)
{
    // Intentionally left empty since we don't support animations on the non-composited content.
}

void NonCompositedContentHost::notifyFlushRequired(const WebCore::GraphicsLayer*)
{
    m_webView->scheduleCompositingLayerSync();
}

void NonCompositedContentHost::paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext& context, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& clipRect)
{
    context.translate(-m_layerAdjust);
    WebCore::IntRect adjustedClipRect = clipRect;
    adjustedClipRect.move(m_layerAdjust);
    m_webView->paintRootLayer(context, adjustedClipRect);
}

void NonCompositedContentHost::setShowDebugBorders(bool showDebugBorders)
{
    m_showDebugBorders = showDebugBorders;
    m_graphicsLayer->updateDebugIndicators();
}

bool NonCompositedContentHost::isTrackingRepaints() const
{
    return m_webView->isTrackingRepaints();
}

} // namespace WebKit
