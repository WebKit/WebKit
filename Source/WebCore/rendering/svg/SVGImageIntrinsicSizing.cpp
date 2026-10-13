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
#include "SVGImageElementSizing.h"
#include "SVGLengthContext.h"
#include "SVGPreserveAspectRatioValue.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleImage.h"

namespace WebCore {

SVGImageIntrinsicSizing resolveSVGImageIntrinsicSizing(CachedImage& cachedImage, float usedZoom)
{
    using HasRatio = SVGImageIntrinsicSizing::HasRatio;

    RefPtr image = cachedImage.image();

    // Raster (non-SVG) sources: the intrinsic size *is* the ratio.
    if (!image || !image->isSVGImage()) {
        auto naturalDimensions = cachedImage.naturalDimensions();
        auto size = naturalDimensions.width && naturalDimensions.height ? FloatSize { *naturalDimensions.width, *naturalDimensions.height } : FloatSize { };
        size.scale(usedZoom);
        return { size, size, size.isEmpty() ? HasRatio::No : HasRatio::Yes };
    }

    auto naturalDimensions = image->naturalDimensions();

    auto concreteObjectSize = SVGImageElementSizing { }.resolve(naturalDimensions);

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

NaturalDimensions calculateSVGImageNaturalDimensions(const Style::Image& styleImage, const RenderElement& renderer)
{
    if (styleImage.errorOccurred()) {
        if (RefPtr cachedImage = styleImage.cachedImage()) {
            if (RefPtr image = cachedImage->image())
                return image->naturalDimensions(renderer.imageOrientation());
        }
    }
    return styleImage.naturalDimensions(renderer, SVGImageElementSizing { });
}

SVGImagePlacement calculateSVGImagePlacement(const Style::Image& styleImage, const RenderElement& renderer, const SVGPreserveAspectRatioValue& preserveAspectRatio, const FloatRect& positioningRectangle)
{
    // "The dimensions of the positioning rectangle ... define the specified size for the embedded object. A concrete
    // object size and final position must be determined for the object using the Default Sizing Algorithm"
    // https://svgwg.org/svg2-draft/embedded.html#Placement
    // FIXME: Apply 'object-fit' and 'object-position'.
    auto concreteObjectSize = styleImage.negotiate(renderer, SVGImageElementSizing { { positioningRectangle.width(), positioningRectangle.height() } });
    FloatRect objectRect { positioningRectangle.location(), concreteObjectSize.size() };

    // "The 'preserveAspectRatio' attribute determines how the referenced image is scaled and positioned to fit into
    // the concrete object size."
    // https://svgwg.org/svg2-draft/embedded.html#ImageElement
    auto imageRenderingRectangle = [&] -> FloatRect {
        auto naturalDimensions = calculateSVGImageNaturalDimensions(styleImage, renderer);
        auto naturalAspectRatio = naturalDimensions.width && naturalDimensions.height ? std::optional<FloatSize> { { *naturalDimensions.width, *naturalDimensions.height } } : naturalDimensions.aspectRatio;
        if (!naturalAspectRatio || naturalAspectRatio->isEmpty() || objectRect.isEmpty())
            return objectRect;

        auto transform = preserveAspectRatio.getCTM(0, 0, naturalAspectRatio->width(), naturalAspectRatio->height(), objectRect.width(), objectRect.height());
        auto rectangle = transform.mapRect(FloatRect { { }, *naturalAspectRatio });
        rectangle.moveBy(objectRect.location());
        return rectangle;
    }();

    auto destination = intersection(imageRenderingRectangle, positioningRectangle);
    return { imageRenderingRectangle, destination, { destination.location() - toFloatSize(imageRenderingRectangle.location()), destination.size() } };
}

} // namespace WebCore
