/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGImageBufferTools_h
#define SVGImageBufferTools_h

#if ENABLE(SVG)
#include "ImageBuffer.h"
#include "IntRect.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class AffineTransform;
class FloatRect;
class FloatSize;
class GraphicsContext;
class RenderObject;

class SVGImageBufferTools {
    WTF_MAKE_NONCOPYABLE(SVGImageBufferTools);

public:
    static bool createImageBuffer(const FloatRect& paintRect, const AffineTransform& absoluteTransform, OwnPtr<ImageBuffer>&, ColorSpace, RenderingMode);
    // Patterns need a different float-to-integer coordinate mapping.
    static bool createImageBufferForPattern(const FloatRect& absoluteTargetRect, const FloatRect& clampedAbsoluteTargetRect, OwnPtr<ImageBuffer>&, ColorSpace, RenderingMode);

    static void renderSubtreeToImageBuffer(ImageBuffer*, RenderObject*, const AffineTransform&);
    static void clipToImageBuffer(GraphicsContext*, const AffineTransform& absoluteTransform, const FloatRect& targetRect, OwnPtr<ImageBuffer>&);

    static void calculateTransformationToOutermostSVGCoordinateSystem(const RenderObject*, AffineTransform& absoluteTransform);
    static IntSize clampedAbsoluteSize(const IntSize&);
    static FloatRect clampedAbsoluteTargetRect(const FloatRect& absoluteTargetRect);
    static void clear2DRotation(AffineTransform&);

    static IntRect calculateImageBufferRect(const FloatRect& targetRect, const AffineTransform& absoluteTransform)
    {
        return enclosingIntRect(absoluteTransform.mapRect(targetRect));
    }

private:
    SVGImageBufferTools() { }
    ~SVGImageBufferTools() { }
};

}

#endif
#endif
