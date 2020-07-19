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

#if USE(CG)

#include "Color.h"
#include "FloatRect.h"
#include "GeometryUtilities.h"
#include "GraphicsContextCG.h"
#include "IntSize.h"
#include "SubimageCacheWithTimer.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>

namespace WebCore {

IntSize nativeImageSize(const NativeImagePtr& image)
{
    return image ? IntSize(CGImageGetWidth(image.get()), CGImageGetHeight(image.get())) : IntSize();
}

bool nativeImageHasAlpha(const NativeImagePtr& image)
{
    CGImageAlphaInfo info = CGImageGetAlphaInfo(image.get());
    return (info >= kCGImageAlphaPremultipliedLast) && (info <= kCGImageAlphaFirst);
}

Color nativeImageSinglePixelSolidColor(const NativeImagePtr& image)
{
    if (!image || nativeImageSize(image) != IntSize(1, 1))
        return Color();

    unsigned char pixel[4]; // RGBA
    auto bitmapContext = adoptCF(CGBitmapContextCreate(pixel, 1, 1, 8, sizeof(pixel), sRGBColorSpaceRef(), kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big));

    if (!bitmapContext)
        return Color();

    CGContextSetBlendMode(bitmapContext.get(), kCGBlendModeCopy);
    CGContextDrawImage(bitmapContext.get(), CGRectMake(0, 0, 1, 1), image.get());

    if (!pixel[3])
        return Color::transparentBlack;

    return clampToComponentBytes<SRGBA>(pixel[0] * 255 / pixel[3], pixel[1] * 255 / pixel[3], pixel[2] * 255 / pixel[3], pixel[3]);
}

void drawNativeImage(const NativeImagePtr& image, GraphicsContext& context, const FloatRect& destRect, const FloatRect& srcRect, const IntSize& srcSize, const ImagePaintingOptions& options)
{
    // Subsampling may have given us an image that is smaller than size().
    IntSize subsampledImageSize = nativeImageSize(image);
    if (options.orientation().usesWidthAsHeight())
        subsampledImageSize = subsampledImageSize.transposedSize();

    // srcRect is in the coordinates of the unsubsampled image, so we have to map it to the subsampled image.
    FloatRect adjustedSrcRect = srcRect;
    if (subsampledImageSize != srcSize)
        adjustedSrcRect = mapRect(srcRect, FloatRect({ }, srcSize), FloatRect({ }, subsampledImageSize));

    context.drawNativeImage(image, subsampledImageSize, destRect, adjustedSrcRect, options);
}

void drawNativeImage(const NativeImagePtr& image, GraphicsContext& context, float scaleFactor, const IntPoint& destination, const IntRect& source)
{
    CGContextRef cgContext = context.platformContext();
    CGContextSaveGState(cgContext);

    CGContextClipToRect(cgContext, CGRectMake(destination.x(), destination.y(), source.width(), source.height()));
    CGContextScaleCTM(cgContext, 1, -1);

    CGFloat imageHeight = CGImageGetHeight(image.get()) / scaleFactor;
    CGFloat imageWidth = CGImageGetWidth(image.get()) / scaleFactor;

    CGFloat destX = destination.x() - source.x();
    CGFloat destY = -imageHeight - destination.y() + source.y();

    CGContextDrawImage(cgContext, CGRectMake(destX, destY, imageWidth, imageHeight), image.get());

    CGContextRestoreGState(cgContext);
}

void clearNativeImageSubimages(const NativeImagePtr& image)
{
#if CACHE_SUBIMAGES
    if (image)
        SubimageCacheWithTimer::clearImage(image.get());
#endif
}

}

#endif // USE(CG)
