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

#if PLATFORM(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#endif

namespace WebCore {

PassRefPtr<ImageLayerChromium> ImageLayerChromium::create(GraphicsLayerChromium* owner)
{
    return adoptRef(new ImageLayerChromium(owner));
}

ImageLayerChromium::ImageLayerChromium(GraphicsLayerChromium* owner)
    : LayerChromium(owner)
    , m_contents(0)
{
}

void ImageLayerChromium::setContents(NativeImagePtr contents)
{
    // Check if the image has changed.
    if (m_contents == contents)
        return;
    m_contents = contents;
    setNeedsDisplay();
}

void ImageLayerChromium::updateTextureContents(unsigned textureId)
{
    void* pixels = 0;
    IntRect dirtyRect(m_dirtyRect);
    IntSize requiredTextureSize;
    IntSize bitmapSize;

#if PLATFORM(SKIA)
    // The layer contains an Image.
    NativeImageSkia* skiaImage = static_cast<NativeImageSkia*>(m_contents);
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
#else
#error "Need to implement for your platform."
#endif
    if (pixels)
        updateTextureRect(pixels, bitmapSize, requiredTextureSize,  dirtyRect, textureId);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
