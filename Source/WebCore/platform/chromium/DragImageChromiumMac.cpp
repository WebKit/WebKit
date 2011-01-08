/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DragImage.h"

#include "Image.h"
#include "NotImplemented.h"
#include <wtf/RetainPtr.h>

#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGImage.h>

namespace WebCore {

IntSize dragImageSize(DragImageRef image)
{
    if (!image)
        return IntSize();
    return IntSize(CGImageGetWidth(image), CGImageGetHeight(image));
}

void deleteDragImage(DragImageRef image)
{
    CGImageRelease(image);
}

DragImageRef scaleDragImage(DragImageRef image, FloatSize scale)
{
    if (!image)
        return 0;
    size_t width = roundf(CGImageGetWidth(image) * scale.width());
    size_t height = roundf(CGImageGetHeight(image) * scale.height());

    RetainPtr<CGColorSpaceRef> deviceRGB(WTF::AdoptCF, CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));
    CGContextRef context = CGBitmapContextCreate(0, width, height, 8, width * 4, deviceRGB.get(), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);

    if (!context)
        return 0;
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
    CGImageRelease(image);

    CGImageRef scaledImage = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    return scaledImage;
}

DragImageRef dissolveDragImageToFraction(DragImageRef image, float delta)
{
    if (!image)
        return 0;
    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);

    RetainPtr<CGColorSpaceRef> deviceRGB(WTF::AdoptCF, CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));
    CGContextRef context = CGBitmapContextCreate(0, width, height, 8, width * 4, deviceRGB.get(), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);

    if (!context)
        return 0;
    // From CGContext.h:
    //     The Porter-Duff "source over" mode is called `kCGBlendModeNormal':
    //       R = S + D*(1 - Sa)
    //  This is the same as NSCompositeSourceOver, which is what -[NSImage dissolveToPoint:fraction:] uses.
    CGContextSetAlpha(context, delta);
    CGContextSetBlendMode(context, kCGBlendModeNormal);
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
    CGImageRelease(image);

    CGImageRef dissolvedImage = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    return dissolvedImage;
}

DragImageRef createDragImageFromImage(Image* image)
{
    if (!image)
        return 0;
    return CGImageCreateCopy(image->nativeImageForCurrentFrame());
}

DragImageRef createDragImageIconForCachedImage(CachedImage*)
{
    notImplemented();
    return 0;
}

} // namespace WebCore
