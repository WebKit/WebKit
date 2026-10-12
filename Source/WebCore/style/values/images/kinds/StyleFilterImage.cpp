/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
#include "StyleFilterImage.h"

#include "BitmapImage.h"
#include "CSSFilterImageValue.h"
#include "CSSFilterRenderer.h"
#include "CSSValuePool.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "DeprecatedCSSOMValue.h"
#include "HostWindow.h"
#include "ImageBuffer.h"
#include "ImageQualityController.h"
#include "NullGraphicsContext.h"
#include "RenderBoxModelObject.h"
#include "RenderElement.h"
#include "RenderObjectInlines.h"
#include "Settings.h"
#include "StyleFilter.h"
#include <wtf/PointerComparison.h>

namespace WebCore {
namespace Style {

FilterImage::FilterImage(RefPtr<Image>&& image, Filter&& filter)
    : GeneratedImage { Type::FilterImage }
    , m_image { WTF::move(image) }
    , m_filter { WTF::move(filter) }
    , m_inputImageIsReady { false }
{
}

FilterImage::~FilterImage()
{
    if (RefPtr cachedImage = m_cachedImage)
        cachedImage->removeClient(*this);
}

bool FilterImage::operator==(const Image& other) const
{
    auto* otherFilterImage = dynamicDowncast<FilterImage>(other);
    return otherFilterImage && equals(*otherFilterImage);
}

bool FilterImage::equals(const FilterImage& other) const
{
    return equalInputImages(other) && m_filter == other.m_filter;
}

bool FilterImage::equalInputImages(const FilterImage& other) const
{
    return arePointingToEqualData(m_image, other.m_image);
}

Ref<CSSValue> FilterImage::computedStyleValue(const Style::ComputedStyle& style) const
{
    RefPtr image = m_image;
    return CSSFilterImageValue::create(
        image ? image->computedStyleValue(style) : upcast<CSSValue>(CSSKeywordValue::create(CSSValueID::None)),
        toCSS(m_filter, style)
    );
}

Ref<DeprecatedCSSOMValue> FilterImage::computedStyleDeprecatedCSSOMValue(CSSValuePool&, const Style::ComputedStyle& style, CSSStyleDeclaration& owner) const
{
    return computedStyleValue(style)->createDeprecatedCSSOMWrapper(owner);
}

bool FilterImage::isPending() const
{
    RefPtr image = m_image;
    return image && image->isPending();
}

void FilterImage::load(CachedResourceLoader& cachedResourceLoader, const ResourceLoaderOptions& options)
{
    RefPtr oldCachedImage = m_cachedImage;

    if (RefPtr image = m_image) {
        image->load(cachedResourceLoader, options);
        m_cachedImage = image->cachedImage();
    } else
        m_cachedImage = nullptr;

    if (m_cachedImage != oldCachedImage) {
        if (oldCachedImage)
            oldCachedImage->removeClient(*this);
        if (RefPtr cachedImage = m_cachedImage)
            cachedImage->addClient(*this);
    }

    for (auto& value : m_filter) {
        WTF::switchOn(value,
            [&](FilterReference& filterReference) {
                filterReference.loadExternalDocumentIfNeeded(cachedResourceLoader, options);
            },
            []<CSSValueID C, typename T>(FunctionNotation<C, T>&) { }
        );
    }

    m_inputImageIsReady = true;
}

RefPtr<BitmapImage> FilterImage::resolvedImage(const RenderElement& renderElement, const FloatSize& size, const GraphicsContext& destinationContext, bool isForFirstLine) const
{
    CheckedRef renderer = renderElement;

    if (size.isEmpty())
        return nullptr;

    RefPtr styleImage = m_image;
    if (!styleImage || !styleImage->canDrawAtSize(renderer, size))
        return nullptr;

    auto preferredFilterRenderingModes = protect(renderer->page())->preferredFilterRenderingModes(destinationContext);
    auto sourceImageRect = FloatRect { { }, size };

    auto renderingOptions(protect(renderer->settings())->showDebugBorders() ? std::make_optional(FilterRenderingOption::ShowDebugOverlay) : std::nullopt);
    auto cssFilter = CSSFilterRenderer::create(const_cast<RenderElement&>(renderer.get()), m_filter, {
            .referenceBox = sourceImageRect,
            .filterRegion = sourceImageRect,
            .scale = { 1, 1 },
        }, preferredFilterRenderingModes, renderingOptions, NullGraphicsContext());
    if (!cssFilter)
        return nullptr;

    cssFilter->setFilterRegion(sourceImageRect);

    auto sourceImage = ImageBuffer::create(size, destinationContext.renderingMode(), RenderingPurpose::DOM, 1, ColorSpace::SRGB(), PixelFormat::BGRA8, renderer->hostWindow());
    if (!sourceImage)
        return nullptr;

    auto filteredImage = sourceImage->filteredNativeImage(*cssFilter, [&](GraphicsContext& context) {
        styleImage->draw(context, renderer, ConcreteObjectSize::fixed(size), sourceImageRect, sourceImageRect, { }, isForFirstLine);
    });
    if (!filteredImage)
        return nullptr;

    return BitmapImage::create(WTF::move(filteredImage));
}

ImageDrawResult FilterImage::draw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options, bool isForFirstLine) const
{
    if (isPending())
        return ImageDrawResult::DidNothing;

    RefPtr image = resolvedImage(renderer, flooredIntSize(destination.size()), context, isForFirstLine);
    if (!image)
        return ImageDrawResult::DidNothing;

    return drawResolved(context, renderer, *image, ConcreteObjectSize::fixed(image->size()), destination, mapSourceToSize(source, concreteObjectSize, image->size(options.orientation())), options);
}

ImageDrawResult FilterImage::drawAsPattern(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options, bool isForFirstLine) const
{
    RefPtr image = resolvedImage(renderer, concreteObjectSize.size() * concreteObjectSize.zoom(), context, isForFirstLine);
    if (!image || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    return drawResolvedAsPattern(context, renderer, *image, ConcreteObjectSize::fixed(image->size()), destination, tile, patternTransform, phase, spacing, options);
}

ImageDrawResult FilterImage::drawTiled(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImagePaintingOptions options, bool isForFirstLine) const
{
    RefPtr image = resolvedImage(renderer, tileSize, context, isForFirstLine);
    if (!image || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    return drawResolvedTiled(context, renderer, *image, NaturalDimensions::fixed(image->size()), ConcreteObjectSize::fixed(image->size()), destination, phase, tileSize, spacing, options);
}

ImageDrawResult FilterImage::drawNinePiece(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const NinePieceGeometry& geometry, ImagePaintingOptions options) const
{
    RefPtr image = resolvedImage(renderer, concreteObjectSize.size() * concreteObjectSize.zoom(), context, false);
    if (!image || context.paintingDisabled())
        return ImageDrawResult::DidNothing;

    return drawResolvedNinePiece(context, renderer, *image, ConcreteObjectSize::fixed(image->size()), geometry, options);
}

bool FilterImage::knownToBeOpaque(const RenderElement&) const
{
    return false;
}

bool FilterImage::canDrawAtSize(const RenderElement& renderer, const FloatSize& size) const
{
    return !size.isEmpty() && m_image && protect(m_image)->canDrawAtSize(renderer, size);
}

DecodingMode FilterImage::decodingModeForImageDraw(const RenderBoxModelObject& renderer, const PaintInfo& paintInfo) const
{
    if (!m_image)
        return Image::decodingModeForImageDraw(renderer, paintInfo);
    return protect(m_image)->decodingModeForImageDraw(renderer, paintInfo);
}

InterpolationQuality FilterImage::interpolationQualityForImageDraw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize, const void* layer, const LayoutSize& size) const
{
    return ImageQualityController::chooseInterpolationQualityForBitmapOfSize(context, renderer, calculateImageBufferBackendSize(size, 1), layer, size);
}

bool FilterImage::containsCurrentColor() const
{
    return (m_image && protect(m_image)->containsCurrentColor())
        || m_filter.hasFilterThatRequiresRepaintForCurrentColorChange();
}

NaturalDimensions FilterImage::naturalDimensions(const RenderElement& renderer, const ImageSizingContext& context) const
{
    if (RefPtr image = m_image)
        return image->naturalDimensions(renderer, context);
    return NaturalDimensions::zero();
}

void FilterImage::imageChanged(WebCore::CachedImage*, const IntRect*)
{
    if (!m_inputImageIsReady)
        return;

    for (auto entry : clients()) {
        CheckedRef client = entry.key;
        client->imageChanged(static_cast<WrappedImagePtr>(this));
    }
}

} // namespace Style
} // namespace WebCore
