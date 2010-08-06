/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(3D_CANVAS)

#include "GraphicsContext3D.h"

#include "Image.h"

#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGImage.h>

#include <wtf/RetainPtr.h>

namespace WebCore {

bool GraphicsContext3D::getImageData(Image* image,
                                     unsigned int format,
                                     unsigned int type,
                                     bool premultiplyAlpha,
                                     Vector<uint8_t>& outputVector)
{
    if (!image)
        return false;
    CGImageRef cgImage = image->nativeImageForCurrentFrame();
    if (!cgImage)
        return false;
    int width = CGImageGetWidth(cgImage);
    int height = CGImageGetHeight(cgImage);
    // FIXME: we should get rid of this temporary copy where possible.
    int tempRowBytes = width * 4;
    Vector<uint8_t> tempVector;
    tempVector.resize(height * tempRowBytes);
    // Try to reuse the color space from the image to preserve its colors.
    // Some images use a color space (such as indexed) unsupported by the bitmap context.
    CGColorSpaceRef colorSpace = CGImageGetColorSpace(cgImage);
    bool releaseColorSpace = false;
    CGColorSpaceModel colorSpaceModel = CGColorSpaceGetModel(colorSpace);
    switch (colorSpaceModel) {
    case kCGColorSpaceModelMonochrome:
    case kCGColorSpaceModelRGB:
    case kCGColorSpaceModelCMYK:
    case kCGColorSpaceModelLab:
    case kCGColorSpaceModelDeviceN:
        break;
    default:
        colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGBLinear);
        releaseColorSpace = true;
        break;
    }
    CGContextRef tempContext = CGBitmapContextCreate(tempVector.data(),
                                                     width, height, 8, tempRowBytes,
                                                     colorSpace,
                                                     // FIXME: change this!
                                                     kCGImageAlphaPremultipliedLast);
    if (releaseColorSpace)
        CGColorSpaceRelease(colorSpace);
    if (!tempContext)
        return false;
    CGContextSetBlendMode(tempContext, kCGBlendModeCopy);
    CGContextDrawImage(tempContext,
                       CGRectMake(0, 0, static_cast<CGFloat>(width), static_cast<CGFloat>(height)),
                       cgImage);
    CGContextRelease(tempContext);
    // Pack the pixel data into the output vector.
    unsigned long componentsPerPixel, bytesPerComponent;
    if (!computeFormatAndTypeParameters(format, type, &componentsPerPixel, &bytesPerComponent))
        return false;
    int rowBytes = width * componentsPerPixel * bytesPerComponent;
    outputVector.resize(height * rowBytes);
    CGImageAlphaInfo info = CGImageGetAlphaInfo(cgImage);
    bool hasAlphaChannel = (info != kCGImageAlphaNone
                            && info != kCGImageAlphaNoneSkipLast
                            && info != kCGImageAlphaNoneSkipFirst);
    AlphaOp neededAlphaOp = kAlphaDoNothing;
    if (!premultiplyAlpha && hasAlphaChannel)
        // FIXME: must fetch the image data before the premultiplication step.
        neededAlphaOp = kAlphaDoUnmultiply;
    return packPixels(tempVector.data(), kSourceFormatRGBA8, width, height, 0,
                      format, type, neededAlphaOp, outputVector.data());
}

void GraphicsContext3D::paintToCanvas(const unsigned char* imagePixels, int imageWidth, int imageHeight, int canvasWidth, int canvasHeight, CGContextRef context)
{
    if (!imagePixels || imageWidth <= 0 || imageHeight <= 0 || canvasWidth <= 0 || canvasHeight <= 0 || !context)
        return;
    int rowBytes = imageWidth * 4;
    RetainPtr<CGDataProviderRef> dataProvider(AdoptCF, CGDataProviderCreateWithData(0, imagePixels, rowBytes * imageHeight, 0));
    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGImageRef> cgImage(AdoptCF, CGImageCreate(imageWidth, imageHeight, 8, 32, rowBytes, colorSpace.get(), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
        dataProvider.get(), 0, false, kCGRenderingIntentDefault));
    // CSS styling may cause the canvas's content to be resized on
    // the page. Go back to the Canvas to figure out the correct
    // width and height to draw.
    CGRect rect = CGRectMake(0, 0, canvasWidth, canvasHeight);
    // We want to completely overwrite the previous frame's
    // rendering results.
    CGContextSaveGState(context);
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextSetInterpolationQuality(context, kCGInterpolationNone);
    CGContextDrawImage(context, rect, cgImage.get());
    CGContextRestoreGState(context);
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
