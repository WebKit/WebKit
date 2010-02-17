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

namespace WebCore {

bool GraphicsContext3D::getImageData(Image* image,
                                     Vector<uint8_t>& outputVector,
                                     bool premultiplyAlpha,
                                     bool* hasAlphaChannel,
                                     AlphaOp* neededAlphaOp,
                                     unsigned int* format)
{
    if (!image)
        return false;
    CGImageRef cgImage = image->nativeImageForCurrentFrame();
    if (!cgImage)
        return false;
    int width = CGImageGetWidth(cgImage);
    int height = CGImageGetHeight(cgImage);
    int rowBytes = width * 4;
    CGImageAlphaInfo info = CGImageGetAlphaInfo(cgImage);
    *hasAlphaChannel = (info != kCGImageAlphaNone
                        && info != kCGImageAlphaNoneSkipLast
                        && info != kCGImageAlphaNoneSkipFirst);
    if (!premultiplyAlpha && *hasAlphaChannel)
        // FIXME: must fetch the image data before the premultiplication step
        *neededAlphaOp = kAlphaDoUnmultiply;
    *format = RGBA;
    outputVector.resize(height * rowBytes);
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
    CGContextRef tmpContext = CGBitmapContextCreate(outputVector.data(),
                                                    width, height, 8, rowBytes,
                                                    colorSpace,
                                                    kCGImageAlphaPremultipliedLast);
    if (releaseColorSpace)
        CGColorSpaceRelease(colorSpace);
    if (!tmpContext)
        return false;
    CGContextSetBlendMode(tmpContext, kCGBlendModeCopy);
    CGContextDrawImage(tmpContext,
                       CGRectMake(0, 0, static_cast<CGFloat>(width), static_cast<CGFloat>(height)),
                       cgImage);
    CGContextRelease(tmpContext);
    return true;
}


} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
