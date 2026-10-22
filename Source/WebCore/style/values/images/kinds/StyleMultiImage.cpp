/*
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Noam Rosenthal (noam@webkit.org)
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "StyleMultiImage.h"

#include "CSSCanvasValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSFilterImageValue.h"
#include "CSSGradientValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSNamedImageValue.h"
#include "CSSPaintImageValue.h"
#include "CSSVariableData.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "RenderElement.h"
#include "RenderView.h"
#include "StyleCachedImage.h"
#include "StyleCanvasImage.h"
#include "StyleCrossfadeImage.h"
#include "StyleFilterImage.h"
#include "StyleGradientImage.h"
#include "StyleNamedImage.h"
#include "StylePaintImage.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MultiImage);

MultiImage::MultiImage(Type type)
    : Image { type }
{
}

MultiImage::~MultiImage() = default;

bool MultiImage::equals(const MultiImage& other) const
{
    return !m_isPending && !other.m_isPending && arePointingToEqualData(m_selectedImage, other.m_selectedImage);
}

void MultiImage::load(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    ASSERT(m_isPending);
    ASSERT(loader.document());

    m_isPending = false;

    auto bestFitImage = selectBestFitImage(protect(*loader.document()));

    ASSERT(is<CachedImage>(bestFitImage.image) || is<GeneratedImage>(bestFitImage.image));

    if (is<GeneratedImage>(bestFitImage.image)) {
        m_selectedImage = bestFitImage.image;
        protect(m_selectedImage)->load(loader, options);
        return;
    }

    if (RefPtr styleCachedImage = dynamicDowncast<CachedImage>(bestFitImage.image)) {
        if (styleCachedImage->imageScaleFactor() == bestFitImage.scaleFactor.value)
            m_selectedImage = WTF::move(styleCachedImage);
        else
            m_selectedImage = CachedImage::copyOverridingScaleFactor(*styleCachedImage, bestFitImage.scaleFactor.value);

        if (protect(m_selectedImage)->isPending())
            protect(m_selectedImage)->load(loader, options);
        return;
    }
}

WebCore::CachedImage* MultiImage::cachedImage() const
{
    if (!m_selectedImage)
        return nullptr;
    return protect(m_selectedImage)->cachedImage();
}

WrappedImagePtr MultiImage::data() const
{
    if (!m_selectedImage)
        return nullptr;
    return protect(m_selectedImage)->data();
}

bool MultiImage::canRender(const RenderElement* renderer) const
{
    return m_selectedImage && protect(m_selectedImage)->canRender(renderer);
}

bool MultiImage::isLoaded(const RenderElement* renderer) const
{
    return m_selectedImage && protect(m_selectedImage)->isLoaded(renderer);
}

bool MultiImage::errorOccurred() const
{
    return m_selectedImage && protect(m_selectedImage)->errorOccurred();
}

bool MultiImage::hasDecodedImage() const
{
    return m_selectedImage && protect(m_selectedImage)->hasDecodedImage();
}

NaturalDimensions MultiImage::naturalDimensions(const RenderElement& renderer, const ImageSizingContext& context) const
{
    if (!m_selectedImage)
        return NaturalDimensions::none();
    return protect(m_selectedImage)->naturalDimensions(renderer, context);
}

ImageDrawingExtras MultiImage::drawingExtrasForRenderer(const RenderElement& renderer) const
{
    if (!m_selectedImage)
        return { };
    return protect(m_selectedImage)->drawingExtrasForRenderer(renderer);
}

void MultiImage::addClient(RenderElement& renderer)
{
    if (!m_selectedImage)
        return;
    protect(m_selectedImage)->addClient(renderer);
}

void MultiImage::removeClient(RenderElement& renderer)
{
    if (!m_selectedImage)
        return;
    protect(m_selectedImage)->removeClient(renderer);
}

bool MultiImage::hasClient(RenderElement& renderer) const
{
    if (!m_selectedImage)
        return false;
    return protect(m_selectedImage)->hasClient(renderer);
}

ImageDrawResult MultiImage::draw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& source, ImageDrawingOptions options, bool isForFirstLine) const
{
    if (!m_selectedImage)
        return ImageDrawResult::DidNothing;
    return protect(m_selectedImage)->draw(context, renderer, concreteObjectSize, destination, source, options, isForFirstLine);
}

ImageDrawResult MultiImage::drawAsPattern(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatRect& tile, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImageDrawingOptions options, bool isForFirstLine) const
{
    if (!m_selectedImage)
        return ImageDrawResult::DidNothing;
    return protect(m_selectedImage)->drawAsPattern(context, renderer, concreteObjectSize, destination, tile, patternTransform, phase, spacing, options, isForFirstLine);
}

ImageDrawResult MultiImage::drawTiled(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImageDrawingOptions options, bool isForFirstLine) const
{
    if (!m_selectedImage)
        return ImageDrawResult::DidNothing;
    return protect(m_selectedImage)->drawTiled(context, renderer, concreteObjectSize, destination, phase, tileSize, spacing, options, isForFirstLine);
}

ImageDrawResult MultiImage::drawNinePiece(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const NinePieceGeometry& geometry, ImageDrawingOptions options) const
{
    if (!m_selectedImage)
        return ImageDrawResult::DidNothing;
    return protect(m_selectedImage)->drawNinePiece(context, renderer, concreteObjectSize, geometry, options);
}

bool MultiImage::currentFrameIsComplete(const RenderElement* renderer) const
{
    return m_selectedImage && protect(m_selectedImage)->currentFrameIsComplete(renderer);
}

float MultiImage::imageScaleFactor() const
{
    if (!m_selectedImage)
        return 1;
    return protect(m_selectedImage)->imageScaleFactor();
}

bool MultiImage::knownToBeOpaque(const RenderElement& renderer) const
{
    return m_selectedImage && protect(m_selectedImage)->knownToBeOpaque(renderer);
}

bool MultiImage::canDraw(const RenderElement& renderer) const
{
    return m_selectedImage && protect(m_selectedImage)->canDraw(renderer);
}

bool MultiImage::canDrawAtSize(const RenderElement& renderer, const FloatSize& size) const
{
    return m_selectedImage && protect(m_selectedImage)->canDrawAtSize(renderer, size);
}

RefPtr<NativeImage> MultiImage::nativeImage(const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, float resolutionScale, ImageDrawingOptions options) const
{
    return m_selectedImage ? protect(m_selectedImage)->nativeImage(renderer, concreteObjectSize, resolutionScale, options) : nullptr;
}

std::optional<IntPoint> MultiImage::hotSpot() const
{
    return m_selectedImage ? protect(m_selectedImage)->hotSpot() : std::nullopt;
}

bool MultiImage::drawsSVGImage() const
{
    return m_selectedImage && protect(m_selectedImage)->drawsSVGImage();
}

WTF::String MultiImage::accessibilityDescription() const
{
    if (!m_selectedImage)
        return { };
    return protect(m_selectedImage)->accessibilityDescription();
}

bool MultiImage::isAnimated() const
{
    return m_selectedImage && protect(m_selectedImage)->isAnimated();
}

void MultiImage::stopAnimation()
{
    if (m_selectedImage)
        protect(m_selectedImage)->stopAnimation();
}

void MultiImage::resetAnimation()
{
    if (m_selectedImage)
        protect(m_selectedImage)->resetAnimation();
}

DecodingMode MultiImage::decodingModeForImageDraw(const RenderBoxModelObject& renderer, const PaintInfo& paintInfo) const
{
    if (!m_selectedImage)
        return Image::decodingModeForImageDraw(renderer, paintInfo);
    return protect(m_selectedImage)->decodingModeForImageDraw(renderer, paintInfo);
}

InterpolationQuality MultiImage::interpolationQualityForImageDraw(GraphicsContext& context, const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const void* layer, const LayoutSize& size) const
{
    if (!m_selectedImage)
        return Image::interpolationQualityForImageDraw(context, renderer, concreteObjectSize, layer, size);
    return protect(m_selectedImage)->interpolationQualityForImageDraw(context, renderer, concreteObjectSize, layer, size);
}

FloatRect MultiImage::mapChangedRectToSource(const RenderElement& renderer, ConcreteObjectSize concreteObjectSize, const FloatRect& changedRect) const
{
    if (!m_selectedImage)
        return Image::mapChangedRectToSource(renderer, concreteObjectSize, changedRect);
    return protect(m_selectedImage)->mapChangedRectToSource(renderer, concreteObjectSize, changedRect);
}

bool MultiImage::containsCurrentColor() const
{
    return m_selectedImage && protect(m_selectedImage)->containsCurrentColor();
}

} // namespace Style
} // namespace WebCore
