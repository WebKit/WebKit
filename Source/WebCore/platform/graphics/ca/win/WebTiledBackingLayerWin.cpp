/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "WebTiledBackingLayerWin.h"

#if USE(CA)

#include "GraphicsContextCG.h"
#include "PlatformCALayer.h"
#include "TileController.h"
#include "TiledBacking.h"
#include <QuartzCore/CACFLayer.h>
#include <wtf/MainThread.h>

using namespace WebCore;

WebTiledBackingLayerWin::WebTiledBackingLayerWin(PlatformCALayer* owner)
    : PlatformCALayerWinInternal(owner)
{
}

WebTiledBackingLayerWin::~WebTiledBackingLayerWin() = default;

struct DisplayOnMainThreadContext {
    RetainPtr<CACFLayerRef> layer;
    RetainPtr<CGContextRef> context;

    DisplayOnMainThreadContext(CACFLayerRef caLayer, CGContextRef caContext)
        : layer(caLayer)
        , context(caContext)
    {
    }
};

static void redispatchOnMainQueue(void* context)
{
    ASSERT(isMainThread());
    std::unique_ptr<DisplayOnMainThreadContext> retainedContext(reinterpret_cast<DisplayOnMainThreadContext*>(context));
    if (!retainedContext)
        return;

    PlatformCALayerWinInternal* self = static_cast<PlatformCALayerWinInternal*>(CACFLayerGetUserData(retainedContext->layer.get()));

    self->displayCallback(retainedContext->layer.get(), retainedContext->context.get());
}

void WebTiledBackingLayerWin::displayCallback(CACFLayerRef caLayer, CGContextRef context)
{
    if (!isMainThread()) {
        dispatch_async_f(dispatch_get_main_queue(), new DisplayOnMainThreadContext(caLayer, context), redispatchOnMainQueue);
        return;
    }
    
    if (!owner() || !owner()->owner())
        return;

    CGContextSaveGState(context);

    CGRect layerBounds = owner()->bounds();
    PlatformCALayer::LayerType layerType = owner()->layerType();
    ASSERT(layerType == PlatformCALayer::LayerTypeTiledBackingLayer || layerType == PlatformCALayer::LayerTypePageTiledBackingLayer);

    PlatformCALayerClient* client = owner()->owner();
    GraphicsLayer::CompositingCoordinatesOrientation orientation = client->platformCALayerContentsOrientation();

    GraphicsContextCG graphicsContext(context);

    // It's important to get the clip from the context, because it may be significantly
    // smaller than the layer bounds (e.g. tiled layers)
    CGRect clipBounds = CGContextGetClipBoundingBox(context);
    IntRect clip(enclosingIntRect(clipBounds));
    client->platformCALayerPaintContents(owner(), graphicsContext, clip, GraphicsLayerPaintNormal);

    if (client->platformCALayerShowRepaintCounter(owner())) {
        int drawCount = client->platformCALayerIncrementRepaintCount(owner());
        drawRepaintCounters(caLayer, context, layerBounds, drawCount);
    }

    CGContextRestoreGState(context);

    client->platformCALayerLayerDidDisplay(owner());
}

void WebTiledBackingLayerWin::setNeedsDisplay()
{
    if (m_tileController)
        return m_tileController->setNeedsDisplay();
}

void WebTiledBackingLayerWin::setNeedsDisplayInRect(const FloatRect& dirtyRect)
{
    if (m_tileController)
        return m_tileController->setNeedsDisplayInRect(enclosingIntRect(dirtyRect));
}

void WebTiledBackingLayerWin::setBounds(const FloatRect& bounds)
{
    if (CGRectEqualToRect(bounds, owner()->bounds()))
        return;

    PlatformCALayerWinInternal::setBounds(bounds);

    if (m_tileController)
        m_tileController->tileCacheLayerBoundsChanged();
}

bool WebTiledBackingLayerWin::isOpaque() const
{
    return m_tileController ? m_tileController->tilesAreOpaque() : true;
}

void WebTiledBackingLayerWin::setOpaque(bool value)
{
    if (m_tileController)
        return m_tileController->setTilesOpaque(value);
}

float WebTiledBackingLayerWin::contentsScale() const
{
    return m_tileController ? m_tileController->contentsScale() : 1.0f;
}

void WebTiledBackingLayerWin::setContentsScale(float scaleFactor)
{
    if (m_tileController)
        return m_tileController->setContentsScale(scaleFactor);
}

void WebTiledBackingLayerWin::setBorderWidth(float value)
{
    if (m_tileController)
        return m_tileController->setTileDebugBorderWidth(value / 2);
}

void WebTiledBackingLayerWin::setBorderColor(const Color& value)
{
    if (m_tileController)
        return m_tileController->setTileDebugBorderColor(value);
}

TileController* WebTiledBackingLayerWin::createTileController(PlatformCALayer* rootLayer)
{
    ASSERT(!m_tileController);
    m_tileController = makeUnique<TileController>(rootLayer);
    return m_tileController.get();
}

TiledBacking* WebTiledBackingLayerWin::tiledBacking()
{
    return m_tileController.get();
}

void WebTiledBackingLayerWin::invalidate()
{
    ASSERT(isMainThread());
    ASSERT(m_tileController);
    m_tileController = nullptr;
}

#endif
