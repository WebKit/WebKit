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

#include "GraphicsContext.h"
#include "RenderObject.h"

namespace WebCore {

AffineTransform SVGImageBufferTools::absoluteTransformFromContext(GraphicsContext* context)
{
    // Extract current transformation matrix used in the original context. Note that this coordinate
    // system is flipped compared to SVGs internal coordinate system, done in WebKit level. Fix
    // this transformation by flipping the y component.
    return context->getCTM() * AffineTransform().flipY();
}

bool SVGImageBufferTools::createImageBuffer(const AffineTransform& absoluteTransform, const FloatRect& absoluteTargetRect, OwnPtr<ImageBuffer>& imageBuffer, ImageColorSpace colorSpace)
{
    IntRect imageRect = enclosingIntRect(absoluteTargetRect);
    if (imageRect.isEmpty())
        return false;

    // Allocate an image buffer as big as the absolute unclipped size of the object
    OwnPtr<ImageBuffer> image = ImageBuffer::create(imageRect.size(), colorSpace);
    if (!image)
        return false;

    GraphicsContext* imageContext = image->context();

    // Transform the mask image coordinate system to absolute screen coordinates
    imageContext->translate(-absoluteTargetRect.x(), -absoluteTargetRect.y());
    imageContext->concatCTM(absoluteTransform);

    imageBuffer = image.release();
    return true;
}

void SVGImageBufferTools::clipToImageBuffer(GraphicsContext* context, const AffineTransform& absoluteTransform, const FloatRect& absoluteTargetRect, ImageBuffer* imageBuffer)
{
    ASSERT(context);
    ASSERT(imageBuffer);

    // The mask image has been created in the device coordinate space, as the image should not be scaled.
    // So the actual masking process has to be done in the device coordinate space as well.
    context->concatCTM(absoluteTransform.inverse());
    context->clipToImageBuffer(absoluteTargetRect, imageBuffer);
    context->concatCTM(absoluteTransform);
}

}

#endif // ENABLE(SVG)
