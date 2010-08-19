/*
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGImageBufferTools.h"

#include "FrameView.h"
#include "GraphicsContext.h"
#include "RenderObject.h"
#include "RenderSVGRoot.h"

namespace WebCore {

AffineTransform SVGImageBufferTools::transformationToOutermostSVGCoordinateSystem(const RenderObject* renderer)
{
    ASSERT(renderer);

    const RenderObject* current = renderer;
    ASSERT(current);

    AffineTransform ctm;
    while (current) {
        ctm.multiply(current->localToParentTransform());
        if (current->isSVGRoot())
            break;

        current = current->parent();
    }

    return ctm;
}

bool SVGImageBufferTools::createImageBuffer(const FloatRect& clampedAbsoluteTargetRect, OwnPtr<ImageBuffer>& imageBuffer, ImageColorSpace colorSpace)
{
    IntSize imageSize(roundedImageBufferSize(clampedAbsoluteTargetRect.size()));

    // Don't create empty ImageBuffers.
    if (imageSize.isEmpty())
        return false;

    OwnPtr<ImageBuffer> image = ImageBuffer::create(imageSize, colorSpace);
    if (!image)
        return false;

    imageBuffer = image.release();
    return true;
}

void SVGImageBufferTools::clipToImageBuffer(GraphicsContext* context, const AffineTransform& absoluteTransform, const FloatRect& clampedAbsoluteTargetRect, ImageBuffer* imageBuffer)
{
    ASSERT(context);
    ASSERT(imageBuffer);

    // The mask image has been created in the device coordinate space, as the image should not be scaled.
    // So the actual masking process has to be done in the device coordinate space as well.
    context->concatCTM(absoluteTransform.inverse());
    context->clipToImageBuffer(imageBuffer, clampedAbsoluteTargetRect);
    context->concatCTM(absoluteTransform);
}

IntSize SVGImageBufferTools::roundedImageBufferSize(const FloatSize& size)
{
    return IntSize(static_cast<int>(lroundf(size.width())), static_cast<int>(lroundf(size.height())));
}

FloatRect SVGImageBufferTools::clampedAbsoluteTargetRectForRenderer(const RenderObject* renderer, const AffineTransform& absoluteTransform, const FloatRect& targetRect)
{
    ASSERT(renderer);

    const RenderSVGRoot* svgRoot = SVGRenderSupport::findTreeRootObject(renderer);
    FloatRect clampedAbsoluteTargetRect = absoluteTransform.mapRect(targetRect);
    clampedAbsoluteTargetRect.intersect(svgRoot->contentBoxRect());
    return clampedAbsoluteTargetRect;
}

}

#endif // ENABLE(SVG)
