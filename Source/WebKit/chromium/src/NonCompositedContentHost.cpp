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
#include "PlatformContextSkia.h"
#include "WebViewImpl.h"
#include <public/WebContentLayer.h>
#include <public/WebFloatPoint.h>

namespace WebKit {

NonCompositedContentHost::NonCompositedContentHost(WebViewImpl* webView)
    : m_webView(webView)
    , m_opaque(true)
    , m_showDebugBorders(false)
    , m_deviceScaleFactor(1.0)
{
    m_graphicsLayer = WebCore::GraphicsLayer::create(this);
#ifndef NDEBUG
    m_graphicsLayer->setName("non-composited content");
#endif
    m_graphicsLayer->setDrawsContent(true);
    WebContentLayer layer = m_graphicsLayer->platformLayer()->to<WebContentLayer>();
    layer.setUseLCDText(true);
    layer.setOpaque(true);
#if !OS(ANDROID)
    layer.setDrawCheckerboardForMissingTiles(true);
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
    m_opaque = opaque;
    m_graphicsLayer->platformLayer()->setOpaque(opaque);
}

void NonCompositedContentHost::setScrollLayer(WebCore::GraphicsLayer* layer)
{
    m_graphicsLayer->setNeedsDisplay();

    if (!layer) {
        m_graphicsLayer->removeFromParent();
        return;
    }

    if (*layer->platformLayer() == scrollLayer())
        return;

    layer->addChildAtIndex(m_graphicsLayer.get(), 0);
    ASSERT(haveScrollLayer());
}

void NonCompositedContentHost::setViewport(const WebCore::IntSize& viewportSize, const WebCore::IntSize& contentsSize, const WebCore::IntPoint& scrollPosition, const WebCore::IntPoint& scrollOrigin, float deviceScale)
{
    if (!haveScrollLayer())
        return;

    bool visibleRectChanged = m_viewportSize != viewportSize;

    m_viewportSize = viewportSize;
    WebScrollableLayer layer = scrollLayer();
    layer.setScrollPosition(scrollPosition + scrollOrigin);
    layer.setPosition(WebFloatPoint(-scrollPosition));
    // Due to the possibility of pinch zoom, the noncomposited layer is always
    // assumed to be scrollable.
    layer.setScrollable(true);
    m_deviceScaleFactor = deviceScale;
    m_graphicsLayer->deviceOrPageScaleFactorChanged();
    m_graphicsLayer->setSize(contentsSize);

    // In RTL-style pages, the origin of the initial containing block for the
    // root layer may be positive; translate the layer to avoid negative
    // coordinates.
    m_layerAdjust = -toSize(scrollOrigin);
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
}

bool NonCompositedContentHost::haveScrollLayer()
{
    return m_graphicsLayer->parent();
}

WebScrollableLayer NonCompositedContentHost::scrollLayer()
{
    if (!m_graphicsLayer->parent())
        return WebScrollableLayer();
    return m_graphicsLayer->parent()->platformLayer()->to<WebScrollableLayer>();
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

void NonCompositedContentHost::notifySyncRequired(const WebCore::GraphicsLayer*)
{
    m_webView->scheduleCompositingLayerSync();
}

void NonCompositedContentHost::paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext& context, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& clipRect)
{
    // On non-android platforms, we want to render text with subpixel antialiasing on the root layer
    // so long as the root is opaque. On android all text is grayscale.
#if !OS(ANDROID)
    if (m_opaque)
        context.platformContext()->setDrawingToImageBuffer(false);
#endif
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

bool NonCompositedContentHost::showDebugBorders(const WebCore::GraphicsLayer*) const
{
    return m_showDebugBorders;
}

bool NonCompositedContentHost::showRepaintCounter(const WebCore::GraphicsLayer*) const
{
    return false;
}

} // namespace WebKit
