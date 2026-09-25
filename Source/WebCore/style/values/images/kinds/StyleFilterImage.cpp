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

#include "CSSFilterImageValue.h"
#include "CSSFilterRenderer.h"
#include "CSSValuePool.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "DeprecatedCSSOMValue.h"
#include "GraphicsContext.h"
#include "HostWindow.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include "NinePieceGeometry.h"
#include "NullGraphicsContext.h"
#include "RenderElement.h"
#include "RenderObjectInlines.h"
#include "Settings.h"
#include "StyleCachedImage.h"
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
        image ? image->computedStyleValue(style) : upcast<CSSValue>(CSSKeywordValue::create(CSSValueNone)),
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
        m_cachedImage = image->cachedImage() ? image->cachedImage()->resource() : nullptr;
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

RefPtr<NativeImage> FilterImage::filteredNativeImage(const RenderElement& renderElement, FloatSize size, const GraphicsContext& destinationContext, bool isForFirstLine) const
{
    CheckedRef renderer = renderElement;

    if (size.isEmpty())
        return nullptr;

    RefPtr styleImage = m_image;
    if (!styleImage || styleImage->hasNothingToDraw(renderer))
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
        styleImage->draw(context, renderer, ConcreteObjectSize::fixed(size), sourceImageRect, FloatRect { { }, size }, { }, isForFirstLine);
    });
    return filteredImage;
}

static ImagePaintingOptions drawingOptions(ImagePaintingOptions options)
{
    return { options, WebCore::ImageOrientation::Orientation::None };
}

ImageDrawResult FilterImage::draw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options, bool isForFirstLine) const
{
    RefPtr nativeImage = filteredNativeImage(renderer, concreteObjectSize.size(), context, isForFirstLine);
    if (!nativeImage)
        return ImageDrawResult::DidNothing;

    context.drawNativeImage(*nativeImage, destination, source, drawingOptions(options));
    return ImageDrawResult::DidDraw;
}

ImageDrawResult FilterImage::drawAsPattern(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options, bool isForFirstLine) const
{
    RefPtr nativeImage = filteredNativeImage(renderer, concreteObjectSize.size(), context, isForFirstLine);
    if (!nativeImage)
        return ImageDrawResult::DidNothing;

    context.drawPattern(*nativeImage, destination, tile, patternTransform, phase, spacing, drawingOptions(options));
    return ImageDrawResult::DidDraw;
}

ImageDrawResult FilterImage::drawTiled(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImagePaintingOptions options, bool isForFirstLine) const
{
    if (tileSize.isEmpty())
        return ImageDrawResult::DidNothing;

    RefPtr nativeImage = filteredNativeImage(renderer, tileSize, context, isForFirstLine);
    if (!nativeImage)
        return ImageDrawResult::DidNothing;

    auto drawTile = [&](GraphicsContext& context, ConcreteObjectSize, const FloatRect& destination, const FloatRect& source) {
        context.drawNativeImage(*nativeImage, destination, source, drawingOptions(options));
        return ImageDrawResult::DidDraw;
    };

    auto drawTilePattern = [&](GraphicsContext& context, ConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing) {
        context.drawPattern(*nativeImage, destination, tile, patternTransform, phase, spacing, drawingOptions(options));
        return ImageDrawResult::DidDraw;
    };

    return drawTiledUsing(context, NaturalDimensions::fixed(nativeImage->size()), drawTile, drawTilePattern, ConcreteObjectSize::fixed(tileSize), destination, phase, tileSize, spacing, options);
}

ImageDrawResult FilterImage::drawNinePiece(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const NinePieceGeometry& geometry, ImagePaintingOptions options) const
{
    RefPtr nativeImage = filteredNativeImage(renderer, concreteObjectSize.size(), context, false);
    if (!nativeImage)
        return ImageDrawResult::DidNothing;

    auto result = ImageDrawResult::DidNothing;
    auto record = [&] {
        result = ImageDrawResult::DidDraw;
    };

    for (auto piece : allImagePieces) {
        if (geometry.shouldSkipPiece(piece))
            continue;

        if (isCornerPiece(piece)) {
            context.drawNativeImage(*nativeImage, geometry.destinationRects[piece], geometry.sourceRects[piece], drawingOptions(options));
            record();
            continue;
        }

        auto horizontalRule = isHorizontalPiece(piece)
            ? static_cast<WebCore::Image::TileRule>(geometry.horizontalRule)
            : WebCore::Image::StretchTile;

        auto verticalRule = isVerticalPiece(piece)
            ? static_cast<WebCore::Image::TileRule>(geometry.verticalRule)
            : WebCore::Image::StretchTile;

        auto drawTilePattern = [&](GraphicsContext& context, ConcreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing) {
            context.drawPattern(*nativeImage, destination, tile, patternTransform, phase, spacing, drawingOptions(options));
            return ImageDrawResult::DidDraw;
        };

        if (drawTiledUsing(context, drawTilePattern, concreteObjectSize, geometry.destinationRects[piece], geometry.sourceRects[piece], geometry.tileScales[piece], horizontalRule, verticalRule) == ImageDrawResult::DidDraw)
            record();
    }

    return result;
}

bool FilterImage::knownToBeOpaque(const RenderElement&) const
{
    return false;
}

Vector<Ref<const CachedImage>, 1> FilterImage::cachedImages() const
{
    if (RefPtr image = m_image)
        return image->cachedImages();
    return { };
}

bool FilterImage::isOriginClean(Document& document) const
{
    return !m_image || protect(m_image)->isOriginClean(document);
}

NaturalDimensions FilterImage::naturalDimensions(const RenderElement& renderer, const ImageSizingContext& context) const
{
    if (RefPtr image = m_image)
        return image->naturalDimensions(renderer, context);

    return NaturalDimensions::zeroSize();
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
