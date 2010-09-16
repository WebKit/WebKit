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

#if PLATFORM(SKIA)
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

void ImageLayerChromium::updateContents()
{
    ASSERT(layerRenderer());

    void* pixels = 0;
    IntRect dirtyRect(m_dirtyRect);
    IntSize requiredTextureSize;
    IntSize bitmapSize;

    NativeImagePtr nativeImage = m_contents->nativeImageForCurrentFrame();

#if PLATFORM(SKIA)
    // The layer contains an Image.
    NativeImageSkia* skiaImage = static_cast<NativeImageSkia*>(nativeImage);
    const SkBitmap* skiaBitmap = skiaImage;
    requiredTextureSize = IntSize(skiaBitmap->width(), skiaBitmap->height());
    ASSERT(skiaBitmap);

    SkAutoLockPixels lock(*skiaBitmap);
    SkBitmap::Config skiaConfig = skiaBitmap->config();
    // FIXME: do we need to support more image configurations?
    if (skiaConfig == SkBitmap::kARGB_8888_Config) {
        pixels = skiaBitmap->getPixels();
        bitmapSize = IntSize(skiaBitmap->width(), skiaBitmap->height());
    }
#elif PLATFORM(CG)
    // NativeImagePtr is a CGImageRef on Mac OS X.
    int width = CGImageGetWidth(nativeImage);
    int height = CGImageGetHeight(nativeImage);
    requiredTextureSize = IntSize(width, height);
    bitmapSize = requiredTextureSize;
    // FIXME: we should get rid of this temporary copy where possible.
    int tempRowBytes = width * 4;
    Vector<uint8_t> tempVector;
    tempVector.resize(height * tempRowBytes);
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
    RetainPtr<CGContextRef> tempContext(AdoptCF, CGBitmapContextCreate(tempVector.data(),
                                                                       width, height, 8, tempRowBytes,
                                                                       colorSpace,
                                                                       kCGImageAlphaPremultipliedLast));
    CGContextSetBlendMode(tempContext.get(), kCGBlendModeCopy);
    CGContextDrawImage(tempContext.get(),
                       CGRectMake(0, 0, static_cast<CGFloat>(width), static_cast<CGFloat>(height)),
                       nativeImage);
    pixels = tempVector.data();
#else
#error "Need to implement for your platform."
#endif
    // FIXME: Remove this test when tiled layers are implemented.
    m_skipsDraw = false;
    if (!layerRenderer()->checkTextureSize(requiredTextureSize)) {
        m_skipsDraw = true;
        return;
    }

    unsigned textureId = m_contentsTexture;
    if (!textureId)
        textureId = layerRenderer()->createLayerTexture();

    if (pixels)
        updateTextureRect(pixels, bitmapSize, requiredTextureSize,  dirtyRect, textureId);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
