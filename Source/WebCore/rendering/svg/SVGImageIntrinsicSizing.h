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

#pragma once

#include "FloatRect.h"
#include "NaturalDimensions.h"

namespace WebCore {

class CachedImage;
class RenderElement;
class SVGImageElement;
class SVGPreserveAspectRatioValue;

namespace Style {
class ComputedStyle;
class Image;
}

struct SVGImageIntrinsicSizing {
    enum class HasRatio : bool { No, Yes };

    FloatSize size;
    FloatSize ratio;
    HasRatio hasRatio { HasRatio::No };
};

// CSS Images Level 3 default sizing algorithm for <svg:image> sources.
// https://www.w3.org/TR/css-images-3/#default-sizing-algorithm
//
// HasRatio distinguishes a real intrinsic aspect ratio from the 300x150 fallback: callers must not
// resolve 'auto' dimensions against a fallback ratio, or they get the wrong size.
SVGImageIntrinsicSizing resolveSVGImageIntrinsicSizing(CachedImage&, float usedZoom);

// Resolves the <svg:image> object bounding box from the CSS 'width' / 'height' computed values and
// the source's intrinsic sizing, per SVG 2 §12.2. Shared by the LBSE and legacy SVG renderers.
FloatRect calculateSVGImageObjectBoundingBox(const SVGImageElement&, const Style::ComputedStyle&, CachedImage*);

struct SVGImageRendering {
    FloatRect destination;
    FloatRect source;
    FloatSize imageRenderingSize;
};

SVGImageRendering fitSVGImage(const SVGPreserveAspectRatioValue&, const FloatRect& positioningRectangle, const NaturalDimensions&);
NaturalDimensions svgImageNaturalDimensions(const Style::Image&, const RenderElement&);

} // namespace WebCore
