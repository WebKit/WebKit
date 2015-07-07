/*
 * Copyright (C) 2013-2014 Apple Inc. All rights reserved.
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
#include "LayerPool.h"
#include "PlatformCALayer.h"
#include <wtf/StringExtras.h>

#if USE(CA)

namespace WebCore {

static GraphicsLayer::PlatformLayerID generateLayerID()
{
    static GraphicsLayer::PlatformLayerID layerID;
    return ++layerID;
}

#if COMPILER(MSVC)
const float PlatformCALayer::webLayerWastedSpaceThreshold = 0.75f;
#endif

PlatformCALayer::PlatformCALayer(LayerType layerType, PlatformCALayerClient* owner)
    : m_layerType(layerType)
    , m_layerID(generateLayerID())
    , m_owner(owner)
{
}

PlatformCALayer::~PlatformCALayer()
{
    // Clear the owner, which also clears it in the delegate to prevent attempts
    // to use the GraphicsLayerCA after it has been destroyed.
    setOwner(nullptr);
}

void PlatformCALayer::drawRepaintIndicator(CGContextRef context, PlatformCALayer* platformCALayer, int repaintCount, CGColorRef customBackgroundColor)
{
    char text[16]; // that's a lot of repaints
    snprintf(text, sizeof(text), "%d", repaintCount);
    
    CGRect indicatorBox = platformCALayer->bounds();
    indicatorBox.size.width = 12 + 10 * strlen(text);
    indicatorBox.size.height = 27;
    CGContextSaveGState(context);
    
    CGContextSetAlpha(context, 0.5f);
    CGContextBeginTransparencyLayerWithRect(context, indicatorBox, 0);
    
    if (customBackgroundColor)
        CGContextSetFillColorWithColor(context, customBackgroundColor);
    else
        CGContextSetRGBFillColor(context, 0, 0.5f, 0.25f, 1);
    
    CGContextFillRect(context, indicatorBox);
    
    if (platformCALayer->acceleratesDrawing())
        CGContextSetRGBFillColor(context, 1, 0, 0, 1);
    else
        CGContextSetRGBFillColor(context, 1, 1, 1, 1);
    
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    CGContextSetTextMatrix(context, CGAffineTransformMakeScale(1, -1));
    CGContextSelectFont(context, "Helvetica", 22, kCGEncodingMacRoman);
    CGContextShowTextAtPoint(context, indicatorBox.origin.x + 5, indicatorBox.origin.y + 22, text, strlen(text));
#pragma clang diagnostic pop
    
    CGContextEndTransparencyLayer(context);
    CGContextRestoreGState(context);
}

PassRefPtr<PlatformCALayer> PlatformCALayer::createCompatibleLayerOrTakeFromPool(PlatformCALayer::LayerType layerType, PlatformCALayerClient* client, IntSize size)
{
    RefPtr<PlatformCALayer> layer;

    if ((layer = layerPool().takeLayerWithSize(size))) {
        layer->setOwner(client);
        return layer.release();
    }

    layer = createCompatibleLayer(layerType, client);
    layer->setBounds(FloatRect(FloatPoint(), size));
    
    return layer.release();
}

void PlatformCALayer::moveToLayerPool()
{
    ASSERT(!superlayer());
    layerPool().addLayer(this);
}

LayerPool& PlatformCALayer::layerPool()
{
    static LayerPool* sharedPool = new LayerPool;
    return *sharedPool;
}

}

#endif // USE(CA)
