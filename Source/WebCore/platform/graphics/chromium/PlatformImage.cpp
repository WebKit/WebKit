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

#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"

namespace WebCore {

PlatformImage::PlatformImage()
{
}

void PlatformImage::updateFromImage(NativeImagePtr nativeImage)
{
    // The layer contains an Image.
    NativeImageSkia* skiaImage = static_cast<NativeImageSkia*>(nativeImage);
    ASSERT(skiaImage);
    const SkBitmap& skiaBitmap = skiaImage->bitmap();

    IntSize bitmapSize(skiaBitmap.width(), skiaBitmap.height());

    size_t bufferSize = bitmapSize.width() * bitmapSize.height() * 4;
    if (m_size != bitmapSize) {
        m_pixelData = adoptArrayPtr(new uint8_t[bufferSize]);
        memset(m_pixelData.get(), 0, bufferSize);
        m_size = bitmapSize;
    }

    SkAutoLockPixels lock(skiaBitmap);
    // FIXME: do we need to support more image configurations?
    ASSERT(skiaBitmap.config()== SkBitmap::kARGB_8888_Config);
    skiaBitmap.copyPixelsTo(m_pixelData.get(), bufferSize);
}

} // namespace WebCore
