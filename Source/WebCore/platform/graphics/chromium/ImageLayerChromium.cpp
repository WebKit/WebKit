/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#if USE(ACCELERATED_COMPOSITING)

#include "ImageLayerChromium.h"

#include "Image.h"
#include "LayerRendererChromium.h"
#include "LayerTexture.h"

#if USE(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#endif

#if PLATFORM(CG)
#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGImage.h>
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {

PassRefPtr<ImageLayerChromium> ImageLayerChromium::create(GraphicsLayerChromium* owner)
{
    return adoptRef(new ImageLayerChromium(owner));
}

ImageLayerChromium::ImageLayerChromium(GraphicsLayerChromium* owner)
    : ContentLayerChromium(owner)
    , m_contents(0)
{
}

void ImageLayerChromium::setContents(Image* contents)
{
    // Check if the image has changed.
    if (m_contents == contents)
        return;
    m_contents = contents;
    setNeedsDisplay();
}

void ImageLayerChromium::updateContentsIfDirty()
{
    ASSERT(layerRenderer());

    // FIXME: Remove this test when tiled layers are implemented.
    if (requiresClippedUpdateRect()) {
        // Use the base version of updateContents which draws a subset of the
        // image to a bitmap, as the pixel contents can't be uploaded directly.
        ContentLayerChromium::updateContentsIfDirty();
        return;
    }

    NativeImagePtr nativeImage = m_contents->nativeImageForCurrentFrame();

#if USE(SKIA)
    // The layer contains an Image.
    NativeImageSkia* skiaImage = static_cast<NativeImageSkia*>(nativeImage);
    const SkBitmap* skiaBitmap = skiaImage;
    IntSize bitmapSize(skiaBitmap->width(), skiaBitmap->height());
    ASSERT(skiaBitmap);
#elif PLATFORM(CG)
    // NativeImagePtr is a CGImageRef on Mac OS X.
    int width = CGImageGetWidth(nativeImage);
    int height = CGImageGetHeight(nativeImage);
    IntSize bitmapSize(width, height);
#endif

    if (m_uploadBufferSize != bitmapSize)
        resizeUploadBufferForImage(bitmapSize);
    m_uploadUpdateRect = IntRect(IntPoint(0, 0), bitmapSize);

#if USE(SKIA)
    SkAutoLockPixels lock(*skiaBitmap);
    // FIXME: do we need to support more image configurations?
    ASSERT(skiaBitmap->config()== SkBitmap::kARGB_8888_Config);
    skiaBitmap->copyPixelsTo(m_uploadPixelData->data(), m_uploadPixelData->size());
#elif PLATFORM(CG)
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
    RetainPtr<CGContextRef> tempContext(AdoptCF, CGBitmapContextCreate(m_uploadPixelData->data(),
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

}
#endif // USE(ACCELERATED_COMPOSITING)
