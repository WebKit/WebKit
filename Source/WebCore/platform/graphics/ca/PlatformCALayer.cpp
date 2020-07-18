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

#if USE(CA)

#include "GraphicsContextCG.h"
#include "LayerPool.h"
#include "PlatformCALayerClient.h"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <QuartzCore/CABase.h>
#include <wtf/text/TextStream.h>

#if PLATFORM(WIN)
#include <pal/spi/win/CoreTextSPIWin.h>
#endif

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

CFTimeInterval PlatformCALayer::currentTimeToMediaTime(MonotonicTime t)
{
    return CACurrentMediaTime() + (t - MonotonicTime::now()).seconds();
}

bool PlatformCALayer::canHaveBackingStore() const
{
    return m_layerType == LayerType::LayerTypeWebLayer
        || m_layerType == LayerType::LayerTypeTiledBackingLayer
        || m_layerType == LayerType::LayerTypePageTiledBackingLayer
        || m_layerType == LayerType::LayerTypeTiledBackingTileLayer;
}

void PlatformCALayer::drawRepaintIndicator(GraphicsContext& graphicsContext, PlatformCALayer* platformCALayer, int repaintCount, Color customBackgroundColor)
{
    const float verticalMargin = 2.5;
    const float horizontalMargin = 5;
    const unsigned fontSize = 22;
    constexpr auto backgroundColor = SRGBA<uint8_t> { 128, 64, 255 };
    constexpr auto acceleratedContextLabelColor = Color::red;
    constexpr auto unacceleratedContextLabelColor = Color::white;
    constexpr auto linearGlyphMaskOutlineColor = Color::black.colorWithAlpha(192);
    constexpr auto displayListBorderColor = Color::black.colorWithAlpha(166);

    TextRun textRun(String::number(repaintCount));

    FontCascadeDescription fontDescription;
    fontDescription.setOneFamily("Helvetica");
    fontDescription.setSpecifiedSize(fontSize);
    fontDescription.setComputedSize(fontSize);

    FontCascade cascade(WTFMove(fontDescription));
    cascade.update(nullptr);

    float textWidth = cascade.width(textRun);

    GraphicsContextStateSaver stateSaver(graphicsContext);

    graphicsContext.beginTransparencyLayer(0.5f);

    graphicsContext.setFillColor(customBackgroundColor.isValid() ? customBackgroundColor : backgroundColor);
    FloatRect indicatorBox(1, 1, horizontalMargin * 2 + textWidth, verticalMargin * 2 + fontSize);
    if (platformCALayer->isOpaque())
        graphicsContext.fillRect(indicatorBox);
    else {
        Path boundsPath;
        boundsPath.moveTo(indicatorBox.maxXMinYCorner());
        boundsPath.addLineTo(indicatorBox.maxXMaxYCorner());
        boundsPath.addLineTo(indicatorBox.minXMaxYCorner());

        const float cornerChunk = 8;
        boundsPath.addLineTo(FloatPoint(indicatorBox.x(), indicatorBox.y() + cornerChunk));
        boundsPath.addLineTo(FloatPoint(indicatorBox.x() + cornerChunk, indicatorBox.y()));
        boundsPath.closeSubpath();

        graphicsContext.fillPath(boundsPath);
    }

    if (platformCALayer->owner()->isUsingDisplayListDrawing(platformCALayer)) {
        graphicsContext.setStrokeColor(displayListBorderColor);
        graphicsContext.strokeRect(indicatorBox, 2);
    }

    if (!platformCALayer->isOpaque() && platformCALayer->supportsSubpixelAntialiasedText() && platformCALayer->acceleratesDrawing()) {
        graphicsContext.setStrokeColor(linearGlyphMaskOutlineColor);
        graphicsContext.setStrokeThickness(4.5);
        graphicsContext.setTextDrawingMode(TextDrawingModeFlags { TextDrawingMode::Fill, TextDrawingMode::Stroke });
    }

    graphicsContext.setFillColor(platformCALayer->acceleratesDrawing() ? acceleratedContextLabelColor : unacceleratedContextLabelColor);

    graphicsContext.drawText(cascade, textRun, FloatPoint(indicatorBox.x() + horizontalMargin, indicatorBox.y() + fontSize));
    graphicsContext.endTransparencyLayer();
}

void PlatformCALayer::flipContext(CGContextRef context, CGFloat height)
{
    CGContextScaleCTM(context, 1, -1);
    CGContextTranslateCTM(context, 0, -height);
}

void PlatformCALayer::drawTextAtPoint(CGContextRef context, CGFloat x, CGFloat y, CGSize scale, CGFloat fontSize, const char* text, size_t length, CGFloat strokeWidthAsPercentageOfFontSize, Color strokeColor) const
{
    auto matrix = CGAffineTransformMakeScale(scale.width, scale.height);
    auto font = adoptCF(CTFontCreateWithName(CFSTR("Helvetica"), fontSize, &matrix));
    auto strokeWidthNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &strokeWidthAsPercentageOfFontSize));

    CFTypeRef keys[] = {
        kCTFontAttributeName,
        kCTForegroundColorFromContextAttributeName,
        kCTStrokeWidthAttributeName,
        kCTStrokeColorAttributeName,
    };
    CFTypeRef values[] = {
        font.get(),
        kCFBooleanTrue,
        strokeWidthNumber.get(),
        cachedCGColor(strokeColor),
    };

    auto attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto string = adoptCF(CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(text), length, kCFStringEncodingUTF8, false, kCFAllocatorNull));
    auto attributedString = adoptCF(CFAttributedStringCreate(kCFAllocatorDefault, string.get(), attributes.get()));
    auto line = adoptCF(CTLineCreateWithAttributedString(attributedString.get()));
    CGContextSetTextPosition(context, x, y);
    CTLineDraw(line.get(), context);
}

Ref<PlatformCALayer> PlatformCALayer::createCompatibleLayerOrTakeFromPool(PlatformCALayer::LayerType layerType, PlatformCALayerClient* client, IntSize size)
{
    if (auto layerFromPool = layerPool().takeLayerWithSize(size)) {
        layerFromPool->setOwner(client);
        return layerFromPool.releaseNonNull();
    }

    auto layer = createCompatibleLayer(layerType, client);
    layer->setBounds(FloatRect(FloatPoint(), size));
    return layer;
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
    case PlatformCALayer::LayerTypeEditableImageLayer:
        ts << "editable-image";
        break;
    case PlatformCALayer::LayerTypeBackdropLayer:
        ts << "backdrop-layer";
        break;
    case PlatformCALayer::LayerTypeAVPlayerLayer:
        ts << "av-player-layer";
        break;
    case PlatformCALayer::LayerTypeContentsProvidedLayer:
        ts << "contents-provided-layer";
        break;
    case PlatformCALayer::LayerTypeShapeLayer:
        ts << "shape-layer";
        break;
    case PlatformCALayer::LayerTypeScrollContainerLayer:
        ts << "scroll-container-layer";
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
