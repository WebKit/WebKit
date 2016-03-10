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
#include "PlatformCALayer.h"

#include "LayerPool.h"
#include "PlatformCALayerClient.h"
#include "TextStream.h"
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

    CGContextSaveGState(context);

    indicatorBox.size.width = 12 + 10 * strlen(text);
    indicatorBox.size.height = 27;
    
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

    if (platformCALayer->owner()->isUsingDisplayListDrawing(platformCALayer)) {
        CGContextSetRGBStrokeColor(context, 0, 0, 0, 0.65);
        CGContextSetLineWidth(context, 2);
        CGContextStrokeRect(context, indicatorBox);
    }
    
    platformCALayer->drawTextAtPoint(context, indicatorBox.origin.x + 5, indicatorBox.origin.y + 22, CGSizeMake(1, -1), 22, text, strlen(text));
    
    CGContextEndTransparencyLayer(context);
    CGContextRestoreGState(context);
}

void PlatformCALayer::flipContext(CGContextRef context, CGFloat height)
{
    CGContextScaleCTM(context, 1, -1);
    CGContextTranslateCTM(context, 0, -height);
}

// This function is needed to work around a bug in Windows CG <rdar://problem/22703470>
void PlatformCALayer::drawTextAtPoint(CGContextRef context, CGFloat x, CGFloat y, CGSize scale, CGFloat fontSize, const char* text, size_t length) const
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    CGContextSetTextMatrix(context, CGAffineTransformMakeScale(scale.width, scale.height));
    CGContextSelectFont(context, "Helvetica", fontSize, kCGEncodingMacRoman);
    CGContextShowTextAtPoint(context, x, y, text, length);
#pragma clang diagnostic pop
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

TextStream& operator<<(TextStream& ts, PlatformCALayer::LayerType layerType)
{
    switch (layerType) {
    case PlatformCALayer::LayerTypeLayer:
    case PlatformCALayer::LayerTypeWebLayer:
    case PlatformCALayer::LayerTypeSimpleLayer:
        ts << "layer";
        break;
    case PlatformCALayer::LayerTypeTransformLayer:
        ts << "transform-layer";
        break;
    case PlatformCALayer::LayerTypeWebTiledLayer:
        ts << "tiled-layer";
        break;
    case PlatformCALayer::LayerTypeTiledBackingLayer:
        ts << "tiled-backing-layer";
        break;
    case PlatformCALayer::LayerTypePageTiledBackingLayer:
        ts << "page-tiled-backing-layer";
        break;
    case PlatformCALayer::LayerTypeTiledBackingTileLayer:
        ts << "tiled-backing-tile";
        break;
    case PlatformCALayer::LayerTypeRootLayer:
        ts << "root-layer";
        break;
    case PlatformCALayer::LayerTypeBackdropLayer:
        ts << "backdrop-layer";
        break;
    case PlatformCALayer::LayerTypeAVPlayerLayer:
        ts << "av-player-layer";
        break;
    case PlatformCALayer::LayerTypeWebGLLayer:
        ts << "web-gl-layer";
        break;
    case PlatformCALayer::LayerTypeShapeLayer:
        ts << "shape-layer";
        break;
    case PlatformCALayer::LayerTypeScrollingLayer:
        ts << "scrolling-layer";
        break;
    case PlatformCALayer::LayerTypeCustom:
        ts << "custom-layer";
        break;
    case PlatformCALayer::LayerTypeLightSystemBackdropLayer:
        ts << "light-system-backdrop-layer";
        break;
    case PlatformCALayer::LayerTypeDarkSystemBackdropLayer:
        ts << "dark-system-backdrop-layer";
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PlatformCALayer::FilterType filterType)
{
    switch (filterType) {
    case PlatformCALayer::Linear:
        ts << "linear";
        break;
    case PlatformCALayer::Nearest:
        ts << "nearest";
        break;
    case PlatformCALayer::Trilinear:
        ts << "trilinear";
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return ts;
}

}

#endif // USE(CA)
