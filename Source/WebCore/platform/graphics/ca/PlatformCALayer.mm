/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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
#include "IOSurface.h"
#include "LayerPool.h"
#include "PlatformCALayerClient.h"
#include "PlatformCALayerDelegatedContents.h"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <QuartzCore/CABase.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

PlatformCALayer::PlatformCALayer(LayerType layerType, PlatformCALayerClient* owner)
    : m_layerType(layerType)
    , m_layerID(PlatformLayerIdentifier::generate())
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
    constexpr auto defaultBackgroundColor = SRGBA<uint8_t> { 128, 64, 255 };
    constexpr auto acceleratedContextLabelColor = Color::red;
    constexpr auto unacceleratedContextLabelColor = Color::white;
    constexpr auto displayListBorderColor = Color::black.colorWithAlphaByte(166);
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    constexpr auto rgba16FBackgroundColor = SRGBA<uint8_t> { 255, 215, 0 };
#endif

    TextRun textRun(String::number(repaintCount));

    FontCascadeDescription fontDescription;
    fontDescription.setOneFamily("Helvetica"_s);
    fontDescription.setSpecifiedSize(fontSize);
    fontDescription.setComputedSize(fontSize);

    FontCascade cascade(WTFMove(fontDescription));
    cascade.update(nullptr);

    float textWidth = cascade.width(textRun);

    auto backgroundColor = customBackgroundColor.isValid() ? customBackgroundColor : defaultBackgroundColor;

#if ENABLE(PIXEL_FORMAT_RGBA16F)
    if (platformCALayer->contentsFormat() == ContentsFormat::RGBA16F)
        backgroundColor = rgba16FBackgroundColor;
#endif

    GraphicsContextStateSaver stateSaver(graphicsContext);

    graphicsContext.beginTransparencyLayer(0.5f);

    graphicsContext.setFillColor(backgroundColor);
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

    graphicsContext.setFillColor(platformCALayer->acceleratesDrawing() ? acceleratedContextLabelColor : unacceleratedContextLabelColor);

    graphicsContext.drawText(cascade, textRun, FloatPoint(indicatorBox.x() + horizontalMargin, indicatorBox.y() + fontSize));
    graphicsContext.endTransparencyLayer();
}

void PlatformCALayer::flipContext(CGContextRef context, CGFloat height)
{
    CGContextScaleCTM(context, 1, -1);
    CGContextTranslateCTM(context, 0, -height);
}

void PlatformCALayer::drawTextAtPoint(CGContextRef context, CGFloat x, CGFloat y, CGSize scale, CGFloat fontSize, std::span<const char8_t> text, CGFloat strokeWidthAsPercentageOfFontSize, Color strokeColor) const
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
    auto strokeCGColor = cachedCGColor(strokeColor);
    CFTypeRef values[] = {
        font.get(),
        kCFBooleanTrue,
        strokeWidthNumber.get(),
        strokeCGColor.get(),
    };

    auto attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, std::size(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto string = adoptCF(CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, byteCast<UInt8>(text.data()), text.size(), kCFStringEncodingUTF8, false, kCFAllocatorNull));
    auto attributedString = adoptCF(CFAttributedStringCreate(kCFAllocatorDefault, string.get(), attributes.get()));
    auto line = adoptCF(CTLineCreateWithAttributedString(attributedString.get()));
    CGContextSetTextPosition(context, x, y);
    CTLineDraw(line.get(), context);
}

Ref<PlatformCALayer> PlatformCALayer::createCompatibleLayerOrTakeFromPool(PlatformCALayer::LayerType layerType, PlatformCALayerClient* client, IntSize size)
{
    if (auto layerFromPool = layerPool() ? layerPool()->takeLayerWithSize(size) : nullptr) {
        layerFromPool->setOwner(client);
        return layerFromPool.releaseNonNull();
    }

    auto layer = createCompatibleLayer(layerType, client);
    layer->setBounds(FloatRect(FloatPoint(), size));
    return layer;
}

ContentsFormat PlatformCALayer::contentsFormatForLayer(PlatformCALayerClient* client)
{
    OptionSet<ContentsFormat> contentsFormats;
    if (client)
        contentsFormats = client->screenContentsFormats();
    if (contentsFormats.isEmpty())
        contentsFormats = screenContentsFormats(nullptr);

#if ENABLE(PIXEL_FORMAT_RGBA16F)
    if (client && client->drawsHDRContent() && contentsFormats.contains(ContentsFormat::RGBA16F))
        return ContentsFormat::RGBA16F;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10)
    if (contentsFormats.contains(ContentsFormat::RGBA10))
        return ContentsFormat::RGBA10;
#endif
    UNUSED_PARAM(client);
    UNUSED_PARAM(contentsFormats);
    ASSERT(contentsFormats.contains(ContentsFormat::RGBA8));
    return ContentsFormat::RGBA8;
}

void PlatformCALayer::moveToLayerPool()
{
    ASSERT(!superlayer());
    if (auto pool = layerPool())
        pool->addLayer(this);
}

LayerPool* PlatformCALayer::layerPool()
{
    static LayerPool* sharedPool = new LayerPool;
    return sharedPool;
}

void PlatformCALayer::clearContents()
{
    setContents(nullptr);
}

void PlatformCALayer::setMaskLayer(RefPtr<PlatformCALayer>&& layer)
{
    m_maskLayer = WTFMove(layer);
}

PlatformCALayer* PlatformCALayer::maskLayer() const
{
    return m_maskLayer.get();
}

void PlatformCALayer::setDelegatedContents(const PlatformCALayerDelegatedContents& contents)
{
    auto surface = WebCore::IOSurface::createFromSendRight(MachSendRight { contents.surface });
    if (!surface) {
        clearContents();
        return;
    }
    setDelegatedContents({ *surface, contents.finishedFence });
}

void PlatformCALayer::setDelegatedContents(const PlatformCALayerInProcessDelegatedContents& contents)
{
    setDelegatedContents({ contents.surface.createSendRight(), contents.finishedFence, std::nullopt });
}

bool PlatformCALayer::needsPlatformContext() const
{
    return m_owner && m_owner->platformCALayerNeedsPlatformContext(this);
}

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
void PlatformCALayer::clearAcceleratedEffectsAndBaseValues()
{
}

void PlatformCALayer::setAcceleratedEffectsAndBaseValues(const AcceleratedEffects&, const AcceleratedEffectValues&)
{
}
#endif

#if HAVE(SUPPORT_HDR_DISPLAY)
bool PlatformCALayer::setNeedsDisplayIfEDRHeadroomExceeds(float)
{
    return false;
}

void PlatformCALayer::setTonemappingEnabled(bool)
{
}

bool PlatformCALayer::tonemappingEnabled() const
{
    return false;
}
#endif

void PlatformCALayer::dumpAdditionalProperties(TextStream&, OptionSet<PlatformLayerTreeAsTextFlags>)
{
}

TextStream& operator<<(TextStream& ts, PlatformCALayer::LayerType layerType)
{
    switch (layerType) {
    case PlatformCALayer::LayerType::LayerTypeLayer:
    case PlatformCALayer::LayerType::LayerTypeWebLayer:
    case PlatformCALayer::LayerType::LayerTypeSimpleLayer:
        ts << "layer"_s;
        break;
    case PlatformCALayer::LayerType::LayerTypeTransformLayer:
        ts << "transform-layer"_s;
        break;
    case PlatformCALayer::LayerType::LayerTypeTiledBackingLayer:
        ts << "tiled-backing-layer"_s;
        break;
    case PlatformCALayer::LayerType::LayerTypePageTiledBackingLayer:
        ts << "page-tiled-backing-layer"_s;
        break;
    case PlatformCALayer::LayerType::LayerTypeTiledBackingTileLayer:
        ts << "tiled-backing-tile"_s;
        break;
    case PlatformCALayer::LayerType::LayerTypeRootLayer:
        ts << "root-layer"_s;
        break;
    case PlatformCALayer::LayerType::LayerTypeBackdropLayer:
        ts << "backdrop-layer"_s;
        break;
#if HAVE(CORE_MATERIAL)
    case PlatformCALayer::LayerType::LayerTypeMaterialLayer:
        ts << "material-layer"_s;
        break;
#endif
#if HAVE(MATERIAL_HOSTING)
    case PlatformCALayer::LayerType::LayerTypeMaterialHostingLayer:
        ts << "material-hosting-layer"_s;
        break;
#endif
    case PlatformCALayer::LayerType::LayerTypeAVPlayerLayer:
        ts << "av-player-layer"_s;
        break;
    case PlatformCALayer::LayerType::LayerTypeContentsProvidedLayer:
        ts << "contents-provided-layer"_s;
        break;
    case PlatformCALayer::LayerType::LayerTypeShapeLayer:
        ts << "shape-layer"_s;
        break;
    case PlatformCALayer::LayerType::LayerTypeScrollContainerLayer:
        ts << "scroll-container-layer"_s;
        break;
    case PlatformCALayer::LayerType::LayerTypeCustom:
        ts << "custom-layer"_s;
        break;
#if ENABLE(MODEL_ELEMENT)
    case PlatformCALayer::LayerType::LayerTypeModelLayer:
        ts << "model-layer"_s;
        break;
#endif
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    case PlatformCALayer::LayerType::LayerTypeSeparatedImageLayer:
        ts << "separated-image-layer"_s;
        break;
#endif
    case PlatformCALayer::LayerType::LayerTypeHost:
        ts << "host"_s;
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, PlatformCALayer::FilterType filterType)
{
    switch (filterType) {
    case PlatformCALayer::FilterType::Linear:
        ts << "linear"_s;
        break;
    case PlatformCALayer::FilterType::Nearest:
        ts << "nearest"_s;
        break;
    case PlatformCALayer::FilterType::Trilinear:
        ts << "trilinear"_s;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return ts;
}

PlatformCALayerDelegatedContentsFence::PlatformCALayerDelegatedContentsFence() = default;

PlatformCALayerDelegatedContentsFence::~PlatformCALayerDelegatedContentsFence() = default;

}

#endif // USE(CA)
