/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "NativeImage.h"

#include "Color.h"
#include "FloatRect.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "IntSize.h"
#include "NotImplemented.h"
#include <d2d1.h>

namespace WebCore {

IntSize nativeImageSize(const NativeImagePtr& image)
{
    return image ? IntSize(image->GetSize()) : IntSize();
}

bool nativeImageHasAlpha(const NativeImagePtr& image)
{
    if (!image)
        return false;

    auto alphaMode = image->GetPixelFormat().alphaMode;
    return (alphaMode >= D2D1_ALPHA_MODE_PREMULTIPLIED) && (alphaMode <= D2D1_ALPHA_MODE_STRAIGHT);
}

Color nativeImageSinglePixelSolidColor(const NativeImagePtr& image)
{
    if (!image || nativeImageSize(image) != IntSize(1, 1))
        return Color();

    notImplemented();

    return Color();
}

void drawNativeImage(const NativeImagePtr& image, GraphicsContext& context, const FloatRect& destRect, const FloatRect& srcRect, const IntSize& srcSize, CompositeOperator compositeOp, BlendMode blendMode, const ImageOrientation& orientation)
{
    auto platformContext = context.platformContext();

    // Subsampling may have given us an image that is smaller than size().
    IntSize subsampledImageSize = nativeImageSize(image);

    // srcRect is in the coordinates of the unsubsampled image, so we have to map it to the subsampled image.
    FloatRect adjustedSrcRect = srcRect;
    if (subsampledImageSize != srcSize)
        adjustedSrcRect = mapRect(srcRect, FloatRect({ }, srcSize), FloatRect({ }, subsampledImageSize));

    float opacity = 1.0f;

    platformContext->DrawBitmap(image.get(), destRect, opacity, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, adjustedSrcRect);
    context.flush();
}

void clearNativeImageSubimages(const NativeImagePtr& image)
{
    notImplemented();

#if CACHE_SUBIMAGES
    if (image)
        subimageCache().clearImage(image.get());
#endif
}


}
