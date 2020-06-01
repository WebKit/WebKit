/*
 * Copyright (C) 2011, 2015 Apple Inc. All rights reserved.
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

#include "PlatformCALayerWinInternal.h"

#if USE(CA)

#include "FontCascade.h"
#include "GraphicsContext.h"
#include "PlatformCALayer.h"
#include "TileController.h"
#include "TiledBacking.h"
#include <QuartzCore/CACFLayer.h>
#include <wtf/MainThread.h>

using namespace WebCore;

PlatformCALayerWinInternal::PlatformCALayerWinInternal(PlatformCALayer* owner)
    : m_owner(owner)
{
}

PlatformCALayerWinInternal::~PlatformCALayerWinInternal() = default;

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

static bool repaintCountersAreDrawnByGridController(PlatformCALayer::LayerType layerType)
{
    return layerType == PlatformCALayer::LayerTypeTiledBackingTileLayer;
}

void PlatformCALayerWinInternal::displayCallback(CACFLayerRef caLayer, CGContextRef context)
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

    PlatformCALayerClient* client = owner()->owner();
    GraphicsLayer::CompositingCoordinatesOrientation orientation = client->platformCALayerContentsOrientation();

    PlatformCALayer::flipContext(context, layerBounds.size.height);

    GraphicsContext graphicsContext(context);

    // It's important to get the clip from the context, because it may be significantly
    // smaller than the layer bounds (e.g. tiled layers)
    CGRect clipBounds = CGContextGetClipBoundingBox(context);
    IntRect clip(enclosingIntRect(clipBounds));
    client->platformCALayerPaintContents(owner(), graphicsContext, clip, GraphicsLayerPaintNormal);

    if (client->platformCALayerShowRepaintCounter(owner())
        && !repaintCountersAreDrawnByGridController(layerType)) {
        int drawCount = client->platformCALayerIncrementRepaintCount(owner());
        drawRepaintCounters(caLayer, context, layerBounds, drawCount);
    }

    CGContextRestoreGState(context);

    client->platformCALayerLayerDidDisplay(owner());
}

void PlatformCALayerWinInternal::drawRepaintCounters(CACFLayerRef caLayer, CGContextRef context, CGRect layerBounds, int drawCount)
{
    // We always update the repaint count, but we do not paint it for layers that
    // only contain tiled backing layers. Those report their repaint counts themselves.
    if (!owner() || owner()->usesTiledBackingLayer())
        return;

    CGColorRef backgroundColor = nullptr;
    // Make the background of the counter the same as the border color,
    // unless there is no border, then make it red
    float borderWidth = CACFLayerGetBorderWidth(caLayer);
    if (borderWidth > 0)
        backgroundColor = CACFLayerGetBorderColor(caLayer);
    else
        backgroundColor = cachedCGColor(makeSimpleColor(255, 0, 0));

    GraphicsContext graphicsContext(context);
    PlatformCALayer::drawRepaintIndicator(graphicsContext, owner(), drawCount, backgroundColor);
}

void PlatformCALayerWinInternal::internalSetNeedsDisplay(const FloatRect* dirtyRect)
{
    if (dirtyRect) {
        CGRect rect = *dirtyRect;
        CACFLayerSetNeedsDisplay(owner()->platformLayer(), &rect);
    } else
        CACFLayerSetNeedsDisplay(owner()->platformLayer(), nullptr);
}

void PlatformCALayerWinInternal::setNeedsDisplay()
{
    internalSetNeedsDisplay(nullptr);
}

void PlatformCALayerWinInternal::setNeedsDisplayInRect(const FloatRect& dirtyRect)
{
    if (!owner())
        return;

    ASSERT(owner()->layerType() != PlatformCALayer::LayerTypeTiledBackingLayer);

    if (owner()->owner()) {
        if (owner()->owner()->platformCALayerShowRepaintCounter(owner())) {
            FloatRect layerBounds = owner()->bounds();
            FloatRect repaintCounterRect = layerBounds;

            // We assume a maximum of 4 digits and a font size of 18.
            repaintCounterRect.setWidth(80);
            repaintCounterRect.setHeight(22);
            if (owner()->owner()->platformCALayerContentsOrientation() == WebCore::GraphicsLayer::CompositingCoordinatesOrientation::TopDown)
                repaintCounterRect.setY(layerBounds.height() - (layerBounds.y() + repaintCounterRect.height()));
            internalSetNeedsDisplay(&repaintCounterRect);
        }
        if (owner()->owner()->platformCALayerContentsOrientation() == WebCore::GraphicsLayer::CompositingCoordinatesOrientation::TopDown) {
            FloatRect flippedDirtyRect = dirtyRect;
            flippedDirtyRect.setY(owner()->bounds().height() - (flippedDirtyRect.y() + flippedDirtyRect.height()));
            internalSetNeedsDisplay(&flippedDirtyRect);
            return;
        }
    }

    internalSetNeedsDisplay(&dirtyRect);

    owner()->setNeedsCommit();
}

void PlatformCALayerWinInternal::setSublayers(const PlatformCALayerList& list)
{
    // Remove all the current sublayers and add the passed layers
    CACFLayerSetSublayers(owner()->platformLayer(), 0);

    // Perform removeFromSuperLayer in a separate pass. CACF requires superlayer to
    // be null or CACFLayerInsertSublayer silently fails.
    for (size_t i = 0; i < list.size(); i++)
        CACFLayerRemoveFromSuperlayer(list[i]->platformLayer());

    for (size_t i = 0; i < list.size(); i++)
        CACFLayerInsertSublayer(owner()->platformLayer(), list[i]->platformLayer(), i);

    owner()->setNeedsCommit();
}

void PlatformCALayerWinInternal::getSublayers(PlatformCALayerList& list) const
{
    CFArrayRef sublayers = CACFLayerGetSublayers(owner()->platformLayer());
    if (!sublayers) {
        list.clear();
        return;
    }

    size_t count = CFArrayGetCount(sublayers);

    list.resize(count);
    for (size_t arrayIndex = 0; arrayIndex < count; ++arrayIndex)
        list[arrayIndex] = PlatformCALayer::platformCALayerForLayer(const_cast<void*>(CFArrayGetValueAtIndex(sublayers, arrayIndex)));
}

void PlatformCALayerWinInternal::removeAllSublayers()
{
    CACFLayerSetSublayers(owner()->platformLayer(), nullptr);
    owner()->setNeedsCommit();
}

void PlatformCALayerWinInternal::insertSublayer(PlatformCALayer& layer, size_t index)
{
    index = std::min(index, sublayerCount());

    layer.removeFromSuperlayer();
    CACFLayerInsertSublayer(owner()->platformLayer(), layer.platformLayer(), index);
    owner()->setNeedsCommit();
}

size_t PlatformCALayerWinInternal::sublayerCount() const
{
    CFArrayRef sublayers = CACFLayerGetSublayers(owner()->platformLayer());
    return sublayers ? CFArrayGetCount(sublayers) : 0;
}

int PlatformCALayerWinInternal::indexOfSublayer(const PlatformCALayer* reference)
{
    CACFLayerRef ref = reference->platformLayer();
    if (!ref)
        return -1;

    CFArrayRef sublayers = CACFLayerGetSublayers(owner()->platformLayer());
    if (!sublayers)
        return -1;

    size_t n = CFArrayGetCount(sublayers);

    for (size_t i = 0; i < n; ++i) {
        if (CFArrayGetValueAtIndex(sublayers, i) == ref)
            return i;
    }

    return -1;
}

PlatformCALayer* PlatformCALayerWinInternal::sublayerAtIndex(int index) const
{
    CFArrayRef sublayers = CACFLayerGetSublayers(owner()->platformLayer());
    if (!sublayers || index < 0 || CFArrayGetCount(sublayers) <= index)
        return nullptr;
    
    return PlatformCALayer::platformCALayerForLayer(static_cast<CACFLayerRef>(const_cast<void*>(CFArrayGetValueAtIndex(sublayers, index)))).get();
}

void PlatformCALayerWinInternal::setBounds(const FloatRect& rect)
{
    if (CGRectEqualToRect(rect, owner()->bounds()))
        return;

    CACFLayerSetBounds(owner()->platformLayer(), rect);
    owner()->setNeedsCommit();
}

void PlatformCALayerWinInternal::setFrame(const FloatRect& rect)
{
    CGRect oldFrame = CACFLayerGetFrame(owner()->platformLayer());
    if (CGRectEqualToRect(rect, oldFrame))
        return;

    CACFLayerSetFrame(owner()->platformLayer(), rect);
    owner()->setNeedsCommit();
}

bool PlatformCALayerWinInternal::isOpaque() const
{
    return CACFLayerIsOpaque(owner()->platformLayer());
}

void PlatformCALayerWinInternal::setOpaque(bool value)
{
    CACFLayerSetOpaque(owner()->platformLayer(), value);
}

float PlatformCALayerWinInternal::contentsScale() const
{
#if HAVE(CACFLAYER_SETCONTENTSSCALE)
    return CACFLayerGetContentsScale(owner()->platformLayer());
#else
    return 1.0f;
#endif
}

void PlatformCALayerWinInternal::setContentsScale(float scaleFactor)
{
#if HAVE(CACFLAYER_SETCONTENTSSCALE)
    CACFLayerSetContentsScale(owner()->platformLayer(), scaleFactor);
#endif
}

void PlatformCALayerWinInternal::setBorderWidth(float value)
{
    CACFLayerSetBorderWidth(owner()->platformLayer(), value);
}

void PlatformCALayerWinInternal::setBorderColor(const Color& value)
{
    CACFLayerSetBorderColor(owner()->platformLayer(), cachedCGColor(value));
}

#endif
