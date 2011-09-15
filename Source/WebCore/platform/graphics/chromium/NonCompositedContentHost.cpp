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

#include "FloatRect.h"
#include "GraphicsLayer.h"
#include "LayerChromium.h"
#include "LayerPainterChromium.h"

namespace WebCore {

NonCompositedContentHost::NonCompositedContentHost(PassOwnPtr<LayerPainterChromium> contentPaint)
    : m_contentPaint(contentPaint)
{
    m_graphicsLayer = GraphicsLayer::create(this);
#ifndef NDEBUG
    m_graphicsLayer->setName("non-composited content");
#endif
    m_graphicsLayer->setDrawsContent(true);
    m_graphicsLayer->platformLayer()->setIsRootLayer(true);
}

NonCompositedContentHost::~NonCompositedContentHost()
{
}

void NonCompositedContentHost::setRootLayer(GraphicsLayer* layer)
{
    m_graphicsLayer->removeAllChildren();
    m_graphicsLayer->setNeedsDisplay();
    if (layer)
        m_graphicsLayer->addChild(layer);
    else
        m_graphicsLayer->platformLayer()->setLayerTreeHost(0);
}

void NonCompositedContentHost::setViewport(const IntSize& viewportSize, const IntSize& contentsSize, const IntPoint& scrollPosition)
{
    bool visibleRectChanged = m_viewportSize != viewportSize;

    m_viewportSize = viewportSize;
    m_graphicsLayer->platformLayer()->setScrollPosition(scrollPosition);
    m_graphicsLayer->setSize(contentsSize);

    if (visibleRectChanged)
        m_graphicsLayer->setNeedsDisplay();
}

void NonCompositedContentHost::protectVisibleTileTextures()
{
    m_graphicsLayer->platformLayer()->protectVisibleTileTextures();
}

void NonCompositedContentHost::invalidateRect(const IntRect& rect)
{
    m_graphicsLayer->setNeedsDisplayInRect(FloatRect(rect));
}

void NonCompositedContentHost::notifyAnimationStarted(const GraphicsLayer*, double /* time */)
{
    // Intentionally left empty since we don't support animations on the non-composited content.
}

void NonCompositedContentHost::notifySyncRequired(const GraphicsLayer*)
{
}

void NonCompositedContentHost::paintContents(const GraphicsLayer*, GraphicsContext& context, GraphicsLayerPaintingPhase, const IntRect& clipRect)
{
    m_contentPaint->paint(context, clipRect);
}

bool NonCompositedContentHost::showDebugBorders() const
{
    return false;
}

bool NonCompositedContentHost::showRepaintCounter() const
{
    return false;
}

} // namespace WebCore

