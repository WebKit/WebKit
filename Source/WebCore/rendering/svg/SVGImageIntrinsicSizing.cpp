/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "SVGImageIntrinsicSizing.h"

#include "CachedImage.h"
#include "Image.h"
#include "RenderElement.h"
#include "SVGImageElement.h"
#include "SVGLengthContext.h"
#include "SVGPreserveAspectRatioValue.h"
#include "StyleCachedImage.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleSVGImageElementSizing.h"

namespace WebCore {

SVGImageIntrinsicSizing resolveSVGImageIntrinsicSizing(CachedImage& cachedImage, float usedZoom)
{
    using HasRatio = SVGImageIntrinsicSizing::HasRatio;

    RefPtr image = cachedImage.image();

    // Raster (non-SVG) sources: the intrinsic size *is* the ratio.
    if (!image || !image->isSVGImage()) {
        auto naturalDimensions = cachedImage.hasImage() ? image->naturalDimensions() : NaturalDimensions::none();
        auto size = naturalDimensions.width && naturalDimensions.height ? FloatSize { *naturalDimensions.width, *naturalDimensions.height } : FloatSize { };
        size.scale(usedZoom);
        return { size, size, size.isEmpty() ? HasRatio::No : HasRatio::Yes };
    }

    auto naturalDimensions = image->naturalDimensions();

    // Both intrinsic dimensions known: the ratio is their ratio, per spec (overriding any
    // viewBox-derived ratio).
    auto concreteObjectSize = Style::SVGImageElementSizing { }.resolve(naturalDimensions);

    return {
        concreteObjectSize.size(),
        naturalDimensions.aspectRatio.value_or(FloatSize { }),
        naturalDimensions.aspectRatio ? HasRatio::Yes : HasRatio::No
    };
}

FloatRect calculateSVGImageObjectBoundingBox(const SVGImageElement& imageElement, const Style::ComputedStyle& style, CachedImage* cachedImage)
{
    SVGImageIntrinsicSizing sizing;
    if (RefPtr protectedCachedImage = cachedImage)
        sizing = resolveSVGImageIntrinsicSizing(*protectedCachedImage, style.usedZoom());

    SVGLengthContext lengthContext(&imageElement);

    auto& width = style.width();
    auto& height = style.height();
    auto usedZoom = style.usedZoomForLength();
    bool hasRatio = sizing.hasRatio == SVGImageIntrinsicSizing::HasRatio::Yes;

    float concreteWidth;
    if (!width.isAuto())
        concreteWidth = lengthContext.valueForLength(width, usedZoom, SVGLengthMode::Width);
    else if (!height.isAuto() && hasRatio)
        concreteWidth = lengthContext.valueForLength(height, usedZoom, SVGLengthMode::Height) * sizing.ratio.width() / sizing.ratio.height();
    else
        concreteWidth = sizing.size.width();

    float concreteHeight;
    if (!height.isAuto())
        concreteHeight = lengthContext.valueForLength(height, usedZoom, SVGLengthMode::Height);
    else if (!width.isAuto() && hasRatio)
        concreteHeight = lengthContext.valueForLength(width, usedZoom, SVGLengthMode::Width) * sizing.ratio.height() / sizing.ratio.width();
    else
        concreteHeight = sizing.size.height();

    return { imageElement.x().value(lengthContext), imageElement.y().value(lengthContext), concreteWidth, concreteHeight };
}

SVGImageRendering fitSVGImage(const SVGPreserveAspectRatioValue& preserveAspectRatio, const FloatRect& positioningRectangle, const NaturalDimensions& naturalDimensions)
{
    auto naturalSize = [&] -> FloatSize {
        if (naturalDimensions.width && naturalDimensions.height)
            return { *naturalDimensions.width, *naturalDimensions.height };
        if (naturalDimensions.aspectRatio)
            return *naturalDimensions.aspectRatio;
        return positioningRectangle.size();
    }();

    if (naturalSize.isEmpty() || positioningRectangle.isEmpty())
        return { positioningRectangle, { { }, positioningRectangle.size() }, positioningRectangle.size() };

    auto destination = positioningRectangle;
    FloatRect source { { }, naturalSize };
    preserveAspectRatio.transformRect(destination, source);

    FloatSize scale { destination.width() / source.width(), destination.height() / source.height() };
    source.scale(scale.width(), scale.height());
    return { destination, source, { naturalSize.width() * scale.width(), naturalSize.height() * scale.height() } };
}

NaturalDimensions svgImageNaturalDimensions(const Style::Image& styleImage, const RenderElement& renderer)
{
    if (styleImage.errorOccurred()) {
        if (RefPtr cachedImage = styleImage.cachedImage()) {
            if (RefPtr image = protect(cachedImage->resource())->image())
                return image->naturalDimensions(renderer.imageOrientation());
        }
    }
    return styleImage.naturalDimensions(renderer, Style::SVGImageElementSizing { });
}

} // namespace WebCore
