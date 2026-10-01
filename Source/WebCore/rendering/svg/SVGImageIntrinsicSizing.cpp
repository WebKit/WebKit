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
#include "SVGImageElement.h"
#include "SVGImageElementSizing.h"
#include "SVGLengthContext.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore {

SVGImageIntrinsicSizing resolveSVGImageIntrinsicSizing(CachedImage& cachedImage, float usedZoom)
{
    using HasRatio = SVGImageIntrinsicSizing::HasRatio;

    RefPtr image = cachedImage.image();

    // Raster (non-SVG) sources: the intrinsic size *is* the ratio.
    if (!image || !image->isSVGImage()) {
        FloatSize size = cachedImage.clampedImageSize(ImageOrientation::Orientation::FromImage, usedZoom);
        return { size, size, size.isEmpty() ? HasRatio::No : HasRatio::Yes };
    }

    float intrinsicWidth = 0;
    float intrinsicHeight = 0;
    FloatSize ratio;
    cachedImage.computeIntrinsicDimensions(intrinsicWidth, intrinsicHeight, ratio);

    NaturalDimensions naturalDimensions;
    if (intrinsicWidth > 0)
        naturalDimensions.width = intrinsicWidth;
    if (intrinsicHeight > 0)
        naturalDimensions.height = intrinsicHeight;
    if (naturalDimensions.width && naturalDimensions.height)
        naturalDimensions.aspectRatio = FloatSize { intrinsicWidth, intrinsicHeight };
    else if (!ratio.isEmpty())
        naturalDimensions.aspectRatio = ratio;

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

} // namespace WebCore
