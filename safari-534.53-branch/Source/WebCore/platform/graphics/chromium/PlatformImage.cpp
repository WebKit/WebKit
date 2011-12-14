/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "PlatformImage.h"

#if USE(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#elif USE(CG)
#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGImage.h>
#include <wtf/RetainPtr.h>
#else
#error "Need to implement for your platform"
#endif

namespace WebCore {

PlatformImage::PlatformImage()
{
}

void PlatformImage::updateFromImage(NativeImagePtr nativeImage)
{
#if USE(SKIA)
    // The layer contains an Image.
    NativeImageSkia* skiaImage = static_cast<NativeImageSkia*>(nativeImage);
    const SkBitmap* skiaBitmap = skiaImage;

    IntSize bitmapSize(skiaBitmap->width(), skiaBitmap->height());
    ASSERT(skiaBitmap);
#elif USE(CG)
    // NativeImagePtr is a CGImageRef on Mac OS X.
    int width = CGImageGetWidth(nativeImage);
    int height = CGImageGetHeight(nativeImage);
    IntSize bitmapSize(width, height);
#endif

    size_t bufferSize = bitmapSize.width() * bitmapSize.height() * 4;
    if (m_size != bitmapSize) {
        m_pixelData = adoptArrayPtr(new uint8_t[bufferSize]);
        memset(m_pixelData.get(), 0, bufferSize);
        m_size = bitmapSize;
    }

#if USE(SKIA)
    SkAutoLockPixels lock(*skiaBitmap);
    // FIXME: do we need to support more image configurations?
    ASSERT(skiaBitmap->config()== SkBitmap::kARGB_8888_Config);
    skiaBitmap->copyPixelsTo(m_pixelData.get(), bufferSize);
#elif USE(CG)
    // FIXME: we should get rid of this temporary copy where possible.
    int tempRowBytes = width * 4;
    // Note we do not zero this vector since we are going to
    // completely overwrite its contents with the image below.
    // Try to reuse the color space from the image to preserve its colors.
    // Some images use a color space (such as indexed) unsupported by the bitmap context.
    RetainPtr<CGColorSpaceRef> colorSpaceReleaser;
    CGColorSpaceRef colorSpace = CGImageGetColorSpace(nativeImage);
    CGColorSpaceModel colorSpaceModel = CGColorSpaceGetModel(colorSpace);
    switch (colorSpaceModel) {
    case kCGColorSpaceModelMonochrome:
    case kCGColorSpaceModelRGB:
    case kCGColorSpaceModelCMYK:
    case kCGColorSpaceModelLab:
    case kCGColorSpaceModelDeviceN:
        break;
    default:
        colorSpaceReleaser.adoptCF(CGColorSpaceCreateDeviceRGB());
        colorSpace = colorSpaceReleaser.get();
        break;
    }
    RetainPtr<CGContextRef> tempContext(AdoptCF, CGBitmapContextCreate(m_pixelData.get(),
                                                                       width, height, 8, tempRowBytes,
                                                                       colorSpace,
                                                                       kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));
    CGContextSetBlendMode(tempContext.get(), kCGBlendModeCopy);
    CGContextDrawImage(tempContext.get(),
                       CGRectMake(0, 0, static_cast<CGFloat>(width), static_cast<CGFloat>(height)),
                       nativeImage);
#else
#error "Need to implement for your platform."
#endif
}

} // namespace WebCore
