/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "StyleCrossfadeImage.h"

#include "AnimationUtilities.h"
#include "BitmapImage.h"
#include "CSSCrossfadeValue.h"
#include "CSSValuePool.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "DeprecatedCSSOMValue.h"
#include "Document.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "ImageSizingContext.h"
#include "LayoutSize.h"
#include "NinePieceGeometry.h"
#include "RenderElement.h"
#include "RenderObjectInlines.h"
#include "SVGImage.h"
#include "StyleCachedImage.h"
#include "StyleCrossfadeInputSizing.h"
#include "StyleImageDrawingExtras.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include <wtf/PointerComparison.h>

namespace WebCore {
namespace Style {

CrossfadeImage::CrossfadeImage(RefPtr<Image>&& from, RefPtr<Image>&& to, Progress progress, bool isPrefixed)
    : GeneratedImage { Type::CrossfadeImage }
    , m_from { WTF::move(from) }
    , m_to { WTF::move(to) }
    , m_progress { progress }
    , m_isPrefixed { isPrefixed }
    , m_inputImagesAreReady { false }
{
}

CrossfadeImage::~CrossfadeImage()
{
    if (m_cachedFromImage)
        protect(m_cachedFromImage)->removeClient(*this);
    if (m_cachedToImage)
        protect(m_cachedToImage)->removeClient(*this);
}

bool CrossfadeImage::operator==(const Image& other) const
{
    auto* otherCrossfadeImage = dynamicDowncast<CrossfadeImage>(other);
    return otherCrossfadeImage && equals(*otherCrossfadeImage);
}

bool CrossfadeImage::equals(const CrossfadeImage& other) const
{
    return equalInputImages(other)
        && m_progress == other.m_progress;
}

bool CrossfadeImage::equalInputImages(const CrossfadeImage& other) const
{
    return arePointingToEqualData(m_from, other.m_from)
        && arePointingToEqualData(m_to, other.m_to);
}

RefPtr<CrossfadeImage> CrossfadeImage::blend(const CrossfadeImage& from, const BlendingContext& context) const
{
    ASSERT(equalInputImages(from));

    if (!m_cachedToImage || !m_cachedFromImage)
        return nullptr;

    auto newProgress = Style::blend(from.m_progress, m_progress, context);
    return CrossfadeImage::create(m_from, m_to, newProgress, from.m_isPrefixed && m_isPrefixed);
}

Ref<CSSValue> CrossfadeImage::computedStyleValue(const Style::ComputedStyle& style) const
{
    auto fromComputedValue = m_from ? protect(m_from)->computedStyleValue(style) : upcast<CSSValue>(CSSKeywordValue::create(CSSValueNone));
    auto toComputedValue = m_to ? protect(m_to)->computedStyleValue(style) : upcast<CSSValue>(CSSKeywordValue::create(CSSValueNone));

    return CSSCrossfadeValue::create(
        WTF::move(fromComputedValue),
        WTF::move(toComputedValue),
        toCSS(m_progress, style),
        m_isPrefixed
    );
}

Ref<DeprecatedCSSOMValue> CrossfadeImage::computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle& style, CSSStyleDeclaration& owner) const
{
    return computedStyleValue(style)->createDeprecatedCSSOMWrapper(owner);
}

bool CrossfadeImage::isPending() const
{
    if (m_from && protect(m_from)->isPending())
        return true;
    if (m_to && protect(m_to)->isPending())
        return true;
    return false;
}

void CrossfadeImage::load(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    auto oldCachedFromImage = m_cachedFromImage;
    auto oldCachedToImage = m_cachedToImage;

    if (m_from) {
        RefPtr from = m_from;
        if (from->isPending())
            from->load(loader, options);
        m_cachedFromImage = from->cachedImage() ? from->cachedImage()->resource() : nullptr;
    } else
        m_cachedFromImage = nullptr;

    if (m_to) {
        RefPtr to = m_to;
        if (to->isPending())
            to->load(loader, options);
        m_cachedToImage = to->cachedImage() ? to->cachedImage()->resource() : nullptr;
    } else
        m_cachedToImage = nullptr;

    if (m_cachedFromImage != oldCachedFromImage) {
        if (oldCachedFromImage)
            protect(oldCachedFromImage)->removeClient(*this);
        if (m_cachedFromImage)
            protect(m_cachedFromImage)->addClient(*this);
    }

    if (m_cachedToImage != oldCachedToImage) {
        if (oldCachedToImage)
            protect(oldCachedToImage)->removeClient(*this);
        if (m_cachedToImage)
            protect(m_cachedToImage)->addClient(*this);
    }

    m_inputImagesAreReady = true;
}

static void drawCrossfadeInput(GraphicsContext& context, const RenderElement& renderer, const Image& input, CompositeOperator operation, float opacity, ConcreteObjectSize crossfadeSize, bool isForFirstLine)
{
    auto concreteSize = negotiate(input, renderer, CrossfadeInputSizing { crossfadeSize });
    auto imageSize = concreteSize.size();

    if (imageSize.isEmpty())
        return;

    bool useTransparencyLayer = input.isSVGImage();

    GraphicsContextStateSaver stateSaver(context);

    ImagePaintingOptions options;
    if (useTransparencyLayer) {
        context.setCompositeOperation(operation);
        context.beginTransparencyLayer(opacity);
    } else {
        context.setAlpha(opacity);
        options = { operation };
    }

    if (auto targetSize = crossfadeSize.size(); targetSize != imageSize)
        context.scale(targetSize / imageSize);

    input.draw(context, renderer, concreteSize, FloatRect { { }, imageSize }, FloatRect { { }, imageSize }, options, isForFirstLine);

    if (useTransparencyLayer)
        context.endTransparencyLayer();
}

void CrossfadeImage::drawCrossfade(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, bool isForFirstLine) const
{
    if (!m_from || !m_to)
        return;

    Ref from = *m_from;
    Ref to = *m_to;
    if (from->hasNothingToDraw(renderer) || to->hasNothingToDraw(renderer))
        return;

    if (concreteObjectSize.size().isEmpty())
        return;

    GraphicsContextStateSaver stateSaver(context);

    context.clip(FloatRect { { }, concreteObjectSize.size() });
    context.beginTransparencyLayer(1);

    auto progress = m_progress.value.value;
    drawCrossfadeInput(context, renderer, from, CompositeOperator::SourceOver, 1 - progress, concreteObjectSize, isForFirstLine);
    drawCrossfadeInput(context, renderer, to, CompositeOperator::PlusLighter, progress, concreteObjectSize, isForFirstLine);

    context.endTransparencyLayer();
}

ImageDrawResult CrossfadeImage::draw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options, bool isForFirstLine) const
{
    return drawIntoDestination(context, destination, source, options, [&](GraphicsContext& context) {
        drawCrossfade(context, renderer, concreteObjectSize, isForFirstLine);
        return ImageDrawResult::DidDraw;
    });
}

ImageDrawResult CrossfadeImage::drawAsPattern(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options, bool isForFirstLine) const
{
    auto imageBuffer = context.createImageBuffer(concreteObjectSize.size());
    if (!imageBuffer)
        return ImageDrawResult::DidNothing;

    drawCrossfade(imageBuffer->context(), renderer, concreteObjectSize, isForFirstLine);
    context.drawPattern(*imageBuffer, destination, tile, patternTransform, phase, spacing, options);
    return ImageDrawResult::DidDraw;
}

bool CrossfadeImage::currentFrameIsComplete() const
{
    if (m_from && !protect(m_from)->currentFrameIsComplete())
        return false;
    if (m_to && !protect(m_to)->currentFrameIsComplete())
        return false;
    return true;
}

bool CrossfadeImage::knownToBeOpaque(const RenderElement& renderer) const
{
    if (m_from && !protect(m_from)->knownToBeOpaque(renderer))
        return false;
    if (m_to && !protect(m_to)->knownToBeOpaque(renderer))
        return false;
    return true;
}

Vector<Ref<const CachedImage>, 1> CrossfadeImage::cachedImages() const
{
    Vector<Ref<const CachedImage>, 1> result;
    if (RefPtr from = m_from)
        result.appendVector(from->cachedImages());
    if (RefPtr to = m_to)
        result.appendVector(to->cachedImages());
    return result;
}

bool CrossfadeImage::isOriginClean(Document& document) const
{
    return (!m_from || protect(m_from)->isOriginClean(document)) && (!m_to || protect(m_to)->isOriginClean(document));
}

NaturalDimensions CrossfadeImage::naturalDimensions(const RenderElement& renderer, const ImageSizingContext& context) const
{
    if (!m_from || !m_to)
        return NaturalDimensions::zeroSize();

    struct Contribution {
        FloatSize size;
        float percentage;
    };
    std::array<std::optional<Contribution>, 2> contributions;

    auto collect = [&](const Image& image, float percentage) -> std::optional<Contribution> {
        auto naturalDimensions = image.naturalDimensions(renderer, context);
        if (naturalDimensions.isNone())
            return std::nullopt;
        return Contribution { context.resolve(naturalDimensions).size(), percentage };
    };

    float progress = m_progress.value.value;
    contributions[0] = collect(*protect(m_from), 1 - progress);
    contributions[1] = collect(*protect(m_to), progress);

    float total = 0;
    for (auto& contribution : contributions) {
        if (contribution)
            total += contribution->percentage;
    }

    if (!total)
        return NaturalDimensions::none();

    auto flooredToDevicePixels = [&](FloatSize size) {
        return NaturalDimensions::fixed(floorSizeToDevicePixels(LayoutSize(size), protect(renderer.document())->deviceScaleFactor()));
    };

    if (contributions[0] && contributions[1] && contributions[0]->size == contributions[1]->size)
        return flooredToDevicePixels(contributions[0]->size);

    auto weighted = FloatSize { };
    for (auto& contribution : contributions) {
        if (contribution)
            weighted = weighted + contribution->size * contribution->percentage;
    }
    return flooredToDevicePixels(weighted / total);
}

void CrossfadeImage::imageChanged(WebCore::CachedImage*, const IntRect*)
{
    if (!m_inputImagesAreReady)
        return;
    for (auto entry : clients()) {
        CheckedRef client = entry.key;
        client->imageChanged(static_cast<WrappedImagePtr>(this));
    }
}

} // namespace Style
} // namespace WebCore
